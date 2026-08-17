#include <cmath>

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>

#include "core/GeorefService.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfields.h>

class TestGeoref : public QObject {
  Q_OBJECT
private slots:
  void helmertTranslateScale();
  void helmertRotate90();
  void affineThreePoints();
  void invertRoundTrip();
  void fitRasterToExtentWorldFile();
  void vectorAffineDoesNotNeedControlPoints();
  void domainLayersNotAlignable();
  void pngWithoutWorldFileLooksUnreferenced();
};

static bool nearly(double a, double b, double eps = 1e-6) {
  return std::abs(a - b) <= eps;
}

void TestGeoref::helmertTranslateScale() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 200000, 450000});
  p.append({10, 0, 200020, 450000});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QCOMPARE(a.pairCount, 2);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 5, 0, &mx, &my));
  QVERIFY2(nearly(mx, 200010, 1e-4), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 450000, 1e-4), qPrintable(QString::number(my)));
  QVERIFY(a.rmsMeters < 1e-6);
}

void TestGeoref::helmertRotate90() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 0, 0});
  p.append({1, 0, 0, 1});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 0, 1, &mx, &my));
  QVERIFY2(nearly(mx, -1, 1e-6), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 0, 1e-6), qPrintable(QString::number(my)));
}

void TestGeoref::affineThreePoints() {
  QVector<GeorefService::Pair> p;
  p.append({0, 0, 100, 200});
  p.append({10, 0, 120, 200});
  p.append({0, 10, 100, 230});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QCOMPARE(a.pairCount, 3);
  double mx = 0, my = 0;
  QVERIFY(GeorefService::transform(a, 10, 10, &mx, &my));
  QVERIFY2(nearly(mx, 120, 1e-4), qPrintable(QString::number(mx)));
  QVERIFY2(nearly(my, 230, 1e-4), qPrintable(QString::number(my)));
}

void TestGeoref::invertRoundTrip() {
  QVector<GeorefService::Pair> p;
  p.append({2, 3, 500, 700});
  p.append({12, 3, 520, 705});
  p.append({2, 13, 498, 730});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  double mx = 0, my = 0, sx = 0, sy = 0;
  QVERIFY(GeorefService::transform(a, 7, 8, &mx, &my));
  QVERIFY(GeorefService::invert(a, mx, my, &sx, &sy));
  QVERIFY2(nearly(sx, 7, 1e-6), qPrintable(QString::number(sx)));
  QVERIFY2(nearly(sy, 8, 1e-6), qPrintable(QString::number(sy)));
}

void TestGeoref::fitRasterToExtentWorldFile() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_wf_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString png = dir + QStringLiteral("/scan.png");
  QImage img(100, 50, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));

  const QgsRectangle ext(200000, 450000, 200200, 450100);
  const GeorefService::Affine a = GeorefService::fitRasterToExtent(100, 50, ext);
  QVERIFY(a.valid);
  QString err;
  QVERIFY2(GeorefService::writeWorldFile(png, a, &err), qPrintable(err));
  const QString pgw = dir + QStringLiteral("/scan.pgw");
  QVERIFY(QFile::exists(pgw));

  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY(GeorefService::writeSidecarPrj(png, crs, &err));
  QVERIFY(QFile::exists(GeorefService::prjPathFor(png)));

  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::applyWorldFileToRaster(rl, a, crs, &err));
  QCOMPARE(rl->crs().authid(), QStringLiteral("EPSG:5186"));
  const QgsRectangle got = rl->extent();
  QVERIFY2(nearly(got.xMinimum(), 200000, 1.0), qPrintable(QString::number(got.xMinimum())));
  QVERIFY2(nearly(got.yMaximum(), 450100, 1.0), qPrintable(QString::number(got.yMaximum())));
  delete rl;
}

void TestGeoref::vectorAffineDoesNotNeedControlPoints() {
  auto* vl = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                QStringLiteral("cad"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  QgsPolylineXY line;
  line << QgsPointXY(0, 0) << QgsPointXY(10, 0) << QgsPointXY(10, 10);
  f.setGeometry(QgsGeometry::fromPolylineXY(line));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());

  QHash<qint64, QgsGeometry> originals;
  QgsFeatureIterator it = vl->getFeatures();
  QgsFeature g;
  while (it.nextFeature(g)) originals.insert(g.id(), g.geometry());
  QVERIFY(!originals.isEmpty());

  QVector<GeorefService::Pair> p;
  p.append({0, 0, 200000, 450000});
  p.append({10, 0, 200100, 450000});
  const GeorefService::Affine a = GeorefService::fromPairs(p);
  QVERIFY(a.valid);
  QString err;
  QVERIFY2(GeorefService::applyAffineToVector(
               vl, a, originals, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")), &err),
           qPrintable(err));

  QgsFeature out;
  QVERIFY(vl->getFeatures().nextFeature(out));
  const QgsPointXY start = out.geometry().asPolyline().first();
  QVERIFY2(nearly(start.x(), 200000, 1e-3), qPrintable(QString::number(start.x())));
  QVERIFY2(nearly(start.y(), 450000, 1e-3), qPrintable(QString::number(start.y())));
  vl->updateExtents();
  const QgsRectangle moved = vl->extent();
  QVERIFY2(moved.xMinimum() > 199990.0, qPrintable(QString::number(moved.xMinimum())));
  QVERIFY2(moved.yMinimum() > 449990.0, qPrintable(QString::number(moved.yMinimum())));
  QVERIFY(!GeorefService::isDomainSurveyLayer(vl));
  QVERIFY(GeorefService::isAlignableLayer(vl));
  delete vl;
}

void TestGeoref::domainLayersNotAlignable() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_dom_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("g"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(sa->isValid());
  LayerOps::markSurveyLayer(sa, QStringLiteral("survey_area"));
  QVERIFY(GeorefService::isDomainSurveyLayer(sa));
  QVERIFY(!GeorefService::isAlignableLayer(sa));
  delete sa;
}

void TestGeoref::pngWithoutWorldFileLooksUnreferenced() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_png_")
                                            + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString png = dir + QStringLiteral("/scan.png");
  QImage img(80, 40, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));
  auto* rl = new QgsRasterLayer(png, QStringLiteral("scan"), QStringLiteral("gdal"));
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY(GeorefService::looksUnreferencedRaster(rl));
  QVERIFY(!QFile::exists(GeorefService::worldFilePathFor(png)));
  delete rl;
}

#include "test_georef.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestGeoref tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
