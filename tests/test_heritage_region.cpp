#include <QtTest>
#include <QUrlQuery>
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#include "core/HeritageRegionResolver.h"

// 계획서 2026-09-11-heritage-intranet-nearby-sites.md 7.1 을 붙잡는다.
// 인트라넷 검색 단위는 시/군 하나다. 읍면동 이하로 쪼개지 않는다.
class HeritageRegionTest : public QObject {
  Q_OBJECT
private slots:
  void addressTextSplitsIntoSidoAndCity() {
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("경상북도 안동시 풍천면 하회리 100"));
    QVERIFY(r.ok());
    QCOMPARE(r.sido, QStringLiteral("경상북도"));
    QCOMPARE(r.city, QStringLiteral("안동시"));
    QCOMPARE(r.display(), QStringLiteral("경상북도 안동시"));
  }

  void countryPrefixAndShortSidoStillWork() {
    const HeritageRegion a =
        HeritageRegionResolver::fromAddressText(QStringLiteral("대한민국 경상북도 안동시"));
    QCOMPARE(a.city, QStringLiteral("안동시"));
    // 강원도·전라북도는 특별자치도로 바뀌었다. 옛 표기로 와도 같은 곳이어야 한다.
    const HeritageRegion b =
        HeritageRegionResolver::fromAddressText(QStringLiteral("강원도 춘천시 효자동"));
    QCOMPARE(b.sido, QStringLiteral("강원특별자치도"));
    QCOMPARE(b.city, QStringLiteral("춘천시"));
  }

  void districtInsideBigCityFoldsToTheCity() {
    // 사이트 시군구 드롭다운은 「수원시」 단위다. 「수원시 장안구」로 와도 접어야 한다.
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("경기도 수원시 장안구 파장동"));
    QCOMPARE(r.sido, QStringLiteral("경기도"));
    QCOMPARE(r.city, QStringLiteral("수원시"));
  }

  void metropolitanDistrictIsTheCityUnit() {
    const HeritageRegion r =
        HeritageRegionResolver::fromAddressText(QStringLiteral("서울특별시 종로구 세종로"));
    QCOMPARE(r.sido, QStringLiteral("서울특별시"));
    QCOMPARE(r.city, QStringLiteral("종로구"));
  }

  void garbageDoesNotPretendToResolve() {
    QVERIFY(!HeritageRegionResolver::fromAddressText(QString()).ok());
    QVERIFY(!HeritageRegionResolver::fromAddressText(QStringLiteral("어딘가 알 수 없는 곳")).ok());
  }

  void vworldResponseIsParsed() {
    const QByteArray body = QStringLiteral(R"({"response":{"status":"OK","result":[
      {"type":"parcel","text":"경상북도 안동시 풍천면 하회리 산1",
       "structure":{"level0":"대한민국","level1":"경상북도","level2":"안동시","level3":"풍천면"}}]}})")
                                .toUtf8();
    const HeritageRegion r = HeritageRegionResolver::parseAddress(body);
    QVERIFY(r.ok());
    QCOMPARE(r.city, QStringLiteral("안동시"));
    QCOMPARE(r.source, QStringLiteral("vworld"));
  }

  void failedResponseIsNotTreatedAsSuccess() {
    const QByteArray body =
        QStringLiteral(R"({"response":{"status":"NOT_FOUND","result":[]}})").toUtf8();
    QVERIFY(!HeritageRegionResolver::parseAddress(body).ok());
    QVERIFY(!HeritageRegionResolver::parseAddress(QByteArray("<html>error</html>")).ok());
  }

  void addressUrlCarriesPointAndKey() {
    const QUrl url = HeritageRegionResolver::buildAddressUrl(QStringLiteral("KEY123"), 128.5, 36.5);
    const QUrlQuery q(url);
    QCOMPARE(url.host(), QStringLiteral("api.vworld.kr"));
    QCOMPARE(q.queryItemValue(QStringLiteral("request")), QStringLiteral("getAddress"));
    QCOMPARE(q.queryItemValue(QStringLiteral("crs")), QStringLiteral("EPSG:4326"));
    QVERIFY(q.queryItemValue(QStringLiteral("point")).startsWith(QStringLiteral("128.5")));
    QCOMPARE(q.queryItemValue(QStringLiteral("key")), QStringLiteral("KEY123"));
  }

  void lookupPointStaysInsideAConcaveSurveyArea() {
    // ㄷ 자 조사구역. 무게중심은 도형 밖으로 나간다.
    const QgsGeometry area = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((0 0, 30 0, 30 10, 10 10, 10 20, 30 20, 30 30, 0 30, 0 0))"));
    QVERIFY(!area.isNull());
    const QgsPointXY inside = HeritageRegionResolver::lookupPoint(area);
    QVERIFY2(area.contains(QgsGeometry::fromPointXY(inside)),
             qPrintable(QStringLiteral("%1,%2").arg(inside.x()).arg(inside.y())));
  }

  void projectCrsPointBecomesLonLat() {
    // 5186 안동 부근 좌표가 위경도로 제대로 넘어가는지.
    const QgsPointXY wgs = HeritageRegionResolver::toWgs84(
        QgsPointXY(388000., 400000.), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QVERIFY2(wgs.x() > 124. && wgs.x() < 132., qPrintable(QString::number(wgs.x())));
    QVERIFY2(wgs.y() > 33. && wgs.y() < 39., qPrintable(QString::number(wgs.y())));
  }

  void boundaryLayerFallbackFindsTheCity() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:4326&field=full_nm:string(80)"),
        QStringLiteral("행정경계"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    layer->startEditing();
    QgsFeature f(layer->fields());
    f.setGeometry(QgsGeometry::fromWkt(
        QStringLiteral("POLYGON((128.5 36.4, 128.9 36.4, 128.9 36.7, 128.5 36.7, 128.5 36.4))")));
    f.setAttribute(QStringLiteral("full_nm"), QStringLiteral("경상북도 안동시 풍천면"));
    layer->addFeature(f);
    layer->commitChanges();
    project.addMapLayer(layer);

    const HeritageRegion hit =
        HeritageRegionResolver::fromBoundaryLayers(&project, QgsPointXY(128.7, 36.55));
    QVERIFY(hit.ok());
    QCOMPARE(hit.city, QStringLiteral("안동시"));
    QCOMPARE(hit.source, QStringLiteral("layer"));
    QVERIFY(hit.insideSidoBounds);

    // 경계 밖 점은 못 찾았다고 해야 한다. 아무 시/군이나 돌려주면 안 된다.
    QVERIFY(!HeritageRegionResolver::fromBoundaryLayers(&project, QgsPointXY(127.0, 37.5)).ok());
  }

  void emptyGeometrySaysSoInsteadOfGuessing() {
    HeritageRegionResolver resolver;
    QSignalSpy failed(&resolver, &HeritageRegionResolver::failed);
    QSignalSpy resolvedSpy(&resolver, &HeritageRegionResolver::resolved);
    resolver.resolve(QgsGeometry(), QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(resolvedSpy.count(), 0);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageRegionTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_region.moc"
