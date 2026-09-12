#include "HeritageImport.h"

#include "HeritageSiteLegend.h"
#include "LayerOps.h"
#include "TopographicArchive.h"

#include <qgsproject.h>
#include <qgsvectorlayer.h>

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

  // 받은 것을 조사폴더 보관함에 풀어 둔다. 원본은 바꾸지 않는다.
  QStringList shapefiles;
  for (const QString& file : downloadedFiles) {
    const TopographicArchive::Result prepared = TopographicArchive::prepare(file, archiveRoot);
    if (!prepared.error.isEmpty()) {
      out.messages << QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), prepared.error);
      continue;
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
      out.messages << QStringLiteral("%1 을(를) 열지 못했습니다.").arg(QFileInfo(shp).fileName());
      delete layer;
      continue;
    }
    if (!encoding.isEmpty()) LayerOps::setShapefileEncoding(layer, encoding);

    // 유적명은 실제 필드에서 고른다. 없으면 그렇다고 말한다.
    const QString nameField = chooseNameField(layer);
    if (nameField.isEmpty()) {
      out.messages << QStringLiteral("%1: 유적명 컬럼을 찾지 못해 범례에 이름을 넣지 못했습니다.")
                          .arg(layerName);
    }
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
  QList<QgsMapLayer*> asMapLayers;
  for (QgsVectorLayer* layer : loaded) asMapLayers.append(layer);
  project->addMapLayers(asMapLayers);
  for (QgsVectorLayer* layer : loaded)
    LayerOps::placeInLegendGroup(project, layer, referenceGroupName(), true);

  out.layers = loaded;
  return out;
}
