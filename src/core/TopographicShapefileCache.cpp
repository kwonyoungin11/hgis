#include "TopographicShapefileCache.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsvectorfilewriter.h>
#include <qgswkbtypes.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

namespace {
using Record = TopographicCatalog::Record;
QByteArray digest(const QString& path, const std::function<bool()>& canceled) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    if (canceled && canceled()) return {};
    const auto bytes = file.read(1024 * 1024);
    if (bytes.isEmpty() && file.error() != QFileDevice::NoError) return {};
    hash.addData(bytes);
  }
  return hash.result().toHex();
}
QString selectionKey(const QString& code, const QString& type) {
  return code + QChar(0x1f) + type;
}
bool validExtent(const Record& r) {
  return r.hasExtent && std::isfinite(r.extent.xMinimum()) && std::isfinite(r.extent.yMinimum()) &&
         std::isfinite(r.extent.xMaximum()) && std::isfinite(r.extent.yMaximum());
}
QJsonObject encode(const Record& r) {
  return {{"file", QFileInfo(r.source).fileName()}, {"layer", r.layerName}, {"name", r.displayName},
    {"sheet", r.sourceSheet}, {"crs", r.crsWkt}, {"category", static_cast<int>(r.category)},
    {"bounds", QJsonArray{r.extent.xMinimum(), r.extent.yMinimum(), r.extent.xMaximum(), r.extent.yMaximum()}}};
}
bool safeName(const QString& name) {
  return !name.isEmpty() && name != "." && name != ".." && !name.contains('/') && !name.contains('\\') && !name.contains(':');
}
struct Group {
  Record record;
  std::unique_ptr<QgsVectorFileWriter> writer;
  qint64 features = 0;
};
}

TopographicShapefileCache::Result TopographicShapefileCache::prepare(
    const QList<Record>& records, const QString& root, const QgsCoordinateTransformContext& context,
    const std::function<bool()>& canceled) {
  Result result;
  auto stopped = [&] { if (canceled && canceled()) { result.canceled = true; return true; } return false; };
  if (stopped() || records.isEmpty()) return result;
  QMap<QString, QList<Record>> sources;
  QMap<QString, QString> assignments;
  QStringList selection;
  for (const auto& r : records) {
    if (QFileInfo(r.source).suffix().compare(QStringLiteral("dxf"), Qt::CaseInsensitive) != 0 ||
        r.layerName.isEmpty() || r.sourceSheet.isEmpty() || r.geometryType.isEmpty() ||
        !QgsCoordinateReferenceSystem(r.crsWkt).isValid() || !validExtent(r)) {
      result.error = QStringLiteral("좌표계와 범위가 확인된 DXF 도엽만 SHP로 변환할 수 있습니다."); return result;
    }
    const QString subset = r.source + QChar(0x1f) + r.layerName + QChar(0x1f) + selectionKey(r.cadLayer, r.geometryType);
    const QString assignment = r.crsWkt + QChar(0x1f) + r.sourceSheet + QChar(0x1f) + QString::number(static_cast<int>(r.category));
    if (assignments.contains(subset)) {
      if (assignments.value(subset) != assignment) { result.error = QStringLiteral("동일한 DXF 도형에 서로 다른 좌표계 또는 분류가 지정되어 있습니다."); return result; }
      continue;
    }
    assignments.insert(subset, assignment);
    sources[r.source].append(r);
    selection.append(r.source + QChar(0x1f) + r.layerName + QChar(0x1f) + r.cadLayer + QChar(0x1f) +
      r.geometryType + QChar(0x1f) + r.crsWkt + QChar(0x1f) + r.sourceSheet + QChar(0x1f) + QString::number(static_cast<int>(r.category)));
  }
  selection.sort();
  QCryptographicHash identity(QCryptographicHash::Sha256);
  QMap<QString, QByteArray> sourceHashes;
  identity.addData("topographic-shp-v1\n");
  identity.addData(selection.join('\n').toUtf8());
  for (auto it = sources.cbegin(); it != sources.cend(); ++it) {
    const auto hash = digest(it.key(), canceled);
    if (stopped()) return result;
    if (hash.isEmpty()) { result.error = QStringLiteral("DXF 원본 파일을 읽지 못했습니다: %1").arg(it.key()); return result; }
    sourceHashes.insert(it.key(), hash);
    identity.addData(hash);
  }
  auto sourcesUnchanged = [&] {
    for (auto source = sourceHashes.cbegin(); source != sourceHashes.cend(); ++source) {
      const auto current = digest(source.key(), canceled);
      if (stopped()) { result.records.clear(); return false; }
      if (current != source.value()) {
        result.records.clear();
        result.error = QStringLiteral("처리 중 DXF 원본이 변경되었습니다. 변환 결과를 게시하지 않았습니다. 파일 저장이 끝난 뒤 다시 시도하세요.");
        return false;
      }
    }
    return true;
  };
  const QString key = QString::fromLatin1(identity.result().toHex());
  if (root.isEmpty() || !QDir().mkpath(root)) { result.error = QStringLiteral("SHP 보관 폴더를 만들지 못했습니다."); return result; }
  QDir cache(root);
  QLockFile lock(cache.filePath(key + QStringLiteral(".lock")));
  if (!lock.tryLock(0)) { result.error = QStringLiteral("이 도엽을 SHP로 변환 중입니다. 잠시 후 다시 시도하세요."); return result; }
  const QString indexPath = cache.filePath(key + QStringLiteral(".json"));
  QFile index(indexPath);
  if (index.open(QIODevice::ReadOnly)) {
    const auto manifest = QJsonDocument::fromJson(index.readAll()).object();
    const QString directory = manifest.value("directory").toString();
    const QDir completed(cache.filePath(directory));
    bool valid = safeName(directory) && !QFileInfo(completed.path()).isSymLink() && !QFileInfo(completed.path()).isJunction() && manifest.value("version").toInt() == 1;
    const auto files = manifest.value("files").toArray();
    valid = valid && !files.isEmpty();
    QSet<QString> checkedFiles;
    for (const auto& entry : files) {
      const auto file = entry.toObject(); const QString name = file.value("name").toString();
      if (!valid || !safeName(name) || QFileInfo(completed.filePath(name)).isSymLink() ||
          digest(completed.filePath(name), canceled) != file.value("sha256").toString().toLatin1()) { valid = false; break; }
      checkedFiles.insert(name);
    }
    if (stopped()) return result;
    if (valid) {
      for (const auto& entry : manifest.value("records").toArray()) {
        const auto object = entry.toObject(); const QString file = object.value("file").toString();
        const auto bounds = object.value("bounds").toArray();
        if (!safeName(file) || !file.endsWith(QStringLiteral(".shp")) || bounds.size() != 4) { valid = false; break; }
        for (const auto& suffix : {"shp", "shx", "dbf", "prj"})
          if (!checkedFiles.contains(file.chopped(3) + suffix)) valid = false;
        if (!valid) break;
        Record r; r.source = completed.filePath(file); r.layerName = object.value("layer").toString();
        r.displayName = object.value("name").toString(); r.sourceSheet = object.value("sheet").toString();
        r.crsWkt = object.value("crs").toString(); r.category = static_cast<TopographicCatalog::Category>(object.value("category").toInt());
        r.extent = QgsRectangle(bounds[0].toDouble(), bounds[1].toDouble(), bounds[2].toDouble(), bounds[3].toDouble());
        r.hasExtent = true; r.signature = key; result.records.append(r);
        if (!QgsCoordinateReferenceSystem(r.crsWkt).isValid() || !validExtent(r) || r.sourceSheet.isEmpty() ||
            r.layerName != QFileInfo(file).completeBaseName() || static_cast<int>(r.category) < 0 ||
            static_cast<int>(r.category) > static_cast<int>(TopographicCatalog::Category::PlaceName)) { valid = false; break; }
      }
      if (valid && !result.records.isEmpty()) {
        if (!sourcesUnchanged()) return result;
        result.reused = true; return result;
      }
      result.records.clear();
    }
  }
  index.close();
  QTemporaryDir staging(cache.filePath(QStringLiteral(".shp-XXXXXX")));
  if (!staging.isValid()) { result.error = QStringLiteral("SHP 임시 폴더를 만들지 못했습니다."); return result; }
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("Layer"), QMetaType::Type::QString, QString(), 100));
  fields.append(QgsField(QStringLiteral("Text"), QMetaType::Type::QString, QString(), 254));
  fields.append(QgsField(QStringLiteral("elevation"), QMetaType::Type::Double, QString(), 24, 6));
  fields.append(QgsField(QStringLiteral("z_valid"), QMetaType::Type::Int));
  fields.append(QgsField(QStringLiteral("sheet"), QMetaType::Type::QString, QString(), 64));
  std::map<QString, Group> groups;
  bool textTruncated = false;
  for (auto source = sources.cbegin(); source != sources.cend(); ++source) {
    if (stopped()) return result;
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(static_cast<GDALDataset*>(GDALOpenEx(
      source.key().toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)), GDALClose);
    if (!dataset) { result.error = QStringLiteral("DXF 도엽을 열지 못했습니다: %1").arg(source.key()); return result; }
    QMap<QString, QMap<QString, Record>> selections;
    for (const auto& r : source.value()) selections[r.layerName].insert(selectionKey(r.cadLayer, r.geometryType), r);
    for (auto selected = selections.cbegin(); selected != selections.cend(); ++selected) {
      auto* input = dataset->GetLayerByName(selected.key().toUtf8().constData());
      if (!input) { result.error = QStringLiteral("DXF 도형 레이어를 찾지 못했습니다."); return result; }
      input->ResetReading();
      while (auto* raw = input->GetNextFeature()) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(raw, OGRFeature::DestroyFeature);
        if (stopped()) return result;
        const auto* ogr = feature->GetGeometryRef(); if (!ogr || ogr->IsEmpty()) continue;
        const int layerField = feature->GetFieldIndex("Layer");
        const QString code = layerField < 0 ? QString() : QString::fromUtf8(feature->GetFieldAsString(layerField));
        QByteArray bytes(static_cast<qsizetype>(ogr->WkbSize()), Qt::Uninitialized);
        if (ogr->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(bytes.data()), wkbVariantIso) != OGRERR_NONE) {
          result.error = QStringLiteral("DXF 도형을 읽지 못했습니다."); return result;
        }
        QgsGeometry geometry; geometry.fromWkb(bytes);
        const QString type = QgsWkbTypes::displayString(QgsWkbTypes::flatType(geometry.wkbType()));
        const auto found = selected->constFind(selectionKey(code, type));
        if (found == selected->cend()) continue;
        const Record& original = found.value();
        const auto family = geometry.type();
        if (family != Qgis::GeometryType::Point && family != Qgis::GeometryType::Line && family != Qgis::GeometryType::Polygon) {
          result.error = QStringLiteral("SHP로 보존할 수 없는 DXF 도형 유형입니다: %1").arg(type); return result;
        }
        if (!geometry.convertToMultiType()) { result.error = QStringLiteral("SHP 도형 유형을 준비하지 못했습니다."); return result; }
        const QString groupKey = original.sourceSheet + QChar(0x1f) + original.crsWkt + QChar(0x1f) +
          QString::number(static_cast<int>(original.category)) + QChar(0x1f) + QgsWkbTypes::displayString(geometry.wkbType());
        auto [groupIt, inserted] = groups.try_emplace(groupKey);
        auto& group = groupIt->second;
        if (inserted) {
          const QString base = QStringLiteral("topo_%1").arg(groups.size(), 3, 10, QLatin1Char('0'));
          group.record = original; group.record.cadLayer.clear(); group.record.geometryType.clear();
          group.record.layerName = base; group.record.source = QDir(staging.path()).filePath(base + QStringLiteral(".shp"));
          group.record.displayName = TopographicCatalog::categoryName(original.category) + QStringLiteral(" · ") + original.sourceSheet;
          group.record.hasExtent = false; group.record.signature = key;
          QgsVectorFileWriter::SaveVectorOptions options;
          options.driverName = QStringLiteral("ESRI Shapefile"); options.fileEncoding = QStringLiteral("UTF-8");
          options.setFieldDomains = false;
          group.writer.reset(QgsVectorFileWriter::create(group.record.source, fields, geometry.wkbType(),
            QgsCoordinateReferenceSystem(original.crsWkt), context, options));
          if (!group.writer || group.writer->hasError() != QgsVectorFileWriter::NoError) {
            result.error = QStringLiteral("SHP 파일을 만들지 못했습니다: %1").arg(group.writer ? group.writer->errorMessage() : QString()); return result;
          }
        }
        bool level = QgsWkbTypes::hasZ(geometry.wkbType()); double z = 0; bool first = true;
        auto vertices = geometry.vertices();
        while (vertices.hasNext()) {
          const double value = vertices.next().z();
          if (!std::isfinite(value)) level = false;
          if (first) { z = value; first = false; }
          else if (std::abs(value - z) >= .001) level = false;
        }
        level = level && !first;
        const int textField = feature->GetFieldIndex("Text");
        QString text = textField < 0 ? QString() : QString::fromUtf8(feature->GetFieldAsString(textField));
        // DBF strings are byte-limited even with UTF-8 encoding.
        while (text.toUtf8().size() > 254) { text.chop(1); textTruncated = true; }
        QgsFeature output(fields); output.setGeometry(geometry);
        output.setAttribute(QStringLiteral("Layer"), code);
        output.setAttribute(QStringLiteral("Text"), text);
        output.setAttribute(QStringLiteral("elevation"), level ? QVariant(z) : QVariant());
        output.setAttribute(QStringLiteral("z_valid"), level ? 1 : 0);
        output.setAttribute(QStringLiteral("sheet"), original.sourceSheet);
        if (!group.writer->addFeature(output)) { result.error = QStringLiteral("SHP 도형 저장 실패: %1").arg(group.writer->lastError()); return result; }
        if (!group.record.hasExtent) { group.record.extent = geometry.boundingBox(); group.record.hasExtent = true; }
        else group.record.extent.combineExtentWith(geometry.boundingBox());
        ++group.features;
      }
    }
  }
  if (groups.empty()) { result.error = QStringLiteral("변환할 수 있는 도형이 없습니다."); return result; }
  for (auto& [unused, group] : groups) {
    Q_UNUSED(unused)
    if (!group.writer->flushBuffer()) { result.error = QStringLiteral("SHP 도형 저장을 완료하지 못했습니다."); return result; }
    group.writer.reset();
  }
  if (textTruncated) result.warnings.append(QStringLiteral("SHP 문자열 길이 제한으로 긴 지명이 잘렸습니다. 전체 원문은 원본 DXF에 보존되어 있습니다."));
  QJsonArray files;
  for (const auto& file : QDir(staging.path()).entryInfoList(QDir::Files)) {
    const auto hash = digest(file.absoluteFilePath(), canceled);
    if (stopped()) return result;
    if (hash.isEmpty()) { result.error = QStringLiteral("SHP 저장 결과를 확인하지 못했습니다."); return result; }
    files.append(QJsonObject{{"name", file.fileName()}, {"sha256", QString::fromLatin1(hash)}});
  }
  for (const auto& [unused, group] : groups) {
    Q_UNUSED(unused)
    for (const auto& suffix : {"shp", "shx", "dbf", "prj"}) {
      if (!QFileInfo::exists(QDir(staging.path()).filePath(group.record.layerName + '.' + suffix))) {
        result.error = QStringLiteral("SHP 구성 파일이 완성되지 않았습니다."); return result;
      }
    }
  }
  if (stopped()) return result;
  if (!sourcesUnchanged()) return result;
  const QString completed = key + '-' + QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!cache.rename(QFileInfo(staging.path()).fileName(), completed)) { result.error = QStringLiteral("SHP 보관을 완료하지 못했습니다."); return result; }
  staging.setAutoRemove(false);
  QJsonArray outputRecords;
  for (auto& [unused, group] : groups) {
    Q_UNUSED(unused)
    group.record.source = QDir(cache.filePath(completed)).filePath(QFileInfo(group.record.source).fileName());
    outputRecords.append(encode(group.record)); result.records.append(group.record);
  }
  QSaveFile saved(indexPath);
  const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"directory", completed}, {"files", files}, {"records", outputRecords}}).toJson();
  if (stopped()) { result.records.clear(); return result; }
  if (!saved.open(QIODevice::WriteOnly) || saved.write(data) != data.size() || !saved.commit()) {
    result.records.clear(); result.error = QStringLiteral("SHP 보관 색인을 저장하지 못했습니다."); return result;
  }
  result.filesWritten = static_cast<int>(groups.size());
  return result;
}
