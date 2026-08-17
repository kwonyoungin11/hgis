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

QTEST_GUILESS_MAIN(TestRecent)
#include "test_recent.moc"
