#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSet>

#include "core/AndongPack.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>

class TestAndong : public QObject {
  Q_OBJECT
private slots:
  void listShapefiles_findsAndongHeritage();
  void preferredLabelField_siteThenHeritageThenLot();
  void isSiteAndCadastral_byTitleOrJibun();
  void cityMask_isInvertedWhiteOn5179();
  void applyLabels_canToggleOff();
  void localRaster_rejectsRemoteXyz();
  void emdFilter_keepsAndongOnly();
  void preferredLabelField_emdName();
  void presentationStyleFor_splitsNationalVsSidoAndSurfaceVsExcavation();
  void presentationStyleFor_keepsEmdAndCadastralOutOfHeritagePalette();
  void cityMarkPath_findsPng();
  void applyRasterOpacityPercent_clamps();
  void themeQss_usesOfficialAndongViColors();
  void satelliteRasterPath_fallsBackToRepoPack();
  void labelPointSize_isTwoPointsLarger();
};

static QgsVectorLayer* memLayer(const QString& name, const QStringList& fields) {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), name, QStringLiteral("memory"));
  Q_ASSERT(vl->isValid());
  QList<QgsField> add;
  for (const QString& n : fields)
    add.append(QgsField(n, QMetaType::QString));
  vl->dataProvider()->addAttributes(add);
  vl->updateFields();
  return vl;
}

void TestAndong::listShapefiles_findsAndongHeritage() {
  const QString root = QStringLiteral("D:/hgis/안동시");
  QVERIFY2(QDir(root).exists(), "D:/hgis/안동시");
  const QStringList shps = AndongPack::listShapefiles(root);
  QVERIFY2(shps.size() >= 10, qPrintable(QString::number(shps.size())));
  bool hasSite = false;
  bool hasDist = false;
  for (const QString& p : shps) {
    if (p.contains(QStringLiteral("지표유적위치도"))) hasSite = true;
    if (p.contains(QStringLiteral("문화유적분포지도"))) hasDist = true;
  }
  QVERIFY(hasSite);
  QVERIFY(hasDist);
}

void TestAndong::preferredLabelField_siteThenHeritageThenLot() {
  auto* site = memLayer(QStringLiteral("발굴유적위치도"),
                        {QStringLiteral("소재지"), QStringLiteral("사업명"), QStringLiteral("유적명")});
  QCOMPARE(AndongPack::preferredLabelField(site), QStringLiteral("유적명"));
  delete site;

  auto* heritage = memLayer(QStringLiteral("국가지정유산"),
                            {QStringLiteral("지정번호"), QStringLiteral("국가유산명"), QStringLiteral("소재지")});
  QCOMPARE(AndongPack::preferredLabelField(heritage), QStringLiteral("국가유산명"));
  delete heritage;

  auto* dist = memLayer(QStringLiteral("문화유적분포지도"),
                        {QStringLiteral("유산코드"), QStringLiteral("명칭")});
  QCOMPARE(AndongPack::preferredLabelField(dist), QStringLiteral("명칭"));
  delete dist;

  auto* lot = memLayer(QStringLiteral("연속지적"),
                       {QStringLiteral("pnu"), QStringLiteral("지번")});
  QCOMPARE(AndongPack::preferredLabelField(lot), QStringLiteral("지번"));
  delete lot;
}

void TestAndong::isSiteAndCadastral_byTitleOrJibun() {
  QVERIFY(AndongPack::isSiteLayer(QStringLiteral("지표유적위치도")));
  QVERIFY(AndongPack::isSiteLayer(QStringLiteral("발굴유적위치도")));
  QVERIFY(!AndongPack::isSiteLayer(QStringLiteral("지표사업허가구역")));

  auto* cad = memLayer(QStringLiteral("필지"), {QStringLiteral("지번")});
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("안동 지적"), cad));
  QVERIFY(AndongPack::isCadastralLayer(QStringLiteral("필지"), cad));
  delete cad;

  auto* other = memLayer(QStringLiteral("구역"), {QStringLiteral("명칭")});
  QVERIFY(!AndongPack::isCadastralLayer(QStringLiteral("현상변경허용기준"), other));
  delete other;
}

void TestAndong::cityMask_isInvertedWhiteOn5179() {
  const QString geo = QStringLiteral("D:/hgis/data/andong/andong_city.geojson");
  QVERIFY2(QFile::exists(geo), qPrintable(geo));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  QgsVectorLayer* mask = AndongPack::loadCityMask(&proj, geo);
  QVERIFY(mask);
  QVERIFY(mask->isValid());
  QVERIFY(dynamic_cast<QgsInvertedPolygonRenderer*>(mask->renderer()));
  QCOMPARE(int(mask->featureCount()), 1);
  const QgsRectangle e = mask->extent();
  QVERIFY2(e.width() > 10000 && e.height() > 10000, qPrintable(e.toString()));
  QVERIFY2(e.xMinimum() > 1000000 && e.xMaximum() < 1200000, "mask should be in EPSG:5179");
}

void TestAndong::applyLabels_canToggleOff() {
  auto* vl = memLayer(QStringLiteral("지표유적위치도"), {QStringLiteral("유적명")});
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  f.setAttribute(QStringLiteral("유적명"), QStringLiteral("하회마을"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 1840000, 1100100, 1840100)));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  QVERIFY(AndongPack::applyLabels(vl));
  QVERIFY(LayerOps::labelsVisible(vl));
  QVERIFY(LayerOps::setLabelsVisible(vl, false));
  QVERIFY(!LayerOps::labelsVisible(vl));
  QVERIFY(LayerOps::setLabelsVisible(vl, true));
  QVERIFY(LayerOps::labelsVisible(vl));
  delete vl;
}

void TestAndong::emdFilter_keepsAndongOnly() {
  auto* vl = memLayer(QStringLiteral("읍면동"),
                      {QStringLiteral("COL_ADM_SE"), QStringLiteral("EMD_NM"), QStringLiteral("EMD_CD")});
  QVERIFY(vl->startEditing());
  QgsFeature a(vl->fields());
  a.setAttribute(QStringLiteral("COL_ADM_SE"), QStringLiteral("47170"));
  a.setAttribute(QStringLiteral("EMD_NM"), QStringLiteral("풍산읍"));
  a.setAttribute(QStringLiteral("EMD_CD"), QStringLiteral("47170250"));
  a.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 1840000, 1100100, 1840100)));
  QVERIFY(vl->addFeature(a));
  QgsFeature b(vl->fields());
  b.setAttribute(QStringLiteral("COL_ADM_SE"), QStringLiteral("47150"));
  b.setAttribute(QStringLiteral("EMD_NM"), QStringLiteral("김천"));
  b.setAttribute(QStringLiteral("EMD_CD"), QStringLiteral("47150101"));
  b.setGeometry(QgsGeometry::fromRect(QgsRectangle(1200000, 1840000, 1200100, 1840100)));
  QVERIFY(vl->addFeature(b));
  QVERIFY(vl->commitChanges());
  QCOMPARE(int(vl->featureCount()), 2);
  QVERIFY(AndongPack::isEmdLayer(QStringLiteral("LSMD_ADM_SECT_UMD_47_202608"), vl));
  QVERIFY2(AndongPack::andongEmdSubset(vl).contains(QLatin1String("47170")),
           qPrintable(AndongPack::andongEmdSubset(vl)));
  QVERIFY(AndongPack::applyAndongEmdFilter(vl));
  QCOMPARE(int(vl->featureCount()), 1);
  QgsFeature only;
  QVERIFY(vl->getFeatures().nextFeature(only));
  QCOMPARE(only.attribute(QStringLiteral("EMD_NM")).toString(), QStringLiteral("풍산읍"));
  QVERIFY(AndongPack::applyPresentationStyle(vl, QStringLiteral("읍면동")));
  delete vl;
}

void TestAndong::preferredLabelField_emdName() {
  auto* vl = memLayer(QStringLiteral("읍면동"),
                      {QStringLiteral("COL_ADM_SE"), QStringLiteral("EMD_NM")});
  QCOMPARE(AndongPack::preferredLabelField(vl), QStringLiteral("EMD_NM"));
  delete vl;
}

void TestAndong::presentationStyleFor_splitsNationalVsSidoAndSurfaceVsExcavation() {
  const QStringList titles = {
      QStringLiteral("문화유적분포지도"),   QStringLiteral("지표유적위치도"),
      QStringLiteral("발굴유적위치도"),     QStringLiteral("지표사업허가구역"),
      QStringLiteral("발굴사업허가구역"),   QStringLiteral("국가지정유산"),
      QStringLiteral("국가등록문화유산"),   QStringLiteral("국가지정유산보호구역"),
      QStringLiteral("시도지정유산"),       QStringLiteral("시도등록문화유산"),
      QStringLiteral("시도지정유산보호구역"), QStringLiteral("현상변경허용기준"),
  };
  QSet<QString> strokes;
  for (const QString& title : titles) {
    auto* vl = memLayer(title, {QStringLiteral("명칭")});
    QVERIFY(AndongPack::applyPresentationStyle(vl, title));
    const AndongPack::PresentationStyle st = AndongPack::presentationStyleFor(title, vl);
    QVERIFY2(st.stroke.isValid(), qPrintable(title));
    const QString hex = st.stroke.name(QColor::HexRgb);
    QVERIFY2(!strokes.contains(hex), qPrintable(title + QLatin1Char(' ') + hex));
    strokes.insert(hex);
    delete vl;
  }
  QCOMPARE(strokes.size(), titles.size());
}

void TestAndong::presentationStyleFor_keepsEmdAndCadastralOutOfHeritagePalette() {
  const AndongPack::PresentationStyle emd =
      AndongPack::presentationStyleFor(QStringLiteral("읍면동"), nullptr);
  QVERIFY(emd.noFill);
  QCOMPARE(emd.stroke.name(QColor::HexRgb).toUpper(), QStringLiteral("#003C78"));
  QVERIFY(emd.mm >= 1.5);

  auto* cad = memLayer(QStringLiteral("안동 지적"), {QStringLiteral("지번")});
  const AndongPack::PresentationStyle lot =
      AndongPack::presentationStyleFor(QStringLiteral("안동 지적"), cad);
  QVERIFY(lot.noFill);
  QVERIFY(lot.mm < 0.5);
  QVERIFY(lot.stroke != emd.stroke);
  delete cad;
}

void TestAndong::cityMarkPath_findsPng() {
  const QString bundled = QStringLiteral("D:/hgis/data/andong/andong_mark.png");
  if (!QFile::exists(bundled))
    QSKIP("andong_mark.png not bundled");
  const QString p = AndongPack::cityMarkPath();
  QVERIFY2(QFile::exists(p), qPrintable(p));
  QVERIFY(p.endsWith(QStringLiteral("andong_mark.png"), Qt::CaseInsensitive));
}

void TestAndong::applyRasterOpacityPercent_clamps() {
  auto* vl = memLayer(QStringLiteral("위성"), {QStringLiteral("명칭")});
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, 40));
  QCOMPARE(vl->opacity(), 0.4);
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, 250));
  QCOMPARE(vl->opacity(), 1.0);
  QVERIFY(AndongPack::applyRasterOpacityPercent(vl, -10));
  QCOMPARE(vl->opacity(), 0.0);
  delete vl;
}

void TestAndong::themeQss_usesOfficialAndongViColors() {
  QFile f(QStringLiteral("D:/hgis/data/theme/andong.qss"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "andong.qss");
  const QString qss = QString::fromUtf8(f.readAll());
  QVERIFY2(qss.contains(QLatin1String("#003C78"), Qt::CaseInsensitive), "AD Blue R0 G60 B120");
  QVERIFY2(qss.contains(QLatin1String("#B41446"), Qt::CaseInsensitive), "AD Red R180 G20 B70");
  QVERIFY2(!qss.contains(QLatin1String("#1B4D3E"), Qt::CaseInsensitive), "old pine leftover");
  QVERIFY2(!qss.contains(QLatin1String("#C9A227"), Qt::CaseInsensitive), "old gold leftover");
  QVERIFY2(qss.contains(QLatin1String("font-size: 15px")), "+2pt body");
  QVERIFY2(!qss.contains(QLatin1String("font-size: 13px")), "old body size");
}

void TestAndong::labelPointSize_isTwoPointsLarger() {
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("지표유적위치도"), nullptr), 10.0);
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("읍면동"), nullptr), 14.0);
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("문화유적분포지도"), nullptr), 9.0);
  auto* cad = memLayer(QStringLiteral("안동 지적"), {QStringLiteral("지번")});
  QCOMPARE(AndongPack::labelPointSize(QStringLiteral("안동 지적"), cad), 8.0);
  delete cad;
}

void TestAndong::satelliteRasterPath_fallsBackToRepoPack() {
  const QString bundled = QStringLiteral("D:/hgis/data/andong/satellite.mbtiles");
  if (!QFile::exists(bundled))
    QSKIP("satellite.mbtiles not packed");
  const QString p = AndongPack::satelliteRasterPath(QStringLiteral("D:/hgis/data/andong/missing"));
  QVERIFY2(QFile::exists(p), qPrintable(p));
  QVERIFY(p.endsWith(QStringLiteral("satellite.mbtiles"), Qt::CaseInsensitive));
}

void TestAndong::localRaster_rejectsRemoteXyz() {
  QVERIFY(AndongPack::satelliteRasterPath(QStringLiteral("D:/hgis/data/andong")).contains(QStringLiteral("satellite")));
  auto* remote = AndongPack::loadLocalRaster(
      QStringLiteral("type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/nokey/Satellite/{z}/{y}/{x}.jpeg"),
      QStringLiteral("위성"));
  QVERIFY2(remote == nullptr, "offline viewer must not open XYZ/WMS");
}

#include "test_andong.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestAndong tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
