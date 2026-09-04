#include "GeologyMapService.h"
#include "LayerOps.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>
#include <cmath>

#include <qgsblockingnetworkrequest.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfields.h>
#include <qgsfillsymbol.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgspallabeling.h>
#include <qgspointxy.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include <qgshillshaderenderer.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsrasterdataprovider.h>

#include <QNetworkRequest>
#include <QPainter>

namespace {

constexpr const char* kWfsUrl = "https://data.kigam.re.kr/geoserver/ows";
constexpr const char* kTypeName = "geoOpen:l_50k_geology_litho_latest";
constexpr const char* kJejuTypeName = "geoOpen:l_jeju_50k_geology_litho_view";
constexpr const char* kWmsRaster = "geoOpen:L_50K_Geology_Map";
constexpr const char* kLayerTitle = "지질도(KIGAM 1:5만)";
constexpr const char* kReliefTitle = "지질 음영";
constexpr const char* kEraSrcField = "시대";
constexpr const char* kEraField = "era_class";
constexpr const char* kSymbolField = "기호";
constexpr const char* kStratField = "지층";

// ICS(국제층서위원회) 표준 지질시대색. 젊은 시대 → 오래된 시대 순서(범례 순서).
// keyword는 서버 시대 문자열에서 찾는 부분 문자열이며, 세분(기) 우선으로 검사한다.
struct EraClassDef {
  const char* keyword;
  const char* name;
  int r, g, b;
};
constexpr EraClassDef kEras[] = {
    {"제4기", "제4기", 249, 249, 127},
    {"네오기", "네오기(신제3기)", 255, 230, 25},
    {"신제3기", "네오기(신제3기)", 255, 230, 25},
    {"팔레오기", "팔레오기(고제3기)", 253, 154, 82},
    {"고제3기", "팔레오기(고제3기)", 253, 154, 82},
    {"백악기", "백악기", 127, 198, 78},
    {"쥐라기", "쥐라기", 52, 178, 201},
    {"쥬라기", "쥐라기", 52, 178, 201},
    {"트라이아스기", "트라이아스기", 129, 43, 146},
    {"페름기", "페름기", 240, 64, 40},
    {"석탄기", "석탄기", 103, 165, 153},
    {"데본기", "데본기", 203, 140, 55},
    {"실루리아기", "실루리아기", 179, 225, 182},
    {"오르도비스기", "오르도비스기", 0, 146, 112},
    {"캄브리아기", "캄브리아기", 127, 160, 86},
    // 대(era) 단위 — 기 단위가 없을 때의 대분류.
    {"신생대", "신생대", 242, 249, 29},
    {"중생대", "중생대", 103, 197, 202},
    {"고생대", "고생대", 153, 192, 141},
    {"원생누대", "원생누대", 247, 53, 99},
    {"시생누대", "시생누대", 240, 4, 127},
    {"선캄브리아", "선캄브리아시대", 247, 67, 112},
};
constexpr const char* kEraUnknown = "시대미상";

QgsSymbol* eraFillSymbol(const QColor& base) {
  QColor fill = base;
  fill.setAlpha(220);
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), QColor(80, 80, 80, 130).name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QStringLiteral("0.12")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

// 제주 WFS는 「제 4기」처럼 기·숫자 사이에 공백이 있다.
QString compactEra(const QString& eraText) {
  QString s = eraText;
  s.remove(QLatin1Char(' '));
  return s;
}

bool eraTextMatches(const QString& eraText, const char* keyword) {
  const QString kw = QString::fromUtf8(keyword);
  if (eraText.contains(kw)) return true;
  return compactEra(eraText).contains(compactEra(kw));
}

// 시대 문자열의 젊은→오래된 순위(범례 정렬용). kEras 배열 순서를 그대로 쓴다.
int eraRank(const QString& eraText) {
  int i = 0;
  for (const EraClassDef& e : kEras) {
    if (eraTextMatches(eraText, e.keyword)) return i;
    ++i;
  }
  return int(std::size(kEras));
}

// 공식 도폭색 샘플링 실패 시: 시대 기준색에 단위별로 밝기 변화를 줘 구분한다.
QColor unitFallbackColor(const QString& eraText, const QString& sym) {
  QColor base = GeologyMapService::eraColor(GeologyMapService::eraClass(eraText));
  int sum = 0;
  const QByteArray b = sym.toUtf8();
  for (const char c : b) sum += static_cast<unsigned char>(c);
  const int step = (sum % 5) - 2;  // -2..+2
  return base.lighter(100 + step * 12);
}

// 이미지의 (px,py) 주변에서 라벨·경계선(어두운 픽셀)을 걸러내고 중간 밝기 색을 고른다.
QColor pickPixelColor(const QImage& img, int px, int py) {
  static const QPoint kOffsets[] = {{0, 0},  {-3, 0}, {3, 0},  {0, -3}, {0, 3},
                                    {-6, 0}, {6, 0},  {0, -6}, {0, 6},  {-3, -3},
                                    {3, 3},  {-3, 3}, {3, -3}};
  QList<QColor> picks;
  for (const QPoint& off : kOffsets) {
    const int x = px + off.x(), y = py + off.y();
    if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) continue;
    const QColor col = img.pixelColor(x, y);
    if (col.alpha() < 200) continue;
    if (col.lightness() < 80) continue;
    picks.append(col);
  }
  if (picks.isEmpty()) return {};
  std::sort(picks.begin(), picks.end(),
            [](const QColor& a, const QColor& b) { return a.lightness() < b.lightness(); });
  QColor out = picks.at(picks.size() / 2);
  out.setAlpha(255);
  return out;
}

// 공식 지질도 래스터(WMS)를 요청 범위 전체로 「한 번만」 받아 단위별 내부점
// 픽셀에서 도폭색을 얻는다. (예전에는 단위마다 별도 요청이라 단위 수만큼
// 순차 왕복이 생겨 내려받기가 수십 초씩 걸리고 UI가 멎은 듯 보였다.)
QHash<QString, QColor> sampleOfficialColors(const QgsRectangle& ext4326,
                                            const QHash<QString, QgsPointXY>& pts5186) {
  QHash<QString, QColor> out;
  if (pts5186.isEmpty()) return out;

  QHash<QString, QgsPointXY> pts4326;
  try {
    const QgsCoordinateTransform tr(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                    QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                    QgsCoordinateTransformContext());
    for (auto it = pts5186.constBegin(); it != pts5186.constEnd(); ++it)
      pts4326.insert(it.key(), tr.transform(it.value()));
  } catch (const QgsException&) {
    return out;
  }

  const double w = std::max(ext4326.width(), 1e-9);
  const double h = std::max(ext4326.height(), 1e-9);
  int imgW = 1400, imgH = 1400;
  if (w >= h)
    imgH = std::max(64, int(std::lround(1400.0 * h / w)));
  else
    imgW = std::max(64, int(std::lround(1400.0 * w / h)));

  const QString url =
      QStringLiteral(
          "%1?service=WMS&version=1.1.1&request=GetMap&layers=%2&styles="
          "&srs=EPSG:4326&bbox=%3,%4,%5,%6&width=%7&height=%8&format=image/png"
          "&transparent=true")
          .arg(QLatin1String(kWfsUrl), QLatin1String(kWmsRaster))
          .arg(ext4326.xMinimum(), 0, 'f', 7)
          .arg(ext4326.yMinimum(), 0, 'f', 7)
          .arg(ext4326.xMaximum(), 0, 'f', 7)
          .arg(ext4326.yMaximum(), 0, 'f', 7)
          .arg(imgW)
          .arg(imgH);
  QgsBlockingNetworkRequest req;
  QNetworkRequest netReq{QUrl(url)};
  netReq.setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
  if (req.get(netReq) != QgsBlockingNetworkRequest::NoError) return out;
  const QImage img = QImage::fromData(req.reply().content());
  if (img.isNull()) return out;

  for (auto it = pts4326.constBegin(); it != pts4326.constEnd(); ++it) {
    const int px = int(std::lround((it.value().x() - ext4326.xMinimum()) / w * (imgW - 1)));
    const int py = int(std::lround((ext4326.yMaximum() - it.value().y()) / h * (imgH - 1)));
    const QColor c = pickPixelColor(img, px, py);
    if (c.isValid()) out.insert(it.key(), c);
  }
  return out;
}

// 현재 화면 bbox(위경도)의 암상 GeoJSON을 임시 파일로 받아 경로를 돌려준다.
QString fetchLithoGeojson(const QgsRectangle& extent4326, const QString& typeName,
                          QString* errorOut) {
  if (typeName.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("암상 레이어 이름이 없습니다.");
    return {};
  }
  const QString url =
      QStringLiteral(
          "%1?service=WFS&version=2.0.0&request=GetFeature&typeNames=%2"
          "&outputFormat=application/json&srsName=EPSG:5186&count=100000"
          "&bbox=%3,%4,%5,%6,urn:ogc:def:crs:EPSG::4326")
          .arg(QLatin1String(kWfsUrl), typeName)
          .arg(extent4326.yMinimum(), 0, 'f', 8)
          .arg(extent4326.xMinimum(), 0, 'f', 8)
          .arg(extent4326.yMaximum(), 0, 'f', 8)
          .arg(extent4326.xMaximum(), 0, 'f', 8);

  QgsBlockingNetworkRequest req;
  QNetworkRequest netReq{QUrl(url)};
  netReq.setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
  if (req.get(netReq) != QgsBlockingNetworkRequest::NoError) {
    if (errorOut) *errorOut = req.errorMessage();
    return {};
  }
  const QByteArray body = req.reply().content();
  if (body.isEmpty() || !body.trimmed().startsWith('{')) {
    if (errorOut) *errorOut = QStringLiteral("서버가 GeoJSON 대신 다른 응답을 보냈습니다.");
    return {};
  }
  const QString path = QDir::temp().filePath(QStringLiteral("ka-hgis-geology.geojson"));
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorOut) *errorOut = QStringLiteral("임시 파일을 쓰지 못했습니다.");
    return {};
  }
  f.write(body);
  return path;
}

}  // namespace

QString GeologyMapService::eraClass(const QString& eraText) {
  for (const EraClassDef& e : kEras)
    if (eraTextMatches(eraText, e.keyword)) return QString::fromUtf8(e.name);
  return QString::fromUtf8(kEraUnknown);
}

QColor GeologyMapService::eraColor(const QString& eraClassName) {
  for (const EraClassDef& e : kEras)
    if (eraClassName == QString::fromUtf8(e.name)) return QColor(e.r, e.g, e.b);
  return QColor(200, 200, 200);
}

bool GeologyMapService::applyGeologyStyle(QgsVectorLayer* layer,
                                          const QHash<QString, QColor>& officialColors) {
  if (!layer || !layer->isValid()) return false;
  const int symIdx = layer->fields().indexOf(QString::fromUtf8(kSymbolField));
  if (symIdx < 0) return false;
  const int stratIdx = layer->fields().indexOf(QString::fromUtf8(kStratField));
  const int eraIdx = layer->fields().indexOf(QString::fromUtf8(kEraSrcField));

  // 데이터에 실제로 있는 지질단위만 모은다. 보고서 지질도 범례 관례(기호+지층명).
  struct UnitInfo {
    QString sym, strat, era;
  };
  QList<UnitInfo> units;
  QStringList seen;
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString sym = f.attribute(symIdx).toString();
    if (sym.isEmpty() || seen.contains(sym)) continue;
    seen << sym;
    units.append({sym, stratIdx >= 0 ? f.attribute(stratIdx).toString() : QString(),
                  eraIdx >= 0 ? f.attribute(eraIdx).toString() : QString()});
  }
  // 젊은 시대 → 오래된 시대 순으로 정렬(같은 시대는 기호순).
  std::sort(units.begin(), units.end(), [](const UnitInfo& a, const UnitInfo& b) {
    const int ra = eraRank(a.era), rb = eraRank(b.era);
    if (ra != rb) return ra < rb;
    return a.sym < b.sym;
  });

  QgsCategoryList cats;
  for (const UnitInfo& u : units) {
    QColor col = officialColors.value(u.sym);
    if (!col.isValid()) col = unitFallbackColor(u.era, u.sym);
    // 범례는 지질단위명(지층)만 — 기호는 지도 라벨로 이미 표시된다.
    const QString label = u.strat.isEmpty() ? u.sym : u.strat;
    if (QgsSymbol* sym = eraFillSymbol(col))
      cats.append(QgsRendererCategory(QVariant(u.sym), sym, label));
  }
  if (QgsSymbol* rest = eraFillSymbol(QColor(200, 200, 200)))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("기타")));
  layer->setRenderer(
      new QgsCategorizedSymbolRenderer(QString::fromUtf8(kSymbolField), cats));

  // 암상 기호(Qa, PCEpgn …) 라벨 — 지질도 관례. (한글 필드명은 UTF-8 변환 필수)
  if (layer->fields().indexOf(QString::fromUtf8(kSymbolField)) >= 0) {
    QgsPalLayerSettings s;
    s.drawLabels = true;
    s.fieldName = QString::fromUtf8(kSymbolField);
    s.isExpression = false;
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
    QgsLabelObstacleSettings obs = s.obstacleSettings();
    obs.setIsObstacle(false);
    s.setObstacleSettings(obs);

    QgsTextFormat fmt;
    QFont font = fmt.font();
    font.setFamily(QStringLiteral("Malgun Gothic"));
    font.setPointSize(8);
    fmt.setFont(font);
    fmt.setSize(8);
    fmt.setSizeUnit(Qgis::RenderUnit::Points);
    fmt.setColor(QColor(40, 40, 40));
    QgsTextBufferSettings buf = fmt.buffer();
    buf.setEnabled(true);
    buf.setSize(0.6);
    buf.setColor(QColor(255, 255, 255, 220));
    fmt.setBuffer(buf);
    s.setFormat(fmt);

    layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
    layer->setLabelsEnabled(true);
  }
  layer->triggerRepaint();
  return true;
}

QString GeologyMapService::reliefLayerTitle() {
  return QString::fromUtf8(kReliefTitle);
}

QgsMapLayer* GeologyMapService::existingGeologyLayer(QgsProject* project) {
  if (!project) return nullptr;
  const QString title = QString::fromUtf8(kLayerTitle);
  QgsMapLayer* prefixed = nullptr;
  for (QgsMapLayer* ml : project->mapLayers()) {
    if (!ml) continue;
    if (ml->name() == title) return ml;
    if (!prefixed && ml->name().startsWith(title)) prefixed = ml;
  }
  return prefixed;
}

void GeologyMapService::drapeOnRelief(QgsMapLayer* layer) {
  if (!layer) return;
  // Colors stay SourceOver. Shading is the 지질 음영 overlay (Multiply on top).
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer))
    vl->setFeatureBlendMode(QPainter::CompositionMode_SourceOver);
  layer->setBlendMode(QPainter::CompositionMode_SourceOver);
  layer->triggerRepaint();
}

namespace {

QgsMapLayer* findLayerNamed(QgsProject* project, const QString& title) {
  if (!project) return nullptr;
  for (QgsMapLayer* ml : project->mapLayers()) {
    if (ml && ml->name() == title) return ml;
  }
  return nullptr;
}

bool elevationCoversGeology(QgsRasterLayer* elev, QgsMapLayer* geology) {
  if (!elev || !elev->isValid()) return false;
  if (!geology) return true;
  QgsRectangle ge = geology->extent();
  if (ge.isEmpty() || !ge.isFinite()) return true;
  QgsRectangle ee = elev->extent();
  if (ee.isEmpty() || !ee.isFinite()) return false;
  if (elev->crs().isValid() && geology->crs().isValid() && elev->crs() != geology->crs()) {
    try {
      const QgsCoordinateTransform tr(geology->crs(), elev->crs(), QgsCoordinateTransformContext());
      ge = tr.transformBoundingBox(ge);
    } catch (...) {
      return false;
    }
  }
  return ee.intersects(ge);
}

QgsRasterLayer* findElevationRaster(QgsProject* project, QgsMapLayer* geology) {
  if (!project) return nullptr;
  QgsRasterLayer* named = nullptr;
  QgsRasterLayer* anySingle = nullptr;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* rl = qobject_cast<QgsRasterLayer*>(ml);
    if (!rl || !rl->isValid() || rl->bandCount() != 1) continue;
    if (rl->name() == QString::fromUtf8(kReliefTitle)) continue;
    const QString p = rl->providerType().toLower();
    if (p == QLatin1String("wms") || p == QLatin1String("xyz")) continue;
    if (!elevationCoversGeology(rl, geology)) continue;
    if (rl->name() == QLatin1String("DEM")) named = rl;
    if (!anySingle) anySingle = rl;
  }
  return named ? named : anySingle;
}

void styleReliefOverlay(QgsMapLayer* shade) {
  if (!shade) return;
  shade->setBlendMode(QPainter::CompositionMode_Multiply);
  shade->setOpacity(0.48);
  shade->triggerRepaint();
}

void stackOver(QgsProject* project, QgsMapLayer* below, QgsMapLayer* over) {
  if (!project || !below || !over) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  QgsLayerTreeLayer* belowN = root->findLayer(below->id());
  QgsLayerTreeLayer* overN = root->findLayer(over->id());
  if (!belowN || !overN) return;
  auto* parent = qobject_cast<QgsLayerTreeGroup*>(belowN->parent());
  if (!parent) parent = root;
  auto* overParent = qobject_cast<QgsLayerTreeGroup*>(overN->parent());
  if (!overParent) overParent = root;
  overParent->removeChildNode(overN);
  belowN = parent->findLayer(below->id());
  const int belowIdx = belowN ? parent->children().indexOf(belowN) : -1;
  parent->insertLayer(belowIdx < 0 ? 0 : belowIdx, over);
}

QgsRasterLayer* addHillshadeLayer(QgsProject* project, QgsRasterLayer* elev) {
  if (!project || !elev) return nullptr;
  auto* hs = new QgsRasterLayer(elev->source(), QString::fromUtf8(kReliefTitle),
                                elev->providerType());
  if (!hs->isValid()) {
    delete hs;
    return nullptr;
  }
  if (elev->crs().isValid()) hs->setCrs(elev->crs());
  auto* rend = new QgsHillshadeRenderer(hs->dataProvider(), 1, 315.0, 45.0);
  rend->setZFactor(hs->crs().isGeographic() ? 111120.0 : 3.5);
  rend->setMultiDirectional(false);
  hs->setRenderer(rend);
  styleReliefOverlay(hs);
  LayerOps::markReferenceLayer(hs);
  hs->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
  LayerOps::applyThematicOverlayScaleRange(hs);
  if (!project->addMapLayer(hs, true)) {
    delete hs;
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, hs, QStringLiteral("참조 지도"));
  return hs;
}

QgsRasterLayer* addWorldHillshadeXyz(QgsProject* project) {
  if (!project) return nullptr;
  const QString uri = QStringLiteral(
      "type=xyz&url=https://server.arcgisonline.com/ArcGIS/rest/services/Elevation/"
      "World_Hillshade/MapServer/tile/%7Bz%7D/%7By%7D/%7Bx%7D"
      "&zmax=16&zmin=1&crs=EPSG:3857&tilePixelRatio=1");
  auto* rl = new QgsRasterLayer(uri, QString::fromUtf8(kReliefTitle), QStringLiteral("wms"));
  if (!rl->isValid()) {
    delete rl;
    return nullptr;
  }
  styleReliefOverlay(rl);
  LayerOps::markReferenceLayer(rl);
  rl->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
  LayerOps::applyThematicOverlayScaleRange(rl);
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  return rl;
}

}  // namespace

bool GeologyMapService::ensureReliefUnderlay(QgsProject* project, QgsMapCanvas* canvas,
                                             QgsMapLayer* geology, QString* errorOut) {
  Q_UNUSED(canvas);
  if (!project || !geology) {
    if (errorOut) *errorOut = QStringLiteral("지질 레이어가 없습니다.");
    return false;
  }
  drapeOnRelief(geology);
  QgsMapLayer* shade = findLayerNamed(project, reliefLayerTitle());
  if (shade && !shade->isValid()) {
    project->removeMapLayer(shade->id());
    shade = nullptr;
  }
  if (!shade) {
    if (QgsRasterLayer* elev = findElevationRaster(project, geology))
      shade = addHillshadeLayer(project, elev);
  }
  if (!shade)
    shade = addWorldHillshadeXyz(project);
  if (!shade) {
    if (errorOut)
      *errorOut = QStringLiteral("지형 음영을 만들지 못했습니다.");
    return false;
  }
  styleReliefOverlay(shade);
  stackOver(project, geology, shade);
  return true;
}

bool GeologyMapService::lithoWfsCoversWgs84(const QgsRectangle& extentWgs84) {
  if (extentWgs84.isNull() || !extentWgs84.isFinite()) return false;
  // geoOpen:l_50k_geology_litho_latest WGS84BoundingBox (GetCapabilities 2026-08-31).
  const QgsRectangle litho(124.60999365859936, 33.96870721929159, 129.58472763044148,
                           38.60507408091925);
  return litho.intersects(extentWgs84);
}

bool GeologyMapService::jejuLithoWfsCoversWgs84(const QgsRectangle& extentWgs84) {
  if (extentWgs84.isNull() || !extentWgs84.isFinite()) return false;
  // geoOpen:l_jeju_50k_geology_litho_view — 제주본섬·추자 주변 (GetFeature 2026-08-31).
  const QgsRectangle jeju(126.10, 33.10, 127.00, 33.62);
  return jeju.intersects(extentWgs84);
}

QString GeologyMapService::lithoTypeNameForWgs84(const QgsRectangle& extentWgs84) {
  if (lithoWfsCoversWgs84(extentWgs84)) return QLatin1String(kTypeName);
  if (jejuLithoWfsCoversWgs84(extentWgs84)) return QLatin1String(kJejuTypeName);
  return {};
}

QString GeologyMapService::officialRasterWmsUri() {
  const QString base = QLatin1String(kWfsUrl);
  const QString enc = QString::fromLatin1(QUrl::toPercentEncoding(base));
  return QStringLiteral(
             "IgnoreGetMapUrl=1&IgnoreGetFeatureInfoUrl=1&contextualWMSLegend=0"
             "&crs=EPSG:4326&dpiMode=7&format=image/png&transparent=true"
             "&layers=%1&styles&url=%2")
      .arg(QLatin1String(kWmsRaster), enc);
}

static void removeOldGeologyLayers(QgsProject* project, const QString& outGpkgPath) {
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    if (old->name() == QString::fromUtf8(kLayerTitle) ||
        old->name().startsWith(QString::fromUtf8(kLayerTitle) + QStringLiteral(" [")) ||
        (!outGpkgPath.isEmpty() && old->source().contains(outGpkgPath)))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);
}

static QgsRasterLayer* addOfficialGeologyRaster(QgsProject* project, QgsMapCanvas* canvas,
                                                const QString& outGpkgPath, QString* errorOut) {
  removeOldGeologyLayers(project, outGpkgPath);
  auto* rl = new QgsRasterLayer(GeologyMapService::officialRasterWmsUri(),
                                QString::fromUtf8(kLayerTitle), QStringLiteral("wms"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("공식 5만 지질도 래스터를 열지 못했습니다.");
    delete rl;
    return nullptr;
  }
  rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("지질도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(rl);
  GeologyMapService::ensureReliefUnderlay(project, canvas, rl, nullptr);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    LayerOps::refreshCanvasIfIdle(canvas);
  }
  return rl;
}

QgsMapLayer* GeologyMapService::downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                                  const QgsRectangle& extent5186,
                                                  const QString& outGpkgPath,
                                                  QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  const QgsRectangle fetch5186 =
      LayerOps::expandExtentToMaxSpan(extent5186, maxSpanMeters());
  if (fetch5186.isEmpty() || fetch5186.width() > maxSpanMeters() ||
      fetch5186.height() > maxSpanMeters()) {
    if (errorOut)
      *errorOut = QStringLiteral(
          "범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 확대한 뒤 다시 "
          "내려받으세요.")
          .arg(maxSpanMeters() / 1000.0, 0, 'f', 0);
    return nullptr;
  }

  QgsRectangle ext4326;
  try {
    const QgsCoordinateTransform tr(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                    QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                    QgsCoordinateTransformContext());
    ext4326 = tr.transformBoundingBox(fetch5186);
  } catch (const QgsException&) {
    if (errorOut) *errorOut = QStringLiteral("좌표 변환에 실패했습니다.");
    return nullptr;
  }

  const QString typeName = lithoTypeNameForWgs84(ext4326);
  if (typeName.isEmpty())
    return addOfficialGeologyRaster(project, canvas, outGpkgPath, errorOut);

  QString netErr;
  const QString jsonPath = fetchLithoGeojson(ext4326, typeName, &netErr);
  if (jsonPath.isEmpty()) {
    if (errorOut)
      *errorOut = netErr.isEmpty() ? QStringLiteral("지질도를 내려받지 못했습니다.")
                                   : QStringLiteral("KIGAM 서버 연결 실패: %1").arg(netErr);
    return nullptr;
  }

  QgsVectorLayer src(jsonPath, QStringLiteral("part"), QStringLiteral("ogr"));
  if (!src.isValid() || src.featureCount() == 0) {
    QFile::remove(jsonPath);
    return addOfficialGeologyRaster(project, canvas, outGpkgPath, errorOut);
  }

  // 메모리 레이어로 복사하며 era_class 파생 필드를 채우고 속성의 HTML 링크를 걷어낸다.
  auto* merged = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                                    QStringLiteral("merge"), QStringLiteral("memory"));
  QList<QgsField> outFields = src.fields().toList();
  outFields.append(QgsField(QLatin1String(kEraField), QMetaType::Type::QString));
  merged->dataProvider()->addAttributes(outFields);
  merged->updateFields();

  const QgsFields memFields = merged->fields();
  const QgsFields srcFields = src.fields();
  const int eraSrcIdx = srcFields.indexOf(QString::fromUtf8(kEraSrcField));
  const int symSrcIdx = srcFields.indexOf(QString::fromUtf8(kSymbolField));
  const int eraDstIdx = memFields.indexOf(QLatin1String(kEraField));
  static const QRegularExpression kTagRe(QStringLiteral("<[^>]*>"));

  // 단위(기호)별 최대 폴리곤의 내부점 — 공식 도폭색 샘플링 위치.
  QHash<QString, double> bestArea;
  QHash<QString, QgsPointXY> bestPt;

  QgsFeatureList batch;
  QgsFeatureIterator it = src.getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    QgsFeature nf(memFields);
    for (int i = 0; i < srcFields.count(); ++i) {
      const int dst = memFields.indexOf(srcFields.at(i).name());
      if (dst < 0) continue;
      QVariant v = f.attribute(i);
      if (v.metaType().id() == QMetaType::QString) {
        QString sv = v.toString();
        if (sv.contains(QLatin1Char('<'))) sv.remove(kTagRe);
        v = sv;
      }
      nf.setAttribute(dst, v);
    }
    if (eraDstIdx >= 0)
      nf.setAttribute(eraDstIdx,
                      eraClass(eraSrcIdx >= 0 ? f.attribute(eraSrcIdx).toString() : QString()));
    QgsGeometry g = f.geometry();
    g.convertToMultiType();
    nf.setGeometry(g);
    if (symSrcIdx >= 0) {
      const QString symVal = f.attribute(symSrcIdx).toString();
      if (!symVal.isEmpty()) {
        const double a = g.area();
        if (a > bestArea.value(symVal, -1.0)) {
          const QgsGeometry ps = g.pointOnSurface();
          if (!ps.isNull()) {
            bestArea.insert(symVal, a);
            bestPt.insert(symVal, ps.asPoint());
          }
        }
      }
    }
    batch.append(nf);
  }
  QFile::remove(jsonPath);
  if (batch.isEmpty()) {
    delete merged;
    if (errorOut) *errorOut = QStringLiteral("이 범위에는 지질도 데이터가 없습니다.");
    return nullptr;
  }
  merged->dataProvider()->addFeatures(batch);
  merged->updateExtents();

  removeOldGeologyLayers(project, outGpkgPath);

  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("geology_map");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString werr, nf2, nl2;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      merged, outGpkgPath, project->transformContext(), opts, &werr, &nf2, &nl2);
  delete merged;
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = QStringLiteral("지질도 저장 실패: %1").arg(werr);
    return nullptr;
  }

  auto* layer = new QgsVectorLayer(outGpkgPath + QStringLiteral("|layername=geology_map"),
                                   QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("저장한 지질도를 여는 데 실패했습니다.");
    delete layer;
    return nullptr;
  }
  // 공식 도폭색 샘플은 blocking WMS GetMap이다. 캔버스가 위성 타일을 그리는
  // 중이면 nested QEventLoop가 provider_wms deleteLater AV를 낸다(2026-09-01).
  QHash<QString, QColor> officialColors;
  if (!(canvas && canvas->isDrawing()))
    officialColors = sampleOfficialColors(ext4326, bestPt);
  applyGeologyStyle(layer, officialColors);
  LayerOps::markReferenceLayer(layer);
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("지질도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);
  GeologyMapService::ensureReliefUnderlay(project, canvas, layer, nullptr);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    // 전체 레이어 캐시 폐기는 배경 타일까지 다시 받는다.
    // isDrawing()이면 refresh도 건너뛴다(WMS deleteLater AV).
    LayerOps::refreshCanvasIfIdle(canvas);
  }
  return layer;
}
