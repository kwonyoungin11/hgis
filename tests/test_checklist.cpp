#include <QtTest>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/SurveyProjectFactory.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsvectorlayer.h>
#include <qgsrectangle.h>
#include <qgsprintlayout.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsmaplayer.h>

class TestKaHgis : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();
  void loadRules();
  void evaluateFailsWithoutSurvey();
  void evaluatePassesHappyFixture();
  void exportBlocksOnError();
  void exportAllowsWhenClean();
  void pdfViaLayout();
  void surveyCreatesGpkg();
  void liveStateFromEmptyProject();
  void fromProject_emptySeededSiteLocation_layoutExistsFalse();
  void fromProject_composedUserSheet_layoutExistsTrue();
private:
  QgsApplication* m_app = nullptr;
};

static QString rulesFile() {
  const QStringList c = {
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
  };
  for (const auto& p : c) if (QFile::exists(p)) return p;
  return c.first();
}

void TestKaHgis::initTestCase() {
  // QgsApplication already constructed by QTEST_MAIN via QCoreApplication - use existing
}

void TestKaHgis::cleanupTestCase() {}

void TestKaHgis::loadRules() {
  ChecklistEngine e;
  QVERIFY2(e.loadRules(rulesFile()), qPrintable(rulesFile()));
  QVERIFY(e.ruleCount() >= 12);
}

void TestKaHgis::evaluateFailsWithoutSurvey() {
  ChecklistEngine e;
  QVERIFY(e.loadRules(rulesFile()));
  QJsonObject st = ProjectStateBuilder::empty();
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
  st.insert(QStringLiteral("survey_is_polygon"), true);
  st.insert(QStringLiteral("feature_poly_count"), 1);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("features_within_survey"), true);
  st.insert(QStringLiteral("geometries_valid"), true);
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
  // API still supports blocking when caller passes blockOnError=true.
  // Phase-1 MainWindow passes false (tool export, not gate).
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      nullptr, QDir::temp().filePath(QStringLiteral("ka_block")), QStringLiteral("UTF-8"),
      QStringLiteral("fail"), true, true, &err);
  QVERIFY(out.isEmpty());
  QVERIFY(!err.isEmpty());
}

void TestKaHgis::exportAllowsWhenClean() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_ok_pkg"));
  QDir(dir).removeRecursively();
  QString err;
  // Phase-1 path: blockOnError=false even if hasChecklistErrors (errors only in README).
  const QString outDirty = ExportService::exportSubmissionPackage(
      nullptr, dir + QStringLiteral("_dirty"), QStringLiteral("UTF-8"),
      QStringLiteral("- [error] demo\n"), false, true, &err);
  QVERIFY2(!outDirty.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(QDir(dir + QStringLiteral("_dirty")).filePath(QStringLiteral("README_submit.txt"))));

  err.clear();
  const QString out = ExportService::exportSubmissionPackage(
      nullptr, dir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("README_submit.txt"))));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("MANIFEST.sha256"))));
}

void TestKaHgis::pdfViaLayout() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  LayoutService::ensureDefaultLayouts(&proj);
  QCOMPARE(LayoutService::defaultLayoutNames().size(), 5);
  const QString p = QDir::temp().filePath(QStringLiteral("ka_layout_test.pdf"));
  QFile::remove(p);
  QString err;
  const QString out = ExportService::writePdfViaLayout(&proj, QStringLiteral("feature_plan"), p, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));
  QVERIFY(QFileInfo(p).size() > 500);
}

void TestKaHgis::surveyCreatesGpkg() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_survey_gpkg"));
  QDir(dir).removeRecursively();
  QDir().mkpath(dir);
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("demo"), &err);
  QVERIFY2(!path.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(path));
}

void TestKaHgis::liveStateFromEmptyProject() {
  QgsProject proj;
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("survey_area_count")).toInt(), 0);
  QVERIFY(st.contains(QStringLiteral("project_crs_set")));
}

void TestKaHgis::fromProject_emptySeededSiteLocation_layoutExistsFalse() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayoutService::ensureDefaultLayouts(&proj);
  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("site_location")));
  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("feature_plan")));
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QVERIFY(!st.value(QStringLiteral("layout_exists:site_location")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:feature_plan")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:survey_area_map")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:feature_detail")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:section")).toBool());
}

void TestKaHgis::fromProject_composedUserSheet_layoutExistsTrue() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* blank = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("layout_blank"), QStringLiteral("memory"));
  QVERIFY(blank->isValid());
  proj.addMapLayer(blank);
  QString err;
  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), &err)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY2(ly, qPrintable(err));
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>{blank});
  map->zoomToExtent(QgsRectangle(200000.0, 450000.0, 200200.0, 450160.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QVERIFY(st.value(QStringLiteral("layout_exists:site_location")).toBool());
  QVERIFY(st.value(QStringLiteral("layout_exists:feature_plan")).toBool());
  QVERIFY(st.value(QStringLiteral("layout_exists:survey_area_map")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:section")).toBool());
  QVERIFY(!st.value(QStringLiteral("layout_exists:feature_detail")).toBool());
}

#include "test_checklist.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev")) ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev") : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestKaHgis tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
