#include "TrenchGridGenerator.h"

#include <cmath>
#include <cstring>

#include <gdal.h>
#include <ogr_feature.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <ogrsf_frmts.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TrenchGridGenerator {

std::vector<Cell> build(const Spec& spec) {
  std::vector<Cell> out;
  const int rows = std::max(0, spec.rows);
  const int cols = std::max(0, spec.cols);
  const double w = std::abs(spec.trenchWidth);
  const double len = std::abs(spec.trenchLength);
  const double balk = std::max(0.0, spec.balkWidth);
  if (rows <= 0 || cols <= 0 || !(w > 0.0) || !(len > 0.0))
    return out;

  // Azimuth clockwise from north. Local +X = east, +Y = north at azimuth 0.
  const double a = spec.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(a);
  const double s = std::sin(a);
  auto world = [&](double lx, double ly) {
    // clockwise about origin: x' = x cos + y sin, y' = -x sin + y cos
    return std::pair<double, double>{lx * c + ly * s + spec.originX,
                                     -lx * s + ly * c + spec.originY};
  };

  out.reserve(static_cast<size_t>(rows) * cols);
  int n = 1;
  for (int r = 0; r < rows; ++r) {
    for (int col = 0; col < cols; ++col) {
      const double x0 = col * (w + balk);
      const double y0 = r * (len + balk);
      Cell cell;
      cell.name = spec.namePrefix + QString::number(n++);
      cell.width = w;
      cell.length = len;
      const auto p0 = world(x0, y0);
      const auto p1 = world(x0 + w, y0);
      const auto p2 = world(x0 + w, y0 + len);
      const auto p3 = world(x0, y0 + len);
      cell.ring[0] = p0;
      cell.ring[1] = p1;
      cell.ring[2] = p2;
      cell.ring[3] = p3;
      cell.ring[4] = p0;
      out.push_back(cell);
    }
  }
  return out;
}

std::vector<Cell> buildInArea(const Spec& spec, const QByteArray& areaWkb) {
  std::vector<Cell> out;
  const double w = std::abs(spec.trenchWidth);
  const double len = std::abs(spec.trenchLength);
  const double balk = std::max(0.0, spec.balkWidth);
  if (areaWkb.isEmpty() || !(w > 0.0) || !(len > 0.0))
    return out;

  OGRGeometry* area = nullptr;
  OGRGeometryFactory::createFromWkb(areaWkb.constData(), nullptr, &area,
                                    static_cast<size_t>(areaWkb.size()));
  if (!area)
    return out;
  const OGRwkbGeometryType gt = wkbFlatten(area->getGeometryType());
  if (gt != wkbPolygon && gt != wkbMultiPolygon) {
    OGRGeometryFactory::destroyGeometry(area);
    return out;
  }

  OGREnvelope env;
  area->getEnvelope(&env);
  const double ax = (env.MinX + env.MaxX) * 0.5;
  const double ay = (env.MinY + env.MaxY) * 0.5;

  const double a = spec.azimuthDeg * M_PI / 180.0;
  const double c = std::cos(a);
  const double s = std::sin(a);
  auto world = [&](double lx, double ly) {
    return std::pair<double, double>{lx * c + ly * s + ax, -lx * s + ly * c + ay};
  };
  auto toLocal = [&](double wx, double wy) {
    const double dx = wx - ax;
    const double dy = wy - ay;
    return std::pair<double, double>{dx * c - dy * s, dx * s + dy * c};
  };

  // Local-frame bounds of the (rotated) envelope corners; superset of the area.
  double uMin = 0, uMax = 0, vMin = 0, vMax = 0;
  bool first = true;
  const double cxs[2] = {env.MinX, env.MaxX};
  const double cys[2] = {env.MinY, env.MaxY};
  for (double cx : cxs) {
    for (double cy : cys) {
      const auto l = toLocal(cx, cy);
      if (first) {
        uMin = uMax = l.first;
        vMin = vMax = l.second;
        first = false;
      } else {
        uMin = std::min(uMin, l.first);
        uMax = std::max(uMax, l.first);
        vMin = std::min(vMin, l.second);
        vMax = std::max(vMax, l.second);
      }
    }
  }

  const double stepU = w + balk;
  const double stepV = len + balk;
  const long long nu = static_cast<long long>((uMax - uMin) / stepU) + 2;
  const long long nv = static_cast<long long>((vMax - vMin) / stepV) + 2;
  if (nu <= 0 || nv <= 0 || nu * nv > 200000) {
    OGRGeometryFactory::destroyGeometry(area);
    return out;
  }

  int n = 1;
  for (long long j = 0; j < nv; ++j) {
    const double v0 = vMin + j * stepV;
    for (long long i = 0; i < nu; ++i) {
      const double u0 = uMin + i * stepU;
      const auto p0 = world(u0, v0);
      const auto p1 = world(u0 + w, v0);
      const auto p2 = world(u0 + w, v0 + len);
      const auto p3 = world(u0, v0 + len);
      OGRLinearRing ring;
      ring.addPoint(p0.first, p0.second);
      ring.addPoint(p1.first, p1.second);
      ring.addPoint(p2.first, p2.second);
      ring.addPoint(p3.first, p3.second);
      ring.addPoint(p0.first, p0.second);
      OGRPolygon poly;
      poly.addRing(&ring);
      if (!area->Contains(&poly))
        continue;
      Cell cell;
      cell.name = spec.namePrefix + QString::number(n++);
      cell.width = w;
      cell.length = len;
      cell.ring[0] = p0;
      cell.ring[1] = p1;
      cell.ring[2] = p2;
      cell.ring[3] = p3;
      cell.ring[4] = p0;
      out.push_back(cell);
    }
  }
  OGRGeometryFactory::destroyGeometry(area);
  return out;
}

double totalArea(const std::vector<Cell>& cells) {
  double sum = 0.0;
  for (const Cell& c : cells)
    sum += c.width * c.length;
  return sum;
}

bool writeGpkg(const QString& gpkgPath, const QString& layerName, const std::vector<Cell>& cells,
               const QString& authid, QString* errorOut) {
  GDALAllRegister();
  if (gpkgPath.isEmpty() || layerName.isEmpty()) {
    if (errorOut)
      *errorOut = QStringLiteral("GPKG 경로가 없습니다.");
    return false;
  }

  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr));
  if (!ds) {
    if (errorOut)
      *errorOut = QStringLiteral("GPKG를 열 수 없습니다. 먼저 새 조사를 만드세요.");
    return false;
  }

  OGRLayer* lyr = ds->GetLayerByName(layerName.toUtf8().constData());
  if (!lyr) {
    OGRSpatialReference srs;
    srs.SetFromUserInput(authid.toUtf8().constData());
    lyr = ds->CreateLayer(layerName.toUtf8().constData(), &srs, wkbPolygon, nullptr);
    if (lyr) {
      OGRFieldDefn fName("name", OFTString);
      fName.SetWidth(64);
      lyr->CreateField(&fName);
      OGRFieldDefn fW("width", OFTReal);
      lyr->CreateField(&fW);
      OGRFieldDefn fL("length", OFTReal);
      lyr->CreateField(&fL);
    }
  }
  if (!lyr) {
    GDALClose(ds);
    if (errorOut)
      *errorOut = QStringLiteral("시굴격자 레이어를 만들 수 없습니다.");
    return false;
  }

  const int iName = lyr->FindFieldIndex("name", TRUE);
  const int iW = lyr->FindFieldIndex("width", TRUE);
  const int iL = lyr->FindFieldIndex("length", TRUE);

  for (const Cell& cell : cells) {
    OGRLinearRing ring;
    for (const auto& p : cell.ring)
      ring.addPoint(p.first, p.second);
    OGRPolygon poly;
    poly.addRing(&ring);
    OGRFeature feat(lyr->GetLayerDefn());
    feat.SetGeometry(&poly);
    if (iName >= 0)
      feat.SetField(iName, cell.name.toUtf8().constData());
    if (iW >= 0)
      feat.SetField(iW, cell.width);
    if (iL >= 0)
      feat.SetField(iL, cell.length);
    if (lyr->CreateFeature(&feat) != OGRERR_NONE) {
      GDALClose(ds);
      if (errorOut)
        *errorOut = QStringLiteral("트렌치를 쓰지 못했습니다: %1").arg(cell.name);
      return false;
    }
  }
  lyr->SyncToDisk();
  GDALClose(ds);
  return true;
}

bool clearLayer(const QString& gpkgPath, const QString& layerName, QString* errorOut) {
  GDALAllRegister();
  if (gpkgPath.isEmpty() || layerName.isEmpty())
    return true;
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr));
  if (!ds)
    return true;  // no file yet -> nothing to clear
  OGRLayer* lyr = ds->GetLayerByName(layerName.toUtf8().constData());
  bool ok = true;
  if (lyr) {
    std::vector<GIntBig> fids;
    lyr->ResetReading();
    while (OGRFeature* f = lyr->GetNextFeature()) {
      fids.push_back(f->GetFID());
      OGRFeature::DestroyFeature(f);
    }
    for (GIntBig fid : fids) {
      if (lyr->DeleteFeature(fid) != OGRERR_NONE) {
        ok = false;
        if (errorOut)
          *errorOut = QStringLiteral("기존 시굴격자를 지우지 못했습니다.");
        break;
      }
    }
    lyr->SyncToDisk();
  }
  GDALClose(ds);
  return ok;
}

}  // namespace TrenchGridGenerator
