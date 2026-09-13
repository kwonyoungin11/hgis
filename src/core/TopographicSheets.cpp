#include "TopographicSheets.h"
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeedback.h>
#include <algorithm>
#include <cmath>

namespace TopographicSheets {
namespace {
constexpr double kGridSize = 0.125; // Official 7 minutes 30 seconds.
constexpr double kWest = 124.;
constexpr double kEast = 132.;
// Densified projected grid edges and GEOS distance use a 1 mm contact tolerance.
constexpr double kContactTolerance = 0.001;
bool canceled(QgsFeedback* feedback, const std::function<bool()>& check) {
  return (check && check()) || (feedback && feedback->isCanceled());
}
Result canceledResult() {
  Result result; result.canceled = true; return result;
}
Result errorResult(const QString& error) {
  Result result; result.error = error; return result;
}
QString numberForCell(int x, int y) {
  const int baseLongitude = x / 8, baseLatitude = y / 8;
  const int northRow = 7 - y % 8;
  const int map50k = (northRow / 2) * 4 + (x % 8) / 2 + 1;
  const int quarter = (northRow % 2) * 2 + (x % 2) + 1;
  return QStringLiteral("%1%2%3%4").arg(baseLatitude, 2, 10, QChar('0'))
      .arg(baseLongitude % 10).arg(map50k, 2, 10, QChar('0')).arg(quarter);
}
}

std::optional<Sheet> resolve(const QString& number, SourceBasis basis) {
  // Do not accept embedded file names, whitespace, non-ASCII numerals or a
  // different scale. The longitude suffix is only unambiguous in [124,132).
  if (number.size() != 6) return std::nullopt;
  for (QChar digit : number) if (digit < QChar('0') || digit > QChar('9')) return std::nullopt;
  const int baseLatitude = number.left(2).toInt();
  const int longitudeDigit = number.at(2).digitValue();
  const int map50k = number.mid(3, 2).toInt();
  const int quarter = number.at(5).digitValue();
  if (baseLatitude >= 90 || longitudeDigit == 2 || longitudeDigit == 3 ||
      map50k < 1 || map50k > 16 || quarter < 1 || quarter > 4) return std::nullopt;
  const int baseLongitude = (longitudeDigit >= 4 ? 120 : 130) + longitudeDigit;
  const int index50k = map50k - 1, index25k = quarter - 1;
  const double west = baseLongitude + (index50k % 4) * .25 + (index25k % 2) * kGridSize;
  const double north = baseLatitude + 1. - (index50k / 4) * .25 - (index25k / 2) * kGridSize;
  Sheet sheet;
  sheet.number = number;
  sheet.geographicExtent = QgsRectangle(west, north - kGridSize, west + kGridSize, north);
  const double centerLongitude = west + kGridSize / 2.;
  int code = 0;
  if (centerLongitude < 126.) { sheet.zone = Zone::West; code = 5185; }
  else if (centerLongitude < 128.) { sheet.zone = Zone::Central; code = 5186; }
  else if (centerLongitude < 130.) { sheet.zone = Zone::East; code = 5187; }
  else { sheet.zone = Zone::EastSea; code = 5188; }
  if (basis == SourceBasis::World2010) sheet.crsAuthId = QStringLiteral("EPSG:%1").arg(code);
  return sheet;
}

QString zoneName(Zone zone) {
  switch (zone) {
  case Zone::West: return QStringLiteral("서부원점");
  case Zone::Central: return QStringLiteral("중부원점");
  case Zone::East: return QStringLiteral("동부원점");
  case Zone::EastSea: return QStringLiteral("동해원점");
  case Zone::Unsupported: return QStringLiteral("원점대 확인 필요");
  }
  return QStringLiteral("원점대 확인 필요");
}

Result select(const QgsPointXY& center, const QgsCoordinateReferenceSystem& workCrs, double radiusKm,
              const QgsCoordinateTransformContext& context, SourceBasis basis, QgsFeedback* feedback,
              const std::function<bool()>& isCanceled) {
  if (canceled(feedback, isCanceled)) return canceledResult();
  if (!std::isfinite(center.x()) || !std::isfinite(center.y()))
    return errorResult(QStringLiteral("검색 중심 좌표가 유효하지 않습니다."));
  if (!std::isfinite(radiusKm) || radiusKm <= 0. || radiusKm > 100.)
    return errorResult(QStringLiteral("검색 반경은 0km보다 크고 100km 이하여야 합니다."));
  if (!workCrs.isValid() || (workCrs.authid() != QStringLiteral("EPSG:5186") &&
                            workCrs.authid() != QStringLiteral("EPSG:5187")))
    return errorResult(QStringLiteral("도엽 검색은 조사 좌표계 EPSG:5186 또는 EPSG:5187이 필요합니다."));

  const double radius = radiusKm * 1000.;
  const QgsGeometry point = QgsGeometry::fromPointXY(center);
  Result result;
  result.searchArea = point.buffer(radius, 90, feedback);
  if (canceled(feedback, isCanceled)) return canceledResult();
  if (result.searchArea.isEmpty()) return errorResult(QStringLiteral("검색 반경을 만들지 못했습니다."));
  try {
    const QgsCoordinateReferenceSystem geographic(QStringLiteral("EPSG:4737"));
    QgsCoordinateTransform toGeographic(workCrs, geographic, context);
    QgsCoordinateTransform toWork(geographic, workCrs, context);
    toGeographic.setAllowFallbackTransforms(false);
    toGeographic.setBallparkTransformsAreAppropriate(false);
    toWork.setAllowFallbackTransforms(false);
    toWork.setBallparkTransformsAreAppropriate(false);
    if (!toGeographic.isValid() || !toWork.isValid())
      return errorResult(QStringLiteral("도엽 좌표 변환을 준비하지 못했습니다."));
    const auto geographicCenter = toGeographic.transform(center);
    if (!std::isfinite(geographicCenter.x()) || !std::isfinite(geographicCenter.y()) ||
        geographicCenter.x() < kWest || geographicCenter.x() >= kEast ||
        geographicCenter.y() < 0. || geographicCenter.y() >= 90.)
      return errorResult(QStringLiteral("검색 중심이 지원하는 도엽 경도 범위(124°~132°) 밖에 있습니다."));

    // Use a bounding square, not the inscribed preview polygon, for candidates.
    // Expand by a full grid cell to retain exact edge/corner contact after the
    // coordinate transform's bounding-box densification and rounding.
    auto bounds = toGeographic.transformBoundingBox(QgsRectangle(center.x() - radius, center.y() - radius,
                                                                 center.x() + radius, center.y() + radius));
    if (!bounds.isFinite()) return errorResult(QStringLiteral("검색 범위를 변환하지 못했습니다."));
    bounds.grow(kGridSize);
    const int minX = static_cast<int>(std::floor(std::clamp(bounds.xMinimum(), kWest, kEast) * 8.));
    const int maxX = std::min(1055, static_cast<int>(std::floor(std::clamp(bounds.xMaximum(), kWest, kEast) * 8.)));
    const int minY = static_cast<int>(std::floor(std::clamp(bounds.yMinimum(), 0., 90.) * 8.));
    const int maxY = std::min(719, static_cast<int>(std::floor(std::clamp(bounds.yMaximum(), 0., 90.) * 8.)));
    const int total = (maxX - minX + 1) * (maxY - minY + 1);
    int completed = 0;
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        if (canceled(feedback, isCanceled)) return canceledResult();
        const auto sheet = resolve(numberForCell(x, y), basis);
        if (!sheet) return errorResult(QStringLiteral("도엽 번호를 구성하지 못했습니다."));
        // Geographic rectangle edges become curves in the working projection.
        // Densify before transforming (at most ~112 m between source vertices).
        auto polygon = QgsGeometry::fromRect(sheet->geographicExtent).densifyByDistance(.001);
        if (polygon.transform(toWork) != Qgis::GeometryOperationResult::Success || polygon.isEmpty())
          return errorResult(QStringLiteral("도엽 경계를 변환하지 못했습니다: %1").arg(sheet->number));
        const double distance = polygon.distance(point);
        if (!std::isfinite(distance) || distance < 0.)
          return errorResult(QStringLiteral("도엽과 검색 반경의 교차를 계산하지 못했습니다: %1").arg(sheet->number));
        if (distance <= radius + kContactTolerance) result.sheets.append(*sheet);
        if (feedback && total > 0) feedback->setProgress(100. * ++completed / total);
      }
    }
    if (canceled(feedback, isCanceled)) return canceledResult();
    std::sort(result.sheets.begin(), result.sheets.end(), [](const Sheet& a, const Sheet& b) { return a.number < b.number; });
    return result;
  } catch (const QgsCsException&) {
    return errorResult(QStringLiteral("조사 좌표와 도엽 좌표를 변환하지 못했습니다."));
  }
}
}
