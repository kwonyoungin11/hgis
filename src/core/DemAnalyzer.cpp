#include "DemAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <gdal_priv.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr double kDeg2Rad = M_PI / 180.0;

void hornDeriv(const std::vector<float>& z, int w, int h, int x, int y, double xres, double yres,
               float nd, bool hasNd, double zf, double* dzdx, double* dzdy, bool* ok) {
  *ok = false;
  if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1)
    return;
  auto cell = [&](int cx, int cy) -> float { return z[static_cast<size_t>(cy) * w + cx]; };
  const float a = cell(x - 1, y - 1);
  const float b = cell(x, y - 1);
  const float c = cell(x + 1, y - 1);
  const float d = cell(x - 1, y);
  const float f = cell(x + 1, y);
  const float g = cell(x - 1, y + 1);
  const float hh = cell(x, y + 1);
  const float i = cell(x + 1, y + 1);
  if (hasNd) {
    if (a == nd || b == nd || c == nd || d == nd || f == nd || g == nd || hh == nd || i == nd)
      return;
  }
  const double dx = ((c + 2.0 * f + i) - (a + 2.0 * d + g)) / (8.0 * xres);
  const double dy = ((g + 2.0 * hh + i) - (a + 2.0 * b + c)) / (8.0 * std::abs(yres));
  *dzdx = dx * zf;
  *dzdy = dy * zf;
  *ok = true;
}

std::uint8_t shadeOne(double dzdx, double dzdy, double zenith, double azimuthMath) {
  const double slope = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy));
  double aspect = std::atan2(dzdy, -dzdx);
  const double hs =
      std::cos(zenith) * std::cos(slope) + std::sin(zenith) * std::sin(slope) * std::cos(azimuthMath - aspect);
  const double v = std::clamp(hs, 0.0, 1.0);
  return static_cast<std::uint8_t>(std::lround(v * 255.0));
}

}  // namespace

namespace DemAnalyzer {

bool readFloatBand(const QString& tifPath, std::vector<float>* out, RasterInfo* info,
                   QString* errorOut) {
  if (!out || !info) {
    if (errorOut)
      *errorOut = QStringLiteral("null buffer");
    return false;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpen(tifPath.toUtf8().constData(), GA_ReadOnly));
  if (!ds) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM을 열 수 없습니다.");
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("밴드가 없습니다.");
    return false;
  }
  info->width = ds->GetRasterXSize();
  info->height = ds->GetRasterYSize();
  ds->GetGeoTransform(info->geotransform);
  if (const char* wkt = ds->GetProjectionRef())
    info->projectionWkt = QString::fromUtf8(wkt);
  int hasNd = 0;
  const double nd = band->GetNoDataValue(&hasNd);
  info->hasNoData = hasNd != 0;
  info->noData = static_cast<float>(nd);
  const size_t n = static_cast<size_t>(info->width) * static_cast<size_t>(info->height);
  out->assign(n, info->noData);
  const CPLErr err =
      band->RasterIO(GF_Read, 0, 0, info->width, info->height, out->data(), info->width,
                     info->height, GDT_Float32, 0, 0);
  GDALClose(ds);
  if (err != CE_None) {
    if (errorOut)
      *errorOut = QStringLiteral("DEM 읽기 실패");
    return false;
  }
  return true;
}

void hillshadeHorn(const std::vector<float>& z, const RasterInfo& info, const Options& opt,
                   std::vector<std::uint8_t>* outGray) {
  const int w = info.width;
  const int h = info.height;
  outGray->assign(static_cast<size_t>(w) * h, 0);
  const double xres = info.geotransform[1] != 0.0 ? info.geotransform[1] : 1.0;
  const double yres = info.geotransform[5] != 0.0 ? info.geotransform[5] : -1.0;
  const double zenith = (90.0 - opt.altitudeDeg) * kDeg2Rad;
  auto azMath = [](double azCwNorth) {
    return (360.0 - azCwNorth + 90.0) * kDeg2Rad;
  };
  const double azs[4] = {225.0, 270.0, 315.0, 360.0};

#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
#endif
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      double dzdx = 0, dzdy = 0;
      bool ok = false;
      hornDeriv(z, w, h, x, y, xres, yres, info.noData, info.hasNoData, opt.zFactor, &dzdx, &dzdy,
                &ok);
      if (!ok)
        continue;
      std::uint8_t v = 0;
      if (opt.hillshade == HillshadeMode::Multi) {
        int sum = 0;
        for (double az : azs)
          sum += shadeOne(dzdx, dzdy, zenith, azMath(az));
        v = static_cast<std::uint8_t>(sum / 4);
      } else {
        v = shadeOne(dzdx, dzdy, zenith, azMath(opt.azimuthDeg));
      }
      (*outGray)[static_cast<size_t>(y) * w + x] = v;
    }
  }
}

void slopeDegrees(const std::vector<float>& z, const RasterInfo& info, double zFactor,
                  std::vector<float>* outDeg) {
  const int w = info.width;
  const int h = info.height;
  outDeg->assign(static_cast<size_t>(w) * h, 0.0f);
  const double xres = info.geotransform[1] != 0.0 ? info.geotransform[1] : 1.0;
  const double yres = info.geotransform[5] != 0.0 ? info.geotransform[5] : -1.0;
#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
#endif
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      double dzdx = 0, dzdy = 0;
      bool ok = false;
      hornDeriv(z, w, h, x, y, xres, yres, info.noData, info.hasNoData, zFactor, &dzdx, &dzdy, &ok);
      if (!ok)
        continue;
      const double deg = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy)) * 180.0 / M_PI;
      (*outDeg)[static_cast<size_t>(y) * w + x] = static_cast<float>(deg);
    }
  }
}

bool writeByteGeoTiff(const QString& outPath, const RasterInfo& info,
                      const std::vector<std::uint8_t>& gray, QString* errorOut) {
  GDALAllRegister();
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!drv) {
    if (errorOut)
      *errorOut = QStringLiteral("GTiff 드라이버 없음");
    return false;
  }
  GDALDataset* ds = drv->Create(outPath.toUtf8().constData(), info.width, info.height, 1, GDT_Byte,
                                nullptr);
  if (!ds) {
    if (errorOut)
      *errorOut = QStringLiteral("GeoTIFF를 만들 수 없습니다.");
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(info.geotransform));
  if (!info.projectionWkt.isEmpty())
    ds->SetProjection(info.projectionWkt.toUtf8().constData());
  GDALRasterBand* band = ds->GetRasterBand(1);
  band->SetNoDataValue(0);
  const CPLErr err =
      band->RasterIO(GF_Write, 0, 0, info.width, info.height,
                     const_cast<std::uint8_t*>(gray.data()), info.width, info.height, GDT_Byte, 0, 0);
  GDALClose(ds);
  if (err != CE_None) {
    if (errorOut)
      *errorOut = QStringLiteral("GeoTIFF 쓰기 실패");
    return false;
  }
  return true;
}

bool runHillshadeFile(const QString& demPath, const QString& outPath, const Options& opt,
                      QString* errorOut) {
  std::vector<float> z;
  RasterInfo info;
  if (!readFloatBand(demPath, &z, &info, errorOut))
    return false;
  std::vector<std::uint8_t> gray;
  hillshadeHorn(z, info, opt, &gray);
  return writeByteGeoTiff(outPath, info, gray, errorOut);
}

}  // namespace DemAnalyzer
