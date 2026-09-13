#include "HeritageImport.h"

#include "HeritageSiteLegend.h"
#include "LayerOps.h"
#include "TopographicArchive.h"

#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

#include <cpl_conv.h>

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace {

// 유적명이 들어 있을 만한 컬럼. 앞에 있는 것부터 본다.
// 자료 종류마다 다르다 — 지표조사구역·발굴조사구역은 사업명으로 검색한다.
const char* const kNameCandidates[] = {
    "유적명", "국가유산명", "문화재명", "명칭",   "유적명칭", "사업명",  "조사명",
    "NAME",   "name",       "NM",       "nm",     "SITE_NM",  "site_nm", "HERITAGE_NM",
    "CULTURE_NM", "RELIC_NM", "BURIAL_NM", "TITLE", "title",
};

bool looksLikeText(const QgsVectorLayer* layer, int index) {
  if (!layer || index < 0 || index >= layer->fields().count()) return false;
  const QMetaType::Type type = static_cast<QMetaType::Type>(layer->fields().at(index).type());
  return type == QMetaType::QString;
}

}  // namespace

QString HeritageImport::referenceGroupName() { return QStringLiteral("참조 지도"); }

QString HeritageImport::chooseNameField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const QgsFields fields = layer->fields();

  for (const char* candidate : kNameCandidates) {
    const int i = fields.indexOf(QString::fromUtf8(candidate));
    if (i >= 0 && looksLikeText(layer, i)) return fields.at(i).name();
  }
  // 대소문자·공백이 다를 수 있다. 느슨하게 한 번 더 본다.
  for (const char* candidate : kNameCandidates) {
    const QString want = QString::fromUtf8(candidate).toLower();
    for (int i = 0; i < fields.count(); ++i) {
      const QString have = fields.at(i).name().trimmed().toLower();
      if (have == want && looksLikeText(layer, i)) return fields.at(i).name();
    }
  }
  // 이름에 「명」이 들어간 글자 필드.
  for (int i = 0; i < fields.count(); ++i) {
    if (looksLikeText(layer, i) && fields.at(i).name().contains(QStringLiteral("명")))
      return fields.at(i).name();
  }
  // 마지막 수단: 첫 글자 필드. 그래도 못 찾으면 빈 값을 돌려주고 호출자가 알린다.
  for (int i = 0; i < fields.count(); ++i) {
    if (looksLikeText(layer, i)) return fields.at(i).name();
  }
  return {};
}

HeritageImport::Result HeritageImport::loadDataset(QgsProject* project, HeritageDataset dataset,
                                                   const QStringList& downloadedFiles,
                                                   const QString& archiveRoot) {
  Result out;
  if (!project) {
    out.error = QStringLiteral("프로젝트가 없어 자료를 올리지 못했습니다.");
    return out;
  }
  if (downloadedFiles.isEmpty()) {
    out.error = QStringLiteral("받은 파일이 없습니다.");
    return out;
  }

  // 받은 것을 보관함에 풀어 둔다. 원본은 바꾸지 않는다.
  //
  // ZIP 안의 파일 이름이 **CP949** 다(국가유산 인트라넷이 주는 ZIP, 2026-09-13 확인).
  // GDAL 의 /vsizip/ 은 기본으로 UTF-8 로 읽어 「국가지정유산」이 「?├??┴÷┴n└≫?Ω」가 된다.
  // 레이어 이름이 그대로 깨지므로 푸는 동안만 인코딩을 알려 준다.
  const QByteArray previousZipEncoding(CPLGetConfigOption("CPL_ZIP_ENCODING", ""));
  CPLSetConfigOption("CPL_ZIP_ENCODING", "CP949");
  struct ZipEncodingGuard {
    QByteArray previous;
    ~ZipEncodingGuard() {
      CPLSetConfigOption("CPL_ZIP_ENCODING", previous.isEmpty() ? nullptr : previous.constData());
    }
  } zipEncodingGuard{previousZipEncoding};

  QStringList shapefiles;
  for (const QString& file : downloadedFiles) {
    const TopographicArchive::Result prepared = TopographicArchive::prepare(file, archiveRoot);
    if (!prepared.error.isEmpty()) {
      out.error = QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), prepared.error);
      return out;
    }
    for (const QString& inner : prepared.files) {
      if (inner.endsWith(QStringLiteral(".shp"), Qt::CaseInsensitive)) shapefiles << inner;
    }
  }
  if (shapefiles.isEmpty()) {
    out.error = QStringLiteral("받은 파일에서 SHP를 찾지 못했습니다. 파일 형식을 확인해야 합니다.");
    return out;
  }

  const QString datasetName = HeritageStyle::layerName(dataset);
  QList<QgsVectorLayer*> loaded;
  for (const QString& shp : shapefiles) {
    const QFileInfo shapeInfo(shp);
    const QString base = shapeInfo.dir().filePath(shapeInfo.completeBaseName());
    for (const QString& suffix : {QStringLiteral(".shx"), QStringLiteral(".dbf"), QStringLiteral(".prj")}) {
      if (!QFileInfo::exists(base + suffix)) {
        out.error = QStringLiteral("%1의 필수 파일 %2가 없습니다. 적재를 중단합니다.")
                        .arg(shapeInfo.fileName(), suffix);
        qDeleteAll(loaded);
        return out;
      }
    }
    const QString encoding = LayerOps::prepareShapefileEncoding(shp);
    // 한 ZIP 에 여러 SHP 가 들어온다(예: 지정유산 →
    // 국가지정유산 · 시도지정유산 · 국가등록문화유산 · 시도등록문화유산 ·
    // 국가지정유산보호구역 · 시도지정유산보호구역, 2026-09-12 실제 파일로 확인).
    // 전부 한 이름으로 올리면 레이어창에서 구분이 안 된다.
    // **이름은 파일 이름 그대로, 색과 범례는 그 종류의 것**을 쓴다.
    const QString baseName = QFileInfo(shp).completeBaseName();
    const QString layerName = baseName.isEmpty() ? datasetName : baseName;
    auto* layer = new QgsVectorLayer(shp, layerName, QStringLiteral("ogr"));
    if (!layer->isValid()) {
      out.error = QStringLiteral("%1 을(를) 열지 못했습니다.").arg(QFileInfo(shp).fileName());
      delete layer;
      qDeleteAll(loaded);
      return out;
    }
    if (!encoding.isEmpty()) LayerOps::setShapefileEncoding(layer, encoding);

    // 유적명은 실제 필드에서 고른다. 없으면 그렇다고 말한다.
    const QString nameField = chooseNameField(layer);
    if (nameField.isEmpty()) {
      out.messages << QStringLiteral("%1: 유적명 컬럼을 찾지 못해 범례에 이름을 넣지 못했습니다.")
                          .arg(layerName);
    }
    // 한 종류는 한 색이다. 지정유산 안의 6종도 모두 지정유산 색을 쓴다
    // (레이어창에서는 「지정유산」 그룹으로 묶어 구분한다).
    const HeritageStyleResult styled = HeritageStyle::apply(layer, dataset, nameField);
    if (!styled.message.isEmpty()) out.messages << styled.message;
    // 레이어창에는 종류 이름만, 도면 범례에는 유적명 한 줄씩.
    HeritageSiteLegend::install(layer);

    // 참조 자료다. 조사 데이터와 섞이지 않게 한다.
    layer->setProperty("readOnly", true);
    LayerOps::markReferenceLayer(layer);
    out.featureCount += static_cast<int>(layer->featureCount());
    loaded.append(layer);
  }

  if (loaded.isEmpty()) {
    out.error = QStringLiteral("%1 자료를 지도에 올리지 못했습니다.").arg(datasetName);
    return out;
  }

  // 한 번에 등록한다. 하나씩 넣으면 그때마다 화면이 다시 그려진다.
  // **범례에 자동으로 넣지 않는다**(addToLegend=false). 「참조 지도」 그룹에 직접 넣기 위해서다.
  QList<QgsMapLayer*> asMapLayers;
  for (QgsVectorLayer* layer : loaded) asMapLayers.append(layer);
  project->addMapLayers(asMapLayers, false);

  // 조사 데이터와 섞이지 않게 「참조 지도」 그룹에 모은다.
  // LayerOps::placeInLegendGroup 은 그룹 이름을 쓰지 않으므로(Q_UNUSED) 여기서 직접 만든다.
  QgsLayerTree* root = project->layerTreeRoot();
  if (root) {
    QgsLayerTreeGroup* reference = root->findGroup(referenceGroupName());
    if (!reference) reference = root->addGroup(referenceGroupName());
    // 한 ZIP 에 그 종류의 여러 갈래가 들어온다(지정유산 → 국가지정유산·시도지정유산·…).
    // 레이어창에서 종류별로 묶어 준다: 참조 지도 › 지정유산 › 국가지정유산 …
    QgsLayerTreeGroup* group = reference;
    if (reference) {
      const QString kind = HeritageStyle::layerName(dataset);
      QgsLayerTreeGroup* kindGroup = reference->findGroup(kind);
      if (!kindGroup) kindGroup = reference->addGroup(kind);
      if (kindGroup) group = kindGroup;
    }
    for (QgsVectorLayer* layer : loaded) {
      if (!group) break;
      QgsLayerTreeLayer* node = group->addLayer(layer);
      if (!node) continue;
      node->setItemVisibilityChecked(true);
      // 유적명이 수백 줄이면 레이어창을 덮는다. 접은 채로 올린다.
      node->setExpanded(false);
    }
  }

  LayerOps::ensureSatelliteAtBottom(project);
  out.layers = loaded;
  return out;
}
