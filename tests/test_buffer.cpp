#include <QtTest>
#include <QFile>
#include <cmath>

#include "core/BufferAnalysis.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>

class TestBuffer : public QObject {
  Q_OBJECT
private slots:
  void ringHasGapAndLabel();
  void offsetIs500mFromSource();
};

void TestBuffer::ringHasGapAndLabel() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* src = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("site"),
                                 QStringLiteral("memory"));
  QVERIFY(src->isValid());
  QVERIFY(src->startEditing());
  QgsFeature f(src->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200080, 450060)));
  QVERIFY(src->addFeature(f));
  QVERIFY(src->commitChanges());
  proj.addMapLayer(src);

  QString err;
  QVERIFY2(BufferAnalysis::addDistanceRing(&proj, nullptr, src, 500.0, &err), qPrintable(err));
  QgsVectorLayer* ring = nullptr;
  QgsVectorLayer* lab = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    const QString k = LayerOps::layerKeyOf(l);
    if (k == QLatin1String("user:buffer_ring_500"))
      ring = qobject_cast<QgsVectorLayer*>(l);
    if (k == QLatin1String("user:buffer_label_500"))
      lab = qobject_cast<QgsVectorLayer*>(l);
  }
  QVERIFY(ring);
  QVERIFY(lab);
  QCOMPARE(int(ring->featureCount()), 1);
  QCOMPARE(int(lab->featureCount()), 4);
  bool dashed = true;
  LayerOps::readSimpleVectorStyle(ring, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  &dashed);
  QVERIFY2(!dashed, "ring must be a solid line; dash is a 도형 색 option");
  QgsFeature lf;
  QVERIFY(lab->getFeatures().nextFeature(lf));
  QCOMPARE(lf.attribute(QStringLiteral("label")).toString(), QStringLiteral("500m"));

  const QgsRectangle ext = ring->extent();
  QVERIFY2(std::abs(ext.xMinimum() - 199500) < 12.0, qPrintable(QString::number(ext.xMinimum())));
  QVERIFY2(std::abs(ext.xMaximum() - 200580) < 12.0, qPrintable(QString::number(ext.xMaximum())));
  QVERIFY2(std::abs(ext.yMinimum() - 449500) < 12.0, qPrintable(QString::number(ext.yMinimum())));
  QVERIFY2(std::abs(ext.yMaximum() - 450560) < 12.0, qPrintable(QString::number(ext.yMaximum())));
}

void TestBuffer::offsetIs500mFromSource() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* src = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("site"),
                                 QStringLiteral("memory"));
  QVERIFY(src->isValid());
  QVERIFY(src->startEditing());
  QgsFeature f(src->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200026, 450026)));
  QVERIFY(src->addFeature(f));
  QVERIFY(src->commitChanges());
  proj.addMapLayer(src);
  QString err;
  QVERIFY2(BufferAnalysis::addDistanceRing(&proj, nullptr, src, 500.0, &err), qPrintable(err));
  QgsVectorLayer* ring = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (LayerOps::layerKeyOf(l) == QLatin1String("user:buffer_ring_500"))
      ring = qobject_cast<QgsVectorLayer*>(l);
  }
  QVERIFY(ring);
  QgsFeature srcF;
  QVERIFY(src->getFeatures().nextFeature(srcF));
  const QgsGeometry site = srcF.geometry();
  QgsFeature rf;
  QVERIFY(ring->getFeatures().nextFeature(rf));
  const QgsGeometry ringG = rf.geometry();
  QVERIFY(!ringG.isEmpty());
  // Vertices of a rectangular offset are the corners (~500√2). Measure the
  // line itself and samples along it — that is the 500m distance.
  const double minD = site.distance(ringG);
  QVERIFY2(minD > 490.0 && minD < 510.0, qPrintable(QStringLiteral("minD=%1").arg(minD)));
  double maxD = minD;
  const double len = ringG.length();
  QVERIFY(len > 100.0);
  for (int i = 0; i <= 48; ++i) {
    const QgsGeometry pt = ringG.interpolate(len * double(i) / 48.0);
    if (pt.isEmpty()) continue;
    const double d = site.distance(pt);
    if (d > maxD) maxD = d;
  }
  QVERIFY2(maxD >= 500.0 && maxD < 720.0, qPrintable(QStringLiteral("maxD=%1").arg(maxD)));
}

#include "test_buffer.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestBuffer tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
