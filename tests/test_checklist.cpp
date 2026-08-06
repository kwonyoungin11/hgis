#include <QtTest>
#include <QJsonObject>
#include <QDir>
#include <QCoreApplication>
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/SurveyProjectFactory.h"

class TestKaHgis : public QObject {
  Q_OBJECT
private slots:
  void loadRules();
  void evaluateFailsWithoutSurvey();
  void evaluatePassesHappyFixture();
  void exportBlocksOnError();
  void exportAllowsWhenClean();
  void pdfWritesFile();
  void stubSurveyCreatesMeta();
};

static QString rulesFile() {
  const QStringList c = {
    QStringLiteral("D:/qgis/data/rules/drawing_checklist.v1.json"),
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json"))
  };
  for (const auto& p : c) if (QFile::exists(p)) return p;
  return c.first();
}

void TestKaHgis::loadRules() {
  ChecklistEngine e;
  QVERIFY2(e.loadRules(rulesFile()), qPrintable(rulesFile()));
  QVERIFY(e.ruleCount() >= 12);
}

void TestKaHgis::evaluateFailsWithoutSurvey() {
  ChecklistEngine e;
  QVERIFY(e.loadRules(rulesFile()));
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 0);
  st.insert(QStringLiteral("control_points_count"), 0);
  st.insert(QStringLiteral("project_crs_set"), false);
  st.insert(QStringLiteral("has_datum"), false);
  st.insert(QStringLiteral("has_ellipsoid"), false);
  st.insert(QStringLiteral("has_projection"), false);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("feature_poly_count"), 0);
  st.insert(QStringLiteral("has_kind_period"), true);
  const auto r = e.evaluate(st);
  int errors = 0;
  for (const auto& x : r) if (!x.passed && x.severity == QLatin1String("error")) errors++;
  QVERIFY2(errors >= 1, "expected checklist errors on empty project");
}

void TestKaHgis::evaluatePassesHappyFixture() {
  ChecklistEngine e;
  QVERIFY(e.loadRules(rulesFile()));
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 1);
  st.insert(QStringLiteral("control_points_count"), 2);
  st.insert(QStringLiteral("project_crs_set"), true);
  st.insert(QStringLiteral("has_datum"), true);
  st.insert(QStringLiteral("has_ellipsoid"), true);
  st.insert(QStringLiteral("has_projection"), true);
  st.insert(QStringLiteral("has_origin"), true);
  st.insert(QStringLiteral("has_accuracy"), true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("feature_poly_count"), 1);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("layout_exists:site_location"), true);
  st.insert(QStringLiteral("layout_exists:feature_plan"), true);
  st.insert(QStringLiteral("layout_exists:feature_detail"), true);
  st.insert(QStringLiteral("layout_exists:section"), true);
  const auto r = e.evaluate(st);
  int errors = 0;
  for (const auto& x : r) if (!x.passed && x.severity == QLatin1String("error")) errors++;
  QCOMPARE(errors, 0);
}

void TestKaHgis::exportBlocksOnError() {
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      QDir::temp().filePath(QStringLiteral("ka_block")), QStringLiteral("UTF-8"),
      QStringLiteral("fail"), true, true, &err);
  QVERIFY(out.isEmpty());
  QVERIFY(!err.isEmpty());
}

void TestKaHgis::exportAllowsWhenClean() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_ok_pkg"));
  QDir(dir).removeRecursively();
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      dir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("README_submit.txt"))));
}

void TestKaHgis::pdfWritesFile() {
  const QString p = QDir::temp().filePath(QStringLiteral("ka_test.pdf"));
  QFile::remove(p);
  QString err;
  QVERIFY(!ExportService::writePdfPlaceholder(p, QStringLiteral("Test"), &err).isEmpty());
  QVERIFY(QFileInfo(p).size() > 20);
}

void TestKaHgis::stubSurveyCreatesMeta() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_survey_stub"));
  QDir(dir).removeRecursively();
  QDir().mkpath(dir);
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("demo"), &err);
  QVERIFY2(!path.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(path) || QFile::exists(QDir(dir).filePath(QStringLiteral("demo.ka-survey.json"))));
}

QTEST_MAIN(TestKaHgis)
#include "test_checklist.moc"

