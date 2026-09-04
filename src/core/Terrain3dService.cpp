#include "Terrain3dService.h"
#include "DemAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QVector3D>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr double kDeg2Rad = M_PI / 180.0;

double niceMeters(double raw) {
  if (!(raw > 0.0) || !std::isfinite(raw))
    return 10.0;
  const double expv = std::pow(10.0, std::floor(std::log10(raw)));
  const double n = raw / expv;
  double f = 1.0;
  if (n <= 1.5)
    f = 1.0;
  else if (n <= 3.5)
    f = 2.0;
  else if (n <= 7.5)
    f = 5.0;
  else
    f = 10.0;
  return f * expv;
}

QPointF projectPoint(const QVector3D& p, const QVector3D& eye, const QVector3D& right,
                     const QVector3D& up, const QVector3D& fwd, float fov, int w, int h) {
  const QVector3D rel = p - eye;
  const float x = QVector3D::dotProduct(rel, right);
  const float y = QVector3D::dotProduct(rel, up);
  const float z = QVector3D::dotProduct(rel, fwd);
  if (z < 0.4f)
    return QPointF(-1e6, -1e6);
  const float s = static_cast<float>(h) / (2.0f * std::tan(fov * 0.5f) * z);
  return QPointF(w * 0.5 + x * s, h * 0.5 - y * s);
}

QRgb sampleTex(const QImage& tex, float u, float v) {
  if (tex.isNull() || tex.width() < 1 || tex.height() < 1)
    return qRgb(140, 150, 140);
  const int x = std::clamp(int(u * (tex.width() - 1)), 0, tex.width() - 1);
  const int y = std::clamp(int((1.0f - v) * (tex.height() - 1)), 0, tex.height() - 1);
  return tex.pixel(x, y);
}

float paperShade(const QVector3D& nrm, const QVector3D& light) {
  return std::clamp(0.86f + 0.14f * std::max(0.0f, QVector3D::dotProduct(nrm, light)), 0.82f,
                    1.0f);
}

QRgb sampleTexBilinear(const QImage& tex, float u, float v) {
  if (tex.isNull() || tex.width() < 2 || tex.height() < 2)
    return sampleTex(tex, u, v);
  const float fx = std::clamp(u, 0.0f, 1.0f) * (tex.width() - 1);
  const float fy = std::clamp(1.0f - v, 0.0f, 1.0f) * (tex.height() - 1);
  const int x0 = static_cast<int>(fx);
  const int y0 = static_cast<int>(fy);
  const int x1 = std::min(tex.width() - 1, x0 + 1);
  const int y1 = std::min(tex.height() - 1, y0 + 1);
  const float tx = fx - x0;
  const float ty = fy - y0;
  const QRgb a = tex.pixel(x0, y0);
  const QRgb b = tex.pixel(x1, y0);
  const QRgb c = tex.pixel(x0, y1);
  const QRgb d = tex.pixel(x1, y1);
  const float r0 = qRed(a) * (1 - tx) + qRed(b) * tx;
  const float g0 = qGreen(a) * (1 - tx) + qGreen(b) * tx;
  const float b0 = qBlue(a) * (1 - tx) + qBlue(b) * tx;
  const float r1 = qRed(c) * (1 - tx) + qRed(d) * tx;
  const float g1 = qGreen(c) * (1 - tx) + qGreen(d) * tx;
  const float b1 = qBlue(c) * (1 - tx) + qBlue(d) * tx;
  return qRgb(int(r0 * (1 - ty) + r1 * ty), int(g0 * (1 - ty) + g1 * ty),
              int(b0 * (1 - ty) + b1 * ty));
}

bool wktLooksGeographic(const QString& wkt) {
  return wkt.contains(QLatin1String("GEOGCS"), Qt::CaseInsensitive) ||
         wkt.contains(QLatin1String("EPSG:4326"), Qt::CaseInsensitive);
}

bool fillSceneFromInfo(Terrain3dService::DemScene* out, const DemAnalyzer::RasterInfo& info,
                       QString* errorOut) {
  out->width = info.width;
  out->height = info.height;
  for (int i = 0; i < 6; ++i)
    out->geotransform[i] = info.geotransform[i];
  if (out->width < 2 || out->height < 2 || out->z.size() < 4) {
    if (errorOut) *errorOut = QStringLiteral("DEM이 너무 작습니다.");
    return false;
  }
  out->zMin = out->z[0];
  out->zMax = out->z[0];
  for (float v : out->z) {
    if (!std::isfinite(v)) continue;
    if (info.hasNoData && std::abs(v - info.noData) < 1e-4f) continue;
    out->zMin = std::min(out->zMin, v);
    out->zMax = std::max(out->zMax, v);
  }
  double xMin = 0, yMin = 0, xMax = 0, yMax = 0;
  if (!Terrain3dService::demWorldRect(*out, &xMin, &yMin, &xMax, &yMax)) {
    xMin = info.geotransform[0];
    yMin = info.geotransform[3] + info.geotransform[5] * info.height;
    xMax = info.geotransform[0] + info.geotransform[1] * info.width;
    yMax = info.geotransform[3];
  }
  double w = std::abs(xMax - xMin);
  double h = std::abs(yMax - yMin);
  if (wktLooksGeographic(info.projectionWkt)) {
    const double lat = ((yMin + yMax) * 0.5) * kDeg2Rad;
    w *= 111320.0 * std::max(0.2, std::cos(lat));
    h *= 110540.0;
  }
  out->groundWidthM = w;
  out->groundHeightM = h;
  out->centerX = (xMin + xMax) * 0.5;
  out->centerY = (yMin + yMax) * 0.5;
  out->projectionWkt = info.projectionWkt;
  return true;
}

}  // namespace

namespace Terrain3dService {

bool loadDem(const QString& path, DemScene* out, QString* errorOut) {
  if (!out) {
    if (errorOut) *errorOut = QStringLiteral("버퍼가 없습니다.");
    return false;
  }
  DemAnalyzer::RasterInfo info;
  if (!DemAnalyzer::readFloatBand(path, &out->z, &info, errorOut))
    return false;
  return fillSceneFromInfo(out, info, errorOut);
}

bool loadDemClip(const QString& path, double xMin, double yMin, double xMax, double yMax,
                 int maxEdge, DemScene* out, QString* errorOut) {
  if (!out) {
    if (errorOut) *errorOut = QStringLiteral("버퍼가 없습니다.");
    return false;
  }
  DemAnalyzer::RasterInfo info;
  if (!DemAnalyzer::readFloatWindow(path, xMin, yMin, xMax, yMax, maxEdge, &out->z, &info,
                                    errorOut))
    return false;
  return fillSceneFromInfo(out, info, errorOut);
}

Mesh buildMesh(const DemScene& scene, int maxEdge, float zExaggeration) {
  Mesh m;
  if (scene.width < 2 || scene.height < 2 || scene.z.empty())
    return m;
  const int stepX = std::max(1, scene.width / std::max(2, maxEdge));
  const int stepY = std::max(1, scene.height / std::max(2, maxEdge));
  const int cols = (scene.width - 1) / stepX + 1;
  const int rows = (scene.height - 1) / stepY + 1;
  const double* gt = scene.geotransform;
  const float zf = zExaggeration > 0.05f ? zExaggeration : 1.0f;
  m.positions.reserve(cols * rows * 3);
  m.uvs.reserve(cols * rows * 2);
  for (int r = 0; r < rows; ++r) {
    const int iy = std::min(scene.height - 1, r * stepY);
    for (int c = 0; c < cols; ++c) {
      const int ix = std::min(scene.width - 1, c * stepX);
      const double gx = gt[0] + gt[1] * (ix + 0.5) + gt[2] * (iy + 0.5);
      const double gy = gt[3] + gt[4] * (ix + 0.5) + gt[5] * (iy + 0.5);
      float z = scene.z[static_cast<size_t>(iy) * scene.width + ix];
      if (!std::isfinite(z))
        z = scene.zMin;
      double mx = gx - scene.centerX;
      double my = gy - scene.centerY;
      if (wktLooksGeographic(scene.projectionWkt)) {
        const double lat = scene.centerY * kDeg2Rad;
        mx *= 111320.0 * std::max(0.2, std::cos(lat));
        my *= 110540.0;
      }
      m.positions.append(static_cast<float>(mx));
      m.positions.append(static_cast<float>(my));
      m.positions.append((z - scene.zMin) * zf);
      m.uvs.append(static_cast<float>(ix) / std::max(1, scene.width - 1));
      m.uvs.append(1.0f - static_cast<float>(iy) / std::max(1, scene.height - 1));
    }
  }
  m.indices.reserve((rows - 1) * (cols - 1) * 6);
  for (int r = 0; r < rows - 1; ++r) {
    for (int c = 0; c < cols - 1; ++c) {
      const unsigned i0 = static_cast<unsigned>(r * cols + c);
      const unsigned i1 = i0 + 1;
      const unsigned i2 = i0 + static_cast<unsigned>(cols);
      const unsigned i3 = i2 + 1;
      m.indices << i0 << i2 << i1 << i1 << i2 << i3;
    }
  }
  m.zMin = 0;
  m.zMax = (scene.zMax - scene.zMin) * zf;
  m.groundWidthM = scene.groundWidthM;
  m.groundHeightM = scene.groundHeightM;
  return m;
}

QImage hillshadeTexture(const DemScene& scene) {
  if (scene.width < 2 || scene.z.empty())
    return {};
  DemAnalyzer::RasterInfo info;
  info.width = scene.width;
  info.height = scene.height;
  for (int i = 0; i < 6; ++i)
    info.geotransform[i] = scene.geotransform[i];
  std::vector<std::uint8_t> gray;
  DemAnalyzer::hillshadeHorn(scene.z, info, DemAnalyzer::Options{}, &gray);
  if (gray.size() != static_cast<size_t>(scene.width * scene.height))
    return {};
  QImage img(scene.width, scene.height, QImage::Format_RGB32);
  for (int y = 0; y < scene.height; ++y) {
    for (int x = 0; x < scene.width; ++x) {
      const int g = gray[static_cast<size_t>(y) * scene.width + x];
      img.setPixel(x, y, qRgb(g, g, g));
    }
  }
  return img;
}

QImage renderPerspective(const Mesh& mesh, const QImage& texture, int pixelW, int pixelH,
                         float yawDeg, float pitchDeg, float distance) {
  QImage img(std::max(64, pixelW), std::max(64, pixelH), QImage::Format_RGB32);
  img.fill(QColor(255, 255, 255));
  if (mesh.positions.size() < 9 || mesh.indices.size() < 3)
    return img;

  const int n = mesh.positions.size() / 3;
  float maxR = 8.0f;
  for (int i = 0; i < n; ++i) {
    const QVector3D p(mesh.positions[i * 3], mesh.positions[i * 3 + 1],
                      mesh.positions[i * 3 + 2]);
    maxR = std::max(maxR, p.length());
  }
  const float yaw = yawDeg * static_cast<float>(kDeg2Rad);
  const float pitch = std::clamp(pitchDeg, 8.0f, 80.0f) * static_cast<float>(kDeg2Rad);
  const QVector3D target(0, 0, mesh.zMax * 0.25f);
  QVector3D dir(std::sin(yaw) * std::cos(pitch), -std::cos(yaw) * std::cos(pitch),
                std::sin(pitch));
  if (dir.lengthSquared() < 1e-8f)
    dir = QVector3D(0.4f, -0.7f, 0.6f);
  dir.normalize();
  const float dist = std::clamp(distance, maxR * 0.55f, maxR * 14.0f);
  const QVector3D eye = target + dir * dist;
  QVector3D fwd = (target - eye);
  if (fwd.lengthSquared() < 1e-6f)
    fwd = QVector3D(0, 1, 0);
  fwd.normalize();
  QVector3D right = QVector3D::crossProduct(fwd, QVector3D(0, 0, 1));
  if (right.lengthSquared() < 1e-6f)
    right = QVector3D(1, 0, 0);
  right.normalize();
  const QVector3D up = QVector3D::crossProduct(right, fwd).normalized();
  const float fov = 50.0f * static_cast<float>(kDeg2Rad);
  QVector<QPointF> proj(n);
  QVector<float> depth(n);
  for (int i = 0; i < n; ++i) {
    const QVector3D p(mesh.positions[i * 3], mesh.positions[i * 3 + 1],
                      mesh.positions[i * 3 + 2]);
    proj[i] = projectPoint(p, eye, right, up, fwd, fov, img.width(), img.height());
    depth[i] = QVector3D::dotProduct(p - eye, fwd);
  }

  struct Face {
    int a, b, c;
    float d;
    float shade;
    QRgb col;
  };
  const QVector3D light = QVector3D(-0.45f, -0.35f, 0.82f).normalized();
  QVector<QVector3D> acc(n);
  for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const int a = static_cast<int>(mesh.indices[t]);
    const int b = static_cast<int>(mesh.indices[t + 1]);
    const int c = static_cast<int>(mesh.indices[t + 2]);
    if (a < 0 || b < 0 || c < 0 || a >= n || b >= n || c >= n)
      continue;
    const QVector3D pa(mesh.positions[a * 3], mesh.positions[a * 3 + 1],
                       mesh.positions[a * 3 + 2]);
    const QVector3D pb(mesh.positions[b * 3], mesh.positions[b * 3 + 1],
                       mesh.positions[b * 3 + 2]);
    const QVector3D pc(mesh.positions[c * 3], mesh.positions[c * 3 + 1],
                       mesh.positions[c * 3 + 2]);
    QVector3D nrm = QVector3D::crossProduct(pb - pa, pc - pa);
    if (nrm.lengthSquared() < 1e-10f)
      continue;
    acc[a] += nrm;
    acc[b] += nrm;
    acc[c] += nrm;
  }
  QVector<float> vshade(n, 0.92f);
  for (int i = 0; i < n; ++i) {
    QVector3D nrm = acc[i];
    if (nrm.lengthSquared() < 1e-12f)
      continue;
    nrm.normalize();
    vshade[i] = paperShade(nrm, light);
  }
  const float zSpan = std::max(1.0f, mesh.zMax - mesh.zMin);
  QVector<Face> faces;
  faces.reserve(mesh.indices.size() / 3);
  for (int t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const int a = static_cast<int>(mesh.indices[t]);
    const int b = static_cast<int>(mesh.indices[t + 1]);
    const int c = static_cast<int>(mesh.indices[t + 2]);
    if (a < 0 || b < 0 || c < 0 || a >= n || b >= n || c >= n)
      continue;
    if (proj[a].x() < -1e5 || proj[b].x() < -1e5 || proj[c].x() < -1e5)
      continue;
    const QVector3D pa(mesh.positions[a * 3], mesh.positions[a * 3 + 1],
                       mesh.positions[a * 3 + 2]);
    const QVector3D pb(mesh.positions[b * 3], mesh.positions[b * 3 + 1],
                       mesh.positions[b * 3 + 2]);
    const QVector3D pc(mesh.positions[c * 3], mesh.positions[c * 3 + 1],
                       mesh.positions[c * 3 + 2]);
    QVector3D nrm = QVector3D::crossProduct(pb - pa, pc - pa);
    if (nrm.lengthSquared() < 1e-10f)
      continue;
    const float shade = (vshade[a] + vshade[b] + vshade[c]) / 3.0f;
    const float u = (mesh.uvs[a * 2] + mesh.uvs[b * 2] + mesh.uvs[c * 2]) / 3.0f;
    const float v = (mesh.uvs[a * 2 + 1] + mesh.uvs[b * 2 + 1] + mesh.uvs[c * 2 + 1]) / 3.0f;
    const float ht = std::clamp(((pa.z() + pb.z() + pc.z()) / 3.0f - mesh.zMin) / zSpan, 0.0f, 1.0f);
    QRgb tex = sampleTex(texture, u, v);
    if (texture.isNull())
      tex = qRgb(int(70 + 90 * ht), int(110 + 70 * ht), int(70 + 40 * (1.0f - ht)));
    Face f;
    f.a = a;
    f.b = b;
    f.c = c;
    f.d = (depth[a] + depth[b] + depth[c]) / 3.0f;
    f.shade = shade;
    f.col = qRgb(int(qRed(tex) * shade), int(qGreen(tex) * shade), int(qBlue(tex) * shade));
    faces.append(f);
  }
  std::sort(faces.begin(), faces.end(), [](const Face& l, const Face& r) { return l.d > r.d; });

  if (!texture.isNull() && mesh.uvs.size() >= n * 2) {
    const int iw = img.width();
    const int ih = img.height();
    std::vector<float> zbuf(static_cast<size_t>(iw) * static_cast<size_t>(ih), 1e9f);
    for (const Face& f : faces) {
      const QPointF pa = proj[f.a];
      const QPointF pb = proj[f.b];
      const QPointF pc = proj[f.c];
      const float ua = mesh.uvs[f.a * 2];
      const float va = mesh.uvs[f.a * 2 + 1];
      const float ub = mesh.uvs[f.b * 2];
      const float vb = mesh.uvs[f.b * 2 + 1];
      const float uc = mesh.uvs[f.c * 2];
      const float vc = mesh.uvs[f.c * 2 + 1];
      const float da = depth[f.a];
      const float db = depth[f.b];
      const float dc = depth[f.c];
      const float ax = static_cast<float>(pa.x());
      const float ay = static_cast<float>(pa.y());
      const float bx = static_cast<float>(pb.x());
      const float by = static_cast<float>(pb.y());
      const float cx = static_cast<float>(pc.x());
      const float cy = static_cast<float>(pc.y());
      const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
      if (std::abs(area) < 1e-3f)
        continue;
      const int minx = std::clamp(int(std::floor(std::min({ax, bx, cx}))), 0, iw - 1);
      const int maxx = std::clamp(int(std::ceil(std::max({ax, bx, cx}))), 0, iw - 1);
      const int miny = std::clamp(int(std::floor(std::min({ay, by, cy}))), 0, ih - 1);
      const int maxy = std::clamp(int(std::ceil(std::max({ay, by, cy}))), 0, ih - 1);
      const float invArea = 1.0f / area;
      for (int y = miny; y <= maxy; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = minx; x <= maxx; ++x) {
          const float px = static_cast<float>(x) + 0.5f;
          const float py = static_cast<float>(y) + 0.5f;
          const float a1 = ((px - ax) * (cy - ay) - (cx - ax) * (py - ay)) * invArea;
          const float a2 = ((bx - ax) * (py - ay) - (px - ax) * (by - ay)) * invArea;
          const float a0 = 1.0f - a1 - a2;
          if (a0 < 0.0f || a1 < 0.0f || a2 < 0.0f)
            continue;
          const float z = a0 * da + a1 * db + a2 * dc;
          float& zb = zbuf[static_cast<size_t>(y) * static_cast<size_t>(iw) + static_cast<size_t>(x)];
          if (z >= zb)
            continue;
          zb = z;
          const QRgb t = sampleTexBilinear(texture, a0 * ua + a1 * ub + a2 * uc,
                                           a0 * va + a1 * vb + a2 * vc);
          const float sh = a0 * vshade[f.a] + a1 * vshade[f.b] + a2 * vshade[f.c];
          row[x] = qRgb(int(qRed(t) * sh), int(qGreen(t) * sh), int(qBlue(t) * sh));
        }
      }
    }
    return img;
  }

  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setPen(Qt::NoPen);
  for (const Face& f : faces) {
    QPolygonF poly;
    poly << proj[f.a] << proj[f.b] << proj[f.c];
    p.setBrush(QColor(f.col));
    p.drawPolygon(poly);
  }
  p.end();
  return img;
}

double visibleWidthAtTarget(float distance, int pixelW, int pixelH) {
  const float fov = 50.0f * static_cast<float>(kDeg2Rad);
  const float z = std::max(8.0f, distance);
  const float w = static_cast<float>(std::max(64, pixelW));
  const float h = static_cast<float>(std::max(64, pixelH));
  return 2.0 * (w / h) * std::tan(fov * 0.5f) * z;
}

double distanceForVisibleWidth(double visibleWidthM, int pixelW, int pixelH) {
  const float fov = 50.0f * static_cast<float>(kDeg2Rad);
  const float w = static_cast<float>(std::max(64, pixelW));
  const float h = static_cast<float>(std::max(64, pixelH));
  const double den = 2.0 * (w / h) * std::tan(fov * 0.5f);
  if (!(den > 1e-9) || !std::isfinite(visibleWidthM) || visibleWidthM <= 0.0)
    return 8.0;
  return std::max(8.0, visibleWidthM / den);
}

double scaleBarSegmentM(double visibleWidthM) {
  const double span = visibleWidthM > 1.0 ? visibleWidthM : 100.0;
  return niceMeters(span * 0.28);
}

double northAzimuthDeg(const double geotransform[6]) {
  if (!geotransform)
    return 0.0;
  const double nx = -geotransform[2];
  const double ny = -geotransform[5];
  if (!(std::hypot(nx, ny) > 1e-12))
    return 0.0;
  return std::atan2(nx, ny) * (180.0 / M_PI);
}

QString pickLocalDemSource(const QVector<DemCandidate>& cands) {
  auto skipName = [](const QString& n) {
    return n.contains(QStringLiteral("위성")) || n.contains(QStringLiteral("Satellite"), Qt::CaseInsensitive) ||
           n.contains(QStringLiteral("지적")) || n.contains(QStringLiteral("음영"));
  };
  auto skipSrc = [](const QString& s) {
    return s.startsWith(QLatin1String("/vsicurl")) || s.contains(QLatin1String("http"));
  };
  auto prefer = [](const QString& n) {
    return n.contains(QStringLiteral("DEM"), Qt::CaseInsensitive) || n.contains(QStringLiteral("고도")) ||
           n.contains(QStringLiteral("표고")) || n.contains(QStringLiteral("elevation"), Qt::CaseInsensitive);
  };
  QVector<DemCandidate> named;
  QVector<DemCandidate> other;
  for (const DemCandidate& c : cands) {
    if (c.source.isEmpty() || skipSrc(c.source) || skipName(c.name))
      continue;
    if (prefer(c.name))
      named.append(c);
    else
      other.append(c);
  }
  if (!named.isEmpty())
    return named.front().source;
  if (other.size() == 1)
    return other.front().source;
  return {};
}

QString pickTerrainDemSource(const QVector<DemCandidate>& cands) {
  const QString local = pickLocalDemSource(cands);
  if (!local.isEmpty())
    return local;
  auto skipName = [](const QString& n) {
    return n.contains(QStringLiteral("위성")) || n.contains(QStringLiteral("Satellite"), Qt::CaseInsensitive) ||
           n.contains(QStringLiteral("지적")) || n.contains(QStringLiteral("음영"));
  };
  auto prefer = [](const QString& n) {
    return n.contains(QStringLiteral("DEM"), Qt::CaseInsensitive) || n.contains(QStringLiteral("고도")) ||
           n.contains(QStringLiteral("표고")) || n.contains(QStringLiteral("elevation"), Qt::CaseInsensitive);
  };
  for (const DemCandidate& c : cands) {
    if (c.source.isEmpty() || skipName(c.name) || !prefer(c.name))
      continue;
    if (c.source.startsWith(QLatin1String("/vsicurl")) || c.source.contains(QLatin1String("http")))
      return c.source;
  }
  return {};
}

QImage composeExport(const QImage& view, double visibleWidthM, double yawDegFromNorth) {
  const int barH = 36;
  QImage out(std::max(160, view.width()), view.height() + barH, QImage::Format_RGB32);
  out.fill(QColor(255, 255, 255));
  QPainter p(&out);
  p.drawImage(0, 0, view);
  p.fillRect(0, view.height(), out.width(), barH, QColor(255, 255, 255));
  p.setPen(QPen(QColor(17, 24, 39), 1.4));
  p.drawLine(0, view.height(), out.width(), view.height());

  const double span = visibleWidthM > 1.0 ? visibleWidthM : 100.0;
  const double seg = scaleBarSegmentM(span);
  const double pxPerM = (view.width() * 0.42) / std::max(seg * 2.0, 1.0);
  const double barW = seg * pxPerM;
  const int y0 = view.height() + 10;
  const int x0 = 14;
  p.setPen(QPen(QColor(17, 24, 39), 2.0));
  p.drawLine(x0, y0 + 10, x0 + int(barW), y0 + 10);
  p.drawLine(x0, y0 + 4, x0, y0 + 14);
  p.drawLine(x0 + int(barW), y0 + 4, x0 + int(barW), y0 + 14);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  p.setPen(QColor(17, 24, 39));
  p.drawText(x0, y0 + 8, QStringLiteral("0"));
  p.drawText(x0 + int(barW) - 6, y0 + 8, QStringLiteral("%1 m").arg(seg, 0, 'f', 0));

  const int nx = out.width() - 34;
  const int ny = view.height() + 18;
  p.save();
  p.translate(nx, ny);
  p.rotate(-yawDegFromNorth);
  p.setPen(QPen(QColor(17, 24, 39), 2.0));
  p.drawLine(0, 8, 0, -10);
  QPolygonF head;
  head << QPointF(0, -12) << QPointF(-4, -4) << QPointF(4, -4);
  p.setBrush(QColor(17, 24, 39));
  p.drawPolygon(head);
  p.restore();
  p.drawText(nx - 6, view.height() + 32, QStringLiteral("N"));
  p.end();
  return out;
}

QString googleSatelliteXyzUri() {
  return QStringLiteral(
      "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
      "&zmax=20&zmin=0&crs=EPSG:3857");
}

bool textureLooksFilled(const QImage& img) {
  if (img.isNull() || img.width() < 8 || img.height() < 8)
    return false;
  int filled = 0;
  int samples = 0;
  for (int y = 2; y < img.height(); y += 7) {
    for (int x = 2; x < img.width(); x += 7) {
      ++samples;
      const QRgb p = img.pixel(x, y);
      if (qAlpha(p) > 12 && qGray(p) > 10)
        ++filled;
    }
  }
  return samples > 0 && filled * 8 > samples;
}

bool demWorldRect(const DemScene& sc, double* xMin, double* yMin, double* xMax, double* yMax) {
  if (!xMin || !yMin || !xMax || !yMax || sc.width < 1 || sc.height < 1)
    return false;
  const double w = static_cast<double>(sc.width);
  const double h = static_cast<double>(sc.height);
  const double ix[4] = {0.0, w, 0.0, w};
  const double iy[4] = {0.0, 0.0, h, h};
  for (int i = 0; i < 4; ++i) {
    const double x = sc.geotransform[0] + sc.geotransform[1] * ix[i] + sc.geotransform[2] * iy[i];
    const double y = sc.geotransform[3] + sc.geotransform[4] * ix[i] + sc.geotransform[5] * iy[i];
    if (i == 0) {
      *xMin = *xMax = x;
      *yMin = *yMax = y;
    } else {
      *xMin = std::min(*xMin, x);
      *xMax = std::max(*xMax, x);
      *yMin = std::min(*yMin, y);
      *yMax = std::max(*yMax, y);
    }
  }
  return std::isfinite(*xMin) && std::isfinite(*yMin) && std::isfinite(*xMax) && std::isfinite(*yMax);
}

QImage composeSheet(const QImage& view, double visibleWidthM, double yawDegFromNorth,
                    const QString& crsLabel, const QString& demName, float zMin, float zMax) {
  const int legendW = 168;
  const int barH = 52;
  const int titleH = 28;
  QImage out(std::max(480, view.width() + legendW + 24),
             view.height() + barH + titleH + 16, QImage::Format_RGB32);
  out.fill(QColor(255, 255, 255));
  QPainter p(&out);
  p.setPen(QColor(17, 24, 39));
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::DemiBold));
  p.drawText(14, 20, QStringLiteral("입체지형 도면"));
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  p.drawText(out.width() - 220, 20,
             crsLabel.isEmpty() ? QStringLiteral("좌표계 —") : QStringLiteral("좌표계 %1").arg(crsLabel));
  p.drawImage(12, titleH + 4, view);

  const int lx = view.width() + 20;
  int ly = titleH + 16;
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::DemiBold));
  p.drawText(lx, ly, QStringLiteral("범례"));
  ly += 18;
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  p.fillRect(lx, ly, 14, 14, QColor(70, 110, 80));
  p.setPen(QPen(QColor(17, 24, 39), 1));
  p.drawRect(lx, ly, 14, 14);
  p.drawText(lx + 20, ly + 12, QStringLiteral("위성 (Google)"));
  ly += 24;
  p.fillRect(lx, ly, 14, 14, QColor(160, 150, 130));
  p.drawRect(lx, ly, 14, 14);
  p.drawText(lx + 20, ly + 12, QStringLiteral("DEM 높이"));
  ly += 20;
  p.drawText(lx, ly + 12,
             QStringLiteral("%1–%2 m").arg(zMin, 0, 'f', 1).arg(zMax, 0, 'f', 1));
  ly += 28;
  p.drawText(lx, ly, demName.isEmpty() ? QStringLiteral("DEM") : demName);

  const int y0 = titleH + 4 + view.height();
  p.setPen(QPen(QColor(17, 24, 39), 1.4));
  p.drawLine(0, y0, out.width(), y0);
  const double span = visibleWidthM > 1.0 ? visibleWidthM : 100.0;
  const double seg = scaleBarSegmentM(span);
  const double pxPerM = (view.width() * 0.36) / std::max(seg * 2.0, 1.0);
  const double barW = seg * pxPerM;
  const int x0 = 14;
  const int by = y0 + 16;
  p.setPen(QPen(QColor(17, 24, 39), 2.0));
  p.drawLine(x0, by + 10, x0 + int(barW), by + 10);
  p.drawLine(x0, by + 4, x0, by + 14);
  p.drawLine(x0 + int(barW), by + 4, x0 + int(barW), by + 14);
  p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  p.setPen(QColor(17, 24, 39));
  p.drawText(x0, by + 8, QStringLiteral("0"));
  p.drawText(x0 + int(barW) - 6, by + 8, QStringLiteral("%1 m").arg(seg, 0, 'f', 0));

  const int nx = 14 + int(barW) + 48;
  const int ny = y0 + 24;
  p.save();
  p.translate(nx, ny);
  p.rotate(-yawDegFromNorth);
  p.setPen(QPen(QColor(17, 24, 39), 2.0));
  p.drawLine(0, 8, 0, -10);
  QPolygonF head;
  head << QPointF(0, -12) << QPointF(-4, -4) << QPointF(4, -4);
  p.setBrush(QColor(17, 24, 39));
  p.drawPolygon(head);
  p.restore();
  p.drawText(nx - 6, y0 + 44, QStringLiteral("N"));
  p.end();
  return out;
}

}  // namespace Terrain3dService
