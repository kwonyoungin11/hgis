#include <QtTest>
#include <QUrl>

#include "core/HeritageFormParser.h"
#include "core/HeritageHttpClient.h"

// 브라우저 화면을 흉내 내는 대신 **폼을 읽어 HTTP 로 보내는** 길의 토대.
// 사이트 없이도 검사할 수 있어야 한다 — 그게 이 방식을 고른 이유다.
class HeritageFormTest : public QObject {
  Q_OBJECT

  // 실제 화면과 같은 함정을 담은 표본:
  //  - 지도 패널 폼(bjdcd*)이 **먼저** 나온다
  //  - 다운로드 폼에는 hidden 과 시/도(codedeta)·시/군/구가 있다
  static QByteArray sample() {
    return QByteArrayLiteral(
        "<html><body>"
        "<form name='mapForm' action='/ngis/gis/jibun.do' method='post'>"
        "  <select id='bjdcd1_newJbn' name='bjdCdNewAddr'>"
        "    <option value=''>시도 선택</option>"
        "    <option value='42'>강원특별자치도</option>"
        "  </select>"
        "  <select id='bjdcd2_newJbn' name='bjdcd_newJbn'><option value=''>시구군 선택</option></select>"
        "</form>"
        "<form name='searchForm' action='/user/data/heritageDownload.do' method='POST'>"
        "  <input type='hidden' name='pageMenuId' value='DAT010010'>"
        "  <input type='hidden' name='pageIndex' value='1'>"
        "  <input type='text' name='chaNm' value=''>"
        "  <input type='checkbox' name='onlyNew' value='Y'>"
        "  <input type='submit' value='검색'>"
        "  <select id='codedetaCd0' name='codedetaCd'>"
        "    <option value=''>시도선택</option>"
        "    <option value='42'>강원특별자치도</option>"
        "    <option value='50'>제주특별자치도</option>"
        "    <option value='29'>전남광주통합특별시</option>"
        "  </select>"
        "  <select id='sigunguCd0' name='sigunguCd'>"
        "    <option value=''>시/군/구선택</option>"
        "    <option value='50110'>제주시</option>"
        "    <option value='42150'>강릉시</option>"
        "  </select>"
        "</form>"
        "<p>검색결과 8,302건</p>"
        "</body></html>");
  }

private slots:
  void picksTheDownloadFormNotTheMapPanel() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    QVERIFY2(f.ok(), qPrintable(f.warnings.join(QLatin1Char(' '))));
    // 지도 패널의 bjdcd* 를 잡으면 전국을 받게 된다. 스무 번 막혔던 지점이다.
    QCOMPARE(f.sidoField, QStringLiteral("codedetaCd"));
    QCOMPARE(f.sigunguField, QStringLiteral("sigunguCd"));
    QCOMPARE(f.action, QStringLiteral("/user/data/heritageDownload.do"));
    QCOMPARE(f.method, QStringLiteral("POST"));
  }

  void hiddenFieldsAreCarriedBack() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    QCOMPARE(f.fields.value(QStringLiteral("pageMenuId")), QStringLiteral("DAT010010"));
    QCOMPARE(f.fields.value(QStringLiteral("pageIndex")), QStringLiteral("1"));
    QVERIFY(f.fields.contains(QStringLiteral("chaNm")));
    // 체크되지 않은 체크상자는 보내지 않는다. 브라우저와 같은 규칙이다.
    QVERIFY(!f.fields.contains(QStringLiteral("onlyNew")));
    // 제출 버튼은 칸이 아니다.
    QVERIFY(!f.fields.contains(QStringLiteral("검색")));
  }

  void optionValuesAreFoundByVisibleText() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    QCOMPARE(HeritageFormParser::matchOption(f.sidoOptions, QStringLiteral("제주특별자치도")),
             QStringLiteral("50"));
    QCOMPARE(HeritageFormParser::matchOption(f.sigunguOptions, QStringLiteral("제주시")),
             QStringLiteral("50110"));
  }

  void provinceNamingDifferencesStillMatch() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    // 앱은 「강원특별자치도」, 옛 표기는 「강원도」.
    QCOMPARE(HeritageFormParser::matchOption(f.sidoOptions, QStringLiteral("강원도")),
             QStringLiteral("42"));
    // 전라남도·광주광역시는 사이트에서 하나로 합쳐져 있다.
    QCOMPARE(HeritageFormParser::matchOption(f.sidoOptions, QStringLiteral("전라남도")),
             QStringLiteral("29"));
    QCOMPARE(HeritageFormParser::matchOption(f.sidoOptions, QStringLiteral("광주광역시")),
             QStringLiteral("29"));
  }

  void unknownRegionIsNotGuessed() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    // 없는 곳을 아무 값으로나 채우면 엉뚱한 시/군을 받는다. 빈 값이어야 한다.
    QVERIFY(HeritageFormParser::matchOption(f.sigunguOptions, QStringLiteral("안동시")).isEmpty());
    QVERIFY(HeritageFormParser::matchOption(f.sidoOptions, QString()).isEmpty());
  }

  void resultCountIsRead() {
    QCOMPARE(HeritageFormParser::resultCount(sample()), 8302);
    QCOMPARE(HeritageFormParser::resultCount(QByteArrayLiteral("<p>없음</p>")), -1);
  }

  void bodyCarriesHiddenFieldsAndOnlySwapsTheRegion() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    const QByteArray body = HeritageHttpClient::buildBody(f, QStringLiteral("50"),
                                                          QStringLiteral("50110"));
    const QString text = QString::fromUtf8(body);
    // 서버가 기대하는 hidden 을 그대로 되돌려 보낸다.
    QVERIFY2(text.contains(QStringLiteral("pageMenuId=DAT010010")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("pageIndex=1")));
    // 지역만 바꾼다.
    QVERIFY(text.contains(QStringLiteral("codedetaCd=50")));
    QVERIFY(text.contains(QStringLiteral("sigunguCd=50110")));
    // 체크 안 된 칸은 안 보낸다.
    QVERIFY(!text.contains(QStringLiteral("onlyNew")));
  }

  void actionIsResolvedAgainstThePageUrl() {
    const HeritageForm f = HeritageFormParser::parse(sample());
    const QUrl page(QStringLiteral("https://intranet.gis-heritage.go.kr/user/data/x.do"));
    QCOMPARE(HeritageHttpClient::resolveAction(page, f).toString(),
             QStringLiteral("https://intranet.gis-heritage.go.kr/user/data/heritageDownload.do"));
    HeritageForm noAction = f;
    noAction.action.clear();
    QCOMPARE(HeritageHttpClient::resolveAction(page, noAction), page);
  }

  void encodedUrlMustNotBeEncodedAgain() {
    // 2026-09-12 두 번 당했다: 이미 인코딩된 주소를 QUrl(문자열) 로 만들면
    // % 가 %25 로 또 인코딩되고 서버가 0바이트를 돌려준다.
    const QByteArray encoded =
        "https://intranet.gis-heritage.go.kr/user/data/heritageDownload/downloadFilesAll.do"
        "?codedetaCd=500000%2CADDR500000&searchParams=%EC%A7%80%EC%97%AD%3A%20";
    const QUrl good = QUrl::fromEncoded(encoded, QUrl::StrictMode);
    QVERIFY2(!good.toEncoded().contains("%25"), good.toEncoded().constData());
    QVERIFY(good.toEncoded().contains("%EC%A7%80"));
    QVERIFY(good.toEncoded().contains("500000%2CADDR500000"));
  }

  void missingFormSaysSoInsteadOfPretending() {
    const HeritageForm f =
        HeritageFormParser::parse(QByteArrayLiteral("<html><body><p>로그인하세요</p></body></html>"));
    QVERIFY(!f.ok());
    QVERIFY(!f.warnings.isEmpty());
  }
};

QTEST_APPLESS_MAIN(HeritageFormTest)
#include "test_heritage_form.moc"
