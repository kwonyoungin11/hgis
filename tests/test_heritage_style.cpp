#include <QtTest>
#include <QFontDatabase>
#include <QSet>
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

#include "core/HeritageSiteLegend.h"
#include "core/HeritageImport.h"
#include "core/HeritageStyle.h"

// 계획서 2026-09-11-heritage-intranet-nearby-sites.md 7.4 · 7.5 를 붙잡는다.
class HeritageStyleTest : public QObject {
  Q_OBJECT

  static QgsVectorLayer* makeLayer(const QStringList& names, bool addEmpty = false) {
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("매장유산유존지역"), QStringLiteral("memory"));
    if (!layer->isValid()) return layer;
    layer->startEditing();
    double x = 190000.;
    auto addOne = [&](const QVariant& value) {
      QgsFeature f(layer->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(x, 550000.) << QgsPointXY(x + 100., 550000.)
           << QgsPointXY(x + 100., 550100.) << QgsPointXY(x, 550100.)
           << QgsPointXY(x, 550000.);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      f.setAttribute(QStringLiteral("nm"), value);
      layer->addFeature(f);
      x += 200.;
    };
    for (const QString& n : names) addOne(n);
    if (addEmpty) addOne(QVariant(QString()));
    layer->commitChanges();
    return layer;
  }

  static QStringList nodeLabels(const QList<QgsLayerTreeModelLegendNode*>& nodes) {
    QStringList out;
    for (QgsLayerTreeModelLegendNode* n : nodes) {
      if (n) out << n->data(Qt::DisplayRole).toString();
    }
    return out;
  }

private slots:
  void colorTableMatchesPlan() {
    QCOMPARE(HeritageStyle::color(HeritageDataset::DesignatedHeritage).name(), QStringLiteral("#8e44ad"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::AlterationStandard).name(), QStringLiteral("#e67e22"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::BuriedHeritageArea).name(), QStringLiteral("#2e86c1"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::HeritageDistributionMap).name(), QStringLiteral("#27ae60"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::SurfaceSurveyArea).name(), QStringLiteral("#16a085"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::ExcavationSurveyArea).name(), QStringLiteral("#a0522d"));
    QCOMPARE(HeritageStyle::allDatasets().size(), 6);
  }

  void everyDatasetHasItsOwnColorAndName() {
    QSet<QString> colors;
    QSet<QString> names;
    for (HeritageDataset ds : HeritageStyle::allDatasets()) {
      colors.insert(HeritageStyle::color(ds).name());
      names.insert(HeritageStyle::layerName(ds));
      QVERIFY(!HeritageStyle::layerName(ds).isEmpty());
      QCOMPARE(HeritageStyle::fromLayerName(HeritageStyle::layerName(ds)).value(), ds);
    }
    QCOMPARE(colors.size(), 6);
    QCOMPARE(names.size(), 6);
  }

  void reservedColorsAreRefused() {
    // 빨강은 조사구역 팔레트가 쓴다.
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#DC2626"))));
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#FF0000"))));
    // 회색은 수치지형도 밑그림이 쓴다.
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#808080"))));
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#000000"))));
    // 표의 여섯 색은 어느 것도 걸리지 않는다.
    for (HeritageDataset ds : HeritageStyle::allDatasets())
      QVERIFY2(!HeritageStyle::isReservedColor(HeritageStyle::color(ds)),
               qPrintable(HeritageStyle::layerName(ds)));
  }

  void legendGetsOneRowPerSiteNameInOneColor() {
    std::unique_ptr<QgsVectorLayer> layer(
        makeLayer({QStringLiteral("안동 저전리유적"), QStringLiteral("안동 마애리유적"),
                   QStringLiteral("안동 조탑리유적")}));
    QVERIFY(layer->isValid());
    const HeritageStyleResult r =
        HeritageStyle::apply(layer.get(), HeritageDataset::BuriedHeritageArea, QStringLiteral("nm"));
    QVERIFY(r.ok);
    QVERIFY(!r.overCap);

    auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer());
    QVERIFY(cat);
    QCOMPARE(cat->classAttribute(), QStringLiteral("nm"));
    // 유적 3곳 + 무명 자리
    QCOMPARE(cat->categories().size(), 4);

    const QColor expected = HeritageStyle::color(HeritageDataset::BuriedHeritageArea);
    QStringList labels;
    for (const QgsRendererCategory& c : cat->categories()) {
      labels << c.label();
      QVERIFY(c.symbol());
      // 한 종류는 언제나 한 색이다.
      QCOMPARE(c.symbol()->color().name(), expected.name());
    }
    QVERIFY(labels.contains(QStringLiteral("안동 저전리유적")));
    QVERIFY(labels.contains(QStringLiteral("안동 마애리유적")));
    QVERIFY(labels.contains(HeritageStyle::unnamedLabel()));
  }

  void emptySiteNameStillDraws() {
    std::unique_ptr<QgsVectorLayer> layer(makeLayer({QStringLiteral("안동 저전리유적")}, true));
    QVERIFY(layer->isValid());
    QVERIFY(HeritageStyle::apply(layer.get(), HeritageDataset::BuriedHeritageArea,
                                 QStringLiteral("nm")).ok);
    auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer());
    QVERIFY(cat);
    // 빈 값을 받는 카테고리가 없으면 그 도형이 아예 안 그려진다.
    bool hasCatchAll = false;
    for (const QgsRendererCategory& c : cat->categories()) {
      if (!c.value().isValid() || c.value().toString().trimmed().isEmpty()) hasCatchAll = true;
    }
    QVERIFY(hasCatchAll);
  }

  void missingNameFieldSaysSoInsteadOfPretending() {
    std::unique_ptr<QgsVectorLayer> layer(makeLayer({QStringLiteral("안동 저전리유적")}));
    const HeritageStyleResult r = HeritageStyle::apply(
        layer.get(), HeritageDataset::BuriedHeritageArea, QStringLiteral("없는필드"));
    QVERIFY(r.ok);
    QVERIFY(!r.message.isEmpty());
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
  }

  void nameFieldIsChosenFromRealFieldsNotGuessed() {
    auto makeWith = [](const QString& uriFields) {
      return new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&%1").arg(uriFields),
                                QStringLiteral("t"), QStringLiteral("memory"));
    };
    // 있는 것 중에서 고른다.
    std::unique_ptr<QgsVectorLayer> a(makeWith(QStringLiteral("field=id:integer&field=유적명:string(80)")));
    QVERIFY(a->isValid());
    QCOMPARE(HeritageImport::chooseNameField(a.get()), QStringLiteral("유적명"));

    // 지표조사구역·발굴조사구역은 사업명으로 온다.
    std::unique_ptr<QgsVectorLayer> b(makeWith(QStringLiteral("field=id:integer&field=사업명:string(80)")));
    QCOMPARE(HeritageImport::chooseNameField(b.get()), QStringLiteral("사업명"));

    // 「명」이 들어간 글자 필드로 떨어진다.
    std::unique_ptr<QgsVectorLayer> c(makeWith(QStringLiteral("field=id:integer&field=지정명칭:string(80)")));
    QCOMPARE(HeritageImport::chooseNameField(c.get()), QStringLiteral("지정명칭"));

    // 숫자만 있으면 고르지 않는다. 없는 이름을 지어내지 않는다.
    std::unique_ptr<QgsVectorLayer> d(makeWith(QStringLiteral("field=id:integer&field=area:double")));
    QVERIFY(HeritageImport::chooseNameField(d.get()).isEmpty());
  }

  void oneZipCanCarryManyShapefiles() {
    // 지정유산 ZIP 하나에 6종이 들어온다(2026-09-12 실제 파일).
    // 색은 종류(지정유산) 하나로 통일하되, 레이어 이름은 파일 이름을 살려야 구분된다.
    const QStringList inZip = {QStringLiteral("국가지정유산"), QStringLiteral("시도지정유산"),
                               QStringLiteral("국가등록문화유산"), QStringLiteral("시도등록문화유산"),
                               QStringLiteral("국가지정유산보호구역"),
                               QStringLiteral("시도지정유산보호구역")};
    for (const QString& name : inZip) {
      // 이 이름들은 종류 이름과 다르다. fromLayerName 이 종류로 오인하면 안 된다.
      QVERIFY2(!HeritageStyle::fromLayerName(name).has_value() ||
                   HeritageStyle::fromLayerName(name).value() == HeritageDataset::DesignatedHeritage,
               qPrintable(name));
    }
  }

  void referenceGroupKeepsHeritageOutOfSurveyData() {
    QCOMPARE(HeritageImport::referenceGroupName(), QStringLiteral("참조 지도"));
  }

  void panelShowsTypeOnlyAndSheetShowsEverySiteName() {
    QgsProject project;
    auto* layer = makeLayer({QStringLiteral("안동 저전리유적"), QStringLiteral("안동 마애리유적")});
    QVERIFY(layer->isValid());
    project.addMapLayer(layer);
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::BuriedHeritageArea,
                                 QStringLiteral("nm")).ok);
    QVERIFY(HeritageSiteLegend::install(layer));
    QVERIFY(HeritageSiteLegend::isInstalled(layer));

    // 레이어창: 프로젝트 트리에 달린 노드
    QgsLayerTreeLayer* panelNode = project.layerTreeRoot()->findLayer(layer->id());
    QVERIFY(panelNode);
    QVERIFY(HeritageSiteLegend::isPanelNode(panelNode, layer));
    QList<QgsLayerTreeModelLegendNode*> panel =
        layer->legend()->createLayerTreeModelLegendNodes(panelNode);
    QCOMPARE(panel.size(), 1);
    QVERIFY(panel.first()->isEmbeddedInParent());
    qDeleteAll(panel);

    // 도면 범례: tuneSheetLegend 가 Manual 동기화로 만드는 복제 트리
    QgsLayerTreeLayer sheetNode(layer);
    QVERIFY(!HeritageSiteLegend::isPanelNode(&sheetNode, layer));
    QList<QgsLayerTreeModelLegendNode*> sheet =
        layer->legend()->createLayerTreeModelLegendNodes(&sheetNode);
    const QStringList labels = nodeLabels(sheet);
    QVERIFY2(labels.size() >= 3, qPrintable(QString::number(labels.size())));
    QVERIFY(labels.contains(QStringLiteral("안동 저전리유적")));
    QVERIFY(labels.contains(QStringLiteral("안동 마애리유적")));
    qDeleteAll(sheet);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString fonts =
      qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts/");
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(fonts + file);
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageStyleTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_style.moc"
