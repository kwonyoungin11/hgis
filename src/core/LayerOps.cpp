#include "LayerOps.h"
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <cmath>
#include <algorithm>
#include <QPainter>
#include <QUrl>

#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbollayer.h>
#include <qgssymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsnetworkaccessmanager.h>
#include "LocationSearch.h"

QString LayerOps::reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                       const QString& outPath, QgsProject* project, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  const QgsCoordinateReferenceSystem dest(targetCrsAuthId);
  if (!dest.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid target CRS");
    return {};
  }
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = outPath.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)
                        ? QStringLiteral("GPKG")
                        : QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  opts.ct = QgsCoordinateTransform(layer->crs(), dest, project ? project->transformContext() : QgsCoordinateTransformContext());
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, outPath, project ? project->transformContext() : QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("reproject write failed") : err;
    return {};
  }
  if (project) {
    auto* vl = new QgsVectorLayer(outPath, layer->name() + QStringLiteral("_") + targetCrsAuthId, QStringLiteral("ogr"));
    if (vl->isValid()) {
      vl->setCrs(dest);
      project->addMapLayer(vl);
    } else {
      delete vl;
    }
  }
  return outPath;
}

int LayerOps::ensureControlPointQualityFields(QgsVectorLayer* controlPoints) {
  if (!controlPoints || !controlPoints->isValid()) return 0;
  int added = 0;
  QgsFields fields = controlPoints->fields();
  auto ensure = [&](const QString& name, QMetaType::Type t) {
    if (fields.indexOf(name) < 0) {
      if (!controlPoints->isEditable()) controlPoints->startEditing();
      if (controlPoints->dataProvider()->addAttributes({QgsField(name, t)})) {
        ++added;
      }
    }
  };
  ensure(QStringLiteral("accuracy_m"), QMetaType::Type::Double);
  ensure(QStringLiteral("pdop"), QMetaType::Type::Double);
  ensure(QStringLiteral("fix_type"), QMetaType::Type::QString);
  ensure(QStringLiteral("pixel_x"), QMetaType::Type::Double);
  ensure(QStringLiteral("pixel_y"), QMetaType::Type::Double);
  if (added > 0) {
    controlPoints->updateFields();
    if (controlPoints->isEditable()) controlPoints->commitChanges();
  }
  return added;
}

bool LayerOps::applyFeaturePolyStyle(QgsVectorLayer* featurePoly) {
  if (!featurePoly || !featurePoly->isValid()) return false;

  auto brightPoly = []() -> QgsSymbol* {
    QgsSymbol* sym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
    if (!sym) return nullptr;
    sym->setColor(QColor(234, 88, 12, 160));
    if (sym->symbolLayerCount() > 0)
      sym->symbolLayer(0)->setStrokeColor(QColor(154, 52, 18));
    return sym;
  };

  QString field = QStringLiteral("kind");
  if (featurePoly->fields().indexOf(field) < 0) field = QStringLiteral("period");
  if (featurePoly->fields().indexOf(field) < 0) {
    if (QgsSymbol* sym = brightPoly()) {
      featurePoly->setRenderer(new QgsSingleSymbolRenderer(sym));
      featurePoly->triggerRepaint();
    }
    return true;
  }

  QSet<QString> values;
  QgsFeatureIterator it = featurePoly->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString v = f.attribute(field).toString().trimmed();
    if (!v.isEmpty()) values.insert(v);
  }

  if (values.isEmpty()) {
    if (QgsSymbol* sym = brightPoly()) {
      featurePoly->setRenderer(new QgsSingleSymbolRenderer(sym));
      featurePoly->triggerRepaint();
    }
    return true;
  }

  QgsCategoryList cats;
  int i = 0;
  const QList<QString> sorted = values.values();
  for (const QString& v : sorted) {
    QColor c = QColor::fromHsv((i * 47) % 360, 180, 230, 140);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(featurePoly->geometryType());
    if (sym) {
      sym->setColor(c);
      if (sym->symbolLayerCount() > 0)
        sym->symbolLayer(0)->setStrokeColor(c.darker(150));
      cats.append(QgsRendererCategory(QVariant(v), sym, v));
    }
    ++i;
  }
  auto* renderer = new QgsCategorizedSymbolRenderer(field, cats);
  if (QgsSymbol* def = brightPoly())
    renderer->setSourceSymbol(def);
  featurePoly->setRenderer(renderer);
  featurePoly->triggerRepaint();
  return true;
}

static void tuneBasemapLayer(QgsRasterLayer* rl) {
  if (!rl || !rl->isValid()) return;
  rl->setBlendMode(QPainter::CompositionMode_SourceOver);
  if (QgsRasterResampleFilter* rf = rl->resampleFilter()) {
    rf->setZoomedInResampler(new QgsBilinearRasterResampler());
    rf->setZoomedOutResampler(new QgsBilinearRasterResampler());
  }
}

static bool addXyzBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& url,
                          const QString& name, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  const auto existing = project->mapLayersByName(name);
  for (QgsMapLayer* old : existing) {
    if (old) project->removeMapLayer(old->id());
  }

  QgsNetworkAccessManager::instance()->setCacheDisabled(false);

  QgsRasterLayer* rl = nullptr;
  const QStringList providers = {QStringLiteral("wms"), QStringLiteral("xyz")};
  for (const QString& prov : providers) {
    auto* tryRl = new QgsRasterLayer(url, name, prov);
    if (tryRl->isValid()) {
      rl = tryRl;
      break;
    }
    delete tryRl;
  }
  if (!rl) {
    if (errorOut) *errorOut = QStringLiteral("배경 레이어 생성 실패: %1\nURI=%2").arg(name, url.left(120));
    return false;
  }
  tuneBasemapLayer(rl);
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트에 레이어 추가 실패: %1").arg(name);
    delete rl;
    return false;
  }
  if (QgsLayerTreeLayer* node = project->layerTreeRoot()->findLayer(rl->id()))
    node->setItemVisibilityChecked(true);
  if (canvas) {
    canvas->setCachingEnabled(true);
    canvas->refreshAllLayers();
  }
  return true;
}

bool LayerOps::applyReferenceVectorStyle(QgsVectorLayer* layer, const QString& role) {
  if (!layer || !layer->isValid()) return false;
  const Qgis::GeometryType gt = layer->geometryType();
  QgsSymbol* sym = QgsSymbol::defaultSymbol(gt);
  if (!sym) return false;
  const bool cadastral = role.contains(QStringLiteral("지적")) || role.contains(QStringLiteral("parcel"), Qt::CaseInsensitive)
                         || layer->name().contains(QStringLiteral("지적")) || layer->name().contains(QStringLiteral("연속"));
  if (gt == Qgis::GeometryType::Polygon) {
    if (cadastral) {
      sym->setColor(QColor(255, 0, 0, 15));
      if (sym->symbolLayerCount() > 0)
        sym->symbolLayer(0)->setStrokeColor(QColor(220, 38, 38));
    } else {
      sym->setColor(QColor(59, 130, 246, 40));
      if (sym->symbolLayerCount() > 0)
        sym->symbolLayer(0)->setStrokeColor(QColor(29, 78, 216));
    }
  } else if (gt == Qgis::GeometryType::Line) {
    sym->setColor(cadastral ? QColor(220, 38, 38) : QColor(37, 99, 235));
  } else {
    sym->setColor(QColor(220, 38, 38));
  }
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  layer->triggerRepaint();
  return true;
}

bool LayerOps::addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  const QString url = QStringLiteral(
      "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&crs=EPSG3857");
  return addXyzBasemap(project, canvas, url, QStringLiteral("OSM"), errorOut);
}

static bool addVworldCadastralWms(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  const QString key = LocationSearch::vworldApiKey();
  if (key.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("VWorld API 키 없음 (지적 WMS)");
    return false;
  }
  // QGIS WMS URI — continuous cadastral parcel boundary
  const QString urlEncoded = QStringLiteral("https://api.vworld.kr/req/wms");
  const QString uri = QStringLiteral(
      "IgnoreGetMapUrl=1&contextualWMSLegend=0&crs=EPSG:3857&dpiMode=7&featureCount=10"
      "&format=image/png&layers=lp_pa_cbnd_bubun&styles=lp_pa_cbnd_bubun"
      "&url=%1?SERVICE%3DWMS%26REQUEST%3DGetCapabilities%26key%3D%2")
                          .arg(QString::fromUtf8(QUrl::toPercentEncoding(urlEncoded)),
                               QString::fromUtf8(QUrl::toPercentEncoding(key)));

  const QString name = QStringLiteral("VWorld 지적(연속)");
  const auto existing = project->mapLayersByName(name);
  for (QgsMapLayer* old : existing) {
    if (old) project->removeMapLayer(old->id());
  }
  auto* rl = new QgsRasterLayer(uri, name, QStringLiteral("wms"));
  if (!rl->isValid()) {
    // alternate layer name
    delete rl;
    const QString uri2 = QStringLiteral(
        "IgnoreGetMapUrl=1&crs=EPSG:3857&dpiMode=7&format=image/png"
        "&layers=lp_pa_cbnd_bonbun&styles=&url=https://api.vworld.kr/req/wms?key%3D%1")
                             .arg(QString::fromUtf8(QUrl::toPercentEncoding(key)));
    rl = new QgsRasterLayer(uri2, name, QStringLiteral("wms"));
  }
  if (!rl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("지적 WMS 실패: %1").arg(rl->error().message());
    delete rl;
    return false;
  }
  project->addMapLayer(rl, true);
  if (QgsLayerTreeLayer* node = project->layerTreeRoot()->findLayer(rl->id())) {
    node->setItemVisibilityChecked(true);
    QgsLayerTreeNode* n = node;
    project->layerTreeRoot()->takeChild(n);
    project->layerTreeRoot()->insertChildNode(0, n);
  }
  if (canvas) canvas->refreshAllLayers();
  return true;
}

bool LayerOps::addVworldParcelOverlay(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  QString lastErr;
  if (addVworldCadastralWms(project, canvas, &lastErr))
    return true;

  const QStringList urls = {
    QStringLiteral(
        "type=xyz&url=https://xdworld.vworld.kr/2d/Hybrid/service/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=18&zmin=8&crs=EPSG3857"),
  };
  for (const QString& url : urls) {
    QString err;
    if (addXyzBasemap(project, canvas, url, QStringLiteral("VWorld 지적(하이브리드)"), &err)) {
      if (project) {
        QgsLayerTree* root = project->layerTreeRoot();
        const auto layers = project->mapLayersByName(QStringLiteral("VWorld 지적(하이브리드)"));
        if (!layers.isEmpty()) {
          if (QgsLayerTreeLayer* node = root->findLayer(layers.first()->id())) {
            QgsLayerTreeNode* n = node;
            root->takeChild(n);
            root->insertChildNode(0, n);
            node->setItemVisibilityChecked(true);
          }
        }
      }
      if (canvas) canvas->refreshAllLayers();
      return true;
    }
    lastErr = err;
  }
  if (errorOut) *errorOut = lastErr.isEmpty() ? QStringLiteral("VWorld 지적 실패") : lastErr;
  return false;
}

bool LayerOps::addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                               QString* errorOut) {
  QString url;
  QString name;
  switch (kind) {
  case KoreaBasemap::VWorldBase:
    name = QStringLiteral("VWorld 배경");
    url = QStringLiteral(
        "type=xyz&url=https://xdworld.vworld.kr/2d/Base/service/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=18&zmin=5&crs=EPSG3857");
    break;
  case KoreaBasemap::VWorldSatellite:
    name = QStringLiteral("VWorld 위성");
    url = QStringLiteral(
        "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg&zmax=18&zmin=5&crs=EPSG3857");
    break;
  case KoreaBasemap::VWorldHybrid:
    name = QStringLiteral("VWorld 하이브리드");
    url = QStringLiteral(
        "type=xyz&url=https://xdworld.vworld.kr/2d/Hybrid/service/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=18&zmin=5&crs=EPSG3857");
    break;
  case KoreaBasemap::VWorldParcel:
    return addVworldParcelOverlay(project, canvas, errorOut);
  case KoreaBasemap::GoogleRoad:
    name = QStringLiteral("Google 도로");
    url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Dm%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D&zmax=20&zmin=0&crs=EPSG3857");
    break;
  case KoreaBasemap::GoogleSatellite:
    name = QStringLiteral("Google 위성");
    url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D&zmax=20&zmin=0&crs=EPSG3857");
    break;
  case KoreaBasemap::Osm:
  default:
    return addOsmBasemap(project, canvas, errorOut);
  }
  if (!addXyzBasemap(project, canvas, url, name, errorOut)) {
    if (kind != KoreaBasemap::Osm)
      return addOsmBasemap(project, canvas, errorOut);
    return false;
  }
  // Basemap at bottom of TOC
  if (project) {
    QgsLayerTree* root = project->layerTreeRoot();
    if (QgsMapLayer* ml = project->mapLayersByName(name).value(0)) {
      if (QgsLayerTreeLayer* node = root->findLayer(ml->id())) {
        QgsLayerTreeNode* n = node;
        root->takeChild(n);
        root->insertChildNode(root->children().size(), n);
      }
    }
  }
  return true;
}

bool LayerOps::addKoreaBasemapWithParcel(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                                         QString* errorOut) {
  QString err;
  if (!addKoreaBasemap(project, canvas, kind, &err)) {
    if (errorOut) *errorOut = err;
    return false;
  }
  QString perr;
  if (!addVworldParcelOverlay(project, canvas, &perr)) {
    // Base/sat ok but parcel failed — still success with warning
    if (errorOut) *errorOut = QStringLiteral("배경 OK, 지적 오버레이 실패: %1").arg(perr);
  }
  if (canvas) canvas->refreshAllLayers();
  return true;
}

bool LayerOps::setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                          QString* errorOut) {
  const QgsCoordinateReferenceSystem crs(epsgAuthId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid CRS %1").arg(epsgAuthId);
    return false;
  }
  if (project) project->setCrs(crs);
  if (canvas) {
    canvas->setDestinationCrs(crs);
    zoomToKorea(canvas, epsgAuthId);
  }
  return true;
}

QgsRectangle LayerOps::koreaExtentForCrs(const QString& epsgAuthId) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest(epsgAuthId);
  const QgsRectangle krWgs(124.5, 33.0, 132.0, 39.5);
  if (!dest.isValid()) return krWgs;
  try {
    const QgsCoordinateTransform xf(wgs, dest, QgsCoordinateTransformContext());
    return xf.transformBoundingBox(krWgs);
  } catch (...) {
    return QgsRectangle();
  }
}

void LayerOps::zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId) {
  if (!canvas) return;
  const QgsRectangle ext = koreaExtentForCrs(epsgAuthId);
  if (!ext.isEmpty() && ext.isFinite()) {
    canvas->setExtent(ext);
    canvas->refreshAllLayers();
  }
}

QString LayerOps::convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                   QgsProject* project, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  QString path = outShpPath;
  if (!path.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive))
    path += QStringLiteral(".shp");
  return reprojectVectorLayer(layer, QStringLiteral("EPSG:5179"), path, project, errorOut);
}

QString LayerOps::convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                       QgsProject* project, QString* errorOut) {
  if (!QFile::exists(inPath)) {
    if (errorOut) *errorOut = QStringLiteral("Input not found");
    return {};
  }
  auto* vl = new QgsVectorLayer(inPath, QFileInfo(inPath).completeBaseName(), QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot open: %1").arg(inPath);
    delete vl;
    return {};
  }
  const QString out = convertToShp5179(vl, outShpPath, project, errorOut);
  delete vl;
  return out;
}

QString LayerOps::georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                          QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!QFile::exists(imagePath)) {
    if (errorOut) *errorOut = QStringLiteral("Image not found");
    return {};
  }
  if (!controlPoints || controlPoints->featureCount() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Need >=2 control points");
    return {};
  }
  QImage img(imagePath);
  if (img.isNull()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot read image");
    return {};
  }
  const double w = img.width();
  const double h = img.height();
  if (w < 2 || h < 2) {
    if (errorOut) *errorOut = QStringLiteral("Image too small");
    return {};
  }

  struct Gcp {
    double px = -1, py = -1;
    QgsPointXY map;
    bool hasPixel = false;
  };
  QVector<Gcp> gcps;
  QgsFeatureIterator it = controlPoints->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    Gcp g;
    g.map = f.geometry().asPoint();
    const int ix = controlPoints->fields().indexOf(QStringLiteral("pixel_x"));
    const int iy = controlPoints->fields().indexOf(QStringLiteral("pixel_y"));
    if (ix >= 0 && iy >= 0) {
      bool okx = false, oky = false;
      const double px = f.attribute(ix).toDouble(&okx);
      const double py = f.attribute(iy).toDouble(&oky);
      if (okx && oky) {
        g.px = px;
        g.py = py;
        g.hasPixel = true;
      }
    }
    gcps.append(g);
  }
  if (gcps.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Control points lack geometry");
    return {};
  }

  double rotA = 0, rotB = 0, rotD = 0, rotE = 0, ulx = 0, uly = 0;
  const bool allPixel = std::all_of(gcps.begin(), gcps.end(), [](const Gcp& g) { return g.hasPixel; })
                        && gcps.size() >= 2;

  if (allPixel && gcps.size() >= 3) {
    double sxx = 0, sxy = 0, sx = 0, syy = 0, sy = 0, sn = 0;
    double sxX = 0, syX = 0, sX = 0, sxY = 0, syY = 0, sY = 0;
    for (const Gcp& g : gcps) {
      const double x = g.px, y = g.py;
      sxx += x * x; sxy += x * y; sx += x;
      syy += y * y; sy += y; sn += 1;
      sxX += x * g.map.x(); syX += y * g.map.x(); sX += g.map.x();
      sxY += x * g.map.y(); syY += y * g.map.y(); sY += g.map.y();
    }
    auto solve3 = [](double a11, double a12, double a13, double a21, double a22, double a23,
                     double a31, double a32, double a33, double b1, double b2, double b3,
                     double& x1, double& x2, double& x3) -> bool {
      const double det = a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31) + a13 * (a21 * a32 - a22 * a31);
      if (std::abs(det) < 1e-18) return false;
      x1 = (b1 * (a22 * a33 - a23 * a32) - a12 * (b2 * a33 - a23 * b3) + a13 * (b2 * a32 - a22 * b3)) / det;
      x2 = (a11 * (b2 * a33 - a23 * b3) - b1 * (a21 * a33 - a23 * a31) + a13 * (a21 * b3 - b2 * a31)) / det;
      x3 = (a11 * (a22 * b3 - b2 * a32) - a12 * (a21 * b3 - b2 * a31) + b1 * (a21 * a32 - a22 * a31)) / det;
      return true;
    };
    double a = 0, b = 0, c = 0, d = 0, e = 0, fpar = 0;
    if (!solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxX, syX, sX, a, b, c)
        || !solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxY, syY, sY, d, e, fpar)) {
      if (errorOut) *errorOut = QStringLiteral("Affine LS solve failed");
      return {};
    }
    rotA = a; rotB = b; rotD = d; rotE = e; ulx = c; uly = fpar;
  } else {
    const QgsPointXY g0 = gcps[0].map;
    const QgsPointXY g1 = gcps[1].map;
    const double dx = g1.x() - g0.x();
    const double dy = g1.y() - g0.y();
    const double dist = std::hypot(dx, dy);
    if (dist < 1e-9) {
      if (errorOut) *errorOut = QStringLiteral("GCP0 and GCP1 too close");
      return {};
    }
    rotA = dx / w;
    rotB = 0.0;
    rotD = 0.0;
    rotE = (std::abs(dy) < 1e-6) ? -std::abs(rotA) : (dy / h);
    if (gcps.size() >= 3) {
      const double dy2 = gcps[2].map.y() - g0.y();
      if (std::abs(dy2) > 1e-6) rotE = dy2 / h;
    }
    ulx = g0.x() + rotA * 0.5;
    uly = g0.y() + rotE * (h - 0.5);
  }

  QString wfPath = imagePath;
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("jgw");
  else if (ext == QLatin1String("png"))
    wfPath = imagePath.left(imagePath.size() - 3) + QStringLiteral("pgw");
  else if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("tfw");
  else
    wfPath = imagePath + QStringLiteral(".wld");

  QFile wf(wfPath);
  if (!wf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write world file");
    return {};
  }
  QTextStream ts(&wf);
  ts.setEncoding(QStringConverter::Utf8);
  // standard world file 6 lines
  ts << QString::number(rotA, 'g', 16) << "\n";
  ts << QString::number(rotD, 'g', 16) << "\n";
  ts << QString::number(rotB, 'g', 16) << "\n";
  ts << QString::number(rotE, 'g', 16) << "\n";
  ts << QString::number(ulx, 'g', 16) << "\n";
  ts << QString::number(uly, 'g', 16) << "\n";
  wf.close();

  auto* rl = new QgsRasterLayer(imagePath, QFileInfo(imagePath).completeBaseName(), QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Georef raster invalid after worldfile");
    delete rl;
    return {};
  }
  if (project) {
    if (project->crs().isValid()) rl->setCrs(project->crs());
    project->addMapLayer(rl);
  }
  if (canvas) {
    canvas->setExtent(rl->extent());
    canvas->refresh();
  }
  return wfPath;
}

