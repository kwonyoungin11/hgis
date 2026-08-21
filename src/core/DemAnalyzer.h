#pragma once

#include <QString>
#include <cstdint>
#include <vector>

// GDAL DEM hillshade / slope. Overlay only — not survey domain, not export 5179.
namespace DemAnalyzer {

enum class HillshadeMode { Single, Multi };

struct Options {
  double azimuthDeg = 315.0;    // light, clockwise from north (GDAL gdaldem)
  double altitudeDeg = 45.0;
  double zFactor = 1.0;
  HillshadeMode hillshade = HillshadeMode::Multi;
};

struct RasterInfo {
  int width = 0;
  int height = 0;
  double geotransform[6] = {0, 1, 0, 0, 0, -1};
  QString projectionWkt;
  float noData = -9999.0f;
  bool hasNoData = false;
};

bool readFloatBand(const QString& tifPath, std::vector<float>* out, RasterInfo* info,
                   QString* errorOut);

void hillshadeHorn(const std::vector<float>& z, const RasterInfo& info, const Options& opt,
                   std::vector<std::uint8_t>* outGray);

void slopeDegrees(const std::vector<float>& z, const RasterInfo& info, double zFactor,
                  std::vector<float>* outDeg);

// Writes a Byte GeoTIFF (same grid as source) for QgsRasterLayer overlay.
bool writeByteGeoTiff(const QString& outPath, const RasterInfo& info,
                      const std::vector<std::uint8_t>& gray, QString* errorOut);

bool runHillshadeFile(const QString& demPath, const QString& outPath, const Options& opt,
                      QString* errorOut);

}  // namespace DemAnalyzer
