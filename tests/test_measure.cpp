#include <cmath>

#include <QtTest>
#include <QFile>
#include <QVector>

#include "core/MeasureOps.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgspointxy.h>

class TestMeasure : public QObject {
  Q_OBJECT
private slots:
  void length10mOn5187();
  void rectangleArea200m2();
  void formatters();
};

void TestMeasure::length10mOn5187() {
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
  QVERIFY(crs.isValid());
  const QgsCoordinateTransformContext ctx;
  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0)};
  const double len = MeasureOps::lineLengthMeters(pts, crs, ctx);
  QVERIFY2(std::abs(len - 10.0) < 0.02, qPrintable(QString::number(len)));
}

void TestMeasure::rectangleArea200m2() {
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
  QVERIFY(crs.isValid());
  const QgsCoordinateTransformContext ctx;
  const QVector<QgsPointXY> pts{QgsPointXY(200000.0, 450000.0), QgsPointXY(200010.0, 450000.0),
                                QgsPointXY(200010.0, 450020.0), QgsPointXY(200000.0, 450020.0)};
  const double area = MeasureOps::polygonAreaSquareMeters(pts, crs, ctx);
  QVERIFY2(std::abs(area - 200.0) < 0.5, qPrintable(QString::number(area)));
  const double peri = MeasureOps::polygonPerimeterMeters(pts, crs, ctx);
  QVERIFY2(std::abs(peri - 60.0) < 0.2, qPrintable(QString::number(peri)));
}

void TestMeasure::formatters() {
  QCOMPARE(MeasureOps::formatLengthM(1.234), QStringLiteral("1.234 m"));
  QCOMPARE(MeasureOps::formatLengthM(12.3), QStringLiteral("12.30 m"));
  QVERIFY(MeasureOps::formatAreaM2(42.1).contains(QStringLiteral("㎡")));
  QVERIFY(MeasureOps::formatAreaM2(20000.0).contains(QStringLiteral("ha")));
}

#include "test_measure.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestMeasure tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
