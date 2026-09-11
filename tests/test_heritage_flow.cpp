#include <QtTest>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QTemporaryDir>

#include "core/HeritageIntranetFlow.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageStyle.h"

// 국가유산 인트라넷 자동화의 단계·스크립트. 사이트 접속 없이 검사한다.
class HeritageIntranetFlowTest : public QObject {
  Q_OBJECT

  // jsString 이 만든 리터럴이 실제로 그 문자열로 되읽히는지.
  static QString roundTrip(const QString& value) {
    const QString literal = HeritageIntranetFlow::jsString(value);
    QJsonParseError err{};
    const QJsonDocument doc =
        QJsonDocument::fromJson(QStringLiteral("[%1]").arg(literal).toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return QStringLiteral("<invalid>");
    return doc.array().at(0).toString();
  }

private slots:
  void loginUsesTheRealFieldsAndThePagesOwnEncryption() {
    const QString js = HeritageIntranetFlow::loginScript(QStringLiteral("A0298"),
                                                         QStringLiteral("secret"));
    // 2026-09-11 실제 화면에서 확인한 것들
    QVERIFY(js.contains(QStringLiteral("getElementById('userId')")));
    QVERIFY(js.contains(QStringLiteral("getElementById('pwd')")));
    // 이 사이트는 브라우저에서 RSA 로 암호화한다. 우리가 암호화하지 않고 goLogin() 에 맡겨야 한다.
    QVERIFY(js.contains(QStringLiteral("goLogin()")));
    // 평문을 직접 POST 하지 않는다.
    QVERIFY(!js.contains(QStringLiteral("j_spring_security_check")));
    QVERIFY(!js.contains(QStringLiteral("USER_PW")));
    // 못 찾으면 못 찾았다고 말한다.
    QVERIFY(js.contains(QStringLiteral("no-form")));
    QVERIFY(js.contains(QStringLiteral("no-goLogin")));
  }

  void passwordWithQuotesAndBackslashesSurvives() {
    const QStringList nasty = {
        QStringLiteral("plain"),
        QStringLiteral("with'single"),
        QStringLiteral("with\"double"),
        QStringLiteral("back\\slash"),
        QStringLiteral("line\nbreak"),
        QStringLiteral("한글비밀번호"),
        QStringLiteral("</script>"),
    };
    for (const QString& value : nasty)
      QCOMPARE(roundTrip(value), value);
  }

  void scriptStaysValidWithAHostilePassword() {
    // 스크립트가 깨지면 아이디·비밀번호가 페이지에 잘못 흘러들 수 있다.
    const QString js = HeritageIntranetFlow::loginScript(
        QStringLiteral("A0298"), QStringLiteral("a'b\"c\\d\ne"));
    // 리터럴 두 개가 모두 온전한 JSON 문자열이어야 한다.
    QCOMPARE(roundTrip(QStringLiteral("a'b\"c\\d\ne")), QStringLiteral("a'b\"c\\d\ne"));
    QVERIFY(!js.contains(QLatin1Char('\n')));  // 스크립트 안에 실제 줄바꿈이 남으면 안 된다
  }

  void unverifiedStagesAreMarkedUnverified() {
    // 모르는 것을 아는 척하지 않는다. 로그인만 실제 화면으로 확인했다.
    QVERIFY(HeritageIntranetFlow::isVerified(HeritageStage::Login));
    for (HeritageStage stage : {HeritageStage::DismissTutorial, HeritageStage::OpenDownloadPage,
                                HeritageStage::AgreeTerms, HeritageStage::SelectDataset,
                                HeritageStage::SelectRegion, HeritageStage::Search,
                                HeritageStage::CollectResults, HeritageStage::Download})
      QVERIFY2(!HeritageIntranetFlow::isVerified(stage),
               qPrintable(HeritageIntranetFlow::stageName(stage)));
  }

  void everyStageHasAKoreanName() {
    for (HeritageStage stage :
         {HeritageStage::Idle, HeritageStage::Login, HeritageStage::DismissTutorial,
          HeritageStage::OpenDownloadPage, HeritageStage::AgreeTerms, HeritageStage::SelectDataset,
          HeritageStage::SelectRegion, HeritageStage::Search, HeritageStage::CollectResults,
          HeritageStage::Download, HeritageStage::Done, HeritageStage::Failed}) {
      const QString name = HeritageIntranetFlow::stageName(stage);
      QVERIFY(!name.isEmpty());
      QVERIFY2(name != QStringLiteral("알 수 없음"), qPrintable(name));
    }
  }

  void everyStepReportsNotFoundInsteadOfPretending() {
    const QStringList scripts = {
        HeritageIntranetFlow::dismissTutorialScript(),
        HeritageIntranetFlow::openDownloadPageScript(),
        HeritageIntranetFlow::agreeTermsScript(),
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시")),
        HeritageIntranetFlow::searchScript(),
    };
    for (const QString& js : scripts) {
      // 못 찾았으면 못 찾았다고 하거나(not-found/no-sido), 할 일이 없다고(none) 말해야 한다.
      QVERIFY2(js.contains(QStringLiteral("not-found")) || js.contains(QStringLiteral("no-sido")) ||
                   js.contains(QStringLiteral("'none'")),
               qPrintable(js.left(60)));
      // 예외를 삼키고 성공처럼 보이면 안 된다.
      QVERIFY(js.contains(QStringLiteral("catch")));
    }
  }

  void knownTabCodeIsUsedAndUnknownFallsBackToTheLabel() {
    // 매장유산유존지역은 코드를 확인했다.
    const QString known =
        HeritageIntranetFlow::selectDatasetScript(HeritageDataset::BuriedHeritageArea);
    QVERIFY(known.contains(QStringLiteral("changeTab")));
    QVERIFY(known.contains(QStringLiteral("\"B\"")));
    // 지정유산 탭 코드는 아직 모른다. 글자로 찾되 아는 척하지 않는다.
    QVERIFY(HeritageStyle::tabCode(HeritageDataset::DesignatedHeritage).isEmpty());
    const QString unknown =
        HeritageIntranetFlow::selectDatasetScript(HeritageDataset::DesignatedHeritage);
    QVERIFY(!unknown.contains(QStringLiteral("changeTab")));
    QVERIFY(unknown.contains(QStringLiteral("지정유산")));
  }

  void regionStepStopsAtSigunguAndNeverGoesDeeper() {
    const QString js =
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    QVERIFY(js.contains(QStringLiteral("경상북도")));
    QVERIFY(js.contains(QStringLiteral("안동시")));
    // 시/군까지만 받는다. 읍면동·리를 고르는 코드가 있으면 안 된다.
    QVERIFY(!js.contains(QStringLiteral("읍면동")));
    QVERIFY(!js.contains(QStringLiteral("emd")));
    QVERIFY(!js.contains(QStringLiteral("sels[2]")));
    QVERIFY(!js.contains(QStringLiteral("sels[3]")));
  }

  void scriptsUseValidBackslashEscapes() {
    // C++ 문자열 안의 \s 는 잘못된 이스케이프라 s 가 되어 정규식이 망가진다.
    // 화면 글자의 공백을 지우지 못하면 버튼을 하나도 못 찾는다.
    const QStringList scripts = {
        HeritageIntranetFlow::dismissTutorialScript(),
        HeritageIntranetFlow::openDownloadPageScript(),
        HeritageIntranetFlow::agreeTermsScript(),
        HeritageIntranetFlow::searchScript(),
        HeritageIntranetFlow::downloadAllScript(),
        HeritageIntranetFlow::downloadPageProbeScript(),
        HeritageIntranetFlow::selectRegionScript(QStringLiteral("경상북도"), QStringLiteral("안동시")),
    };
    for (const QString& js : scripts) {
      if (!js.contains(QStringLiteral("replace(/"))) continue;
      // 역슬래시를 소스에 직접 쓰지 않는다(이 파일도 같은 함정에 빠진다).
      const QString backslash(QChar(0x5C));
      QVERIFY2(!js.contains(QStringLiteral("replace(/s")), qPrintable(js.left(80)));
      QVERIFY2(js.contains(QStringLiteral("replace(/") + backslash + QStringLiteral("s")) ||
                   js.contains(QStringLiteral("replace(/[^0-9]")),
               qPrintable(js.left(80)));
    }
  }

  void downloadPageIsTheCodedetaFormNotTheMapPanel() {
    // 2026-09-11 실패 outline(main.do): 지도 칸 bjdcd1_newJbn「시도 선택」과
    // 메뉴 「전체다운로드」는 메인 화면에 있다. 다운로드 폼(codedeta)은 없다.
    // 그걸 도착으로 치면 서약서·시군 선택으로 가지 못하고 여기서 멈춘다.
    const QString probe = HeritageIntranetFlow::downloadPageProbeScript();
    QVERIFY(probe.contains(QStringLiteral("codedeta")));
    QVERIFY(!probe.contains(QStringLiteral("전체다운로드")));
    QVERIFY(!probe.contains(QStringLiteral("시도선택")));
    QVERIFY(probe.contains(QStringLiteral("서약서에동의")));
    QVERIFY(probe.contains(QStringLiteral("not-yet")));
  }

  void wrapPrefersCodedetaFrameOverParentSearchFunction() {
    // 같은 outline: searchGisChaRirList 는 바깥 창 메뉴 onclick 에 있고,
    // codedeta 는 그 document 에 없다. pick 이 함수만 보면 지도 창에 머문다.
    const QString js = HeritageIntranetFlow::searchScript();
    const int codedetaAt = js.indexOf(QStringLiteral("name*=codedeta"));
    const int searchFnAt = js.indexOf(QStringLiteral("searchGisChaRirList"));
    QVERIFY(codedetaAt >= 0);
    QVERIFY(searchFnAt >= 0);
    QVERIFY2(codedetaAt < searchFnAt, "pick must take the codedeta frame before the parent search function");
    QVERIFY(js.contains(QStringLiteral("frames[i]")));
  }

  void formHintMarksTheDownloadFormNotTheMap() {
    const QString js = HeritageIntranetFlow::formHintScript();
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("'codedeta'")));
    QVERIFY(!js.contains(QStringLiteral("지정유산")));
    QVERIFY(!js.contains(QStringLiteral("bjdcd")));
    QVERIFY(!js.contains(QStringLiteral("서약서에동의")));
  }

  void agreeTermsSkipsWhenDownloadFormIsAlreadyOpen() {
    const QString js = HeritageIntranetFlow::agreeTermsScript();
    QVERIFY(js.contains(QStringLiteral("already-open")));
    QVERIFY(js.contains(QStringLiteral("codedeta")));
  }

  void searchAndDownloadStayOnTheCodedetaWindow() {
    // 부모 메뉴의 searchGisChaRirList 는 지도 document 를 본다.
    // fn() 이 그 함수를 집어 오면 codedeta 에서 고른 시·군이 검색에 안 실린다.
    const QString search = HeritageIntranetFlow::searchScript();
    const QString down = HeritageIntranetFlow::downloadAllScript();
    QVERIFY(!search.contains(QStringLiteral("fn('searchGisChaRirList')")));
    QVERIFY(!down.contains(QStringLiteral("fn('searchGisChaRirList')")));
    QVERIFY(search.contains(QStringLiteral("W.searchGisChaRirList")));
    QVERIFY(down.contains(QStringLiteral("W.searchGisChaRirList")));
    QVERIFY(search.contains(QStringLiteral("'검색'")));
    QVERIFY(!search.contains(QStringLiteral("new Function")));
    QVERIFY(!down.contains(QStringLiteral("new Function")));
  }

  void wrapWalksNestedFrames() {
    const QString js = HeritageIntranetFlow::pageOutlineScript();
    QVERIFY(js.contains(QStringLiteral("codedeta")));
    QVERIFY(js.contains(QStringLiteral("walk(")) || js.contains(QStringLiteral("depth")));
  }

  void resultCollectionKnowsThereCanBeManyPages() {
    const QString js = HeritageIntranetFlow::resultPageInfoScript();
    QVERIFY(js.contains(QStringLiteral("lastPage")));
    QVERIFY(js.contains(QStringLiteral("rows")));
  }

  void portalAndLoginUrlsAreTheRealSite() {
    QCOMPARE(HeritageIntranetFlow::portalUrl().host(),
             QStringLiteral("intranet.gis-heritage.go.kr"));
    // 루트는 frameset(top/main) 이라 프레임을 지정해야 한다. 이 주소로 바로 들어가면 프레임이 없다.
    QCOMPARE(HeritageIntranetFlow::loginUrl().path(), QStringLiteral("/user/checkNet.do"));
    QCOMPARE(HeritageIntranetFlow::loginUrl().scheme(), QStringLiteral("https"));
  }

  void credentialsStoreIsSeparateFromTheTopographicOne() {
    // 수치지형도(NGII) 계정과 같은 자리에 쓰면 둘 다 깨진다.
    HeritageIntranetSettings::Credentials written{QStringLiteral("TESTID"),
                                                  QStringLiteral("testpw\\'\"")};
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("heritage-account.ini"));
    QString error;
    QVERIFY2(HeritageIntranetSettings::saveToFile(file, written, &error), qPrintable(error));
    const auto read = HeritageIntranetSettings::readFromFiles(file, {});
    QCOMPARE(read.username, written.username);
    QCOMPARE(read.password, written.password);

    // 파일 안의 구획 이름이 ngii 가 아니라 heritage 여야 한다.
    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    QVERIFY(text.contains(QStringLiteral("[heritage]")));
    QVERIFY(!text.contains(QStringLiteral("[ngii]")));
  }

  void logDescriptionNeverCarriesThePassword() {
    // 계정 값을 로그에 남기지 않는다. 이 PC에 실제로 저장된 비밀번호로 확인한다.
    const auto stored = HeritageIntranetSettings::credentials();
    const QString line = HeritageIntranetSettings::describeForLog();
    QVERIFY(!line.isEmpty());
    if (!stored.password.isEmpty())
      QVERIFY2(!line.contains(stored.password), "로그 문구에 비밀번호가 들어갔다");
    if (!stored.username.trimmed().isEmpty())
      QVERIFY(line.contains(stored.username.trimmed()));
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  HeritageIntranetFlowTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_heritage_flow.moc"
