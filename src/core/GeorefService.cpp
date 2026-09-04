#include "GeorefService.h"
#include "LayerOps.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>
#include <cmath>
#include <algorithm>
#include <memory>

#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsgeometry.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <QPainter>
#include <qgsdataprovider.h>
#include <qgsrasterdataprovider.h>
#include <qgsrastertransparency.h>
#include <qgsrasterrenderer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsrasterblock.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgis.h>
#include <qgswkbtypes.h>
#include <gdal.h>

namespace GeorefService {
namespace {

constexpr double kDetEps = 1e-18;

bool solve3(double a11, double a12, double a13, double a21, double a22, double a23,
            double a31, double a32, double a33, double b1, double b2, double b3,
            double& x1, double& x2, double& x3) {
  const double det = a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31)
                     + a13 * (a21 * a32 - a22 * a31);
  if (std::abs(det) < kDetEps) return false;
  x1 = (b1 * (a22 * a33 - a23 * a32) - a12 * (b2 * a33 - a23 * b3) + a13 * (b2 * a32 - a22 * b3)) / det;
  x2 = (a11 * (b2 * a33 - a23 * b3) - b1 * (a21 * a33 - a23 * a31) + a13 * (a21 * b3 - b2 * a31)) / det;
  x3 = (a11 * (a22 * b3 - b2 * a32) - a12 * (a21 * b3 - b2 * a31) + b1 * (a21 * a32 - a22 * a31)) / det;
  return true;
}

Affine helmertFromTwo(const Pair& p0, const Pair& p1) {
  Affine out;
  const double dsx = p1.srcX - p0.srcX;
  const double dsy = p1.srcY - p0.srcY;
  const double dmx = p1.mapX - p0.mapX;
  const double dmy = p1.mapY - p0.mapY;
  const double lenS = std::hypot(dsx, dsy);
  const double lenM = std::hypot(dmx, dmy);
  if (lenS < 1e-12 || lenM < 1e-12) return out;
  const double scale = lenM / lenS;
  const double ang = std::atan2(dmy, dmx) - std::atan2(dsy, dsx);
  const double c = std::cos(ang);
  const double s = std::sin(ang);
  out.a = scale * c;
  out.b = -scale * s;
  out.d = scale * s;
  out.e = scale * c;
  out.c = p0.mapX - out.a * p0.srcX - out.b * p0.srcY;
  out.f = p0.mapY - out.d * p0.srcX - out.e * p0.srcY;
  out.valid = true;
  out.pairCount = 2;
  return out;
}

Affine affineLeastSquares(const QVector<Pair>& pairs) {
  Affine out;
  if (pairs.size() < 3) return out;
  double sxx = 0, sxy = 0, sx = 0, syy = 0, sy = 0, sn = 0;
  double sxX = 0, syX = 0, sX = 0, sxY = 0, syY = 0, sY = 0;
  for (const Pair& g : pairs) {
    const double x = g.srcX, y = g.srcY;
    sxx += x * x;
    sxy += x * y;
    sx += x;
    syy += y * y;
    sy += y;
    sn += 1;
    sxX += x * g.mapX;
    syX += y * g.mapX;
    sX += g.mapX;
    sxY += x * g.mapY;
    syY += y * g.mapY;
    sY += g.mapY;
  }
  double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
  if (!solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxX, syX, sX, a, b, c))
    return out;
  if (!solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxY, syY, sY, d, e, f))
    return out;
  out.a = a;
  out.b = b;
  out.c = c;
  out.d = d;
  out.e = e;
  out.f = f;
  out.valid = true;
  out.pairCount = pairs.size();
  return out;
}

}  // namespace

bool transform(const Affine& a, double sx, double sy, double* mx, double* my) {
  if (!a.valid || !mx || !my) return false;
  *mx = a.a * sx + a.b * sy + a.c;
  *my = a.d * sx + a.e * sy + a.f;
  return true;
}

bool invert(const Affine& a, double mx, double my, double* sx, double* sy) {
  if (!a.valid || !sx || !sy) return false;
  const double det = a.a * a.e - a.b * a.d;
  if (std::abs(det) < kDetEps) return false;
  const double x = mx - a.c;
  const double y = my - a.f;
  *sx = (a.e * x - a.b * y) / det;
  *sy = (-a.d * x + a.a * y) / det;
  return true;
}

QTransform toQTransform(const Affine& a) {
  return QTransform(a.a, a.d, a.b, a.e, a.c, a.f);
}

double rmsMeters(const Affine& a, const QVector<Pair>& pairs) {
  if (!a.valid || pairs.isEmpty()) return 0;
  double acc = 0;
  int n = 0;
  for (const Pair& p : pairs) {
    double mx = 0, my = 0;
    if (!transform(a, p.srcX, p.srcY, &mx, &my)) continue;
    acc += (mx - p.mapX) * (mx - p.mapX) + (my - p.mapY) * (my - p.mapY);
    ++n;
  }
  if (n <= 0) return 0;
  return std::sqrt(acc / n);
}

Affine fromPairs(const QVector<Pair>& pairs) {
  Affine out;
  if (pairs.size() < 2) return out;
  if (pairs.size() == 2) {
    out = helmertFromTwo(pairs[0], pairs[1]);
  } else {
    out = affineLeastSquares(pairs);
    if (!out.valid) out = helmertFromTwo(pairs[0], pairs[1]);
  }
  if (out.valid) {
    out.pairCount = pairs.size();
    out.rmsMeters = rmsMeters(out, pairs);
  }
  return out;
}

Affine fitSrcBoxToExtent(double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
                         const QgsRectangle& dest) {
  Affine out;
  const double sw = srcMaxX - srcMinX;
  const double sh = srcMaxY - srcMinY;
  if (sw < 1e-12 || sh < 1e-12 || dest.isEmpty() || !dest.isFinite()) return out;
  out.a = dest.width() / sw;
  out.b = 0;
  out.c = dest.xMinimum() - out.a * srcMinX;
  out.d = 0;
  out.e = dest.height() / sh;
  out.f = dest.yMinimum() - out.e * srcMinY;
  out.valid = true;
  out.pairCount = 0;
  return out;
}

Affine fitRasterToExtent(int pixelW, int pixelH, const QgsRectangle& dest) {
  Affine out;
  if (pixelW < 2 || pixelH < 2 || dest.isEmpty() || !dest.isFinite()) return out;
  out.a = dest.width() / static_cast<double>(pixelW);
  out.b = 0;
  out.c = dest.xMinimum();
  out.d = 0;
  out.e = -dest.height() / static_cast<double>(pixelH);
  out.f = dest.yMaximum();
  out.valid = true;
  return out;
}

QString worldFilePathFor(const QString& imagePath) {
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    return imagePath.left(imagePath.size() - ext.size()) + QStringLiteral("jgw");
  if (ext == QLatin1String("png"))
    return imagePath.left(imagePath.size() - 3) + QStringLiteral("pgw");
  if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
    return imagePath.left(imagePath.size() - ext.size()) + QStringLiteral("tfw");
  return imagePath + QStringLiteral(".wld");
}

QString prjPathFor(const QString& imagePath) {
  const QFileInfo fi(imagePath);
  return fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".prj");
}

bool writeWorldFile(const QString& imagePath, const Affine& a, QString* errorOut) {
  if (!a.valid) {
    if (errorOut) *errorOut = QStringLiteral("변환이 없습니다");
    return false;
  }
  const QString wfPath = worldFilePathFor(imagePath);
  QFile wf(wfPath);
  if (!wf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("월드파일을 쓸 수 없습니다");
    return false;
  }
  const double A = a.a;
  const double D = a.d;
  const double B = a.b;
  const double E = a.e;
  const double C = a.c + 0.5 * (a.a + a.b);
  const double F = a.f + 0.5 * (a.d + a.e);
  QTextStream ts(&wf);
  ts.setEncoding(QStringConverter::Utf8);
  ts << QString::number(A, 'g', 16) << "\n";
  ts << QString::number(D, 'g', 16) << "\n";
  ts << QString::number(B, 'g', 16) << "\n";
  ts << QString::number(E, 'g', 16) << "\n";
  ts << QString::number(C, 'g', 16) << "\n";
  ts << QString::number(F, 'g', 16) << "\n";
  return true;
}

bool writeSidecarPrj(const QString& imagePath, const QgsCoordinateReferenceSystem& crs,
                     QString* errorOut) {
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계가 없습니다");
    return false;
  }
  QFile f(prjPathFor(imagePath));
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("prj 파일을 쓸 수 없습니다");
    return false;
  }
  f.write(crs.toWkt().toUtf8());
  return true;
}

bool stampGdalGeoTransform(const QString& imagePath, const Affine& a) {
  if (!a.valid || imagePath.isEmpty()) return false;
  GDALAllRegister();
  GDALDatasetH ds = GDALOpen(qUtf8Printable(imagePath), GA_Update);
  if (!ds)
    ds = GDALOpen(qUtf8Printable(imagePath), GA_ReadOnly);
  if (!ds) return false;
  double gt[6] = {a.c, a.a, a.b, a.f, a.d, a.e};
  const CPLErr err = GDALSetGeoTransform(ds, gt);
  GDALClose(ds);
  return err == CE_None;
}

void dropGdalPamSidecar(const QString& imagePath) {
  const QFileInfo fi(imagePath);
  const QStringList pam = {
      imagePath + QStringLiteral(".aux.xml"),
      fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".aux.xml"),
  };
  for (const QString& p : pam) {
    if (QFile::exists(p)) QFile::remove(p);
  }
}

bool applyWorldFileToRaster(QgsRasterLayer* layer, const Affine& a,
                            const QgsCoordinateReferenceSystem& crs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("그림 레이어가 없습니다");
    return false;
  }
  const QString src = layer->source();
  if (!writeWorldFile(src, a, errorOut)) return false;
  if (crs.isValid()) writeSidecarPrj(src, crs, nullptr);
  dropGdalPamSidecar(src);
  stampGdalGeoTransform(src, a);
  if (crs.isValid()) layer->setCrs(crs);
  if (QgsDataProvider* p = layer->dataProvider()) p->reloadData();
  if (crs.isValid()) layer->setCrs(crs);
  LayerOps::setAlignPending(layer, false);
  if (QgsProject* proj = QgsProject::instance()) {
    if (QgsLayerTree* root = proj->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = root->findLayer(layer->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  layer->triggerRepaint();
  return layer->isValid();
}

bool persistAlignedRaster(QgsRasterLayer* layer, const Affine& a,
                          const QgsCoordinateReferenceSystem& crs, QString* errorOut) {
  if (!applyWorldFileToRaster(layer, a, crs, errorOut)) return false;
  const QString src = layer->source();
  const QString name = layer->name();
  layer->setDataSource(src, name, QStringLiteral("gdal"), false);
  if (crs.isValid()) layer->setCrs(crs);
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("맞춰진 그림을 다시 열지 못했습니다");
    return false;
  }
  if (looksUnreferencedRaster(layer)) {
    if (errorOut)
      *errorOut = QStringLiteral("그림을 지도 좌표로 붙이지 못했습니다. 점을 다시 찍고 이동하세요.");
    return false;
  }
  return true;
}

bool mustRebuildRasterAfterWorldFile(const QgsRasterLayer* layer) {
  return looksUnreferencedRaster(layer);
}

bool transformGeometry(QgsGeometry* geom, const Affine& a) {
  if (!geom || geom->isEmpty() || !a.valid) return false;
  const Qgis::GeometryOperationResult r = geom->transform(toQTransform(a));
  return r == Qgis::GeometryOperationResult::Success;
}

bool applyAffineToVector(QgsVectorLayer* layer, const Affine& a,
                         const QHash<qint64, QgsGeometry>& originals,
                         const QgsCoordinateReferenceSystem& destCrs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("도면 레이어가 없습니다");
    return false;
  }
  if (!a.valid) {
    if (errorOut) *errorOut = QStringLiteral("변환이 없습니다");
    return false;
  }
  if (!layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("도면을 고칠 수 없습니다");
    return false;
  }
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    QgsGeometry g = originals.value(f.id());
    if (g.isEmpty()) g = f.geometry();
    if (g.isEmpty()) continue;
    if (!transformGeometry(&g, a)) {
      layer->rollBack();
      if (errorOut) *errorOut = QStringLiteral("도형 변환 실패");
      return false;
    }
    layer->changeGeometry(f.id(), g);
  }
  if (!layer->commitChanges()) {
    layer->rollBack();
    if (errorOut) *errorOut = QStringLiteral("도면 저장 실패");
    return false;
  }
  if (destCrs.isValid()) layer->setCrs(destCrs);
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* cloneToMemory(QgsVectorLayer* src, const QString& name, QString* errorOut) {
  if (!src || !src->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("원본 도면이 없습니다");
    return nullptr;
  }
  auto makeMem = [&](const QString& geomName) -> QgsVectorLayer* {
    const QString crs = src->crs().isValid() ? src->crs().authid() : QString();
    QString uri = geomName;
    if (!crs.isEmpty()) uri += QStringLiteral("?crs=%1").arg(crs);
    auto* mem = new QgsVectorLayer(uri, name, QStringLiteral("memory"));
    if (!mem->isValid()) {
      delete mem;
      return nullptr;
    }
    QList<QgsField> add;
    const QgsFields fields = src->fields();
    for (int i = 0; i < fields.size(); ++i) add.append(fields.at(i));
    if (!add.isEmpty()) {
      mem->dataProvider()->addAttributes(add);
      mem->updateFields();
    }
    return mem;
  };

  QString geom = QgsWkbTypes::displayString(src->wkbType());
  if (geom.isEmpty() || geom.contains(QLatin1String("Unknown"), Qt::CaseInsensitive))
    geom = QStringLiteral("LineString");

  auto fill = [&](QgsVectorLayer* mem, bool forceLine) -> int {
    QgsFeatureIterator it = src->getFeatures();
    QgsFeature f;
    QgsFeatureList copied;
    while (it.nextFeature(f)) {
      QgsGeometry g = f.geometry();
      if (g.isEmpty()) continue;
      if (forceLine && g.type() != Qgis::GeometryType::Line)
        g = g.convertToType(Qgis::GeometryType::Line, true);
      else if (g.type() != mem->geometryType())
        g = g.convertToType(mem->geometryType(), true);
      if (g.isEmpty()) continue;
      QgsFeature nf(mem->fields());
      nf.setGeometry(g);
      const QgsFields sf = src->fields();
      for (int i = 0; i < sf.size(); ++i) {
        const int di = mem->fields().indexOf(sf.at(i).name());
        if (di >= 0) nf.setAttribute(di, f.attribute(i));
      }
      copied.append(nf);
    }
    if (copied.isEmpty()) return 0;
    mem->dataProvider()->addFeatures(copied);
    return static_cast<int>(copied.size());
  };

  QgsVectorLayer* mem = makeMem(geom);
  if (!mem) {
    if (errorOut) *errorOut = QStringLiteral("맞춤 도면을 만들지 못했습니다");
    return nullptr;
  }
  int n = fill(mem, false);
  if (n <= 0) {
    delete mem;
    mem = makeMem(QStringLiteral("LineString"));
    if (!mem) {
      if (errorOut) *errorOut = QStringLiteral("맞춤 도면을 만들지 못했습니다");
      return nullptr;
    }
    n = fill(mem, true);
  }
  if (n <= 0) {
    delete mem;
    if (errorOut)
      *errorOut = QStringLiteral("CAD에서 선을 읽지 못했습니다. DXF로 저장한 뒤 다시 열어 보세요.");
    return nullptr;
  }
  return mem;
}

QString saveVectorCopyGpkg(QgsVectorLayer* layer, const QString& outPath,
                           const QgsCoordinateReferenceSystem& destCrs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("도면이 없습니다");
    return {};
  }
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.fileEncoding = QStringLiteral("UTF-8");
  if (destCrs.isValid() && layer->crs().isValid() && destCrs != layer->crs()) {
    opts.ct = QgsCoordinateTransform(layer->crs(), destCrs, QgsCoordinateTransformContext());
  }
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, outPath, QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("GPKG 저장 실패") : err;
    return {};
  }
  return outPath;
}

bool isDomainSurveyLayer(const QgsMapLayer* layer) {
  const QString key = LayerOps::layerKeyOf(layer);
  return key == QLatin1String("survey_area") || key == QLatin1String("feature_poly")
         || key == QLatin1String("feature_line") || key == QLatin1String("section_line")
         || key == QLatin1String("control_points") || key == QLatin1String("artifact_point")
         || key == QLatin1String("trial_trench");
}

bool isAlignableLayer(const QgsMapLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (isDomainSurveyLayer(layer)) return false;
  if (const auto* rl = qobject_cast<const QgsRasterLayer*>(layer))
    return rl->providerType() == QLatin1String("gdal");
  if (const auto* vl = qobject_cast<const QgsVectorLayer*>(layer)) {
    const QString p = vl->providerType();
    return p == QLatin1String("ogr") || p == QLatin1String("memory");
  }
  return false;
}

bool isImagePath(const QString& path) {
  const QString low = path.toLower();
  return low.endsWith(QLatin1String(".jpg")) || low.endsWith(QLatin1String(".jpeg"))
         || low.endsWith(QLatin1String(".png")) || low.endsWith(QLatin1String(".tif"))
         || low.endsWith(QLatin1String(".tiff")) || low.endsWith(QLatin1String(".gtiff"));
}

bool isCadPath(const QString& path) {
  const QString low = path.toLower();
  return low.endsWith(QLatin1String(".dxf")) || low.endsWith(QLatin1String(".dwg"));
}

bool looksLikePaperScan(const QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  QgsRasterDataProvider* p = const_cast<QgsRasterLayer*>(layer)->dataProvider();
  if (p && p->bandCount() >= 1) {
    const int w = std::min(std::max(layer->width(), 1), 32);
    const int h = std::min(std::max(layer->height(), 1), 32);
    if (w >= 2 && h >= 2) {
      std::unique_ptr<QgsRasterBlock> r(p->block(1, layer->extent(), w, h));
      std::unique_ptr<QgsRasterBlock> g(
          p->bandCount() >= 2 ? p->block(2, layer->extent(), w, h) : nullptr);
      std::unique_ptr<QgsRasterBlock> b(
          p->bandCount() >= 3 ? p->block(3, layer->extent(), w, h) : nullptr);
      if (r && r->isValid()) {
        int white = 0;
        int n = 0;
        for (int y = 0; y < h; ++y) {
          for (int x = 0; x < w; ++x) {
            if (r->isNoData(x, y)) continue;
            const double rv = r->value(x, y);
            const double gv = (g && g->isValid()) ? g->value(x, y) : rv;
            const double bv = (b && b->isValid()) ? b->value(x, y) : rv;
            ++n;
            if (rv >= 236.0 && gv >= 236.0 && bv >= 236.0)
              ++white;
          }
        }
        if (n > 0) return (white * 100 / n) >= 42;
      }
    }
  }
  const QString src = layer->source().toLower();
  if (src.contains(QLatin1String(".tif"))) return false;
  return src.contains(QLatin1String(".jpg")) || src.contains(QLatin1String(".jpeg"))
         || src.contains(QLatin1String(".png")) || src.contains(QLatin1String(".bmp"))
         || src.contains(QLatin1String(".gif"));
}

void styleAlignedInkScan(QgsRasterLayer* layer) {
  layer->setOpacity(1.0);
  layer->setBlendMode(QPainter::CompositionMode_Multiply);
  layer->setResamplingStage(Qgis::RasterResamplingStage::Provider);
  if (QgsRasterDataProvider* p = layer->dataProvider()) {
    p->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    p->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
  }
  if (QgsBrightnessContrastFilter* bf = layer->brightnessFilter()) {
    bf->setContrast(55);
    bf->setBrightness(-28);
    bf->setGamma(0.75);
  }
  QgsRasterRenderer* rend = layer->renderer();
  if (!rend) return;
  auto* tr = new QgsRasterTransparency();
  QVector<QgsRasterTransparency::TransparentThreeValuePixel> rgb;
  rgb.append(QgsRasterTransparency::TransparentThreeValuePixel(255, 255, 255, 0.0, 8, 8, 8));
  tr->setTransparentThreeValuePixelList(rgb);
  rend->setRasterTransparency(tr);
}

void styleAlignedColorRaster(QgsRasterLayer* layer) {
  // 항공 GeoTIFF 등은 먹선 필터를 쓰면 전혀 다른 색으로 이동한다.
  layer->setOpacity(1.0);
  layer->setBlendMode(QPainter::CompositionMode_SourceOver);
  layer->setResamplingStage(Qgis::RasterResamplingStage::Provider);
  if (QgsRasterDataProvider* p = layer->dataProvider()) {
    p->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
    p->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
  }
  if (QgsBrightnessContrastFilter* bf = layer->brightnessFilter()) {
    bf->setContrast(0);
    bf->setBrightness(0);
    bf->setGamma(1.0);
  }
  if (QgsRasterRenderer* rend = layer->renderer())
    rend->setRasterTransparency(new QgsRasterTransparency());
}

void styleAlignedRasterOverlay(QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return;
  if (looksLikePaperScan(layer))
    styleAlignedInkScan(layer);
  else
    styleAlignedColorRaster(layer);
  layer->triggerRepaint();
}

bool looksUnreferencedRaster(const QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return true;
  const int pw = layer->width();
  const int ph = layer->height();
  if (pw < 2 || ph < 2) return true;
  const QgsRectangle e = layer->extent();
  if (e.isEmpty() || !e.isFinite()) return true;
  // 픽셀 평면(0..폭, 0..높이)에만 앉은 그림. 월드파일·내장 GT가 없어도
  // 여기 해당하면 맞추기 대상이다. 내장 GT가 지도 좌표면 참조된 것이다
  // (사이드카 .tfw가 없다고 맞추기 대기로 숨기면 안 된다).
  if (e.xMinimum() > -2.0 && e.yMinimum() > -2.0 && e.xMaximum() < pw + 2.0
      && e.yMaximum() < ph + 2.0)
    return true;
  // Size ≈ pixels is only a raw scan when the box is still near the pixel origin.
  // 1 m/pixel on EPSG:5186 must not be treated as unreferenced.
  if (std::abs(e.width() - pw) < 2.0 && std::abs(e.height() - ph) < 2.0
      && e.xMinimum() < pw + 10.0 && e.yMinimum() < ph + 10.0)
    return true;
  return false;
}

}  // namespace GeorefService
