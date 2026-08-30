#include "SoilMapService.h"
#include "LayerOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

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
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

#include <QNetworkRequest>

namespace {

constexpr const char* kWfsBase = "https://gis.naas.go.kr/geoserver/soilmap/ows";
constexpr const char* kTerrainField = "soil_type_geo";
constexpr const char* kLayerTitle = "토양도(흙토람)";

// 흙토람 범례 API(mapService_select_color_data, colorgoup=PTR)의 공식 팔레트.
struct TerrainClass {
  const char* code;
  const char* name;
  int r, g, b;
};
constexpr TerrainClass kTerrain[] = {
    {"01", "산악지", 1, 178, 0},
    {"02", "구릉지", 177, 210, 93},
    {"03", "산록경사지", 139, 68, 108},
    {"04", "곡간지/선상지", 254, 244, 182},
    {"05", "해성평탄지", 132, 186, 230},
    {"06", "하성평탄지", 192, 239, 245},
    {"07", "고원지", 78, 78, 78},
    {"08", "홍적대지", 254, 160, 0},
    {"09", "용암류대지", 192, 151, 155},
    {"10", "용암류평탄", 194, 179, 182},
    {"99", "기타", 255, 255, 255},
};

QgsSymbol* terrainFillSymbol(const QColor& base) {
  QColor fill = base;
  fill.setAlpha(165);
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), QColor(70, 70, 70, 120).name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QStringLiteral("0.1")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

// 현재 화면 bbox(위경도)로 한 테이블의 GeoJSON을 받아 임시 파일 경로를 돌려준다.
QString fetchTableGeojson(int tableNo, const QgsRectangle& extent4326, QString* errorOut) {
  const QString url =
      QStringLiteral(
          "%1?service=WFS&version=2.0.0&request=GetFeature&typeNames=soilmap:SOIL_%2"
          "&outputFormat=application/json&srsName=EPSG:5186&count=100000"
          "&bbox=%3,%4,%5,%6,urn:ogc:def:crs:EPSG::4326")
          .arg(QLatin1String(kWfsBase))
          .arg(tableNo)
          .arg(extent4326.yMinimum(), 0, 'f', 8)
          .arg(extent4326.xMinimum(), 0, 'f', 8)
          .arg(extent4326.yMaximum(), 0, 'f', 8)
          .arg(extent4326.xMaximum(), 0, 'f', 8);

  QgsBlockingNetworkRequest req;
  QNetworkRequest netReq{QUrl(url)};
  netReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ka-hgis/0.3"));
  if (req.get(netReq) != QgsBlockingNetworkRequest::NoError) {
    if (errorOut) *errorOut = req.errorMessage();
    return {};
  }
  const QByteArray body = req.reply().content();
  if (body.isEmpty() || !body.trimmed().startsWith('{')) {
    if (errorOut) *errorOut = QStringLiteral("서버가 GeoJSON 대신 다른 응답을 보냈습니다.");
    return {};
  }
  const QString path =
      QDir::temp().filePath(QStringLiteral("ka-hgis-soil-%1.geojson").arg(tableNo));
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorOut) *errorOut = QStringLiteral("임시 파일을 쓰지 못했습니다.");
    return {};
  }
  f.write(body);
  return path;
}

}  // namespace

QString SoilMapService::terrainName(const QString& code) {
  for (const TerrainClass& t : kTerrain)
    if (code == QLatin1String(t.code)) return QString::fromUtf8(t.name);
  return QStringLiteral("미분류");
}

QColor SoilMapService::terrainColor(const QString& code) {
  for (const TerrainClass& t : kTerrain)
    if (code == QLatin1String(t.code)) return QColor(t.r, t.g, t.b);
  return QColor(200, 200, 200);
}

bool SoilMapService::applyTerrainStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->fields().indexOf(QLatin1String(kTerrainField)) < 0) return false;

  QgsCategoryList cats;
  for (const TerrainClass& t : kTerrain) {
    if (QgsSymbol* sym = terrainFillSymbol(QColor(t.r, t.g, t.b)))
      cats.append(QgsRendererCategory(QVariant(QString::fromUtf8(t.code)), sym,
                                      QString::fromUtf8(t.name)));
  }
  if (QgsSymbol* rest = terrainFillSymbol(QColor(200, 200, 200)))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("미분류")));
  if (cats.isEmpty()) return false;

  layer->setRenderer(
      new QgsCategorizedSymbolRenderer(QLatin1String(kTerrainField), cats));
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* SoilMapService::downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
                                               const QgsRectangle& extent5186,
                                               const QString& outGpkgPath,
                                               QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  if (extent5186.isEmpty() || extent5186.width() > maxSpanMeters() ||
      extent5186.height() > maxSpanMeters()) {
    if (errorOut)
      *errorOut = QStringLiteral(
          "범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 확대한 뒤 다시 "
          "내려받으세요.")
          .arg(maxSpanMeters() / 1000.0, 0, 'f', 0);
    return nullptr;
  }

  // bbox는 축 순서 혼선이 없는 위경도(urn:4326, 위도-경도)로 요청한다.
  QgsRectangle ext4326;
  try {
    const QgsCoordinateTransform tr(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                    QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                    QgsCoordinateTransformContext());
    ext4326 = tr.transformBoundingBox(extent5186);
  } catch (const QgsException&) {
    if (errorOut) *errorOut = QStringLiteral("좌표 변환에 실패했습니다.");
    return nullptr;
  }

  // 지역 분할 테이블 3개를 모두 받아 하나로 병합한다.
  auto* merged = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                                    QStringLiteral("merge"), QStringLiteral("memory"));
  bool fieldsReady = false;
  long long total = 0;
  QString netErr;
  for (int tableNo = 1; tableNo <= 3; ++tableNo) {
    QString err;
    const QString jsonPath = fetchTableGeojson(tableNo, ext4326, &err);
    if (jsonPath.isEmpty()) {
      if (netErr.isEmpty()) netErr = err;
      continue;
    }
    QgsVectorLayer part(jsonPath, QStringLiteral("part"), QStringLiteral("ogr"));
    if (!part.isValid()) continue;
    if (!fieldsReady) {
      merged->dataProvider()->addAttributes(part.fields().toList());
      merged->updateFields();
      fieldsReady = true;
    }
    const QgsFields memFields = merged->fields();
    const QgsFields srcFields = part.fields();
    QgsFeatureList batch;
    QgsFeatureIterator it = part.getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      QgsFeature nf(memFields);
      for (int i = 0; i < srcFields.count(); ++i) {
        const int dst = memFields.indexOf(srcFields.at(i).name());
        if (dst >= 0) nf.setAttribute(dst, f.attribute(i));
      }
      QgsGeometry g = f.geometry();
      g.convertToMultiType();
      nf.setGeometry(g);
      batch.append(nf);
    }
    if (!batch.isEmpty()) {
      merged->dataProvider()->addFeatures(batch);
      total += batch.size();
    }
    QFile::remove(jsonPath);
  }

  if (total == 0) {
    delete merged;
    if (errorOut) {
      *errorOut = netErr.isEmpty()
                      ? QStringLiteral("이 범위에는 토양도 데이터가 없습니다. "
                                       "(군사지역·간척지 등 미구축 지역이거나 바다입니다)")
                      : QStringLiteral("흙토람 서버 연결 실패: %1").arg(netErr);
    }
    return nullptr;
  }
  merged->updateExtents();

  // 같은 GPKG를 쓰는 기존 레이어를 먼저 내려 파일 잠금을 푼다.
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    // 한글 레이어 제목은 QLatin1String으로 비교하면 UTF-8 바이트가 깨져 매치 실패한다.
    if (old->name() == QString::fromUtf8(kLayerTitle) ||
        old->name().startsWith(QString::fromUtf8(kLayerTitle) + QStringLiteral(" [")) ||
        old->source().contains(outGpkgPath))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);

  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.layerName = QStringLiteral("soil_map");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString werr, nf2, nl2;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      merged, outGpkgPath, project->transformContext(), opts, &werr, &nf2, &nl2);
  delete merged;
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = QStringLiteral("토양도 저장 실패: %1").arg(werr);
    return nullptr;
  }

  auto* layer = new QgsVectorLayer(outGpkgPath + QStringLiteral("|layername=soil_map"),
                                   QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("저장한 토양도를 여는 데 실패했습니다.");
    delete layer;
    return nullptr;
  }
  applyTerrainStyle(layer);
  LayerOps::markReferenceLayer(layer);
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("토양도 레이어를 프로젝트에 넣지 못했습니다.");
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
    canvas->refresh();
  }
  return layer;
}
