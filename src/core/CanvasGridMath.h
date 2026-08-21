#pragma once

#include <cmath>

// Adaptive map-grid intervals. Projected metres and geographic degrees.
namespace CanvasGridMath {

inline double niceStepMeters(double targetMeters) {
  if (!(targetMeters > 0.0) || !std::isfinite(targetMeters))
    return 10.0;
  const double mag = std::pow(10.0, std::floor(std::log10(targetMeters)));
  const double n = targetMeters / mag;
  if (n < 1.5)
    return mag;
  if (n < 3.5)
    return 2.0 * mag;
  if (n < 7.5)
    return 5.0 * mag;
  return 10.0 * mag;
}

// target in degrees. Returns a DMS-friendly step.
inline double niceStepDegrees(double targetDeg) {
  if (!(targetDeg > 0.0) || !std::isfinite(targetDeg))
    return 1.0 / 60.0;
  static const double kSteps[] = {
      0.1 / 3600.0, 1.0 / 3600.0, 10.0 / 3600.0, 30.0 / 3600.0,
      1.0 / 60.0,   10.0 / 60.0,  30.0 / 60.0,   1.0,
      2.0,          5.0,          10.0,          30.0, 45.0};
  for (double s : kSteps) {
    if (s >= targetDeg * 0.6)
      return s;
  }
  return 10.0;
}

}  // namespace CanvasGridMath
