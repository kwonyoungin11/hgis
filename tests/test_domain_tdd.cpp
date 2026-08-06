#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QDateTime>
#include <QCoreApplication>
#include <QTextStream>
#include <cmath>

#include "core/SurveyProjectFactory.h"
#include "core/ProjectStateBuilder.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/FeatureWriteService.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsprintlayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemlabel.h>
#include <qgsmasterlayoutinterface.h>

class TestDomainTdd : public QObject {
  Q_OBJECT
private slots:
  void workCrs_defaultIs5186();
  void workCrs_createSurvey5187();
  void digitize_savePolygonOn5186_persistsAfterCommit();
  void digitize_saveLineRequiresTwoPoints();
  void digitize_polygonRejectsFewerThanThreeVertices();
  void convert_5186To5179_crsAndCoordinatesShift();
  void convert_5187To5179_featureCountPreserved();
  void checklist_blocksMissingGcpAndKind();
  void checklist_passesCompleteFieldProject();
  void submission_packageContainsShpAndManifest();
  void layout_rebuildCreatesLegendScaleAndTitle();
  void uploadCrs_constantIs5179();
  void featureWrite_rejectsBadPolygonAndMissingKind();
  void featureWrite_acceptsValidFeatureWithKindPeriod();
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

static QString tempDir(const char* prefix) {
  const QString d = QDir::temp().filePath(QString::fromUtf8(prefix) + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(d);
  return d;
}

static QgsVectorLayer* openLayer(const QString& gpkg, const char* name) {
  return new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkg, QString::fromUtf8(name)),
                            QString::fromUtf8(name), QStringLiteral("ogr"));
}

static QgsGeometry rectPoly(double x, double y, double s = 50) {
  QgsPolylineXY ring;
  ring << QgsPointXY(x, y) << QgsPointXY(x + s, y) << QgsPointXY(x + s, y + s)
       << QgsPointXY(x, y + s) << QgsPointXY(x, y);
  return QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
}

void TestDomainTdd::workCrs_defaultIs5186() {
  QCOMPARE(QString::fromUtf8(SurveyProjectFactory::defaultWorkCrsAuthId()), QStringLiteral("EPSG:5186"));
  QCOMPARE(QString::fromUtf8(SurveyProjectFactory::uploadCrsAuthId()), QStringLiteral("EPSG:5179"));
  const QString dir = tempDir("ka_tdd_def_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("def"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fp = openLayer(gpkg, "feature_poly");
  QVERIFY(fp->isValid());
  QCOMPARE(fp->crs().authid(), QStringLiteral("EPSG:5186"));
  delete fp;
}

void TestDomainTdd::workCrs_createSurvey5187() {
  const QString dir = tempDir("ka_tdd_5187_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("east"), &err,
                                                             QStringLiteral("EPSG:5187"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* sa = openLayer(gpkg, "survey_area");
  QVERIFY(sa->isValid());
  QCOMPARE(sa->crs().authid(), QStringLiteral("EPSG:5187"));
  delete sa;
}

void TestDomainTdd::digitize_savePolygonOn5186_persistsAfterCommit() {
  const QString dir = tempDir("ka_tdd_dig_poly_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("dig"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fp = openLayer(gpkg, "feature_poly");
  QVERIFY(fp->isValid());
  QCOMPARE(int(fp->featureCount()), 0);
  QVERIFY(fp->startEditing());
  QgsFeature f(fp->fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  f.setGeometry(rectPoly(210000, 460000, 40));
  QVERIFY(f.geometry().isGeosValid());
  QVERIFY(fp->addFeature(f));
  QVERIFY(fp->commitChanges());
  QCOMPARE(int(fp->featureCount()), 1);

  auto* reopened = openLayer(gpkg, "feature_poly");
  QVERIFY(reopened->isValid());
  QCOMPARE(int(reopened->featureCount()), 1);
  QgsFeature got;
  reopened->getFeatures().nextFeature(got);
  QVERIFY(got.hasGeometry());
  QVERIFY(got.geometry().area() > 100);
  QCOMPARE(got.attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈주거지"));
  delete fp;
  delete reopened;
}

void TestDomainTdd::digitize_saveLineRequiresTwoPoints() {
  const QString dir = tempDir("ka_tdd_dig_line_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("line"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fl = openLayer(gpkg, "feature_line");
  QVERIFY(fl->isValid());
  QVERIFY(fl->startEditing());

  QgsPolylineXY onePt;
  onePt << QgsPointXY(1, 1);
  QgsGeometry bad = QgsGeometry::fromPolylineXY(onePt);
  QVERIFY(bad.length() == 0.0 || !bad.isGeosValid() || bad.constGet()->nCoordinates() < 2);

  QgsPolylineXY two;
  two << QgsPointXY(210000, 460000) << QgsPointXY(210100, 460050);
  QgsFeature f(fl->fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("구"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("조선"));
  f.setGeometry(QgsGeometry::fromPolylineXY(two));
  QVERIFY(fl->addFeature(f));
  QVERIFY(fl->commitChanges());
  QCOMPARE(int(fl->featureCount()), 1);
  delete fl;
}

void TestDomainTdd::digitize_polygonRejectsFewerThanThreeVertices() {
  QgsPolylineXY two;
  two << QgsPointXY(0, 0) << QgsPointXY(10, 0) << QgsPointXY(0, 0);
  QgsGeometry g = QgsGeometry::fromPolygonXY(QgsPolygonXY() << two);
  QVERIFY(!g.isEmpty());
  QVERIFY2(!g.isGeosValid() || g.area() < 1e-6,
           "two-vertex ring must not be treated as valid survey polygon");
}

void TestDomainTdd::convert_5186To5179_crsAndCoordinatesShift() {
  const QString dir = tempDir("ka_tdd_c5179_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("c"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fp = openLayer(gpkg, "feature_poly");
  QVERIFY(fp->startEditing());
  QgsFeature f(fp->fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("k"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("p"));
  const double x0 = 200000.0, y0 = 450000.0;
  f.setGeometry(rectPoly(x0, y0, 20));
  QVERIFY(fp->addFeature(f));
  QVERIFY(fp->commitChanges());

  QgsPointXY srcPt(x0 + 10, y0 + 10);
  QgsCoordinateTransform xf(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                            QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),
                            QgsCoordinateTransformContext());
  const QgsPointXY expect = xf.transform(srcPt);

  const QString out = QDir(dir).filePath(QStringLiteral("out_5179.shp"));
  QString cerr;
  QVERIFY2(!LayerOps::convertToShp5179(fp, out, nullptr, &cerr).isEmpty(), qPrintable(cerr));
  QgsVectorLayer loaded(out, QStringLiteral("o"), QStringLiteral("ogr"));
  QVERIFY(loaded.isValid());
  QCOMPARE(loaded.crs().authid(), QStringLiteral("EPSG:5179"));
  QCOMPARE(int(loaded.featureCount()), 1);
  QgsFeature lf;
  loaded.getFeatures().nextFeature(lf);
  QVERIFY(lf.hasGeometry());
  const QgsPointXY c = lf.geometry().centroid().asPoint();
  const double d = std::hypot(c.x() - expect.x(), c.y() - expect.y());
  QVERIFY2(d < 5.0, qPrintable(QStringLiteral("centroid drift %1 m").arg(d)));
  delete fp;
}

void TestDomainTdd::convert_5187To5179_featureCountPreserved() {
  const QString dir = tempDir("ka_tdd_c5187_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("e"), &err,
                                                             QStringLiteral("EPSG:5187"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* sa = openLayer(gpkg, "survey_area");
  QVERIFY(sa->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature f(sa->fields());
    f.setGeometry(rectPoly(180000 + i * 100, 500000, 30));
    QVERIFY(sa->addFeature(f));
  }
  QVERIFY(sa->commitChanges());
  const QString out = QDir(dir).filePath(QStringLiteral("sa_5179.shp"));
  QString cerr;
  QVERIFY2(!LayerOps::convertToShp5179(sa, out, nullptr, &cerr).isEmpty(), qPrintable(cerr));
  QgsVectorLayer loaded(out, QStringLiteral("o"), QStringLiteral("ogr"));
  QVERIFY(loaded.isValid());
  QCOMPARE(loaded.crs().authid(), QStringLiteral("EPSG:5179"));
  QCOMPARE(int(loaded.featureCount()), 2);
  delete sa;
}

void TestDomainTdd::checklist_blocksMissingGcpAndKind() {
  ChecklistEngine eng;
  QVERIFY(eng.loadRules(rulesFile()));
  QJsonObject st = ProjectStateBuilder::empty();
  st.insert(QStringLiteral("survey_area_count"), 1);
  st.insert(QStringLiteral("survey_is_polygon"), true);
  st.insert(QStringLiteral("project_crs_set"), true);
  st.insert(QStringLiteral("feature_poly_count"), 1);
  st.insert(QStringLiteral("has_kind_period"), false);
  st.insert(QStringLiteral("control_points_count"), 0);
  st.insert(QStringLiteral("geometries_valid"), true);
  const auto r = eng.evaluate(st);
  int errors = 0;
  QStringList ids;
  for (const auto& x : r) {
    if (!x.passed && x.severity == QLatin1String("error")) {
      ++errors;
      ids << x.id;
    }
  }
  QVERIFY2(errors >= 2, qPrintable(ids.join(QLatin1Char(','))));
}

void TestDomainTdd::checklist_passesCompleteFieldProject() {
  const QString dir = tempDir("ka_tdd_chk_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("ok"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  for (const char* n : {"survey_area", "feature_poly", "feature_line", "section_line", "control_points"}) {
    auto* vl = openLayer(gpkg, n);
    QVERIFY2(vl->isValid(), n);
    proj.addMapLayer(vl);
  }
  auto* sa = qobject_cast<QgsVectorLayer*>(proj.mapLayersByName(QStringLiteral("survey_area")).first());
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  sf.setGeometry(rectPoly(200000, 450000, 200));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  auto* fp = qobject_cast<QgsVectorLayer*>(proj.mapLayersByName(QStringLiteral("feature_poly")).first());
  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("주거지"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동"));
  ff.setGeometry(rectPoly(200050, 450050, 30));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  auto* cp = qobject_cast<QgsVectorLayer*>(proj.mapLayersByName(QStringLiteral("control_points")).first());
  QVERIFY(cp->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature cf(cp->fields());
    cf.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
    cf.setAttribute(QStringLiteral("datum"), QStringLiteral("세계측지계"));
    cf.setAttribute(QStringLiteral("ellipsoid"), QStringLiteral("GRS80"));
    cf.setAttribute(QStringLiteral("projection"), QStringLiteral("TM"));
    cf.setAttribute(QStringLiteral("origin"), QStringLiteral("중부"));
    cf.setAttribute(QStringLiteral("accuracy"), QStringLiteral("1m"));
    cf.setAttribute(QStringLiteral("accuracy_m"), 1.0);
    cf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000.0 + i * 80, 450000.0)));
    QVERIFY(cp->addFeature(cf));
  }
  QVERIFY(cp->commitChanges());
  LayoutService::ensureDefaultLayouts(&proj);

  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  ChecklistEngine eng;
  QVERIFY(eng.loadRules(rulesFile()));
  int errors = 0;
  QString msg;
  for (const auto& r : eng.evaluate(st)) {
    if (!r.passed && r.severity == QLatin1String("error")) {
      ++errors;
      msg += r.id + QLatin1Char(' ');
    }
  }
  QVERIFY2(errors == 0, qPrintable(msg));
}

void TestDomainTdd::submission_packageContainsShpAndManifest() {
  const QString dir = tempDir("ka_tdd_pkg_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("pkg"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = openLayer(gpkg, "survey_area");
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature f(sa->fields());
  f.setGeometry(rectPoly(200000, 450000, 80));
  QVERIFY(sa->addFeature(f));
  QVERIFY(sa->commitChanges());

  const QString pkg = QDir(dir).filePath(QStringLiteral("outpkg"));
  QString e2;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"),
                                                   QStringLiteral("OK"), true, false, &e2)
                .isEmpty(),
           qPrintable(e2));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("survey_area.shp"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("survey_area.dbf"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("survey_area.shx"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("MANIFEST.sha256"))));
  QFile man(QDir(pkg).filePath(QStringLiteral("MANIFEST.sha256")));
  QVERIFY(man.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString body = QString::fromUtf8(man.readAll());
  QVERIFY(body.contains(QStringLiteral("survey_area.shp")));
}

void TestDomainTdd::layout_rebuildCreatesLegendScaleAndTitle() {
  const QString dir = tempDir("ka_tdd_lay_");
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("lay"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = openLayer(gpkg, "survey_area");
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature f(sa->fields());
  f.setGeometry(rectPoly(200000, 450000, 100));
  QVERIFY(sa->addFeature(f));
  QVERIFY(sa->commitChanges());

  const int n = LayoutService::rebuildDefaultLayouts(&proj);
  QVERIFY(n >= 5);
  QgsMasterLayoutInterface* master = proj.layoutManager()->layoutByName(QStringLiteral("feature_plan"));
  QVERIFY(master);
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  QVERIFY(layout);

  bool hasMap = false, hasLegend = false, hasScale = false, hasTitle = false;
  for (QGraphicsItem* gi : layout->items()) {
    if (dynamic_cast<QgsLayoutItemMap*>(gi)) hasMap = true;
    if (dynamic_cast<QgsLayoutItemLegend*>(gi)) hasLegend = true;
    if (dynamic_cast<QgsLayoutItemScaleBar*>(gi)) hasScale = true;
    if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(gi)) {
      if (lbl->text().contains(QStringLiteral("유구배치도")) || lbl->text().contains(QStringLiteral("축척")))
        hasTitle = true;
    }
  }
  QVERIFY2(hasMap, "map missing");
  QVERIFY2(hasLegend, "legend missing");
  QVERIFY2(hasScale, "scalebar missing");
  QVERIFY2(hasTitle, "title/scale label missing");
}

void TestDomainTdd::uploadCrs_constantIs5179() {
  QCOMPARE(QString::fromUtf8(SurveyProjectFactory::uploadCrsAuthId()), QStringLiteral("EPSG:5179"));
  QVERIFY(QString::fromUtf8(SurveyProjectFactory::defaultWorkCrsAuthId()) !=
          QString::fromUtf8(SurveyProjectFactory::uploadCrsAuthId()));
}

void TestDomainTdd::featureWrite_rejectsBadPolygonAndMissingKind() {
  QString err;
  QVERIFY(!FeatureWriteService::isAcceptablePolygon(QgsGeometry(), &err));
  QVERIFY(!err.isEmpty());

  QgsPolylineXY two;
  two << QgsPointXY(0, 0) << QgsPointXY(1, 0) << QgsPointXY(0, 0);
  QVERIFY(!FeatureWriteService::isAcceptablePolygon(
      QgsGeometry::fromPolygonXY(QgsPolygonXY() << two), &err));

  const QString dir = tempDir("ka_tdd_fw_bad_");
  QString serr;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("fw"), &serr,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(serr));
  auto* fp = openLayer(gpkg, "feature_poly");
  QVERIFY(fp->isValid());
  const auto bad = FeatureWriteService::addFeature(fp, rectPoly(200000, 450000, 20), {});
  QVERIFY(!bad.ok);
  QVERIFY(bad.errorKo.contains(QStringLiteral("종류")) || bad.errorKo.contains(QStringLiteral("시대")));
  delete fp;
}

void TestDomainTdd::featureWrite_acceptsValidFeatureWithKindPeriod() {
  const QString dir = tempDir("ka_tdd_fw_ok_");
  QString serr;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("fwok"), &serr,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(serr));
  auto* fp = openLayer(gpkg, "feature_poly");
  QVariantMap attrs;
  attrs.insert(QStringLiteral("kind"), QStringLiteral("수혈"));
  attrs.insert(QStringLiteral("period"), QStringLiteral("청동"));
  const auto ok = FeatureWriteService::addFeature(fp, rectPoly(200000, 450000, 25), attrs);
  QVERIFY2(ok.ok, qPrintable(ok.errorKo));
  QVERIFY(fp->commitChanges());
  QCOMPARE(int(fp->featureCount()), 1);
  delete fp;
}

#include "test_domain_tdd.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("D:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  TestDomainTdd tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
