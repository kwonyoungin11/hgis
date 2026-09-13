#include "TopographicCatalog.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QResource>
#include <QHash>
#include <QDebug>
#include <qgswkbtypes.h>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_string.h>
#include <cpl_vsi.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgspointxy.h>
#include <qgsfeedback.h>
#include <qgsexception.h>
#include <cmath>
#include <memory>
#include <algorithm>

static void initializeTopographicResources() { Q_INIT_RESOURCE(topographic); }

namespace TopographicCatalog {
namespace {
constexpr int kCacheVersion = 3;
struct Input { QString source; QString file; QString signature; };
bool canceled(QgsFeedback* feedback, const std::function<bool()>& check) {
  return (check && check()) || (feedback && feedback->isCanceled());
}
bool supported(const QString& path) {
  const QString suffix = QFileInfo(path).suffix().toLower();
  return suffix == QLatin1String("shp") || suffix == QLatin1String("ngi") ||
         suffix == QLatin1String("dxf") || suffix == QLatin1String("gpkg");
}
QString signatureFor(const QString& path) {
  const QFileInfo file(path);
  QFileInfoList related{file};
  const QString suffix = file.suffix().toLower();
  if (suffix == QLatin1String("shp") || suffix == QLatin1String("ngi")) {
    related = file.dir().entryInfoList({file.completeBaseName() + QStringLiteral(".*")},
        QDir::Files | QDir::NoSymLinks, QDir::Name);
  } else if (suffix == QLatin1String("gpkg")) {
    const QFileInfo wal(path + QStringLiteral("-wal"));
    if (wal.exists()) related.append(wal);
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (const auto& part : related)
    hash.addData(part.fileName().toUtf8() + '\0' + QByteArray::number(part.size()) + '\0' +
                 QByteArray::number(part.lastModified().toMSecsSinceEpoch()) + '\n');
  return QString::fromLatin1(hash.result().toHex());
}
struct CodeInfo { Category category = Category::Unknown; QString name; };
const QHash<QString, CodeInfo>& codeBook() {
  static const auto result = [] {
    initializeTopographicResources();
    QHash<QString, CodeInfo> entries;
    QFile file(QStringLiteral(":/topographic/layer-codes.json"));
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "Topographic layer code resource could not be read"; return entries;
    }
    const QHash<QString, Category> categories{
      {QStringLiteral("Contour"), Category::Contour}, {QStringLiteral("Road"), Category::Road},
      {QStringLiteral("Building"), Category::Building}, {QStringLiteral("Water"), Category::Water},
      {QStringLiteral("Vegetation"), Category::Vegetation}, {QStringLiteral("Boundary"), Category::Boundary},
      {QStringLiteral("Facility"), Category::Facility}, {QStringLiteral("ElevationPoint"), Category::ElevationPoint},
      {QStringLiteral("PlaceName"), Category::PlaceName}};
    for (const auto& value : QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("codes")).toArray()) {
      const auto entry = value.toObject();
      const QString code = entry.value(QStringLiteral("code")).toString();
      const QString name = entry.value(QStringLiteral("name")).toString();
      if (code.isEmpty() || name.isEmpty()) continue;
      entries.insert(code, {categories.value(entry.value(QStringLiteral("category")).toString(), Category::Unknown), name});
    }
    return entries;
  }();
  return result;
}
CodeInfo inferCode(const QString& name) {
  for (const auto category : {Category::Contour, Category::Road, Category::Building,
       Category::Water, Category::Vegetation, Category::Boundary, Category::Facility,
       Category::ElevationPoint, Category::PlaceName})
    if (name.trimmed() == categoryName(category)) return {category, name.trimmed()};
  static const QRegularExpression token(QStringLiteral("(?<![\\p{L}\\p{N}])([A-H][0-9]{7})(?![\\p{L}\\p{N}])"));
  CodeInfo result;
  auto matches = token.globalMatch(name);
  while (matches.hasNext()) {
    const auto entry = codeBook().constFind(matches.next().captured(1));
    if (entry == codeBook().cend() || (result.category != Category::Unknown && result.category != entry->category)) return {};
    result = *entry;
  }
  return result;
}
QString geometryName(OGRwkbGeometryType type) {
  // QGIS OGR geometry filters deliberately flatten Z/M. Index that same family
  // once, otherwise 2D and 3D rows load the same features twice. Geometry Z itself
  // stays in the read-only datasource and is used by the labeling expression.
  return QgsWkbTypes::displayString(static_cast<Qgis::WkbType>(wkbFlatten(type)));
}
QJsonObject encode(const Record& record) {
  return {{QStringLiteral("source"), record.source}, {QStringLiteral("layer"), record.layerName},
      {QStringLiteral("crs"), record.crsWkt},
      {QStringLiteral("cadLayer"), record.cadLayer}, {QStringLiteral("geometryType"), record.geometryType},
      {QStringLiteral("displayName"), record.displayName}, {QStringLiteral("hasExtent"), record.hasExtent},
      {QStringLiteral("extent"), QJsonArray{record.extent.xMinimum(), record.extent.yMinimum(),
                                          record.extent.xMaximum(), record.extent.yMaximum()}},
      {QStringLiteral("category"), int(record.category)}};
}
bool decode(const QJsonObject& entry, const Input& input, QList<Record>* records) {
  if (entry.value(QStringLiteral("signature")).toString() != input.signature ||
      !entry.value(QStringLiteral("records")).isArray()) return false;
  QList<Record> parsed;
  for (const auto& value : entry.value(QStringLiteral("records")).toArray()) {
    const auto object = value.toObject();
    Record record;
    record.source = object.value(QStringLiteral("source")).toString();
    record.layerName = object.value(QStringLiteral("layer")).toString();
    record.crsWkt = object.value(QStringLiteral("crs")).toString();
    record.cadLayer = object.value(QStringLiteral("cadLayer")).toString();
    record.geometryType = object.value(QStringLiteral("geometryType")).toString();
    record.displayName = object.value(QStringLiteral("displayName")).toString();
    record.hasExtent = object.value(QStringLiteral("hasExtent")).toBool();
    record.signature = input.signature;
    const auto bounds = object.value(QStringLiteral("extent")).toArray();
    const int category = object.value(QStringLiteral("category")).toInt(-1);
    if (record.source != input.source || record.layerName.isEmpty() || bounds.size() != 4 ||
        category < 0 || category > int(Category::PlaceName)) return false;
    for (const auto& coordinate : bounds)
      if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble())) return false;
    record.extent = QgsRectangle(bounds[0].toDouble(), bounds[1].toDouble(),
                                 bounds[2].toDouble(), bounds[3].toDouble());
    // Mapping-file updates must also affect cached geometry indices.
    const auto info = inferCode(record.geometryType.isEmpty() ? record.layerName : record.cadLayer);
    record.category = info.category;
    record.displayName = info.name;
    if (!record.geometryType.isEmpty() && info.name.isEmpty()) record.displayName = QStringLiteral("미분류 · ") + record.cadLayer;
    parsed.append(record);
  }
  *records = parsed; return true;
}
bool readInput(const Input& input, QList<Record>* records, QStringList* warnings,
               QgsFeedback* feedback, const std::function<bool()>& check) {
  CPLPushErrorHandler(CPLQuietErrorHandler);
  const auto errors = qScopeGuard([] { CPLPopErrorHandler(); });
  CPLErrorReset();
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      static_cast<GDALDataset*>(GDALOpenEx(input.source.toUtf8().constData(),
          GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)), GDALClose);
  if (!dataset) {
    warnings->append(QStringLiteral("자료를 읽지 못했습니다: %1 · %2")
        .arg(input.source, QString::fromUtf8(CPLGetLastErrorMsg()))); return false;
  }
  for (int i = 0; i < dataset->GetLayerCount(); ++i) {
    if (canceled(feedback, check)) return false;
    auto* layer = dataset->GetLayer(i);
    if (!layer || layer->GetLayerDefn()->GetGeomFieldCount() == 0 ||
        wkbFlatten(layer->GetGeomType()) == wkbNone) continue;
    Record record;
    record.source = input.source; record.layerName = QString::fromUtf8(layer->GetName());
    record.signature = input.signature;
    const auto code = inferCode(record.layerName); record.category = code.category; record.displayName = code.name;
    const auto* crs = layer->GetSpatialRef();
    if (crs && !crs->IsEmpty()) {
      const QString name = QString::fromUtf8(crs->GetName());
      if (name != QLatin1String("Undefined geographic SRS") && name != QLatin1String("Undefined Cartesian SRS")) {
        char* wkt = nullptr;
        if (crs->exportToWkt(&wkt) == OGRERR_NONE && wkt) record.crsWkt = QString::fromUtf8(wkt);
        CPLFree(wkt);
      }
    }
    if (QString::fromUtf8(dataset->GetDriverName()) == QLatin1String("DXF") &&
        layer->GetLayerDefn()->GetFieldIndex("Layer") >= 0) {
      QHash<QString, Record> cadRecords;
      layer->ResetReading();
      while (!canceled(feedback, check)) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(layer->GetNextFeature(), OGRFeature::DestroyFeature);
        if (!feature) break;
        const auto* geometry = feature->GetGeometryRef();
        if (!geometry || geometry->IsEmpty()) continue;
        const QString cadLayer = QString::fromUtf8(feature->GetFieldAsString("Layer"));
        const QString type = geometryName(geometry->getGeometryType());
        const QString key = cadLayer + QChar(0) + type;
        auto found = cadRecords.find(key);
        if (found == cadRecords.end()) {
          auto cad = record; cad.cadLayer = cadLayer; cad.geometryType = type;
          const auto info = inferCode(cadLayer); cad.category = info.category;
          cad.displayName = info.name.isEmpty() ? QStringLiteral("미분류 · ") + cadLayer : info.name;
          found = cadRecords.insert(key, cad);
        }
        OGREnvelope one; geometry->getEnvelope(&one);
        if (!std::isfinite(one.MinX) || !std::isfinite(one.MaxX) || !std::isfinite(one.MinY) || !std::isfinite(one.MaxY)) continue;
        const QgsRectangle bounds(one.MinX, one.MinY, one.MaxX, one.MaxY);
        if (!found->hasExtent) found->extent = bounds; else found->extent.combineExtentWith(bounds);
        found->hasExtent = true;
      }
      if (canceled(feedback, check)) return false;
      auto keys = cadRecords.keys(); std::sort(keys.begin(), keys.end());
      for (const auto& key : keys) records->append(cadRecords.value(key));
      continue;
    }
    OGREnvelope envelope;
    if (layer->GetExtent(&envelope, false) == OGRERR_NONE) record.hasExtent = true;
    else {
      // Avoid an uninterruptible forced scan when metadata has no extent.
      layer->ResetReading();
      while (!canceled(feedback, check)) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(
            layer->GetNextFeature(), OGRFeature::DestroyFeature);
        if (!feature) break;
        const auto* geometry = feature->GetGeometryRef();
        if (!geometry || geometry->IsEmpty()) continue;
        OGREnvelope one; geometry->getEnvelope(&one);
        if (!record.hasExtent) envelope = one; else envelope.Merge(one);
        record.hasExtent = true;
      }
    }
    if (canceled(feedback, check)) return false;
    if (record.hasExtent) {
      record.hasExtent = std::isfinite(envelope.MinX) && std::isfinite(envelope.MinY) &&
                         std::isfinite(envelope.MaxX) && std::isfinite(envelope.MaxY);
      if (record.hasExtent) record.extent = QgsRectangle(envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY);
    }
    records->append(record);
  }
  return true;
}
}
QString categoryName(Category category) {
  switch (category) {
    case Category::Contour: return QStringLiteral("등고선");
    case Category::Road: return QStringLiteral("도로");
    case Category::Building: return QStringLiteral("건물");
    case Category::Water: return QStringLiteral("수계");
    case Category::Vegetation: return QStringLiteral("식생");
    case Category::Boundary: return QStringLiteral("경계");
    case Category::Facility: return QStringLiteral("시설물");
    case Category::ElevationPoint: return QStringLiteral("표고점");
    case Category::PlaceName: return QStringLiteral("지명");
    default: return QStringLiteral("미분류");
  }
}
ScanResult scan(const QString& folder, QgsFeedback* feedback, const QString& cacheDirectory,
                const std::function<bool()>& check) {
  ScanResult result;
  const auto stop = [&] {
    if (!canceled(feedback, check)) return false;
    result.canceled = true; result.records.clear(); return true;
  };
  if (stop()) return result;
  const QFileInfo rootInfo(folder);
  if (folder.isEmpty() || !rootInfo.isDir() || !rootInfo.isReadable()) {
    result.error = QStringLiteral("수치지형도 폴더를 읽을 수 없습니다: %1").arg(folder); return result;
  }
  const QString root = rootInfo.canonicalFilePath();
  const QString cacheDir = cacheDirectory.isEmpty()
      ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/topographic-catalog") : cacheDirectory;
  const QString cachePath = QDir(cacheDir).filePath(QString::fromLatin1(
      QCryptographicHash::hash(root.toUtf8(), QCryptographicHash::Sha256).toHex()) + QStringLiteral(".json"));
  QJsonObject oldEntries;
  QFile cache(cachePath);
  if (cache.exists()) {
    if (cache.open(QIODevice::ReadOnly) && cache.size() <= 128 * 1024 * 1024) {
      const auto object = QJsonDocument::fromJson(cache.readAll()).object();
      if (object.value(QStringLiteral("version")).toInt() == kCacheVersion &&
          object.value(QStringLiteral("root")).toString() == root && object.value(QStringLiteral("entries")).isObject())
        oldEntries = object.value(QStringLiteral("entries")).toObject();
      else result.warnings.append(QStringLiteral("색인 캐시를 다시 만듭니다."));
    } else result.warnings.append(QStringLiteral("기존 색인 캐시를 읽지 못해 다시 만듭니다."));
    cache.close();
  }
  GDALAllRegister();
  QList<Input> inputs;
  QDirIterator iterator(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    if (stop()) return result;
    const QString file = iterator.next();
    if (supported(file)) inputs.append({file, file, signatureFor(file)});
    else if (QFileInfo(file).suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) == 0) {
      const QString archive = QStringLiteral("/vsizip/") + QDir::fromNativeSeparators(file);
      char** members = VSIReadDirRecursive(archive.toUtf8().constData());
      const auto release = qScopeGuard([members] { CSLDestroy(members); });
      if (!members) { result.warnings.append(QStringLiteral("압축 목록을 읽지 못했습니다: %1").arg(file)); continue; }
      const QString signature = signatureFor(file);
      for (int i = 0; members[i]; ++i) {
        if (stop()) return result;
        const QString member = QString::fromUtf8(members[i]);
        if (supported(member)) inputs.append({archive + QLatin1Char('/') + member, file, signature});
      }
    }
  }
  QJsonObject newEntries;
  for (const auto& input : inputs) {
    if (stop()) return result;
    QList<Record> records;
    QJsonObject entry = oldEntries.value(input.source).toObject();
    if (decode(entry, input, &records)) ++result.cacheHits;
    else {
      ++result.filesRead;
      if (!readInput(input, &records, &result.warnings, feedback, check)) {
        if (stop()) return result;
        continue;
      }
      if (signatureFor(input.file) != input.signature) {
        result.error = QStringLiteral("색인 중 자료가 변경되었습니다. 다시 시도하세요: %1").arg(input.file);
        result.records.clear(); return result;
      }
      QJsonArray encoded; for (const auto& record : records) encoded.append(encode(record));
      entry = {{QStringLiteral("signature"), input.signature}, {QStringLiteral("records"), encoded}};
    }
    for (const auto& record : records) {
      if (!record.geometryType.isEmpty() && record.category == Category::Unknown) {
        const QString warning = QStringLiteral("미분류 DXF 레이어: %1 · %2").arg(record.source, record.cadLayer);
        result.warnings.append(warning); qWarning().noquote() << warning;
      }
      if (record.crsWkt.isEmpty()) result.warnings.append(QStringLiteral("좌표계 확인 필요: %1 · %2").arg(record.source, record.layerName));
      if (!record.hasExtent) result.warnings.append(QStringLiteral("도형 범위 없음: %1 · %2").arg(record.source, record.layerName));
    }
    result.records.append(records); newEntries.insert(input.source, entry);
    if (feedback) feedback->setProgress(100. * (result.cacheHits + result.filesRead) / qMax(qsizetype(1), inputs.size()));
  }
  if (stop()) return result;
  QSaveFile output(cachePath);
  const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), kCacheVersion},
      {QStringLiteral("root"), root}, {QStringLiteral("entries"), newEntries}}).toJson(QJsonDocument::Compact);
  if (!QDir().mkpath(cacheDir) || !output.open(QIODevice::WriteOnly) || output.write(data) != data.size())
    result.warnings.append(QStringLiteral("색인 캐시를 저장하지 못했습니다. 자료는 읽기 전용으로 유지됩니다."));
  else if (stop()) { output.cancelWriting(); return result; }
  else if (!output.commit()) result.warnings.append(QStringLiteral("색인 캐시를 교체하지 못했습니다."));
  return result;
}
QgsRectangle coverageBounds(const QgsRectangle& view, double radiusMeters) {
  if (!std::isfinite(radiusMeters) || radiusMeters <= 0.) return view;
  if (!view.isFinite() || view.isEmpty()) return view;
  const QgsPointXY center = view.center();
  const QgsRectangle radius(center.x() - radiusMeters, center.y() - radiusMeters,
      center.x() + radiusMeters, center.y() + radiusMeters);
  QgsRectangle cover = view;
  cover.combineExtentWith(radius);
  return cover;
}
bool visibleAtScale(Category category, double) {
  switch (category) {
    case Category::Contour:
    case Category::Road:
    case Category::Building:
    case Category::Water:
    case Category::Boundary:
      return true;
    case Category::ElevationPoint:
      return false;
    default:
      return false;
  }
}
QueryResult query(const QList<Record>& records, const QgsRectangle& bounds, const QString& boundsCrs,
                  const QgsCoordinateTransformContext& context, QgsFeedback* feedback,
                  const std::function<bool()>& check, double scale) {
  QueryResult result;
  const auto stop = [&] {
    if (!canceled(feedback, check)) return false;
    result.canceled = true; result.matches.clear(); result.unknownCrs.clear(); return true;
  };
  if (stop()) return result;
  const QgsCoordinateReferenceSystem destination(boundsCrs);
  if (!destination.isValid() || !bounds.isFinite() || bounds.isEmpty()) {
    result.error = QStringLiteral("조회 범위와 작업 좌표계를 확인할 수 없습니다."); return result;
  }
  for (const auto& record : records) {
    if (stop()) return result;
    const QgsCoordinateReferenceSystem source(record.crsWkt);
    if (record.crsWkt.isEmpty() || !source.isValid()) { result.unknownCrs.append(record); continue; }
    if (!record.hasExtent || !record.extent.isFinite()) continue;
    try {
      QgsCoordinateTransform transform(source, destination, context);
      transform.setAllowFallbackTransforms(false); transform.setBallparkTransformsAreAppropriate(false);
      const auto projected = transform.transformBoundingBox(record.extent);
      if (!projected.isFinite()) {
        result.warnings.append(QStringLiteral("자료 범위를 변환하지 못했습니다: %1").arg(record.source)); continue;
      }
      if (projected.intersects(bounds) && visibleAtScale(record.category, scale))
        result.matches.append(record);
    } catch (const QgsCsException&) {
      result.warnings.append(QStringLiteral("자료 좌표계를 변환하지 못했습니다: %1 · %2").arg(record.source, record.layerName));
    }
  }
  stop(); return result;
}
}
