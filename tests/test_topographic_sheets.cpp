#include <QtTest>
#include <QSet>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeedback.h>
#include "core/TopographicSheets.h"
#include <cmath>
#include <limits>
#include <numbers>

using namespace TopographicSheets;
namespace {
QgsPointXY projected(double longitude, double latitude, const QString& authId = QStringLiteral("EPSG:5186")) {
  return QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
      QgsCoordinateReferenceSystem(authId), QgsCoordinateTransformContext()).transform(QgsPointXY(longitude, latitude));
}
QSet<QString> numbers(const Result& result) {
  QSet<QString> values;
  for (const auto& sheet : result.sheets) values.insert(sheet.number);
  return values;
}
}
class TopographicSheetsTest : public QObject {
  Q_OBJECT
private slots:
  void officialExamplesAndExactGrid() {
    // Official 도엽코드 및 도곽의 크기, 1:25000 example (flSeq=87548299).
    const auto example = resolve(QStringLiteral("367154"));
    QVERIFY(example); QCOMPARE(example->geographicExtent, QgsRectangle(127.625, 36., 127.75, 36.125));
    QCOMPARE(example->zone, Zone::Central); QVERIFY(example->crsAuthId.isEmpty());
    // Actual NGII public suchiQuery 1:25000 DXF: 378044 = 강릉.
    const auto gangneung = resolve(QStringLiteral("378044"), SourceBasis::World2010);
    QVERIFY(gangneung); QCOMPARE(gangneung->geographicExtent, QgsRectangle(128.875, 37.75, 129., 37.875));
    QCOMPARE(gangneung->crsAuthId, QStringLiteral("EPSG:5187"));
    QCOMPARE(gangneung->geographicExtent.width(), 7.5 / 60.);
    QCOMPARE(gangneung->geographicExtent.height(), 7.5 / 60.);
  }
  void rejectsMalformedAndUnsupportedLongitudeCodes() {
    for (const auto& bad : {QStringLiteral("36715"), QStringLiteral("3671544"), QStringLiteral("367004"),
         QStringLiteral("367174"), QStringLiteral("367150"), QStringLiteral("367155"), QStringLiteral("36715A"),
         QStringLiteral(" 367154"), QStringLiteral("３６７１５４"), QStringLiteral("363154"), QStringLiteral("362154")})
      QVERIFY2(!resolve(bad), qPrintable(bad));
  }
  void datumNameDoesNotDetermineFalseNorthing() {
    for (const auto basis : {SourceBasis::Unknown, SourceBasis::WorldPre2010, SourceBasis::Tokyo}) {
      const auto sheet = resolve(QStringLiteral("367154"), basis);
      QVERIFY(sheet); QVERIFY(sheet->crsAuthId.isEmpty());
    }
    const auto point = QgsPointXY(127.6875, 36.0625);
    const QgsCoordinateReferenceSystem geo(QStringLiteral("EPSG:4737"));
    const auto oldPosition = QgsCoordinateTransform(geo, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5181")),
        QgsCoordinateTransformContext()).transform(point);
    const auto currentPosition = projected(point.x(), point.y());
    QVERIFY(std::abs(currentPosition.x() - oldPosition.x()) < 1e-6);
    QVERIFY(std::abs(currentPosition.y() - oldPosition.y() - 100000.) < 1e-6);
    const QList<QString> codes = {QStringLiteral("364011"), QStringLiteral("366011"),
                                 QStringLiteral("368011"), QStringLiteral("370011")};
    for (int i = 0; i < codes.size(); ++i) {
      const auto sheet = resolve(codes.at(i), SourceBasis::World2010);
      QVERIFY(sheet); QCOMPARE(sheet->crsAuthId, QStringLiteral("EPSG:%1").arg(5185 + i));
      QVERIFY(!zoneName(sheet->zone).isEmpty());
    }
  }
  void metricRadiusAndCrsProduceSameCoverage() {
    const auto central = select(projected(128.94, 37.76), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
        5., QgsCoordinateTransformContext());
    const auto east = select(projected(128.94, 37.76, QStringLiteral("EPSG:5187")),
        QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")), 5., QgsCoordinateTransformContext());
    QVERIFY2(central.error.isEmpty(), qPrintable(central.error)); QVERIFY(east.error.isEmpty());
    QCOMPARE(numbers(central), numbers(east)); QVERIFY(numbers(east).contains(QStringLiteral("378044")));
    QVERIFY(!central.searchArea.isEmpty());
    QVERIFY(std::abs(central.searchArea.boundingBox().width() - 10000.) < 1e-6);
    QVERIFY(std::abs(central.searchArea.area() / (std::numbers::pi * 25000000.) - 1.) < 0.001);
    const auto tiny = select(projected(128.9375, 37.8125, QStringLiteral("EPSG:5187")),
        QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")),
        .01, QgsCoordinateTransformContext());
    QVERIFY(tiny.error.isEmpty()); QCOMPARE(numbers(tiny), QSet<QString>{QStringLiteral("378044")});
  }
  void includesBothSidesOfZoneAndFourWayGridBoundary() {
    const auto result = select(projected(128., 36.875), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
        .001, QgsCoordinateTransformContext(), SourceBasis::World2010);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.sheets.size(), 4);
    const auto ids = numbers(result);
    QVERIFY(ids.contains(QStringLiteral("367042"))); QVERIFY(ids.contains(QStringLiteral("367044")));
    QVERIFY(ids.contains(QStringLiteral("368011"))); QVERIFY(ids.contains(QStringLiteral("368013")));
    QSet<QString> crs;
    for (const auto& sheet : result.sheets) crs.insert(sheet.crsAuthId);
    QCOMPARE(crs, (QSet<QString>{QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")}));
  }
  void includesCircleTouchingCornerWithoutIncludingFarCells() {
    const auto corner = projected(127.75, 36.125);
    const QgsPointXY center(corner.x() - 3000., corner.y() - 4000.);
    const auto result = select(center, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
        5., QgsCoordinateTransformContext());
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    // NE cell is touched at its SW corner. A polygonal circle can otherwise miss it.
    QVERIFY(numbers(result).contains(QStringLiteral("367161")));
    QVERIFY(!numbers(result).contains(QStringLiteral("367041")));
  }
  void invalidAndCanceledRequestsDoNotReturnPartialData() {
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
    const auto center = projected(127.7, 36.1);
    for (double radius : {0., -1., 100.001, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
      const auto result = select(center, crs, radius, QgsCoordinateTransformContext());
      QVERIFY(!result.error.isEmpty()); QVERIFY(result.sheets.isEmpty()); QVERIFY(result.searchArea.isNull());
    }
    QVERIFY(!select(center, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")), 5.,
        QgsCoordinateTransformContext()).error.isEmpty());
    QgsFeedback feedback; feedback.cancel();
    const auto canceled = select(center, crs, 5., QgsCoordinateTransformContext(), SourceBasis::Unknown, &feedback);
    QVERIFY(canceled.canceled); QVERIFY(canceled.sheets.isEmpty()); QVERIFY(canceled.searchArea.isNull());
    int checks = 0;
    const auto midScan = select(center, crs, 100., QgsCoordinateTransformContext(), SourceBasis::Unknown, nullptr,
        [&checks] { return ++checks >= 4; });
    QVERIFY(midScan.canceled); QVERIFY(midScan.sheets.isEmpty()); QVERIFY(midScan.searchArea.isNull());
  }
};
int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result;
  { TopographicSheetsTest tests; result = QTest::qExec(&tests, argc, argv); }
  QgsApplication::exitQgis(); return result;
}
#include "test_topographic_sheets.moc"
