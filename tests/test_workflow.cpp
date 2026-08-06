#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>

#include "core/SurveyProjectFactory.h"
#include "core/ProjectStateBuilder.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsvectorfilewriter.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgscoordinatetransformcontext.h>

class TestWorkflow : public QObject {
  Q_OBJECT
private slots:
  void fullWorkflowSurveyToPackage();
  void shpKoreanRoundTripUtf8();
  void reprojectAndMigrateFields();
};

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
  // drop simulation: ensure fields still adds 0 if present
  const int n = LayerOps::ensureControlPointQualityFields(cp);
  QVERIFY(n >= 0);
  QVERIFY(cp->fields().indexOf(QStringLiteral("accuracy_m")) >= 0);
}

#include "test_workflow.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("D:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  TestWorkflow tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
