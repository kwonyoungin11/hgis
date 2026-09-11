#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QWebEngineView>

#include "core/KaPortableRuntime.h"

// 인트라넷 자동화(국가유산 GIS통합인트라넷)와 수치지형도 내려받기는 둘 다 QtWebEngine 을 쓴다.
// 이 검사는 그 자식 프로세스(QtWebEngineProcess.exe)가 이 PC에서 실제로 뜨는지만 본다.
// 인터넷도, 로그인도, 사이트도 필요 없다. 여기가 깨지면 위 두 기능은 만들 수 없다.
class WebEngineSmokeTest : public QObject {
  Q_OBJECT
private slots:
  void blankPageLoads() {
    QWebEngineView view;
    QSignalSpy finished(&view, &QWebEngineView::loadFinished);
    view.resize(320, 240);
    view.load(QUrl(QStringLiteral("about:blank")));
    QVERIFY2(finished.wait(30000),
             "QtWebEngine 자식 프로세스가 뜨지 않았다. QTWEBENGINEPROCESS_PATH 와 "
             "apps/Qt6/resources · translations/qtwebengine_locales 를 확인하라.");
    QCOMPARE(finished.count(), 1);
    QVERIFY2(finished.first().first().toBool(), "about:blank 로드가 실패로 끝났다.");
  }

  void localHtmlRendersItsText() {
    // 빈 페이지만이 아니라 실제 DOM 이 살아 있어야 한다. 로그인 폼을 다루려면 이게 돼야 한다.
    QWebEngineView view;
    QSignalSpy finished(&view, &QWebEngineView::loadFinished);
    view.resize(320, 240);
    view.setHtml(QStringLiteral("<html><body><p id='x'>주변유적</p></body></html>"));
    QVERIFY(finished.wait(30000));
    QVERIFY(finished.first().first().toBool());

    QString text;
    bool done = false;
    view.page()->runJavaScript(QStringLiteral("document.getElementById('x').textContent"),
                               [&](const QVariant& v) {
                                 text = v.toString();
                                 done = true;
                               });
    QTRY_VERIFY_WITH_TIMEOUT(done, 15000);
    QCOMPARE(text, QStringLiteral("주변유적"));
  }
};

int main(int argc, char** argv) {
  // KaApplication 과 같은 순서로, 같은 함수를 부른다. QApplication 보다 먼저 걸어야 한다.
  // 여기서 직접 플래그를 박지 않는다 — 앱이 실제로 거는 설정이 맞는지를 검사해야 한다.
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  KaPortableRuntime::applyWebEngineFlags();
  QApplication app(argc, argv);
  WebEngineSmokeTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_webengine_smoke.moc"
