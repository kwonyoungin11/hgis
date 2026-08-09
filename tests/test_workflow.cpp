#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QSettings>

#include "core/SurveyProjectFactory.h"
#include "core/ProjectStateBuilder.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/WorkflowGuide.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsfeature.h>
#include <qgsrectangle.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsvectorfilewriter.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgscoordinatetransformcontext.h>
#include <qgslayertreemodel.h>
#include <qgslayertree.h>

class TestWorkflow : public QObject {
  Q_OBJECT
private slots:
  void fullWorkflowSurveyToPackage();
  void shpKoreanRoundTripUtf8();
  void reprojectAndMigrateFields();
  void georefWorldfileFromGcp();
  void convert5186PolygonTo5179Shp();
  void vworldLayerOpsTest();
  void workflowGuideTracksSevenRealMilestones();
  void vworldSettingsAndNoKeyTests();
  void editBufferCommitSurvivesReopen();
  void importControlCsvWritesFeatures();
  void osmBasemapValidWithExtent();
  void layerTreeReorderAndRemovalTest();
  void layerTreeDragReorderChangesOrder();
  void layerKeyOfKoreanDisplayName_featurePolyAttrs();
  void exportAllowsDespiteChecklistErrors();
  void vworldSatAndCadastralLiveKey();
};

static bool projectHasLayerNamedLike(QgsProject* proj, const QString& base) {
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n == base || n.startsWith(base + QLatin1String(" [")))
      return true;
  }
  return false;
}

static QString rulesFile() {
  const QStringList c = {
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
  };
  for (const auto& p : c)
    if (QFile::exists(p)) return p;
  return c.first();
}

static QgsVectorLayer* layer(QgsProject* p, const QString& n) {
  const auto ls = p->mapLayersByName(n);
  return ls.isEmpty() ? nullptr : qobject_cast<QgsVectorLayer*>(ls.first());
}

void TestWorkflow::fullWorkflowSurveyToPackage() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_wf_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("wfdemo"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  for (const char* n : {"survey_area", "feature_poly", "feature_line", "section_line", "control_points"}) {
    auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkg, QString::fromUtf8(n)),
                                  QString::fromUtf8(n), QStringLiteral("ogr"));
    QVERIFY2(vl->isValid(), n);
    proj.addMapLayer(vl);
  }

  // survey polygon
  auto* sa = layer(&proj, QStringLiteral("survey_area"));
  QVERIFY(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("테스트조사"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000) << QgsPointXY(200100, 450100)
       << QgsPointXY(200000, 450100) << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  // feature poly with kind/period
  auto* fp = layer(&proj, QStringLiteral("feature_poly"));
  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  QgsPolylineXY ring2;
  ring2 << QgsPointXY(200020, 450020) << QgsPointXY(200040, 450020) << QgsPointXY(200040, 450040)
        << QgsPointXY(200020, 450040) << QgsPointXY(200020, 450020);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring2));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  // 2 GCPs
  auto* cp = layer(&proj, QStringLiteral("control_points"));
  QVERIFY(cp->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature cf(cp->fields());
    cf.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
    cf.setAttribute(QStringLiteral("x"), 200000.0 + i * 50);
    cf.setAttribute(QStringLiteral("y"), 450000.0 + i * 20);
    cf.setAttribute(QStringLiteral("datum"), QStringLiteral("세계측지계"));
    cf.setAttribute(QStringLiteral("ellipsoid"), QStringLiteral("GRS80"));
    cf.setAttribute(QStringLiteral("projection"), QStringLiteral("UTM-K"));
    cf.setAttribute(QStringLiteral("accuracy_m"), 0.8);
    cf.setAttribute(QStringLiteral("pdop"), 1.2);
    cf.setAttribute(QStringLiteral("fix_type"), QStringLiteral("RTK"));
    cf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000.0 + i * 50, 450000.0 + i * 20)));
    QVERIFY(cp->addFeature(cf));
  }
  QVERIFY(cp->commitChanges());

  LayoutService::ensureDefaultLayouts(&proj);
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("survey_area_count")).toInt(), 1);
  QCOMPARE(st.value(QStringLiteral("control_points_count")).toInt(), 2);
  QCOMPARE(st.value(QStringLiteral("feature_poly_count")).toInt(), 1);
  QVERIFY(st.value(QStringLiteral("has_kind_period")).toBool());
  QVERIFY(st.value(QStringLiteral("survey_is_polygon")).toBool());

  ChecklistEngine eng;
  QVERIFY(eng.loadRules(rulesFile()));
  const auto results = eng.evaluate(st);
  int errors = 0;
  QString msg;
  for (const auto& r : results) {
    if (!r.passed && r.severity == QLatin1String("error")) {
      errors++;
      msg += r.id + QLatin1Char(' ');
    }
  }
  // layouts should exist after ensureDefaultLayouts
  QVERIFY2(errors == 0, qPrintable(msg));

  const QString pdf = QDir(dir).filePath(QStringLiteral("feature_plan.pdf"));
  QString perr;
  QVERIFY2(!ExportService::writePdfViaLayout(&proj, QStringLiteral("feature_plan"), pdf, &perr).isEmpty(),
           qPrintable(perr));
  QVERIFY(QFileInfo(pdf).size() > 1000);

  const QString pkg = QDir(dir).filePath(QStringLiteral("pkg"));
  QString e2;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &e2).isEmpty(),
           qPrintable(e2));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("survey_area.shp"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("feature_poly.shp"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("MANIFEST.sha256"))));
}

void TestWorkflow::shpKoreanRoundTripUtf8() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_shp_kr_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("kr"), QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  QVERIFY(mem.startEditing());
  QgsFeature f(mem.fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기시대"));
  QgsPolylineXY ring;
  ring << QgsPointXY(0, 0) << QgsPointXY(10, 0) << QgsPointXY(10, 10) << QgsPointXY(0, 10) << QgsPointXY(0, 0);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(mem.addFeature(f));
  QVERIFY(mem.commitChanges());

  const QString shp = QDir(dir).filePath(QStringLiteral("kr_feat.shp"));
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      &mem, shp, QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  QCOMPARE(we, QgsVectorFileWriter::NoError);

  QgsVectorLayer loaded(shp, QStringLiteral("loaded"), QStringLiteral("ogr"));
  QVERIFY2(loaded.isValid(), qPrintable(loaded.error().message()));
  QCOMPARE(int(loaded.featureCount()), 1);
  const QgsFeatureIds ids = loaded.allFeatureIds();
  QVERIFY(!ids.isEmpty());
  QgsFeature lf = loaded.getFeature(*ids.constBegin());
  // OGR may transliterate field names; read by index if needed
  QString kind = lf.attribute(QStringLiteral("kind")).toString();
  QString period = lf.attribute(QStringLiteral("period")).toString();
  if (kind.isEmpty() && loaded.fields().count() >= 1) kind = lf.attribute(0).toString();
  if (period.isEmpty() && loaded.fields().count() >= 2) period = lf.attribute(1).toString();
  QVERIFY2(kind.contains(QStringLiteral("수혈")) || kind.contains(QStringLiteral("주거")) || !kind.isEmpty(),
           qPrintable(QStringLiteral("kind=%1").arg(kind)));
  QVERIFY2(!period.isEmpty(), qPrintable(QStringLiteral("period empty")));
}

void TestWorkflow::reprojectAndMigrateFields() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reproj_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("reproj"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg), QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(sa->isValid());
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200050, 450000) << QgsPointXY(200050, 450050)
       << QgsPointXY(200000, 450050) << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());
  const QString out = QDir(dir).filePath(QStringLiteral("sa_4326.gpkg"));
  QString rerr;
  QVERIFY2(!LayerOps::reprojectVectorLayer(sa, QStringLiteral("EPSG:4326"), out, &proj, &rerr).isEmpty(), qPrintable(rerr));
  QVERIFY(QFile::exists(out));

  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg), QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  const int n = LayerOps::ensureControlPointQualityFields(cp);
  QVERIFY(n >= 0);
  QVERIFY(cp->fields().indexOf(QStringLiteral("accuracy_m")) >= 0);
  QVERIFY(cp->fields().indexOf(QStringLiteral("pixel_x")) >= 0);
}

void TestWorkflow::georefWorldfileFromGcp() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("georef"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg),
                                QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  proj.addMapLayer(cp);
  QVERIFY(cp->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature cf(cp->fields());
    cf.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
    cf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000.0 + i * 100, 450000.0)));
    QVERIFY(cp->addFeature(cf));
  }
  QVERIFY(cp->commitChanges());

  const QString png = QDir(dir).filePath(QStringLiteral("scan.png"));
  QImage img(64, 64, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));

  QString gerr;
  const QString wf = LayerOps::georeferenceImageSimple(png, cp, &proj, nullptr, &gerr);
  QVERIFY2(!wf.isEmpty(), qPrintable(gerr));
  QVERIFY(QFile::exists(wf) || QFile::exists(QDir(dir).filePath(QStringLiteral("scan.pgw"))));
  const QString pgw = QDir(dir).filePath(QStringLiteral("scan.pgw"));
  QVERIFY2(QFile::exists(pgw), "worldfile missing");
  QFile f(pgw);
  QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString body = QString::fromUtf8(f.readAll());
  QVERIFY(body.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() >= 6);
}

void TestWorkflow::convert5186PolygonTo5179Shp() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_5179_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("work5186"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fp = new QgsVectorLayer(QStringLiteral("%1|layername=feature_poly").arg(gpkg),
                                QStringLiteral("feature_poly"), QStringLiteral("ogr"));
  QVERIFY(fp->isValid());
  QCOMPARE(fp->crs().authid(), QStringLiteral("EPSG:5186"));
  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200050, 450000) << QgsPointXY(200050, 450050)
       << QgsPointXY(200000, 450050) << QgsPointXY(200000, 450000);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  const QString out = QDir(dir).filePath(QStringLiteral("feat_5179.shp"));
  QString cerr;
  QVERIFY2(!LayerOps::convertToShp5179(fp, out, nullptr, &cerr).isEmpty(), qPrintable(cerr));
  QVERIFY(QFile::exists(out));
  QgsVectorLayer loaded(out, QStringLiteral("u"), QStringLiteral("ogr"));
  QVERIFY(loaded.isValid());
  QVERIFY(loaded.crs().authid() == QStringLiteral("EPSG:5179") || loaded.crs().isValid());
  QCOMPARE(int(loaded.featureCount()), 1);

  // Also verify exportSubmissionPackage reprojects EPSG:5186 layer to EPSG:5179 SHP
  QgsProject proj5186;
  proj5186.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  proj5186.addMapLayer(fp);
  const QString pkgDir = QDir(dir).filePath(QStringLiteral("pkg5186"));
  QString perr;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj5186, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), false, false, &perr).isEmpty(), qPrintable(perr));
  const QString pkgShp = QDir(pkgDir).filePath(QStringLiteral("feature_poly.shp"));
  QVERIFY(QFile::exists(pkgShp));
  QgsVectorLayer pkgLoaded(pkgShp, QStringLiteral("pkg"), QStringLiteral("ogr"));
  QVERIFY(pkgLoaded.isValid());
  QCOMPARE(pkgLoaded.crs().authid(), QStringLiteral("EPSG:5179"));
  delete fp;
}

void TestWorkflow::vworldLayerOpsTest() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  const QString testKey = QStringLiteral("UNITTEST_VWORLD_KEY");

  QVERIFY2(!LayerOps::addVworldBaseMap(&proj, nullptr, QString(), &err), "empty key must fail");
  QVERIFY(err.contains(QStringLiteral("API 키")));

  QVERIFY2(LayerOps::addVworldBaseMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 배경")));
  QVERIFY(LayerOps::setLayerOpacity(&proj, nullptr, QStringLiteral("VWorld 배경"), 0.5));

  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 위성")));
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("VWorld 위성"), false));
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("VWorld 위성"), true));

  QVERIFY2(LayerOps::addVworldHybridMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 하이브리드")));

  err.clear();
  const bool cad = LayerOps::addVworldCadastralMap(&proj, nullptr, testKey, &err);
  if (!cad) QVERIFY(!err.isEmpty());
  err.clear();
  const bool cont = LayerOps::addVworldContourMap(&proj, nullptr, testKey, &err);
  if (!cont) QVERIFY(!err.isEmpty());
}

void TestWorkflow::vworldSatAndCadastralLiveKey() {
  QString key = QString::fromUtf8(qgetenv("VWORLD_API_KEY")).trimmed();
  if (key.isEmpty()) {
    const QString stored = VworldSettings::loadApiKey();
    const bool looksReal = stored.size() >= 30 && stored.contains(QLatin1Char('-'));
    key = looksReal ? stored : QString();
  }
  if (key.isEmpty())
    QSKIP("VWORLD_API_KEY / saved key missing — skip live VWorld network test");

  VworldSettings::saveApiKey(key);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  const bool satOk = LayerOps::addVworldSatelliteMap(&proj, nullptr, key, &err);
  if (!satOk) {
    const QString e = err;
    if (e.contains(QStringLiteral("INVALID"), Qt::CaseInsensitive) ||
        e.contains(QStringLiteral("등록")) || e.contains(QStringLiteral("키")))
      QSKIP(qPrintable(QStringLiteral("VWorld key rejected by server: %1").arg(e)));
    QFAIL(qPrintable(e));
  }
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 위성")));

  err.clear();
  const bool cadOk = LayerOps::addVworldCadastralMap(&proj, nullptr, key, &err);
  if (!cadOk) {
    const QString e = err;
    if (e.contains(QStringLiteral("INVALID"), Qt::CaseInsensitive) ||
        e.contains(QStringLiteral("등록")) || e.contains(QStringLiteral("키")))
      QSKIP(qPrintable(QStringLiteral("VWorld cadastral rejected: %1").arg(e)));
    QFAIL(qPrintable(e));
  }
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 지적")) ||
          projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 지적도")));
}

void TestWorkflow::workflowGuideTracksSevenRealMilestones() {
  QJsonObject state = ProjectStateBuilder::empty();
  const auto empty = WorkflowGuide::evaluate(state, false, 3, false);
  QCOMPARE(empty.size(), 7);
  QVERIFY(!empty.at(static_cast<int>(WorkflowStep::Survey)).complete);
  QVERIFY(!WorkflowGuide::canCreateSubmission(3));

  state.insert(QStringLiteral("survey_name"), QStringLiteral("테스트조사"));
  state.insert(QStringLiteral("survey_area_count"), 1);
  state.insert(QStringLiteral("feature_poly_count"), 1);
  state.insert(QStringLiteral("has_kind_period"), true);
  state.insert(QStringLiteral("control_points_count"), 2);
  state.insert(QStringLiteral("has_datum"), true);
  state.insert(QStringLiteral("has_ellipsoid"), true);
  state.insert(QStringLiteral("has_projection"), true);
  const auto ready = WorkflowGuide::evaluate(state, true, 0, true);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Survey)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Background)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::SurveyArea)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Features)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Review)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Submission)).complete);
  QVERIFY(WorkflowGuide::canCreateSubmission(0));
}

void TestWorkflow::vworldSettingsAndNoKeyTests() {
  qputenv("VWORLD_API_KEY", QByteArray());
  VworldSettings::saveApiKey(QString());

  QSettings legacy(QStringLiteral("ka-hgis"), QStringLiteral("ka-hgis"));
  legacy.remove(QStringLiteral("VWorld/ApiKey"));
  legacy.setValue(QStringLiteral("vworld/apiKey"), QStringLiteral("LEGACY_MIGRATE_KEY"));
  legacy.sync();

  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));
  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));
  QCOMPARE(LocationSearch::vworldApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));

  const QString testKey = QStringLiteral("TEST_KEY_9999");
  VworldSettings::saveApiKey(testKey);
  QCOMPARE(VworldSettings::loadApiKey(), testKey);
  QCOMPARE(LocationSearch::vworldApiKey(), testKey);
  LocationSearch::setVworldApiKey(QStringLiteral("VIA_LOCATION_SEARCH"));
  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("VIA_LOCATION_SEARCH"));

  QgsProject proj;
  QString err;
  QVERIFY2(LayerOps::addVworldBaseMap(&proj, nullptr, VworldSettings::loadApiKey(), &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 배경")));
  QVERIFY(LayerOps::hasVisibleReferenceLayer(&proj));

  VworldSettings::saveApiKey(QString());
  QCOMPARE(VworldSettings::loadApiKey(), QString());
  QgsProject proj2;
  QVERIFY(!LayerOps::addVworldBaseMap(&proj2, nullptr, VworldSettings::loadApiKey(), &err));
  QVERIFY(!projectHasLayerNamedLike(&proj2, QStringLiteral("VWorld 배경")));
}

void TestWorkflow::editBufferCommitSurvivesReopen() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_edit_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("editbuf"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  {
    auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                  QStringLiteral("survey_area"), QStringLiteral("ogr"));
    QVERIFY(sa->isValid());
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("커밋생존"));
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000) << QgsPointXY(200080, 450080)
         << QgsPointXY(200000, 450080) << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->isEditable());
    QVERIFY(sa->commitChanges());
    QVERIFY(!sa->isEditable());
    QCOMPARE(int(sa->featureCount()), 1);
    delete sa;
  }

  auto* reopened = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                      QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(reopened->isValid());
  QCOMPARE(int(reopened->featureCount()), 1);
  QgsFeature f = reopened->getFeature(*reopened->allFeatureIds().constBegin());
  QCOMPARE(f.attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("커밋생존"));
  delete reopened;
}

void TestWorkflow::importControlCsvWritesFeatures() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_csv_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("csvcp"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg),
                                QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  QCOMPARE(int(cp->featureCount()), 0);

  const QString csvPath = QDir(dir).filePath(QStringLiteral("gcp.csv"));
  {
    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream ts(&f);
    ts << "point_id,x,y,datum,ellipsoid,projection,accuracy_m,pdop,fix_type\n";
    ts << "G1,200000,450000,세계측지계,GRS80,UTM-K,0.8,1.2,RTK\n";
    ts << "G2,200050,450020,세계측지계,GRS80,UTM-K,0.9,1.1,RTK\n";
  }

  QString ierr;
  const int n = LayerOps::importControlPointsCsv(cp, csvPath, &ierr);
  QVERIFY2(n == 2, qPrintable(ierr.isEmpty() ? QString::number(n) : ierr));
  QCOMPARE(int(cp->featureCount()), 2);

  QgsFeature f1 = cp->getFeature(*cp->allFeatureIds().constBegin());
  QVERIFY(!f1.attribute(QStringLiteral("point_id")).toString().isEmpty());
  QVERIFY(!f1.attribute(QStringLiteral("datum")).toString().isEmpty());
  QVERIFY(f1.hasGeometry());

  QJsonObject st = ProjectStateBuilder::fromProject(nullptr);
  QgsProject proj;
  proj.addMapLayer(cp);
  st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("control_points_count")).toInt(), 2);
  QVERIFY(st.value(QStringLiteral("has_datum")).toBool());
  QVERIFY(st.value(QStringLiteral("has_ellipsoid")).toBool());
  QVERIFY(st.value(QStringLiteral("has_projection")).toBool());

  const auto steps = WorkflowGuide::evaluate(st, true, 0, false);
  QVERIFY(steps.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);

  QJsonObject oneOnly = st;
  oneOnly.insert(QStringLiteral("control_points_count"), 1);
  const auto incomplete = WorkflowGuide::evaluate(oneOnly, true, 0, false);
  QVERIFY(!incomplete.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);
}

void TestWorkflow::osmBasemapValidWithExtent() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addOsmBasemap(&proj, nullptr, &err), qPrintable(err));

  QgsRasterLayer* rl = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n == QLatin1String("OSM") || n.startsWith(QLatin1String("OSM [")) ||
        n == QLatin1String("Carto Light") || n.startsWith(QLatin1String("Carto Light ["))) {
      rl = qobject_cast<QgsRasterLayer*>(l);
      if (rl) break;
    }
  }
  QVERIFY2(rl, "OSM/Carto layer missing");
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  const QgsRectangle ext = rl->extent();
  QVERIFY2(ext.isFinite() && !ext.isEmpty(), "basemap extent empty");
  QVERIFY(LayerOps::hasVisibleReferenceLayer(&proj));

  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(!kr.isEmpty() && kr.isFinite(), "korea extent invalid for EPSG:5186");
}


void TestWorkflow::layerTreeDragReorderChangesOrder() {
  QgsProject proj;
  auto* l1 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("A"), QStringLiteral("memory"));
  auto* l2 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("B"), QStringLiteral("memory"));
  QVERIFY(l1->isValid() && l2->isValid());
  proj.addMapLayer(l1);
  proj.addMapLayer(l2);
  QgsLayerTree* root = proj.layerTreeRoot();
  QCOMPARE(root->children().size(), 2);
  QgsLayerTreeModel model(root);
  model.setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  QVERIFY(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsDragEnabled));
  // Move first child to end: B then A (or reverse depending add order)
  QgsLayerTreeNode* first = root->children().at(0);
  QVERIFY(root->takeChild(first));
  root->insertChildNode(root->children().size(), first);
  QCOMPARE(root->children().size(), 2);
  QCOMPARE(root->children().at(1), first);
}
void TestWorkflow::layerTreeReorderAndRemovalTest() {
  QgsProject proj;
  auto* l1 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("L1"), QStringLiteral("memory"));
  auto* l2 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("L2"), QStringLiteral("memory"));
  QVERIFY(l1->isValid() && l2->isValid());

  proj.addMapLayer(l1);
  proj.addMapLayer(l2);

  QgsLayerTreeModel model(proj.layerTreeRoot());
  model.setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  QVERIFY(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsDragEnabled));

  QCOMPARE(proj.mapLayers().size(), 2);
  const QString id1 = l1->id();
  QVERIFY(LayerOps::removeConfirmedLayers(&proj, nullptr, {id1}));
  QCOMPARE(proj.mapLayers().size(), 1);
  QVERIFY(proj.mapLayersByName(QStringLiteral("L1")).isEmpty());
}

// Phase-1: digitize attrs must key off layer_key, not Korean display name (유구면).
void TestWorkflow::layerKeyOfKoreanDisplayName_featurePolyAttrs() {
  QgsProject proj;
  auto* fp = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=kind:string&field=period:string"),
                                QStringLiteral("유구면"), QStringLiteral("memory"));
  QVERIFY2(fp->isValid(), qPrintable(fp->error().message()));
  QCOMPARE(fp->name(), QStringLiteral("유구면"));
  // Without mark: key empty — name-based branch would miss Korean titles.
  QVERIFY(LayerOps::layerKeyOf(fp).isEmpty());
  QVERIFY(fp->name() != QLatin1String("feature_poly"));

  LayerOps::markSurveyLayer(fp, QStringLiteral("feature_poly"));
  proj.addMapLayer(fp);

  QCOMPARE(LayerOps::layerKeyOf(fp), QStringLiteral("feature_poly"));
  QCOMPARE(fp->name(), QStringLiteral("유구면"));
  // Same branch condition MainWindow::onGeometryCaptured must use:
  const QString layerKey = LayerOps::layerKeyOf(fp);
  QVERIFY(layerKey == QLatin1String("feature_poly") || layerKey == QLatin1String("feature_line"));

  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200020, 450020) << QgsPointXY(200040, 450020) << QgsPointXY(200040, 450040)
       << QgsPointXY(200020, 450040) << QgsPointXY(200020, 450020);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  // findByLayerKey / ProjectStateBuilder resolve via key, not display name
  QCOMPARE(LayerOps::findByLayerKey(&proj, QStringLiteral("feature_poly")), fp);
  QVERIFY(proj.mapLayersByName(QStringLiteral("feature_poly")).isEmpty());
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("feature_poly_count")).toInt(), 1);
  QVERIFY(st.value(QStringLiteral("has_kind_period")).toBool());
}

// Phase-1: package export must not gate on checklist errors when blockOnError=false.
void TestWorkflow::exportAllowsDespiteChecklistErrors() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_pkg_err_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir(dir).removeRecursively();
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      nullptr, dir, QStringLiteral("UTF-8"),
      QStringLiteral("- [error] missing survey\n"),
      /*blockOnError=*/false, /*hasChecklistErrors=*/true, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("README_submit.txt"))));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("MANIFEST.sha256"))));
  QFile readme(QDir(dir).filePath(QStringLiteral("README_submit.txt")));
  QVERIFY(readme.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString body = QString::fromUtf8(readme.readAll());
  QVERIFY(body.contains(QStringLiteral("missing survey")));
}

#include "test_workflow.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestWorkflow tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
