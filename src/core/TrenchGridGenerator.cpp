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

// 가장자리에서 잘린 트렌치가 이보다 짧으면 조사에 쓸모가 없어 버린다.
constexpr double kMinTrimLen = 4.0;

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

// 폴리곤/멀티폴리곤의 모든 꼭짓점을 훑는다(교차 결과의 국소 좌표 범위를 잴 때).
template <typename F>
void forEachVertex(const OGRGeometry* g, F&& fn) {
  if (!g) return;
  const OGRwkbGeometryType t = wkbFlatten(g->getGeometryType());
  if (t == wkbPolygon) {
    const auto* poly = g->toPolygon();
    for (const OGRLinearRing* ring : *poly)
      for (const OGRPoint& pt : *ring) fn(pt.getX(), pt.getY());
  } else if (t == wkbMultiPolygon || t == wkbGeometryCollection) {
    const auto* coll = g->toGeometryCollection();
    for (const OGRGeometry* part : *coll) forEachVertex(part, fn);
  }
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
        double keepV0 = v0;
        double keepLen = len;
        if (centroidOk) {
          OGRPoint mid((p0.first + p2.first) * 0.5, (p0.second + p2.second) * 0.5);
          if (!area->Contains(&mid))
            continue;
        } else if (!area->Contains(&poly)) {
          // 경계에 걸치면 통째로 버리지 않는다. 그러면 가장자리가 비어 배치가
          // 고르지 않다. 폭 2 m는 그대로 두고 길이만 잘라 구역 안에 넣는다.
          if (!area->Intersects(&poly))
            continue;
          OGRGeometry* inter = area->Intersection(&poly);
          if (!inter || inter->IsEmpty()) {
            if (inter) OGRGeometryFactory::destroyGeometry(inter);
            continue;
          }
          double loV = 1e300, hiV = -1e300;
          forEachVertex(inter, [&](double wx, double wy) {
            const double lv = toLocal(wx, wy).second;
            loV = std::min(loV, lv);
            hiV = std::max(hiV, lv);
          });
          OGRGeometryFactory::destroyGeometry(inter);
          loV = std::max(loV, v0);
          hiV = std::min(hiV, v0 + len);
          double cut = hiV - loV;
          if (!(cut >= kMinTrimLen) || cut > len)
            continue;
          // 교차 상자는 모서리에서 조금 넘칠 수 있다. 들어갈 때까지 양끝을 줄인다.
          bool fits = false;
          for (int attempt = 0; attempt < 4 && cut >= kMinTrimLen; ++attempt) {
            const auto q0 = world(u0, loV);
            const auto q1 = world(u0 + w, loV);
            const auto q2 = world(u0 + w, loV + cut);
            const auto q3 = world(u0, loV + cut);
            OGRLinearRing tr;
            tr.addPoint(q0.first, q0.second);
            tr.addPoint(q1.first, q1.second);
            tr.addPoint(q2.first, q2.second);
            tr.addPoint(q3.first, q3.second);
            tr.addPoint(q0.first, q0.second);
            OGRPolygon tp;
            tp.addRing(&tr);
            if (area->Contains(&tp)) {
              fits = true;
              break;
            }
            const double shrink = std::max(0.25, cut * 0.05);
            loV += shrink * 0.5;
            cut -= shrink;
          }
          if (!fits || cut < kMinTrimLen)
            continue;
          keepV0 = loV;
          keepLen = cut;
        }
        const auto k0 = world(u0, keepV0);
        const auto k1 = world(u0 + w, keepV0);
        const auto k2 = world(u0 + w, keepV0 + keepLen);
        const auto k3 = world(u0, keepV0 + keepLen);
        Cell cell;
        cell.name = spec.namePrefix + QString::number(n++);
        cell.width = w;
        cell.length = keepLen;
        cell.ring[0] = k0;
        cell.ring[1] = k1;
        cell.ring[2] = k2;
        cell.ring[3] = k3;
        cell.ring[4] = k0;
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

SlopeAspect upslopeAspect(const std::vector<ElevSample>& samples, double minSlopePct) {
  SlopeAspect out;
  if (samples.size() < 3)
    return out;

  // 표본에 z = a·x + b·y + c 평면을 최소제곱으로 맞춘다. 중심을 빼서 좌표가
  // 큰 5186/5187 미터값이어도 정규방정식이 수치적으로 무너지지 않게 한다.
  double mx = 0.0, my = 0.0, mz = 0.0;
  for (const ElevSample& p : samples) {
    mx += p.x;
    my += p.y;
    mz += p.z;
  }
  const double n = static_cast<double>(samples.size());
  mx /= n;
  my /= n;
  mz /= n;

  double sxx = 0.0, sxy = 0.0, syy = 0.0, sxz = 0.0, syz = 0.0;
  for (const ElevSample& p : samples) {
    const double dx = p.x - mx;
    const double dy = p.y - my;
    const double dz = p.z - mz;
    sxx += dx * dx;
    sxy += dx * dy;
    syy += dy * dy;
    sxz += dx * dz;
    syz += dy * dz;
  }
  const double det = sxx * syy - sxy * sxy;
  if (std::abs(det) < 1e-9)
    return out;  // 표본이 한 줄로 서 있으면 기울기를 못 정한다.

  const double a = (sxz * syy - syz * sxy) / det;  // dz/dx
  const double b = (syz * sxx - sxz * sxy) / det;  // dz/dy

  const double grad = std::hypot(a, b);
  out.slopePct = grad * 100.0;
  if (out.slopePct < minSlopePct)
    return out;  // 평지 — 방향을 정할 근거가 없다.

  // (a, b)는 오르막을 가리킨다. 방위는 북(+y) 기준 시계 방향이므로 atan2(동, 북).
  double az = std::atan2(a, b) * 180.0 / M_PI;
  if (az < 0.0)
    az += 360.0;
  out.azimuthDeg = az;
  out.valid = true;
  return out;
}

RatioFill buildForTargetRatio(const QByteArray& areaWkb, double targetPct, double width,
                              double azimuthDeg) {
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
  s.azimuthDeg = azimuthDeg;
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
        best.azimuthDeg = azimuthDeg;
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
