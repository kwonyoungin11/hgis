#include "TrenchGridGenerator.h"

#include <cmath>
#include <cstring>
#include <unordered_set>

#include <gdal.h>
#include <ogr_api.h>
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

namespace {

OGRGeometry* geomFromWkb(const QByteArray& wkb) {
  if (wkb.isEmpty())
    return nullptr;
  OGRGeometry* g = nullptr;
  OGRGeometryFactory::createFromWkb(wkb.constData(), nullptr, &g, static_cast<size_t>(wkb.size()));
  return g;
}

QByteArray wkbFromGeom(OGRGeometry* g) {
  QByteArray out;
  if (!g)
    return out;
  out.resize(static_cast<int>(g->WkbSize()));
  g->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(out.data()));
  return out;
}

}  // namespace

PickedArea pickAutoFillArea(const std::vector<SurveyPoly>& features,
                            const std::vector<qint64>& selectedFids) {
  PickedArea out;
  std::vector<const SurveyPoly*> valid;
  valid.reserve(features.size());
  for (const SurveyPoly& f : features) {
    if (f.wkb.isEmpty())
      continue;
    valid.push_back(&f);
  }
  out.totalCount = static_cast<int>(valid.size());
  if (valid.empty())
    return out;

  std::vector<const SurveyPoly*> use;
  const std::unordered_set<qint64> sel(selectedFids.begin(), selectedFids.end());
  if (!sel.empty()) {
    for (const SurveyPoly* f : valid) {
      if (sel.count(f->fid))
        use.push_back(f);
    }
    if (!use.empty())
      out.usedSelection = true;
  }
  if (use.empty()) {
    const SurveyPoly* newest = valid.front();
    for (const SurveyPoly* f : valid) {
      if (f->fid > newest->fid)
        newest = f;
    }
    use.push_back(newest);
    out.usedSelection = false;
  }
  out.usedCount = static_cast<int>(use.size());

  OGRGeometry* acc = nullptr;
  for (const SurveyPoly* f : use) {
    OGRGeometry* g = geomFromWkb(f->wkb);
    if (!g)
      continue;
    if (!acc) {
      acc = g;
      continue;
    }
    OGRGeometry* uni = acc->Union(g);
    OGRGeometryFactory::destroyGeometry(acc);
    OGRGeometryFactory::destroyGeometry(g);
    acc = uni;
  }
  if (!acc)
    return out;
  out.areaM2 = OGR_G_Area(OGRGeometry::ToHandle(acc));
  out.wkb = wkbFromGeom(acc);
  OGRGeometryFactory::destroyGeometry(acc);
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

  auto collect = [&](bool centroidOk) {
    int n = 1;
    std::vector<Cell> got;
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
        if (centroidOk) {
          OGRPoint mid((p0.first + p2.first) * 0.5, (p0.second + p2.second) * 0.5);
          if (!area->Contains(&mid))
            continue;
        } else if (!area->Contains(&poly)) {
          continue;
        }
        Cell cell;
        cell.name = spec.namePrefix + QString::number(n++);
        cell.width = w;
        cell.length = len;
        cell.ring[0] = p0;
        cell.ring[1] = p1;
        cell.ring[2] = p2;
        cell.ring[3] = p3;
        cell.ring[4] = p0;
        got.push_back(cell);
      }
    }
    return got;
  };

  out = collect(false);
  if (out.empty())
    out = collect(true);
  OGRGeometryFactory::destroyGeometry(area);
  return out;
}

RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width) {
  RatioFill best;
  const double w = std::abs(width);
  if (areaWkb.isEmpty() || !(w > 0.0) || !(targetPct > 0.0))
    return best;

  OGRGeometry* area = geomFromWkb(areaWkb);
  if (!area)
    return best;
  const double areaM2 = OGR_G_Area(OGRGeometry::ToHandle(area));
  OGRGeometryFactory::destroyGeometry(area);
  if (!(areaM2 > 1e-6))
    return best;
  best.areaM2 = areaM2;

  Spec s;
  s.trenchWidth = w;
  s.azimuthDeg = 0.0;
  s.namePrefix = targetPct <= 3.5 ? QStringLiteral("Sp-") : QStringLiteral("Tr-");

  double bestScore = 1e300;
  const double lengths[] = {8.0, 10.0, 12.0, 16.0, 20.0, 24.0, 30.0, 40.0};
  for (double len : lengths) {
    s.trenchLength = len;
    for (double balk = 2.0; balk <= 120.0; balk += 2.0) {
      s.balkWidth = balk;
      auto cells = buildInArea(s, areaWkb);
      if (cells.empty())
        continue;
      const double pct = totalArea(cells) / areaM2 * 100.0;
      double score = std::abs(pct - targetPct);
      if (pct > targetPct + 1.0)
        score += (pct - targetPct) * 2.0;
      if (score < bestScore) {
        bestScore = score;
        best.cells = std::move(cells);
        best.length = len;
        best.balk = balk;
        best.ratioPct = pct;
      }
      if (bestScore < 0.12)
        return best;
    }
  }
  return best;
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
