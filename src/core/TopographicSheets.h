#pragma once

#include <QList>
#include <QString>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <functional>
#include <optional>

class QgsFeedback;

namespace TopographicSheets {
// GRS80 alone is Unknown: it does not distinguish 500000/550000/600000 northing.
// World2010 requires independently verified source metadata stating that basis.
enum class SourceBasis { Unknown, World2010, WorldPre2010, Tokyo };
enum class Zone { West, Central, East, EastSea, Unsupported };
struct Sheet {
  QString number;
  QgsRectangle geographicExtent; // Longitude/latitude grid, EPSG:4737 preview.
  Zone zone = Zone::Unsupported;
  QString crsAuthId; // Empty unless World2010 was explicitly established.
};
struct Result {
  QList<Sheet> sheets;
  QgsGeometry searchArea; // Metres in the caller's workCrs; preview circle.
  QString error;
  bool canceled = false;
};

// Six ASCII digits only, 1:25000 grid in the Korean longitude domain [124,132).
// Resolving a grid cell does not assert that a downloadable sheet exists.
std::optional<Sheet> resolve(const QString& number, SourceBasis basis = SourceBasis::Unknown);
QString zoneName(Zone zone);
// 0 < radiusKm <= 100; only work CRS EPSG:5186/5187 is accepted. Boundary
// contact counts. On error/cancel no partial sheets or preview are returned.
// Feedback is worker-local; a QgsTask caller can instead pass isCanceled.
Result select(const QgsPointXY& center, const QgsCoordinateReferenceSystem& workCrs,
              double radiusKm, const QgsCoordinateTransformContext& context,
              SourceBasis basis = SourceBasis::Unknown, QgsFeedback* feedback = nullptr,
              const std::function<bool()>& isCanceled = {});
}
