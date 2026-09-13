#pragma once
#include "TopographicCatalog.h"

namespace TopographicShapefileCache {
struct Result {
  QList<TopographicCatalog::Record> records;
  QStringList warnings;
  QString error;
  bool canceled = false;
  bool reused = false;
  int filesWritten = 0;
};
// Only CRS-verified DXF records are accepted. Originals are read-only; completed
// SHP datasets are immutable and become visible together through an atomic index.
Result prepare(const QList<TopographicCatalog::Record>& verifiedRecords,
               const QString& cacheRoot, const QgsCoordinateTransformContext& context,
               const std::function<bool()>& isCanceled = {});
}
