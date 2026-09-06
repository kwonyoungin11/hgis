#include "RiverMapService.h"
#include "LayerOps.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

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
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include <QNetworkRequest>

namespace {

constexpr const char* kWfsUrl = "https://api.vworld.kr/req/wfs";
constexpr const char* kTypeName = "lt_c_wkmstrm";
constexpr const char* kLayerTitle = "수계도(하천망)";
constexpr const char* kNameField = "riv_nm";
constexpr const char* kLevelField = "riv_level";

// 하천 등급별 물색 — 상위 등급일수록 진하게(보고서 수계도 관례).
struct LevelDef {
  const char* keyword;  // riv_level에 포함되는 문자열
  const char* label;    // 범례 표기
  int r, g, b;
};
constexpr LevelDef kLevels[] = {
    {"국가", "국가하천", 0, 126, 212},
    {"지방1급", "지방1급하천", 16, 146, 222},
    {"지방2급", "지방2급하천", 40, 166, 232},
};

int levelRank(const QString& levelText) {
  int i = 0;
  for (const LevelDef& d : kLevels) {
    if (levelText.contains(QString::fromUtf8(d.keyword))) return i;
    ++i;
  }
  return int(std::size(kLevels));
}

QgsSymbol* waterFillSymbol(const QColor& base) {
  QColor fill = base;
  fill.setAlpha(230);
  QColor line = base.darker(150);
  line.setAlpha(255);
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), line.name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QStringLiteral("0.32")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

// VWorld가 200 + XML(ServiceException)로 돌려주는 오류에서 한국어 사유를 뽑는다.
QString vworldExceptionText(const QByteArray& body) {
  static const QRegularExpression re(
      QStringLiteral("<ServiceException[^>]*>([^<]+)</ServiceException>"));
  const QRegularExpressionMatch m = re.match(QString::fromUtf8(body));
  return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

// 현재 화면 bbox(위경도)의 하천망 GeoJSON을 임시 파일로 받아 경로를 돌려준다.
// VWorld WFS는 인증키와 DOMAIN 파라미터가 필요하고(배경지도와 같은 키),
// MAXFEATURES 상한이 1000이라 STARTINDEX로 나눠 받는다.
QString fetchRiverGeojson(const QgsRectangle& extent4326, const QString& apiKey,
                          QString* errorOut) {
  constexpr int kPageSize = 1000;
  constexpr int kMaxPages = 10;
  QJsonObject rootDoc;
  QJsonArray allFeatures;

  for (int page = 0; page < kMaxPages; ++page) {
    const QString url =
        QStringLiteral(
            "%1?SERVICE=WFS&REQUEST=GetFeature&VERSION=2.0.0&TYPENAME=%2"
            "&OUTPUT=application/json&SRSNAME=EPSG:4326&MAXFEATURES=%3&STARTINDEX=%4"
            // DOMAIN 은 보내지 않는다 — VWorld 는 DOMAIN=localhost 가 붙으면 같은 키·같은
            // Referer 라도 INCORRECT_KEY 로 거절한다(2026-09-06 실측: 붙이면 407바이트
            // 오류, 빼면 2.4 MB 피처). 인증은 아래 Referer 헤더가 한다.
            "&BBOX=%5,%6,%7,%8,urn:ogc:def:crs:EPSG::4326&KEY=%9")
            .arg(QLatin1String(kWfsUrl), QLatin1String(kTypeName))
            .arg(kPageSize)
            .arg(page * kPageSize)
            .arg(extent4326.yMinimum(), 0, 'f', 8)
            .arg(extent4326.xMinimum(), 0, 'f', 8)
            .arg(extent4326.yMaximum(), 0, 'f', 8)
            .arg(extent4326.xMaximum(), 0, 'f', 8)
            .arg(apiKey);

    QgsBlockingNetworkRequest req;
    QNetworkRequest netReq{QUrl(url)};
    netReq.setHeader(QNetworkRequest::UserAgentHeader,
                     QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
    netReq.setRawHeader("Referer", "https://localhost");
    if (req.get(netReq) != QgsBlockingNetworkRequest::NoError) {
      if (errorOut) *errorOut = req.errorMessage();
      return {};
    }
    const QByteArray body = req.reply().content();
    if (body.isEmpty() || !body.trimmed().startsWith('{')) {
      const QString reason = vworldExceptionText(body);
      if (errorOut)
        *errorOut = reason.isEmpty()
                        ? QStringLiteral("서버가 GeoJSON 대신 다른 응답을 보냈습니다. "
                                         "VWorld 인증키를 확인하세요.")
                        : QStringLiteral("VWorld: %1").arg(reason);
      return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
      if (errorOut) *errorOut = QStringLiteral("VWorld 응답(JSON)을 해석하지 못했습니다.");
      return {};
    }
    const QJsonObject obj = doc.object();
    const QJsonArray feats = obj.value(QStringLiteral("features")).toArray();
    if (page == 0) rootDoc = obj;
    for (const QJsonValue& v : feats) allFeatures.append(v);
    if (feats.size() < kPageSize) break;  // 마지막 페이지
  }

  rootDoc.insert(QStringLiteral("features"), allFeatures);
  const QString path = QDir::temp().filePath(QStringLiteral("ka-hgis-river.geojson"));
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorOut) *errorOut = QStringLiteral("임시 파일을 쓰지 못했습니다.");
    return {};
  }
  f.write(QJsonDocument(rootDoc).toJson(QJsonDocument::Compact));
  return path;
}

}  // namespace

bool RiverMapService::applyRiverStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  const int levelIdx = layer->fields().indexOf(QLatin1String(kLevelField));
  const int nameIdx = layer->fields().indexOf(QLatin1String(kNameField));
  if (levelIdx < 0 && nameIdx < 0) return false;

  // 데이터에 실제로 있는 등급만 범례에 올린다(상위 등급 먼저).
  QStringList present;
  if (levelIdx >= 0) {
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QString lv = f.attribute(levelIdx).toString();
      if (!lv.isEmpty() && !present.contains(lv)) present << lv;
    }
  }
  std::sort(present.begin(), present.end(), [](const QString& a, const QString& b) {
    const int ra = levelRank(a), rb = levelRank(b);
    if (ra != rb) return ra < rb;
    return a < b;
  });

  QgsCategoryList cats;
  for (const QString& lv : present) {
    const int rank = levelRank(lv);
    const QColor col = rank < int(std::size(kLevels))
                           ? QColor(kLevels[rank].r, kLevels[rank].g, kLevels[rank].b)
                           : QColor(120, 196, 236);
    if (QgsSymbol* sym = waterFillSymbol(col))
      cats.append(QgsRendererCategory(QVariant(lv), sym, lv));
  }
  if (QgsSymbol* rest = waterFillSymbol(QColor(120, 196, 236)))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("기타 수계")));
  layer->setRenderer(
      new QgsCategorizedSymbolRenderer(QLatin1String(kLevelField), cats));

  // 하천명(riv_nm) 라벨 — 파란 글씨 + 흰 테두리(수계도 관례).
  if (nameIdx >= 0) {
    QgsPalLayerSettings s;
    s.drawLabels = true;
    s.fieldName = QLatin1String(kNameField);
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
    font.setBold(true);
    fmt.setFont(font);
    fmt.setSize(8);
    fmt.setSizeUnit(Qgis::RenderUnit::Points);
    fmt.setColor(QColor(0, 72, 186));
    QgsTextBufferSettings buf = fmt.buffer();
    buf.setEnabled(true);
    buf.setSize(0.7);
    buf.setColor(QColor(255, 255, 255, 225));
    fmt.setBuffer(buf);
    s.setFormat(fmt);

    layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
    layer->setLabelsEnabled(true);
  }
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* RiverMapService::downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                                const QgsRectangle& extent5186,
                                                const QString& apiKey,
                                                const QString& outGpkgPath,
                                                QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  if (apiKey.trimmed().isEmpty()) {
    if (errorOut)
      *errorOut = QStringLiteral(
          "VWorld 인증키가 없습니다. 지도 탭의 배경지도 설정에서 키를 먼저 등록하세요.");
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

  QString netErr;
  const QString jsonPath = fetchRiverGeojson(ext4326, apiKey, &netErr);
  if (jsonPath.isEmpty()) {
    if (errorOut)
      *errorOut = netErr.isEmpty() ? QStringLiteral("수계도를 내려받지 못했습니다.")
                                   : QStringLiteral("VWorld 서버 연결 실패: %1").arg(netErr);
    return nullptr;
  }

  QgsVectorLayer src(jsonPath, QStringLiteral("part"), QStringLiteral("ogr"));
  if (!src.isValid() || src.featureCount() == 0) {
    QFile::remove(jsonPath);
    if (errorOut)
      *errorOut = QStringLiteral("이 범위에는 하천망 데이터가 없습니다. "
                                 "(국가·지방하천이 없는 지역입니다)");
    return nullptr;
  }

  // 4326 응답을 5186으로 재투영하며 메모리 레이어로 복사한다.
  auto* merged = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                                    QStringLiteral("merge"), QStringLiteral("memory"));
  merged->dataProvider()->addAttributes(src.fields().toList());
  merged->updateFields();

  const QgsFields memFields = merged->fields();
  const QgsFields srcFields = src.fields();
  const QgsCoordinateTransform to5186(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                      QgsCoordinateTransformContext());

  QgsFeatureList batch;
  QgsFeatureIterator it = src.getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    QgsFeature nf(memFields);
    for (int i = 0; i < srcFields.count(); ++i) {
      const int dst = memFields.indexOf(srcFields.at(i).name());
      if (dst >= 0) nf.setAttribute(dst, f.attribute(i));
    }
    QgsGeometry g = f.geometry();
    try {
      if (g.transform(to5186) != Qgis::GeometryOperationResult::Success) continue;
    } catch (const QgsException&) {
      continue;
    }
    g.convertToMultiType();
    nf.setGeometry(g);
    batch.append(nf);
  }
  QFile::remove(jsonPath);
  if (batch.isEmpty()) {
    delete merged;
    if (errorOut) *errorOut = QStringLiteral("이 범위에는 하천망 데이터가 없습니다.");
    return nullptr;
  }
  merged->dataProvider()->addFeatures(batch);
  merged->updateExtents();

  // 같은 GPKG를 쓰는 기존 레이어를 먼저 내려 파일 잠금을 푼다.
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    if (old->name() == QString::fromUtf8(kLayerTitle) ||
        old->name().startsWith(QString::fromUtf8(kLayerTitle) + QStringLiteral(" [")) ||
        old->source().contains(outGpkgPath))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);

  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("river_map");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString werr, nf2, nl2;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      merged, outGpkgPath, project->transformContext(), opts, &werr, &nf2, &nl2);
  delete merged;
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = QStringLiteral("수계도 저장 실패: %1").arg(werr);
    return nullptr;
  }

  auto* layer = new QgsVectorLayer(outGpkgPath + QStringLiteral("|layername=river_map"),
                                   QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("저장한 수계도를 여는 데 실패했습니다.");
    delete layer;
    return nullptr;
  }
  applyRiverStyle(layer);
  LayerOps::markReferenceLayer(layer);
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("수계도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    // refreshAllLayers()는 배경 타일 캐시까지 버려 재다운로드를 유발한다.
    LayerOps::refreshCanvasIfIdle(canvas);
  }
  return layer;
}
