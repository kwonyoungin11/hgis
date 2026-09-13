#include <QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsvectorlayer.h>
#include "core/TopographicShapefileCache.h"
#include <memory>

namespace {
QByteArray hash(const QString& path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}
bool fixture(const QString& path, double shift = 0.) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("DXF"); if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
  if (!dataset) return false;
  auto* layer = dataset->CreateLayer("entities", nullptr, wkbUnknown, nullptr); if (!layer) return false;
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> f(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
    f->SetField("Layer", i == 4 ? "unknown" : (i == 1 ? "F0017114" : "F0017111"));
    OGRLineString line;
    if (i == 3) { line.addPoint(200000.+shift, 450000.+i*10.); line.addPoint(200100.+shift, 450100.+i*10.); }
    else { line.addPoint(200000.+shift, 450000.+i*10., 50.); line.addPoint(200100.+shift, 450100.+i*10., i == 2 ? 60. : 50.); }
    if (f->SetGeometry(&line) != OGRERR_NONE || layer->CreateFeature(f.get()) != OGRERR_NONE) return false;
  }
  return true;
}
}
class TopographicShapefileTest : public QObject {
  Q_OBJECT
private slots:
  void sourceChangesAfterInitialHashCannotPublishOrReuseCache() {
    QTemporaryDir originals, catalog, cache;
    const QString path = originals.filePath(QStringLiteral("37701.dxf")); QVERIFY(fixture(path));
    QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const auto bytes = original.readAll(); original.close();
    auto records = TopographicCatalog::scan(originals.path(), nullptr, catalog.path()).records;
    for (auto& r : records) { r.crsWkt = QgsCoordinateReferenceSystem("EPSG:5186").toWkt(); r.sourceSheet = "37701"; }
    auto restore = [&] { QFile f(path); if (!f.open(QIODevice::WriteOnly)) return false; return f.write(bytes) == bytes.size(); };
    int calls = 0; bool changed = false;
    // Call 3 occurs after the first file digest has been read and closed.
    auto mutate = [&] { if (++calls == 3) changed = fixture(path, 1000.); return false; };
    const auto rejected = TopographicShapefileCache::prepare(records, cache.path(), {}, mutate);
    QVERIFY(changed); QVERIFY(!rejected.error.isEmpty()); QVERIFY(rejected.records.isEmpty()); QVERIFY(!rejected.reused);
    QCOMPARE(QDir(cache.path()).entryList({"*.json"}, QDir::Files).size(), 0);
    QVERIFY(restore());
    const auto valid = TopographicShapefileCache::prepare(records, cache.path(), {});
    QVERIFY2(valid.error.isEmpty(), qPrintable(valid.error)); QVERIFY(!valid.records.isEmpty());
    calls = 0; changed = false;
    const auto rejectedReuse = TopographicShapefileCache::prepare(records, cache.path(), {}, mutate);
    QVERIFY(changed); QVERIFY(!rejectedReuse.error.isEmpty()); QVERIFY(rejectedReuse.records.isEmpty()); QVERIFY(!rejectedReuse.reused);
    QVERIFY(restore());
    const auto reused = TopographicShapefileCache::prepare(records, cache.path(), {});
    QVERIFY2(reused.error.isEmpty(), qPrintable(reused.error)); QVERIFY(reused.reused);
    for (const auto& r : reused.records) QVERIFY(r.extent.xMaximum() <= 200100.);
  }
  void convertsGroupedReadOnlySourcesAndReusesCompleteCache() {
    QTemporaryDir originals, catalog, cache;
    const QString source = originals.filePath(QStringLiteral("37701.dxf")); QVERIFY(fixture(source));
    const auto originalHash = hash(source);
    auto scanned = TopographicCatalog::scan(originals.path(), nullptr, catalog.path()); QVERIFY(scanned.error.isEmpty());
    for (auto& r : scanned.records) { r.crsWkt = QgsCoordinateReferenceSystem("EPSG:5186").toWkt(); r.sourceSheet = "37701"; }
    const auto made = TopographicShapefileCache::prepare(scanned.records, cache.path(), {});
    QVERIFY2(made.error.isEmpty(), qPrintable(made.error)); QVERIFY(!made.canceled); QVERIFY(!made.reused);
    QVERIFY(made.records.size() < scanned.records.size()+1); QCOMPARE(made.filesWritten, made.records.size());
    int count = 0, validZ = 0, missingZ = 0, unknown = 0; QSet<QString> codes;
    for (const auto& r : made.records) {
      QVERIFY(r.source.endsWith(".shp")); QVERIFY(r.cadLayer.isEmpty()); QVERIFY(r.geometryType.isEmpty());
      QCOMPARE(r.sourceSheet, QString("37701")); QVERIFY(r.hasExtent);
      if (r.category == TopographicCatalog::Category::Unknown) ++unknown;
      QgsVectorLayer::LayerOptions options; options.forceReadOnly = true;
      QgsVectorLayer layer(r.source, "SHP", "ogr", options); QVERIFY(layer.isValid());
      QCOMPARE(layer.crs().authid(), QString("EPSG:5186")); QVERIFY(!layer.startEditing());
      QgsFeature feature; auto features = layer.getFeatures();
      while (features.nextFeature(feature)) {
        ++count; codes.insert(feature.attribute("Layer").toString());
        if (feature.attribute("z_valid").toInt() == 1) { ++validZ; QCOMPARE(feature.attribute("elevation").toDouble(), 50.); }
        else { ++missingZ; QVERIFY(feature.attribute("elevation").isNull()); }
      }
    }
    QCOMPARE(count, 5); QCOMPARE(validZ, 3); QCOMPARE(missingZ, 2); QCOMPARE(unknown, 1); QCOMPARE(codes.size(), 3);
    QCOMPARE(hash(source), originalHash);
    const auto again = TopographicShapefileCache::prepare(scanned.records, cache.path(), {});
    QVERIFY2(again.error.isEmpty(), qPrintable(again.error)); QVERIFY(again.reused); QCOMPARE(again.filesWritten, 0);
    QCOMPARE(again.records.first().source, made.records.first().source);
    // A broken sidecar must trigger a new immutable dataset, never reuse partial output.
    QFile dbf(made.records.first().source.chopped(3) + "dbf"); QVERIFY(dbf.open(QIODevice::WriteOnly)); dbf.write("broken"); dbf.close();
    const auto repaired = TopographicShapefileCache::prepare(scanned.records, cache.path(), {});
    QVERIFY2(repaired.error.isEmpty(), qPrintable(repaired.error)); QVERIFY(!repaired.reused);
    QVERIFY(repaired.records.first().source != made.records.first().source); QCOMPARE(hash(source), originalHash);
  }
  void cancellationAndUnknownCrsNeverPublishPartialRecords() {
    QTemporaryDir originals, catalog, cache;
    QVERIFY(fixture(originals.filePath("37701.dxf")));
    auto records = TopographicCatalog::scan(originals.path(), nullptr, catalog.path()).records;
    QVERIFY(!records.isEmpty());
    auto rejected = TopographicShapefileCache::prepare(records, cache.path(), {});
    QVERIFY(!rejected.error.isEmpty()); QVERIFY(rejected.records.isEmpty());
    for (auto& r : records) { r.crsWkt = QgsCoordinateReferenceSystem("EPSG:5186").toWkt(); r.sourceSheet = "37701"; }
    int callbacks = 0;
    auto canceled = TopographicShapefileCache::prepare(records, cache.path(), {}, [&] { return ++callbacks >= 6; });
    QVERIFY(canceled.canceled); QVERIFY(canceled.records.isEmpty());
    QCOMPARE(QDir(cache.path()).entryList({"*.json"}, QDir::Files).size(), 0);
    auto retried = TopographicShapefileCache::prepare(records, cache.path(), {});
    QVERIFY2(retried.error.isEmpty(), qPrintable(retried.error)); QVERIFY(!retried.records.isEmpty()); QVERIFY(!retried.reused);
  }
};
int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis(); GDALAllRegister();
  TopographicShapefileTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis(); return result;
}
#include "test_topographic_shapefile.moc"
