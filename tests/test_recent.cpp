#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "core/RecentSurveys.h"

class TestRecent : public QObject {
  Q_OBJECT
private slots:
  void rememberPutsNewestFirstAndDropsMissing();
  void forgetRemovesPath();
  void lastPath_isNewestRemembered();
  void takeSkipAutoRestore_clearsOneShot();
  void bootStaysOnHome_doesNotAutoOpenLastSurvey();
  void closeEvent_asksBeforeDiscardingUnsavedWork();
  void captureTool_dragsSavedPolygonVertex();
};

void TestRecent::rememberPutsNewestFirstAndDropsMissing() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("siteA.gpkg"));
  const QString b = dir.filePath(QStringLiteral("siteB.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QVERIFY(QFile(b).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("조사A"));
  RecentSurveys::remember(st, b, QStringLiteral("조사B"));
  auto items = RecentSurveys::load(st);
  QCOMPARE(items.size(), 2);
  QCOMPARE(items.at(0).name, QStringLiteral("조사B"));
  QCOMPARE(items.at(0).path, QFileInfo(b).absoluteFilePath());
  QCOMPARE(items.at(1).name, QStringLiteral("조사A"));
  QVERIFY(QFile::remove(a));
  items = RecentSurveys::load(st);
  QCOMPARE(items.size(), 1);
  QCOMPARE(items.at(0).name, QStringLiteral("조사B"));
}

void TestRecent::takeSkipAutoRestore_clearsOneShot() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  QVERIFY(!RecentSurveys::takeSkipAutoRestore(st));
  RecentSurveys::setSkipAutoRestore(st, true);
  QVERIFY(RecentSurveys::takeSkipAutoRestore(st));
  QVERIFY(!RecentSurveys::takeSkipAutoRestore(st));
}

void TestRecent::forgetRemovesPath() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("gone.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("지울조사"));
  QCOMPARE(RecentSurveys::load(st).size(), 1);
  RecentSurveys::forget(st, a);
  QCOMPARE(RecentSurveys::load(st).size(), 0);
}

void TestRecent::lastPath_isNewestRemembered() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("old.gpkg"));
  const QString b = dir.filePath(QStringLiteral("new.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QVERIFY(QFile(b).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("옛조사"));
  RecentSurveys::remember(st, b, QStringLiteral("새조사"));
  QCOMPARE(RecentSurveys::lastPath(st), QFileInfo(b).absoluteFilePath());
}

void TestRecent::bootStaysOnHome_doesNotAutoOpenLastSurvey() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("QTimer::singleShot(0, this, &MainWindow::restoreLastSurvey)")),
           "다시 실행하면 마지막 조사를 열어야 한다");
  QVERIFY2(src.contains(QLatin1String("OpenSurveyMode::LayersOnly")),
           "부팅 복원은 내장/.qgz를 읽지 말고 GPKG 테이블만 연다");
  QFile h(QStringLiteral("src/app/MainWindow.h"));
  QVERIFY2(h.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.h");
  const QString hdr = QString::fromUtf8(h.readAll());
  QVERIFY2(hdr.contains(QLatin1String("m_restoreLastSurveyEnabled = true")),
           "자동 복원 기본값은 켜짐(GPKG 전용)");
}

// 20초 자동 저장은 없앴다. 저장은 사용자가 「저장」을 누를 때만 일어난다.
// 그래서 창을 끌 때 말없이 저장해서도, 말없이 버려서도 안 된다 — 물어야 한다.
void TestRecent::closeEvent_asksBeforeDiscardingUnsavedWork() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());

  QVERIFY2(!src.contains(QLatin1String("m_autosaveTimer")),
           "20초 자동 저장 타이머는 없어야 한다");

  const int close = src.indexOf(QLatin1String("void MainWindow::closeEvent"));
  QVERIFY2(close >= 0, "closeEvent");
  const QString fn = src.mid(close, 1200);
  QVERIFY2(fn.contains(QLatin1String("surveyHasUnsavedChanges")),
           "닫기 전에 저장 안 된 작업이 있는지 봐야 한다");
  QVERIFY2(fn.contains(QLatin1String("QMessageBox::Save")) &&
               fn.contains(QLatin1String("QMessageBox::Discard")) &&
               fn.contains(QLatin1String("QMessageBox::Cancel")),
           "저장 / 저장 안 함 / 취소를 물어야 한다");
  QVERIFY2(fn.contains(QLatin1String("persistSurveyWork")),
           "저장을 고르면 실제로 저장해야 한다");
  QVERIFY2(fn.contains(QLatin1String("event->ignore()")),
           "취소를 고르면 창이 닫히면 안 된다");

  QVERIFY2(src.contains(QLatin1String("bool MainWindow::persistSurveyWork()")),
           "persistSurveyWork");
  const int persist = src.indexOf(QLatin1String("bool MainWindow::persistSurveyWork()"));
  const QString body = src.mid(persist, 900);
  QVERIFY2(body.contains(QLatin1String("commitSurveyEdits")),
           "저장은 편집 버퍼를 GPKG에 써야 한다");
  QVERIFY2(src.contains(QLatin1String("lastPath")),
           "최근 목록용 lastPath는 유지한다");
  QVERIFY2(src.contains(QLatin1String("m_surveySessionReady")),
           "열기에 실패한 홈 화면이 조사 파일을 덮어쓰면 안 된다");

  // 바탕화면이 OneDrive 로 리디렉션된 PC에서 새 조사가 그리로 가지 않아야 한다.
  QVERIFY2(src.contains(QLatin1String("preferredSurveyDir()")),
           "새 조사·다른 이름으로 저장은 마지막에 쓴 조사 폴더에서 시작해야 한다");
}

void TestRecent::captureTool_dragsSavedPolygonVertex() {
  QFile f(QStringLiteral("src/app/KaCaptureMapTool.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaCaptureMapTool.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("hitSavedVertex")),
           "그린 폴리곤의 꼭짓점을 다시 집을 수 있어야 한다");
  QVERIFY2(src.contains(QLatin1String("moveFeatureVertex")) ||
               src.contains(QLatin1String("moveVertex")),
           "집은 꼭짓점을 옮겨 도형을 고쳐야 한다");
}

QTEST_GUILESS_MAIN(TestRecent)
#include "test_recent.moc"
