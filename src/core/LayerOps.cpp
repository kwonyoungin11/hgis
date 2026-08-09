#include "LayerOps.h"
#include "VworldSettings.h"
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>
#include <QPainter>
#include <QUrl>

#include <qgis.h>
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
#include <qgssymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsprojectviewsettings.h>
#include <qgsreferencedgeometry.h>
#include <QNetworkRequest>

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
  QString field = QStringLiteral("kind");
  if (featurePoly->fields().indexOf(field) < 0) field = QStringLiteral("period");
  if (featurePoly->fields().indexOf(field) < 0) return false;

  QSet<QString> values;
  QgsFeatureIterator it = featurePoly->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString v = f.attribute(field).toString().trimmed();
    if (!v.isEmpty()) values.insert(v);
  }
  QgsCategoryList cats;
  int i = 0;
  const QList<QString> sorted = values.values();
  for (const QString& v : sorted) {
    QColor c = QColor::fromHsv((i * 47) % 360, 160, 220, 140);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(featurePoly->geometryType());
    if (sym) {
      sym->setColor(c);
      cats.append(QgsRendererCategory(QVariant(v), sym, v));
    }
    ++i;
  }
  auto* renderer = new QgsCategorizedSymbolRenderer(field, cats);
  featurePoly->setRenderer(renderer);
  featurePoly->triggerRepaint();
  return true;
}

bool LayerOps::mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 묶을 수 있습니다");
    return false;
  }

  QVector<QgsGeometry> geoms;
  QgsFeatureIds ids;
  QgsFeature first;
  bool hasFirst = false;
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    QgsGeometry g = f.geometry();
    if (!g.isGeosValid())
      g = g.makeValid();
    if (g.isEmpty()) continue;
    geoms.append(g);
    ids.insert(f.id());
    if (!hasFirst) {
      first = QgsFeature(f);
      hasFirst = true;
    }
  }
  if (geoms.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("묶을 폴리곤이 2개 이상 필요합니다 (현재 %1개)").arg(geoms.size());
    return false;
  }

  QgsGeometry multi = QgsGeometry::collectGeometry(geoms);
  if (multi.isEmpty())
    multi = QgsGeometry::unaryUnion(geoms);
  if (multi.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 결합 실패");
    return false;
  }
  if (!multi.isGeosValid())
    multi = multi.makeValid();

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }
  if (!layer->deleteFeatures(ids)) {
    if (errorOut) *errorOut = QStringLiteral("기존 피처 삭제 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  QgsFeature out(layer->fields());
  out.setAttributes(first.attributes());
  out.setGeometry(multi);
  if (!layer->addFeature(out)) {
    if (errorOut) *errorOut = QStringLiteral("결합 피처 추가 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }
  layer->triggerRepaint();
  return true;
}

static void ensureTileNetworkIdentity() {
  static bool once = false;
  if (once) return;
  once = true;
  QgsNetworkAccessManager::instance()->setCacheDisabled(false);
  QgsNetworkAccessManager::setRequestPreprocessor([](QNetworkRequest* req) {
    if (!req) return;
    req->setHeader(QNetworkRequest::UserAgentHeader,
                   QStringLiteral("ka-hgis/0.3 (QGIS-based archaeology field HGIS)"));
    req->setRawHeader("Accept", "image/avif,image/webp,image/apng,image/*,*/*;q=0.8");
  });
}

static void tuneBasemapLayer(QgsRasterLayer* rl, bool crispText = false) {
  if (!rl || !rl->isValid()) return;
  rl->setBlendMode(QPainter::CompositionMode_SourceOver);
  if (!rl->crs().isValid() || rl->crs().authid().isEmpty()) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  }
  if (QgsRasterResampleFilter* rf = rl->resampleFilter()) {
    if (crispText) {
      rf->setZoomedInResampler(nullptr);
      rf->setZoomedOutResampler(nullptr);
    } else {
      rf->setZoomedInResampler(new QgsBilinearRasterResampler());
      rf->setZoomedOutResampler(new QgsBilinearRasterResampler());
    }
  }
  if (crispText) {
    if (QgsRasterDataProvider* dp = rl->dataProvider()) {
      dp->setDpi(192);
      dp->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
      dp->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    }
  }
}

QgsLayerTreeGroup* LayerOps::ensureLegendGroup(QgsProject* project, const QString& groupName) {
  Q_UNUSED(project);
  Q_UNUSED(groupName);
  // ORIG-3: never create empty legend groups. Flat additive layer list only.
  return nullptr;
}

void LayerOps::placeInLegendGroup(QgsProject* project, QgsMapLayer* layer, const QString& groupName,
                                  bool insertAtBottom) {
  Q_UNUSED(groupName);
  Q_UNUSED(insertAtBottom);
  if (!project || !layer) return;
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id()))
      node->setItemVisibilityChecked(true);
  }
  pruneEmptyLegendGroups(project);
}

void LayerOps::markSurveyLayer(QgsMapLayer* layer, const QString& layerKey) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerKey), layerKey);
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleSurvey));
}

void LayerOps::markReferenceLayer(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleReference));
}

QString LayerOps::layerKeyOf(const QgsMapLayer* layer) {
  if (!layer) return {};
  return layer->customProperty(QString::fromUtf8(kPropLayerKey)).toString();
}

void LayerOps::applyLegendCrsLabel(QgsMapLayer* layer) {
  if (!layer) return;
  QString base = layer->name();
  int bracket = base.lastIndexOf(QStringLiteral(" [EPSG:"));
  if (bracket < 0) bracket = base.lastIndexOf(QStringLiteral(" [CRS"));
  if (bracket >= 0) base = base.left(bracket).trimmed();
  if (base.isEmpty()) base = layer->name();
  const QString auth = layer->crs().isValid() ? layer->crs().authid() : QString();
  layer->setName(base + QStringLiteral(" [%1]").arg(auth.isEmpty() ? QStringLiteral("CRS?") : auth));
}

bool LayerOps::isReferenceLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleReference))
    return true;
  const QString n = layer->name();
  return n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("VWorld")) ||
         n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google"));
}

QgsVectorLayer* LayerOps::findByLayerKey(QgsProject* project, const QString& layerKey) {
  if (!project || layerKey.isEmpty()) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (!v) continue;
    if (layerKeyOf(v) == layerKey) return v;
  }
  const auto byName = project->mapLayersByName(layerKey);
  if (!byName.isEmpty())
    return qobject_cast<QgsVectorLayer*>(byName.first());
  return nullptr;
}

QStringList LayerOps::domainLayerKeys() {
  return {QStringLiteral("survey_area"), QStringLiteral("feature_poly"), QStringLiteral("feature_line"),
          QStringLiteral("section_line"), QStringLiteral("control_points")};
}

void LayerOps::removeSurveyDomainLayers(QgsProject* project) {
  if (!project) return;
  QStringList ids;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    const QString key = layerKeyOf(l);
    if (!key.isEmpty() && domainLayerKeys().contains(key))
      ids.append(l->id());
    else if (l->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
             QLatin1String(kRoleSurvey))
      ids.append(l->id());
  }
  for (const QString& id : ids)
    project->removeMapLayer(id);
  pruneEmptyLegendGroups(project);
}

void LayerOps::pruneEmptyLegendGroups(QgsProject* project) {
  if (!project) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  const QList<QgsLayerTreeNode*> children = root->children();
  for (QgsLayerTreeNode* n : children) {
    auto* g = qobject_cast<QgsLayerTreeGroup*>(n);
    if (!g) continue;
    if (g->children().isEmpty())
      root->removeChildNode(g);
  }
}

QgsVectorLayer* LayerOps::ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                            const QString& layerKey, const QString& titleKo,
                                            QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return nullptr;
  }
  if (auto* existing = findByLayerKey(project, layerKey))
    return existing;
  if (gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.");
    return nullptr;
  }
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = vl->error().message();
    delete vl;
    return nullptr;
  }
  vl->setName(titleKo);
  markSurveyLayer(vl, layerKey);
  applyLegendCrsLabel(vl);
  project->addMapLayer(vl, true);
  pruneEmptyLegendGroups(project);
  return vl;
}

void LayerOps::syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea) {
  if (!project || !canvas) return;
  QgsLayerTree* root = project->layerTreeRoot();

  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    if (QgsLayerTreeLayer* n = root->findLayer(l->id())) {
      n->setItemVisibilityChecked(true);
      n->setExpanded(true);
    }
  }

  QList<QgsMapLayer*> ordered = root->layerOrder();
  if (ordered.isEmpty()) {
    const QMap<QString, QgsMapLayer*> all = project->mapLayers();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
      if (it.value() && it.value()->isValid())
        ordered.append(it.value());
    }
  }

  QList<QgsMapLayer*> visible;
  for (QgsMapLayer* l : ordered) {
    if (!l || !l->isValid()) continue;
    if (QgsLayerTreeLayer* n = root->findLayer(l->id())) {
      if (!n->itemVisibilityChecked()) continue;
    }
    visible.append(l);
  }
  if (visible.isEmpty()) {
    for (QgsMapLayer* l : project->mapLayers()) {
      if (l && l->isValid()) visible.append(l);
    }
  }

  if (project->crs().isValid())
    canvas->setDestinationCrs(project->crs());
  else
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::ensureOtfEnabled(project, canvas, canvas->mapSettings().destinationCrs().authid());

  canvas->setLayers(visible);
  canvas->setCachingEnabled(true);
  canvas->setRenderFlag(true);
  canvas->freeze(false);
  canvas->setPreviewJobsEnabled(true);
  canvas->setParallelRenderingEnabled(true);

  if (zoomKorea) {
    const QString auth = canvas->mapSettings().destinationCrs().isValid()
                             ? canvas->mapSettings().destinationCrs().authid()
                             : QStringLiteral("EPSG:5186");
    LayerOps::applyKoreaMapLimits(project, canvas);
    QgsRectangle kr = LayerOps::koreaExtentForCrs(auth);
    if (kr.isEmpty() || !kr.isFinite())
      kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    if (!kr.isEmpty() && kr.isFinite()) {
      canvas->setExtent(kr);
      canvas->zoomToFeatureExtent(kr);
      if (canvas->scale() > 3000000.0)
        canvas->zoomScale(1200000.0, true);
    }
    LayerOps::clampCanvasToKorea(canvas);
  }
  canvas->redrawAllLayers();
  canvas->refreshAllLayers();
  canvas->refresh();
}

void LayerOps::zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer) {
  if (!canvas || !layer || !layer->isValid()) return;
  QgsRectangle ext = layer->extent();
  if (ext.isEmpty() || !ext.isFinite()) {
    zoomToKorea(canvas, canvas->mapSettings().destinationCrs().authid());
    return;
  }
  const QgsCoordinateReferenceSystem layerCrs = layer->crs();
  const QgsCoordinateReferenceSystem mapCrs = canvas->mapSettings().destinationCrs();
  if (layerCrs.isValid() && mapCrs.isValid() && layerCrs.authid() != mapCrs.authid()) {
    try {
      QgsCoordinateTransform xf(layerCrs, mapCrs, QgsProject::instance()
                                                      ? QgsProject::instance()->transformContext()
                                                      : QgsCoordinateTransformContext());
      xf.setBallparkTransformsAreAppropriate(true);
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
      zoomToKorea(canvas, mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186"));
      return;
    }
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    zoomToKorea(canvas, mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186"));
    return;
  }
  ext.scale(1.02);
  canvas->setExtent(ext);
  canvas->zoomToFeatureExtent(ext);
  clampCanvasToKorea(canvas);
  canvas->refreshAllLayers();
  canvas->refresh();
}

void LayerOps::zoomToFullMax(QgsMapCanvas* canvas) {
  if (!canvas) return;
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  LayerOps::zoomToKorea(canvas, auth);
  LayerOps::clampCanvasToKorea(canvas);
}

static void syncCanvasToProject(QgsProject* project, QgsMapCanvas* canvas) {
  LayerOps::syncMapCanvas(project, canvas, true);
}

static QString friendlyBasemapError(const QString& raw) {
  const QString r = raw;
  if (r.contains(QStringLiteral("INVALID_KEY"), Qt::CaseInsensitive) ||
      r.contains(QStringLiteral("InvalidParameterValue"), Qt::CaseInsensitive) ||
      r.contains(QStringLiteral("등록되지 않은")) ||
      r.contains(QStringLiteral("인증키"))) {
    return QStringLiteral(
        "등록되지 않은 VWorld 키입니다. 도움말 → VWorld API 키 설정에서 확인하세요.");
  }
  return r;
}

static QgsRasterLayer* tryCreateXyzLayer(const QString& url, const QString& name, QString* errDetail) {
  auto* rl = new QgsRasterLayer(url, name, QStringLiteral("wms"));
  if (rl->isValid()) return rl;
  if (errDetail) *errDetail = friendlyBasemapError(rl->error().message());
  delete rl;
  return nullptr;
}

static bool addXyzBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& url,
                          const QString& name, QString* errorOut, bool crispText = false) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  ensureTileNetworkIdentity();

  {
    QStringList removeIds;
    for (QgsMapLayer* old : project->mapLayers()) {
      if (!old) continue;
      const QString n = old->name();
      if (n == name || n.startsWith(name + QLatin1String(" [")))
        removeIds.append(old->id());
    }
    for (const QString& id : removeIds)
      project->removeMapLayer(id);
  }

  QString detail;
  QgsRasterLayer* rl = tryCreateXyzLayer(url, name, &detail);
  if (!rl) {
    if (errorOut) {
      *errorOut = QStringLiteral("Basemap 실패 (%1): %2").arg(name, detail.isEmpty()
                                                                   ? QStringLiteral("invalid wms/xyz layer")
                                                                   : detail);
    }
    return false;
  }
  tuneBasemapLayer(rl, crispText);
  LayerOps::markReferenceLayer(rl);
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): addMapLayer rejected").arg(name);
    delete rl;
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(added->id()))
      node->setItemVisibilityChecked(true);
  }
  LayerOps::pruneEmptyLegendGroups(project);
  if (project->mapLayer(added->id()) == nullptr) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): layer removed after add").arg(name);
    return false;
  }
  if (canvas) {
    syncCanvasToProject(project, canvas);
    LayerOps::zoomToKorea(canvas, project && project->crs().isValid()
                                      ? project->crs().authid()
                                      : QStringLiteral("EPSG:5186"));
  }
  return project->mapLayer(added->id()) != nullptr;
}

bool LayerOps::addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  ensureTileNetworkIdentity();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&crs=EPSG:3857"),
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0"),
      QStringLiteral(
          "type=xyz&url=https://basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0&crs=EPSG:3857"),
      QStringLiteral(
          "type=xyz&url=https://a.basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0"),
  };
  QString lastErr;
  for (int i = 0; i < uris.size(); ++i) {
    const QString layerName = (i >= 2) ? QStringLiteral("Carto Light") : QStringLiteral("OSM");
    QString err;
    if (addXyzBasemap(project, canvas, uris.at(i), layerName, &err)) {
      if (canvas) {
        syncCanvasToProject(project, canvas);
        LayerOps::zoomToKorea(canvas, project && project->crs().isValid()
                                          ? project->crs().authid()
                                          : QStringLiteral("EPSG:5186"));
      }
      if (errorOut && i >= 2)
        *errorOut = QStringLiteral("OSM 대체: Carto Light 사용");
      return true;
    }
    lastErr = err;
  }
  if (errorOut) *errorOut = lastErr.isEmpty() ? QStringLiteral("OSM/Carto 타일 레이어 생성 실패") : lastErr;
  return false;
}

static bool requireVworldKey(const QString& apiKey, QString* errorOut) {
  if (!apiKey.trimmed().isEmpty()) return true;
  if (errorOut) {
    *errorOut = QStringLiteral(
        "VWorld API 키가 없습니다. 도움말 → VWorld API 키 설정에서 키를 입력하세요.");
  }
  return false;
}

static bool addBasemapWithFallbacks(QgsProject* project, QgsMapCanvas* canvas,
                                    const QStringList& uris, const QString& name,
                                    QString* errorOut) {
  QString last;
  for (const QString& uri : uris) {
    QString err;
    if (addXyzBasemap(project, canvas, uri, name, &err))
      return true;
    last = err;
  }
  if (errorOut) *errorOut = last;
  return false;
}

bool LayerOps::addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  // Official WMTS GetTile: /{key}/{layer}/{tileMatrix}/{tileRow}/{tileCol}.ext  => XYZ {z}/{y}/{x}
  // https://www.vworld.kr/dev/v4dv_wmtsguide_s001.do
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Base/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Base/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857"),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 배경"), errorOut);
}

bool LayerOps::addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
          "&zmax=19&zmin=0&crs=EPSG:3857")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
          "&zmax=19&zmin=0&crs=EPSG:3857"),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 위성"), errorOut);
}

static QString makeVworldWmsUri(const QString& apiKey, const QString& layers, const QString& styles,
                                const QString& crsAuthId) {
  const QString key = apiKey.trimmed();
  const QString baseUrl =
      QStringLiteral("https://api.vworld.kr/req/wms?KEY=%1&DOMAIN=localhost").arg(key);
  const QString encUrl = QString::fromLatin1(QUrl::toPercentEncoding(baseUrl));
  const QString stylePart = styles.isEmpty() ? QStringLiteral("styles")
                                             : QStringLiteral("styles=%1").arg(styles);
  return QStringLiteral(
             "IgnoreGetMapUrl=1&IgnoreGetFeatureInfoUrl=1&contextualWMSLegend=0"
             "&crs=%1&dpiMode=7&format=image/png&transparent=true&featureCount=10"
             "&tilePixelRatio=2&stepWidth=512&stepHeight=512"
             "&layers=%2&%3&url=%4")
      .arg(crsAuthId, layers, stylePart, encUrl);
}

bool LayerOps::addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString uriBoth = makeVworldWmsUri(
      apiKey, QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBon = makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bonbun"), QString(),
                                          QStringLiteral("EPSG:3857"));
  const QString uriBu = makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bubun"), QString(),
                                         QStringLiteral("EPSG:3857"));

  QString err;
  bool ok = addXyzBasemap(project, canvas, uriBoth, QStringLiteral("VWorld 지적(본번·부번)"), &err, true);
  if (!ok) {
    const bool okBon = addXyzBasemap(project, canvas, uriBon, QStringLiteral("VWorld 지적 본번"), &err, true);
    const bool okBu = addXyzBasemap(project, canvas, uriBu, QStringLiteral("VWorld 지적 부번"), &err, true);
    ok = okBon || okBu;
  }
  if (!ok) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("지적도 WMS 추가 실패") : err;
    return false;
  }
  if (canvas) {
    LayerOps::syncMapCanvas(project, canvas, false);
    const double s = canvas->scale();
    if (s > 6000.0 || s < 500.0)
      canvas->zoomScale(3000.0, true);
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  return true;
}

bool LayerOps::addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Hybrid/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Hybrid/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857"),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 하이브리드"), errorOut);
}

bool LayerOps::addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QStringList uris = {
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QStringLiteral("lt_c_upisuq"),
                       QStringLiteral("EPSG:3857")),
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QString(), QStringLiteral("EPSG:3857")),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 등고선"), errorOut);
}

static QList<QgsMapLayer*> layersMatchingBaseName(QgsProject* project, const QString& name) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n == name || n.startsWith(name + QLatin1String(" [")))
      out.append(l);
  }
  return out;
}

bool LayerOps::setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  const double op = qBound(0.0, opacity, 1.0);
  for (QgsMapLayer* l : layers) {
    if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      rl->setOpacity(op);
    }
  }
  if (canvas) canvas->refreshAllLayers();
  return true;
}

bool LayerOps::toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  for (QgsMapLayer* l : layers) {
    if (!l) continue;
    if (QgsLayerTreeLayer* node = root->findLayer(l->id())) {
      node->setItemVisibilityChecked(visible);
    }
  }
  if (canvas) canvas->refreshAllLayers();
  return true;
}

bool LayerOps::addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                               QString* errorOut) {
  const QString vworldKey = VworldSettings::loadApiKey();
  switch (kind) {
  case KoreaBasemap::VWorldBase:
    return addVworldBaseMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldSatellite:
    return addVworldSatelliteMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldHybrid:
    return addVworldHybridMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::GoogleRoad: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Dm%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 도로"), errorOut);
  }
  case KoreaBasemap::GoogleSatellite: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 위성"), errorOut);
  }
  case KoreaBasemap::Osm:
  default:
    return addOsmBasemap(project, canvas, errorOut);
  }
}

bool LayerOps::ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId) {
  const QString auth = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId.trimmed();
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) return false;

  QgsCoordinateTransformContext ctx;
  if (project) {
    ctx = project->transformContext();
    project->setTransformContext(ctx);
    project->setCrs(crs);
  }
  if (canvas)
    canvas->setDestinationCrs(crs);
  return true;
}

bool LayerOps::setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                          QString* errorOut, bool zoomKorea) {
  const QgsCoordinateReferenceSystem crs(epsgAuthId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid CRS %1").arg(epsgAuthId);
    return false;
  }
  QgsRectangle prev;
  QgsCoordinateReferenceSystem prevCrs;
  if (canvas) {
    prev = canvas->extent();
    prevCrs = canvas->mapSettings().destinationCrs();
  }
  ensureOtfEnabled(project, canvas, epsgAuthId);
  if (canvas) {
    if (zoomKorea) {
      zoomToKorea(canvas, epsgAuthId);
    } else if (!prev.isEmpty() && prevCrs.isValid() && prevCrs != crs) {
      try {
        QgsCoordinateTransform xf(prevCrs, crs, project ? project->transformContext()
                                                        : QgsCoordinateTransformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        canvas->setExtent(xf.transformBoundingBox(prev));
      } catch (...) {
        zoomToKorea(canvas, epsgAuthId);
      }
    } else if (!prev.isEmpty()) {
      canvas->setExtent(prev);
    }
    canvas->refresh();
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

void LayerOps::applyKoreaMapLimits(QgsProject* project, QgsMapCanvas* canvas) {
  const QString auth = (project && project->crs().isValid())
                           ? project->crs().authid()
                           : (canvas && canvas->mapSettings().destinationCrs().isValid()
                                  ? canvas->mapSettings().destinationCrs().authid()
                                  : QStringLiteral("EPSG:5186"));
  QgsRectangle kr = koreaExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return;
  const QgsCoordinateReferenceSystem crs(auth);
  if (project && crs.isValid() && project->viewSettings()) {
    const QgsReferencedRectangle ref(kr, crs);
    project->viewSettings()->setPresetFullExtent(ref);
    project->viewSettings()->setDefaultViewExtent(ref);
  }
  if (canvas) {
    if (crs.isValid())
      canvas->setDestinationCrs(crs);
    canvas->setExtent(kr);
    canvas->refresh();
  }
}

bool LayerOps::clampCanvasToKorea(QgsMapCanvas* canvas) {
  if (!canvas) return false;
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  QgsRectangle kr = koreaExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return false;
  kr.scale(1.02);

  const QgsRectangle cur = canvas->extent();
  if (cur.isEmpty() || !cur.isFinite()) {
    canvas->setExtent(kr);
    return true;
  }

  if (cur.width() > kr.width() * 1.12 || cur.height() > kr.height() * 1.12) {
    canvas->setExtent(kr);
    return true;
  }

  double minX = cur.xMinimum();
  double maxX = cur.xMaximum();
  double minY = cur.yMinimum();
  double maxY = cur.yMaximum();
  const double w = cur.width();
  const double h = cur.height();

  if (minX < kr.xMinimum()) {
    minX = kr.xMinimum();
    maxX = minX + w;
  }
  if (maxX > kr.xMaximum()) {
    maxX = kr.xMaximum();
    minX = maxX - w;
  }
  if (minY < kr.yMinimum()) {
    minY = kr.yMinimum();
    maxY = minY + h;
  }
  if (maxY > kr.yMaximum()) {
    maxY = kr.yMaximum();
    minY = maxY - h;
  }

  if (w >= kr.width()) {
    const double cx = kr.center().x();
    minX = cx - w * 0.5;
    maxX = cx + w * 0.5;
  }
  if (h >= kr.height()) {
    const double cy = kr.center().y();
    minY = cy - h * 0.5;
    maxY = cy + h * 0.5;
  }

  const QgsRectangle clamped(minX, minY, maxX, maxY);
  const double eps = qMax(kr.width(), kr.height()) * 1e-9;
  if (qAbs(clamped.xMinimum() - cur.xMinimum()) > eps ||
      qAbs(clamped.yMinimum() - cur.yMinimum()) > eps ||
      qAbs(clamped.xMaximum() - cur.xMaximum()) > eps ||
      qAbs(clamped.yMaximum() - cur.yMaximum()) > eps) {
    canvas->setExtent(clamped);
    return true;
  }
  return false;
}

void LayerOps::zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId) {
  if (!canvas) return;
  QgsRectangle ext = koreaExtentForCrs(epsgAuthId);
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    const QgsCoordinateReferenceSystem destCrs = canvas->mapSettings().destinationCrs();
    if (destCrs.isValid() && destCrs.authid() != QLatin1String("EPSG:3857")) {
      try {
        const QgsCoordinateTransform xf(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")),
                                        destCrs, QgsCoordinateTransformContext());
        ext = xf.transformBoundingBox(ext);
      } catch (...) {
      }
    }
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = QgsRectangle(124.5, 33.0, 132.0, 39.5);
  }
  canvas->setExtent(ext);
  canvas->setRenderFlag(true);
  canvas->freeze(false);
  canvas->refreshAllLayers();
  canvas->refresh();
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

  bool lsSolved = false;
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
    if (solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxX, syX, sX, a, b, c)
        && solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxY, syY, sY, d, e, fpar)) {
      rotA = a; rotB = b; rotD = d; rotE = e;
      ulx = c + 0.5 * (rotA + rotB);
      uly = fpar + 0.5 * (rotD + rotE);
      lsSolved = true;
    }
  }

  if (!lsSolved) {
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

bool LayerOps::hasVisibleReferenceLayer(QgsProject* project) {
  if (!project) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;

  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l || !isReferenceLayer(l)) continue;
    QgsLayerTreeLayer* node = root->findLayer(l->id());
    if (node && node->isVisible()) return true;
  }
  return false;
}

bool LayerOps::removeConfirmedLayers(QgsProject* project, QgsMapCanvas* canvas, const QStringList& layerIds) {
  if (!project || layerIds.isEmpty()) return false;
  for (const QString& id : layerIds) {
    project->removeMapLayer(id);
  }
  if (canvas) {
    canvas->refreshAllLayers();
  }
  return true;
}

static QString normalizeCsvHeader(QString h) {
  h = h.trimmed().toLower();
  h.remove(QLatin1Char('"'));
  h.replace(QLatin1Char(' '), QLatin1Char('_'));
  if (h == QLatin1String("id") || h == QLatin1String("point") || h == QLatin1String("pid"))
    return QStringLiteral("point_id");
  if (h == QLatin1String("lon") || h == QLatin1String("easting") || h == QLatin1String("east"))
    return QStringLiteral("x");
  if (h == QLatin1String("lat") || h == QLatin1String("northing") || h == QLatin1String("north"))
    return QStringLiteral("y");
  if (h == QLatin1String("acc") || h == QLatin1String("accuracy"))
    return QStringLiteral("accuracy_m");
  if (h == QLatin1String("fix") || h == QLatin1String("fixtype"))
    return QStringLiteral("fix_type");
  if (h == QLatin1String("proj") || h == QLatin1String("crs"))
    return QStringLiteral("projection");
  return h;
}

int LayerOps::importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath, QString* errorOut) {
  if (!controlPoints || !controlPoints->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 레이어가 없습니다. 먼저 새 조사를 만드세요.");
    return -1;
  }
  QFile f(csvPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("CSV를 열 수 없습니다: %1").arg(csvPath);
    return -1;
  }
  QTextStream ts(&f);
  const QRegularExpression sep(QStringLiteral("[,;\\t]"));

  QStringList headers;
  QList<QStringList> rows;
  while (!ts.atEnd()) {
    const QString line = ts.readLine().trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
    const QStringList parts = line.split(sep);
    if (parts.isEmpty()) continue;
    const QString h0 = normalizeCsvHeader(parts.first());
    if (headers.isEmpty() &&
        (h0 == QLatin1String("point_id") || h0 == QLatin1String("x") ||
         parts.first().trimmed().compare(QStringLiteral("id"), Qt::CaseInsensitive) == 0 ||
         parts.first().trimmed().compare(QStringLiteral("point_id"), Qt::CaseInsensitive) == 0)) {
      for (const QString& p : parts) headers.append(normalizeCsvHeader(p));
      continue;
    }
    if (parts.size() < 3) continue;
    rows.append(parts);
  }
  if (headers.isEmpty()) {
    headers = {QStringLiteral("point_id"), QStringLiteral("x"), QStringLiteral("y"),
               QStringLiteral("datum"), QStringLiteral("ellipsoid"), QStringLiteral("projection"),
               QStringLiteral("accuracy_m"), QStringLiteral("pdop"), QStringLiteral("fix_type")};
  }
  if (rows.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 가져올 데이터 행이 없습니다.");
    return 0;
  }

  auto col = [&](const QString& name) -> int {
    return headers.indexOf(name);
  };
  const int iId = col(QStringLiteral("point_id"));
  const int iX = col(QStringLiteral("x"));
  const int iY = col(QStringLiteral("y"));
  if (iX < 0 || iY < 0) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 x,y(또는 lon/lat) 열이 필요합니다.");
    return -1;
  }

  if (!controlPoints->isEditable() && !controlPoints->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 편집 모드 실패");
    return -1;
  }

  int added = 0;
  for (const QStringList& p : rows) {
    auto cell = [&](int idx) -> QString {
      if (idx < 0 || idx >= p.size()) return {};
      return p.at(idx).trimmed().remove(QLatin1Char('"'));
    };
    bool okX = false, okY = false;
    const double x = cell(iX).toDouble(&okX);
    const double y = cell(iY).toDouble(&okY);
    if (!okX || !okY) continue;

    QgsFeature feat(controlPoints->fields());
    const QString pid = iId >= 0 ? cell(iId) : QStringLiteral("P%1").arg(added + 1);
    auto setStr = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi >= 0 && idx >= 0) feat.setAttribute(fi, cell(idx));
    };
    auto setNum = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi < 0 || idx < 0) return;
      bool ok = false;
      const double v = cell(idx).toDouble(&ok);
      if (ok) feat.setAttribute(fi, v);
      else if (!cell(idx).isEmpty()) feat.setAttribute(fi, cell(idx));
    };
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("point_id"));
      if (fi >= 0) feat.setAttribute(fi, pid);
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("x"));
      if (fi >= 0) feat.setAttribute(fi, x);
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("y"));
      if (fi >= 0) feat.setAttribute(fi, y);
    }
    setStr("datum", col(QStringLiteral("datum")));
    setStr("ellipsoid", col(QStringLiteral("ellipsoid")));
    setStr("projection", col(QStringLiteral("projection")));
    setStr("origin", col(QStringLiteral("origin")));
    setStr("fix_type", col(QStringLiteral("fix_type")));
    setNum("accuracy_m", col(QStringLiteral("accuracy_m")));
    setNum("pdop", col(QStringLiteral("pdop")));
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("accuracy"));
      const int ia = col(QStringLiteral("accuracy_m"));
      if (fi >= 0 && ia >= 0 && !cell(ia).isEmpty()) feat.setAttribute(fi, cell(ia));
    }
    feat.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(x, y)));
    if (controlPoints->addFeature(feat)) ++added;
  }

  if (!controlPoints->commitChanges()) {
    if (errorOut) {
      *errorOut = QStringLiteral("커밋 실패: %1").arg(controlPoints->commitErrors().join(QLatin1Char(';')));
    }
    controlPoints->rollBack();
    return -1;
  }
  return added;
}

