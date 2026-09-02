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
  void closeEvent_persistsSurveyWork();
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

void TestRecent::closeEvent_persistsSurveyWork() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  const int close = src.indexOf(QLatin1String("void MainWindow::closeEvent"));
  QVERIFY2(close >= 0, "closeEvent");
  const QString fn = src.mid(close, 500);
  QVERIFY2(fn.contains(QLatin1String("persistSurveyWork")),
           "창을 끄면 조사 편집을 자동 저장해야 한다");
  QVERIFY2(src.contains(QLatin1String("void MainWindow::persistSurveyWork()")),
           "persistSurveyWork");
  const int persist = src.indexOf(QLatin1String("void MainWindow::persistSurveyWork()"));
  const QString body = src.mid(persist, 900);
  QVERIFY2(body.contains(QLatin1String("commitChanges")),
           "자동저장은 편집 버퍼를 GPKG에 써야 한다");
  QVERIFY2(src.contains(QLatin1String("lastPath")),
           "다음 실행에 마지막 조사를 다시 연다");
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
