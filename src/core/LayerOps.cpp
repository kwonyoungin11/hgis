#include "LayerOps.h"
#include "VworldSettings.h"
#include <QDateTime>
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
#include <QColor>
#include <QFont>
#include <QDir>
#include <QTemporaryFile>

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
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
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
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
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

bool LayerOps::applyDomainDrawStyle(QgsVectorLayer* layer, const QString& layerKeyIn) {
  if (!layer || !layer->isValid()) return false;
  const QString key = layerKeyIn.isEmpty() ? layerKeyOf(layer) : layerKeyIn;
  const Qgis::GeometryType gt = layer->geometryType();

  QColor fill(37, 99, 235, 90);
  QColor stroke(37, 99, 235, 255);
  double strokeW = 1.2;
  double markerSize = 3.5;

  if (key == QLatin1String("survey_area")) {
    fill = QColor(180, 83, 9, 70);
    stroke = QColor(146, 64, 14, 255);
    strokeW = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    fill = QColor(22, 163, 74, 90);
    stroke = QColor(21, 128, 61, 255);
    strokeW = 1.4;
  } else if (key == QLatin1String("feature_line") || key == QLatin1String("section_line")) {
    stroke = key == QLatin1String("section_line") ? QColor(190, 24, 93, 255) : QColor(202, 138, 4, 255);
    strokeW = 1.8;
  } else if (key == QLatin1String("control_points")) {
    fill = QColor(234, 179, 8, 255);
    stroke = QColor(161, 98, 7, 255);
    markerSize = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    fill = QColor(185, 28, 28, 255);
    stroke = QColor(127, 29, 29, 255);
    markerSize = 3.6;
  }

  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    auto fs = QgsFillSymbol::createSimple({
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(strokeW)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
    });
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(strokeW)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
    });
    sym = ls.release();
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QStringLiteral("0.6")},
        {QStringLiteral("size"), QString::number(markerSize)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    sym = QgsSymbol::defaultSymbol(gt);
    if (sym)
      sym->setColor(stroke);
  }
  if (!sym) return false;
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeW);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSize);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  if (gt == Qgis::GeometryType::Polygon)
    applyAreaM2Labels(layer);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::applyAreaM2Labels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("format_number(area($geometry), 2) || ' ㎡'");
  s.isExpression = true;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(9);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(9);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(15, 23, 42));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fillIn, const QColor& strokeIn,
                                      double strokeWidthMm, double markerSizeMm, bool noFill,
                                      bool noStroke, bool dashed) {
  if (!layer || !layer->isValid()) return false;
  QColor fill = fillIn.isValid() ? fillIn : QColor(37, 99, 235, 90);
  QColor stroke = strokeIn.isValid() ? strokeIn : QColor(37, 99, 235, 255);
  if (strokeWidthMm <= 0.0) strokeWidthMm = 1.0;
  if (markerSizeMm <= 0.0) markerSizeMm = 3.5;
  if (noFill) fill = QColor(0, 0, 0, 0);
  if (noStroke) stroke = QColor(0, 0, 0, 0);
  if (noFill && noStroke) {
    noStroke = false;
    stroke = QColor(100, 100, 100, 255);
    strokeWidthMm = 0.4;
  }

  const Qgis::GeometryType gt = layer->geometryType();
  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    QVariantMap props{
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("style"), noFill ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("outline_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    };
    auto fs = QgsFillSymbol::createSimple(props);
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    });
    sym = ls.release();
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), noFill ? QStringLiteral("#00000000") : fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), noStroke ? QStringLiteral("0") : QStringLiteral("0.6")},
        {QStringLiteral("outline_style"), noStroke ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("size"), QString::number(markerSizeMm)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    return false;
  }
  if (!sym) return false;

  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeWidthMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSizeMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_fill"), noFill);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_stroke"), noStroke);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_dashed"), dashed);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  layer->triggerRepaint();
  return true;
}

bool LayerOps::readSimpleVectorStyle(const QgsVectorLayer* layer, QColor* fill, QColor* stroke,
                                     double* strokeWidthMm, double* markerSizeMm, bool* noFill,
                                     bool* noStroke, bool* dashed) {
  if (!layer) return false;

  QColor f(37, 99, 235, 90);
  QColor s(37, 99, 235, 255);
  double w = 1.2;
  double m = 3.5;
  bool nf = false;
  bool ns = false;
  bool dash = false;

  const QString key = layerKeyOf(layer);
  if (key == QLatin1String("survey_area")) {
    f = QColor(180, 83, 9, 70);
    s = QColor(146, 64, 14, 255);
    w = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    f = QColor(22, 163, 74, 90);
    s = QColor(21, 128, 61, 255);
    w = 1.4;
  } else if (key == QLatin1String("feature_line")) {
    s = QColor(202, 138, 4, 255);
    w = 1.8;
  } else if (key == QLatin1String("section_line")) {
    s = QColor(190, 24, 93, 255);
    w = 1.8;
  } else if (key == QLatin1String("control_points")) {
    f = QColor(234, 179, 8, 255);
    s = QColor(161, 98, 7, 255);
    m = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    f = QColor(185, 28, 28, 255);
    s = QColor(127, 29, 29, 255);
    m = 3.6;
  }

  const QVariant cf = layer->customProperty(QStringLiteral("ka_hgis/style_fill"));
  const QVariant cs = layer->customProperty(QStringLiteral("ka_hgis/style_stroke"));
  const QVariant cw = layer->customProperty(QStringLiteral("ka_hgis/style_width_mm"));
  const QVariant cm = layer->customProperty(QStringLiteral("ka_hgis/style_marker_mm"));
  const QVariant cnf = layer->customProperty(QStringLiteral("ka_hgis/style_no_fill"));
  const QVariant cns = layer->customProperty(QStringLiteral("ka_hgis/style_no_stroke"));
  const QVariant cd = layer->customProperty(QStringLiteral("ka_hgis/style_dashed"));
  if (cf.isValid()) {
    const QColor parsed(cf.toString());
    if (parsed.isValid()) f = parsed;
  }
  if (cs.isValid()) {
    const QColor parsed(cs.toString());
    if (parsed.isValid()) s = parsed;
  }
  if (cw.isValid()) w = cw.toDouble();
  if (cm.isValid()) m = cm.toDouble();
  if (cnf.isValid()) nf = cnf.toBool();
  if (cns.isValid()) ns = cns.toBool();
  if (cd.isValid()) dash = cd.toBool();
  if (f.alpha() == 0) nf = true;
  if (s.alpha() == 0) ns = true;

  if (const QgsFeatureRenderer* ren = layer->renderer()) {
    if (const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(ren)) {
      if (const QgsSymbol* sym = single->symbol()) {
        if (sym->color().isValid()) {
          if (layer->geometryType() == Qgis::GeometryType::Line)
            s = sym->color();
          else if (!nf)
            f = sym->color();
        }
      }
    }
  }

  if (fill) *fill = f;
  if (stroke) *stroke = s;
  if (strokeWidthMm) *strokeWidthMm = w;
  if (markerSizeMm) *markerSizeMm = m;
  if (noFill) *noFill = nf;
  if (noStroke) *noStroke = ns;
  if (dashed) *dashed = dash;
  return true;
}

bool LayerOps::applyFeaturePolyStyle(QgsVectorLayer* featurePoly) {
  if (!featurePoly || !featurePoly->isValid()) return false;
  QString field = QStringLiteral("kind");
  if (featurePoly->fields().indexOf(field) < 0) field = QStringLiteral("period");
  if (featurePoly->fields().indexOf(field) < 0)
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QSet<QString> values;
  QgsFeatureIterator it = featurePoly->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString v = f.attribute(field).toString().trimmed();
    if (!v.isEmpty()) values.insert(v);
  }
  if (values.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QgsCategoryList cats;
  int i = 0;
  const QList<QString> sorted = values.values();
  for (const QString& v : sorted) {
    QColor c = QColor::fromHsv((i * 47) % 360, 180, 230, 160);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(featurePoly->geometryType());
    if (sym) {
      sym->setColor(c);
      cats.append(QgsRendererCategory(QVariant(v), sym, v));
    }
    ++i;
  }
  if (cats.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));
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

static void applyKaNetworkHeaders(QNetworkRequest* req) {
  if (!req) return;
  req->setHeader(QNetworkRequest::UserAgentHeader,
                 QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3 QGIS"));
  const QString host = req->url().host();
  if (host.contains(QLatin1String("vworld.kr"), Qt::CaseInsensitive))
    req->setRawHeader("Referer", "https://localhost");
}

static void ensureTileNetworkIdentity() {
  static bool once = false;
  if (once) return;
  once = true;
  QgsNetworkAccessManager::instance()->setCacheDisabled(false);
  QgsNetworkAccessManager::setRequestPreprocessor(&applyKaNetworkHeaders);
}

static bool uriLooksLikeXyz(const QString& source) {
  return source.contains(QLatin1String("type=xyz"), Qt::CaseInsensitive);
}

static void tuneBasemapLayer(QgsRasterLayer* rl, bool crispText = false) {
  if (!rl || !rl->isValid()) return;
  rl->setBlendMode(QPainter::CompositionMode_SourceOver);
  const QString src = rl->source();
  if (src.contains(QLatin1String("crs=EPSG:4326"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
  } else if (uriLooksLikeXyz(src) || src.contains(QLatin1String("ka-hgis-vworld-cadastral"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:3857"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:900913"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  } else if (!rl->crs().isValid()) {
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

static void zoomCanvasToWorkingScale(QgsMapCanvas* canvas, const QString& crsAuthId,
                                     double targetScale = 50000.0) {
  if (!canvas) return;
  const QString auth = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : crsAuthId;
  const QgsRectangle cur = canvas->extent();
  const bool hasLocalView = !cur.isEmpty() && cur.isFinite() && cur.width() > 0 && cur.height() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
  if (!hasLocalView)
    LayerOps::zoomToKorea(canvas, auth);
  if (targetScale > 0.0)
    canvas->zoomScale(targetScale, true);
  LayerOps::clampCanvasToKorea(canvas);
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

void LayerOps::setAlignPending(QgsMapLayer* layer, bool pending) {
  if (!layer) return;
  if (pending)
    layer->setCustomProperty(QString::fromUtf8(kPropAlignPending), true);
  else
    layer->removeCustomProperty(QString::fromUtf8(kPropAlignPending));
}

bool LayerOps::isAlignPending(const QgsMapLayer* layer) {
  return layer && layer->customProperty(QString::fromUtf8(kPropAlignPending)).toBool();
}

QString LayerOps::layerKeyOf(const QgsMapLayer* layer) {
  if (!layer) return {};
  return layer->customProperty(QString::fromUtf8(kPropLayerKey)).toString();
}

static QString stripLegendCrsSuffix(QString name) {
  int bracket = name.lastIndexOf(QStringLiteral(" [EPSG:"));
  if (bracket < 0) bracket = name.lastIndexOf(QStringLiteral(" [CRS"));
  if (bracket >= 0) name = name.left(bracket).trimmed();
  return name;
}

static QString friendlyLegendName(const QString& name) {
  const QString base = stripLegendCrsSuffix(name);
  if (base.contains(QStringLiteral("지적"))) {
    const bool bon = base.contains(QStringLiteral("본번"));
    const bool bu = base.contains(QStringLiteral("부번"));
    if (bon && !bu) return QStringLiteral("지적 본번");
    if (bu && !bon) return QStringLiteral("지적 부번");
    return QStringLiteral("지적");
  }
  if (base.contains(QStringLiteral("위성")))
    return QStringLiteral("위성");
  return base;
}

static bool legendTitlesMatch(const QString& a, const QString& b) {
  if (a == b) return true;
  if (a.startsWith(b + QLatin1String(" [")) || b.startsWith(a + QLatin1String(" [")))
    return true;
  return friendlyLegendName(a) == friendlyLegendName(b);
}

void LayerOps::applyLegendCrsLabel(QgsMapLayer* layer) {
  if (!layer) return;
  const QString shown = friendlyLegendName(layer->name());
  if (!shown.isEmpty() && layer->name() != shown)
    layer->setName(shown);
  const QString auth = layer->crs().isValid() ? layer->crs().authid() : QString();
  if (!auth.isEmpty()) {
    layer->setAbstract(QStringLiteral("좌표계 %1").arg(auth));
    layer->setCustomProperty(QStringLiteral("ka_hgis/crs_label"), auth);
  }
}

bool LayerOps::isReferenceLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleReference))
    return true;
  const QString n = layer->name();
  return n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("VWorld")) ||
         n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google")) ||
         n == QLatin1String("위성") || n.startsWith(QLatin1String("지적"));
}

bool LayerOps::isBasemapLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  // Live tiles only. A user SHP named "지적…" must not survive 새 조사.
  const QString p = layer->providerType();
  return p == QLatin1String("wms") || p == QLatin1String("xyz") || p == QLatin1String("vectortile");
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

QgsVectorLayer* LayerOps::digitizeTargetLayer(QgsProject* project, QgsVectorLayer* current,
                                              const QString& requiredKey) {
  if (requiredKey.isEmpty())
    return nullptr;
  if (current && current->isValid() && layerKeyOf(current) == requiredKey)
    return current;
  return findByLayerKey(project, requiredKey);
}

QStringList LayerOps::domainLayerKeys() {
  return {QStringLiteral("survey_area"), QStringLiteral("feature_poly"), QStringLiteral("feature_line"),
          QStringLiteral("section_line"), QStringLiteral("control_points"),
          QStringLiteral("artifact_point")};
}

void LayerOps::removeSurveyDomainLayers(QgsProject* project) {
  if (!project) return;
  QStringList ids;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    if (isBasemapLayer(l)) continue;
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable())
        v->rollBack();
    }
    ids.append(l->id());
  }
  if (!ids.isEmpty())
    project->removeMapLayers(ids);
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
  if (!vl->isValid() && layerKey == QLatin1String("artifact_point")) {
    delete vl;
    vl = nullptr;
    const QgsCoordinateReferenceSystem crs = project->crs().isValid()
        ? project->crs()
        : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    QgsVectorLayer mem(QStringLiteral("Point?crs=%1").arg(crs.authid()), titleKo, QStringLiteral("memory"));
    if (mem.isValid()) {
      QgsFields fields;
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("artifact_no"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      mem.dataProvider()->addAttributes(fields.toList());
      mem.updateFields();
      mem.setCrs(crs);
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("GPKG");
      opts.layerName = layerKey;
      opts.fileEncoding = QStringLiteral("UTF-8");
      opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      QString errMsg, newFn, newLayer;
      if (QgsVectorFileWriter::writeAsVectorFormatV3(
              &mem, gpkgPath, project->transformContext(), opts, &errMsg, &newFn, &newLayer) ==
          QgsVectorFileWriter::NoError) {
        vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
      } else if (errorOut) {
        *errorOut = errMsg;
      }
    }
  }
  if (!vl || !vl->isValid()) {
    if (errorOut && errorOut->isEmpty())
      *errorOut = vl ? vl->error().message() : QStringLiteral("레이어를 열 수 없습니다.");
    delete vl;
    return nullptr;
  }
  vl->setName(titleKo);
  markSurveyLayer(vl, layerKey);
  applyLegendCrsLabel(vl);
  applyDomainDrawStyle(vl, layerKey);
  project->addMapLayer(vl, true);
  pruneEmptyLegendGroups(project);
  return vl;
}

QgsVectorLayer* LayerOps::createUserPolygonLayer(QgsProject* project, const QString& gpkgPath,
                                                 const QString& titleKo, const QString& crsAuthId,
                                                 QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  QString crsId = crsAuthId.trimmed();
  if (crsId.isEmpty()) crsId = QStringLiteral("EPSG:5186");
  QgsCoordinateReferenceSystem crs(crsId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계가 올바르지 않습니다.");
    return nullptr;
  }

  const QString key = QStringLiteral("user_poly_%1").arg(QDateTime::currentMSecsSinceEpoch());
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=%1").arg(crs.authid()), titleKo, QStringLiteral("memory"));
  if (!mem.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("면 레이어를 만들 수 없습니다.");
    return nullptr;
  }
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  mem.setCrs(crs);

  QString loadPath;
  if (!gpkgPath.isEmpty() && QFile::exists(gpkgPath)) {
    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = key;
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QString errMsg, newFn, newLayer;
    const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
        &mem, gpkgPath, QgsCoordinateTransformContext(), opts, &errMsg, &newFn, &newLayer);
    if (we != QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errMsg.isEmpty() ? QStringLiteral("GPKG에 레이어를 쓰지 못했습니다.") : errMsg;
      return nullptr;
    }
    loadPath = QStringLiteral("%1|layername=%2").arg(gpkgPath, key);
  } else {
    loadPath = QStringLiteral("Polygon?crs=%1").arg(crs.authid());
  }

  auto* vl = new QgsVectorLayer(loadPath, titleKo, gpkgPath.isEmpty() ? QStringLiteral("memory")
                                                                      : QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = vl->error().message();
    delete vl;
    return nullptr;
  }
  if (gpkgPath.isEmpty()) {
    vl->dataProvider()->addAttributes(fields.toList());
    vl->updateFields();
    vl->setCrs(crs);
  }
  vl->setName(titleKo);
  markSurveyLayer(vl, key);
  applyLegendCrsLabel(vl);
  applyAreaM2Labels(vl);
  project->addMapLayer(vl, true);
  placeInLegendGroup(project, vl, QString::fromUtf8(kGroupSurveyData));
  pruneEmptyLegendGroups(project);
  return vl;
}

QList<QgsMapLayer*> LayerOps::visibleLayersPaintOrder(QgsProject* project) {
  QList<QgsMapLayer*> visible;
  if (!project) return visible;
  QgsLayerTree* root = project->layerTreeRoot();
  QList<QgsMapLayer*> ordered = root ? root->layerOrder() : QList<QgsMapLayer*>();
  if (ordered.isEmpty()) {
    const QMap<QString, QgsMapLayer*> all = project->mapLayers();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
      if (it.value() && it.value()->isValid())
        ordered.append(it.value());
    }
  }

  auto pushVisible = [&](QgsMapLayer* l) {
    if (!l || !l->isValid()) return;
    if (isAlignPending(l)) return;
    if (root) {
      if (QgsLayerTreeLayer* n = root->findLayer(l->id())) {
        if (!n->itemVisibilityChecked()) return;
      }
    }
    visible.append(l);
  };
  for (QgsMapLayer* l : ordered)
    pushVisible(l);
  if (visible.isEmpty()) {
    for (QgsMapLayer* l : project->mapLayers())
      pushVisible(l);
  }
  return visible;
}

void LayerOps::syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea) {
  if (!project || !canvas) return;

  QList<QgsMapLayer*> visible = visibleLayersPaintOrder(project);
  const bool layersChanged = (visible != canvas->layers());

  if (project->crs().isValid())
    canvas->setDestinationCrs(project->crs());
  else
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::ensureOtfEnabled(project, canvas, canvas->mapSettings().destinationCrs().authid());

  if (layersChanged)
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
  if (layersChanged || zoomKorea)
    canvas->refresh();
}

static bool extentUsable(const QgsRectangle& ext) {
  return !ext.isNull() && ext.isFinite();
}

static QgsRectangle vectorFeatureExtent(QgsVectorLayer* vl) {
  if (!vl) return {};
  vl->updateExtents();
  QgsRectangle ext = vl->extent();
  if (extentUsable(ext)) return ext;
  QgsRectangle acc;
  bool any = false;
  QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setNoAttributes());
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    const QgsRectangle b = f.geometry().boundingBox();
    if (!extentUsable(b)) continue;
    if (!any) {
      acc = b;
      any = true;
    } else {
      acc.combineExtentWith(b);
    }
  }
  return any ? acc : QgsRectangle();
}

bool LayerOps::zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer) {
  if (!canvas || !layer || !layer->isValid()) return false;

  QgsRectangle ext;
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer))
    ext = vectorFeatureExtent(vl);
  else
    ext = layer->extent();

  const QgsCoordinateReferenceSystem layerCrs = layer->crs();
  const QgsCoordinateReferenceSystem mapCrs = canvas->mapSettings().destinationCrs().isValid()
                                                  ? canvas->mapSettings().destinationCrs()
                                                  : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
  const QString mapAuth = mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = koreaExtentForCrs(mapAuth);

  if (layerCrs.isValid() && mapCrs.isValid() && extentUsable(ext) &&
      layerCrs.authid() != mapCrs.authid()) {
    try {
      QgsCoordinateTransform xf(layerCrs, mapCrs, QgsProject::instance()
                                                      ? QgsProject::instance()->transformContext()
                                                      : QgsCoordinateTransformContext());
      xf.setBallparkTransformsAreAppropriate(true);
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
      if (qobject_cast<QgsRasterLayer*>(layer)) {
        zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
        canvas->refreshAllLayers();
        canvas->refresh();
        return true;
      }
      return false;
    }
  }

  const bool worldLike = !extentUsable(ext) ||
                         (!kr.isNull() && kr.isFinite() &&
                          (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2));
  if (qobject_cast<QgsRasterLayer*>(layer) && worldLike) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    canvas->refreshAllLayers();
    canvas->refresh();
    return true;
  }

  if (!extentUsable(ext))
    return false;

  if (!kr.isEmpty() && kr.isFinite() && (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2)) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    canvas->refreshAllLayers();
    canvas->refresh();
    return true;
  }

  const double minW = mapCrs.isGeographic() ? 0.004 : 80.0;
  if (ext.width() < minW || ext.height() < minW) {
    const QgsPointXY c = ext.center();
    const double pad = minW * 0.5;
    ext = QgsRectangle(c.x() - pad, c.y() - pad, c.x() + pad, c.y() + pad);
  }
  ext.scale(1.15);
  canvas->setExtent(ext);
  canvas->zoomToFeatureExtent(ext);
  const QgsRectangle after = canvas->extent();
  if (!kr.isEmpty() && kr.isFinite() &&
      (after.width() > kr.width() * 1.12 || after.height() > kr.height() * 1.12))
    clampCanvasToKorea(canvas);
  canvas->refreshAllLayers();
  canvas->refresh();
  return true;
}

bool LayerOps::isolateAndZoomToLayer(QgsProject* project, QgsMapCanvas* canvas, QgsMapLayer* layer,
                                     bool keepReference) {
  if (!layer || !layer->isValid()) return false;
  if (canvas && !zoomToLayerMax(canvas, layer))
    return false;

  if (project) {
    if (QgsLayerTree* root = project->layerTreeRoot()) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (!l) continue;
        QgsLayerTreeLayer* n = root->findLayer(l->id());
        if (!n) continue;
        const bool show =
            (l == layer) ||
            (keepReference && !isReferenceLayer(layer) && isReferenceLayer(l));
        n->setItemVisibilityChecked(show);
      }
    }
  }
  if (canvas) {
    QList<QgsMapLayer*> vis;
    vis.append(layer);
    if (project && keepReference && !isReferenceLayer(layer)) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (l && l != layer && l->isValid() && isReferenceLayer(l))
          vis.append(l);
      }
    }
    canvas->setLayers(vis);
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  return true;
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
  LayerOps::syncMapCanvas(project, canvas, false);
}

static bool isFatalVworldAuthError(const QString& raw) {
  return raw.contains(QStringLiteral("INVALID_KEY"), Qt::CaseInsensitive) ||
         raw.contains(QStringLiteral("등록되지 않은")) ||
         raw.contains(QStringLiteral("인증키")) ||
         raw.contains(QStringLiteral("인증URL 불일치"));
}

static QString friendlyBasemapError(const QString& raw) {
  const QString r = raw;
  if (isFatalVworldAuthError(r) ||
      r.contains(QStringLiteral("InvalidParameterValue"), Qt::CaseInsensitive)) {
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
      if (legendTitlesMatch(n, name))
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
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    const QgsRectangle before = canvas->extent();
    const QgsPointXY centerBefore = before.center();
    const bool keepCenter = !before.isEmpty() && before.isFinite() && before.width() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
    const bool needScaleOnly = canvas->extent().isEmpty() || !canvas->extent().isFinite() ||
                               canvas->scale() > 400000.0 || canvas->scale() < 100.0;
    syncCanvasToProject(project, canvas);
    if (keepCenter) {
      canvas->setCenter(centerBefore);
      if (canvas->scale() > 80000.0)
        canvas->zoomScale(10000.0, true);
    } else if (needScaleOnly) {
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    }
    canvas->refreshAllLayers();
    canvas->refresh();
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
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Base/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Base/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857"),
  };
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 배경"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    if (canvas->scale() > 200000.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
  }
  return ok;
}

bool LayerOps::addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  const QString key = apiKey.trimmed();
  QStringList uris;
  if (!key.isEmpty()) {
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857&http-header:referer=https://localhost")
                .arg(key);
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857")
                .arg(key);
  }
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857&http-header:referer=https://localhost");
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857");
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 위성"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    canvas->clearCache();
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  return ok;
}

static QString makeVworldWmsUri(const QString& apiKey, const QString& layers, const QString& styles,
                                const QString& crsAuthId) {
  const QString key = apiKey.trimmed();
  const QString crs = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:3857") : crsAuthId.trimmed();
  // GitHub baseline (eac6c9c): KEY/DOMAIN + tiled WMS (tilePixelRatio=2). That path drew parcels.
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
      .arg(crs, layers, stylePart, encUrl);
}

static bool addGdalVworldCadastral(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
                                   const QString& layers, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  const QString xml = QStringLiteral(
                          "<GDAL_WMS>"
                          "<Service name=\"WMS\">"
                          "<Version>1.3.0</Version>"
                          "<ServerUrl>https://api.vworld.kr/req/wms?key=%1&amp;domain=localhost&amp;</ServerUrl>"
                          "<Layers>%2</Layers>"
                          "<Styles>lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun</Styles>"
                          "<CRS>EPSG:3857</CRS>"
                          "<ImageFormat>image/png</ImageFormat>"
                          "<Transparent>TRUE</Transparent>"
                          "<BBoxOrder>xyXY</BBoxOrder>"
                          "</Service>"
                          "<DataWindow>"
                          "<UpperLeftX>13500000</UpperLeftX>"
                          "<UpperLeftY>4800000</UpperLeftY>"
                          "<LowerRightX>14800000</LowerRightX>"
                          "<LowerRightY>3800000</LowerRightY>"
                          "<SizeX>16384</SizeX>"
                          "<SizeY>16384</SizeY>"
                          "</DataWindow>"
                          "<Projection>EPSG:3857</Projection>"
                          "<BandsCount>4</BandsCount>"
                          "<BlockSizeX>512</BlockSizeX>"
                          "<BlockSizeY>512</BlockSizeY>"
                          "<UserAgent>Mozilla/5.0 ka-hgis/0.3</UserAgent>"
                          "<Referer>https://localhost</Referer>"
                          "</GDAL_WMS>")
                          .arg(apiKey.trimmed(), layers);
  const QString xmlPath = QDir::temp().filePath(QStringLiteral("ka-hgis-vworld-cadastral.xml"));
  {
    QFile f(xmlPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      if (errorOut) *errorOut = QStringLiteral("지적 설정 파일을 쓰지 못했습니다.");
      return false;
    }
    f.write(xml.toUtf8());
  }

  const QString name = QStringLiteral("VWorld 지적(본번·부번)");
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    const QString n = old->name();
    if (n == name || n.startsWith(name + QLatin1String(" [")))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);

  auto* rl = new QgsRasterLayer(xmlPath, name, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = friendlyBasemapError(rl->error().message());
    delete rl;
    return false;
  }
  // OTF only works if the layer CRS is the server CRS (3857), not the work CRS.
  rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  LayerOps::markReferenceLayer(rl);
  rl->setOpacity(1.0);
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("지적 레이어를 프로젝트에 넣지 못했습니다.");
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    syncCanvasToProject(project, canvas);
    if (canvas->scale() > 8000.0 || canvas->scale() < 200.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 3000.0);
    canvas->clearCache();
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  return true;
}

QStringList LayerOps::cadastralWmsCrsCandidates(const QString& workCrsAuthId) {
  Q_UNUSED(workCrsAuthId);
  // Do not put 5186/5187/5179 here. Caps only list 4326 bbox; QGIS then
  // reports "Cannot calculate extent" and the layer never draws.
  return {
      QStringLiteral("EPSG:4326"),
      QStringLiteral("EPSG:3857"),
      QStringLiteral("EPSG:900913"),
  };
}

bool LayerOps::addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString workCrs = (project && project->crs().isValid())
                              ? project->crs().authid()
                              : QStringLiteral("EPSG:5186");
  // Restore GitHub baseline path that drew parcels: EPSG:3857 tiled WMS, empty styles.
  const QString uriBoth = makeVworldWmsUri(
      apiKey, QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBon =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bonbun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBu =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));

  QString err;
  bool ok = addXyzBasemap(project, canvas, uriBoth, QStringLiteral("VWorld 지적(본번·부번)"), &err, true);
  if (!ok) {
    const bool okBon = addXyzBasemap(project, canvas, uriBon, QStringLiteral("VWorld 지적 본번"), &err, true);
    const bool okBu = addXyzBasemap(project, canvas, uriBu, QStringLiteral("VWorld 지적 부번"), &err, true);
    ok = okBon || okBu;
  }
  if (!ok)
    ok = addGdalVworldCadastral(project, canvas, apiKey,
                                QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), &err);
  if (!ok) {
    if (errorOut)
      *errorOut = err.isEmpty() ? QStringLiteral("지적도 WMS 추가 실패") : err;
    return false;
  }
  if (canvas) {
    LayerOps::ensureOtfEnabled(project, canvas, workCrs);
    for (QgsMapLayer* l : project->mapLayers()) {
      if (!l) continue;
      const QString n = l->name();
      const bool vworldCad = (n.contains(QStringLiteral("VWorld")) && n.contains(QStringLiteral("지적"))) ||
                             n == QLatin1String("지적") || n.startsWith(QLatin1String("지적 본번")) ||
                             n.startsWith(QLatin1String("지적 부번")) || n.startsWith(QLatin1String("지적("));
      if (vworldCad)
        l->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
    }
    LayerOps::syncMapCanvas(project, canvas, false);
    const double s = canvas->scale();
    if (s > 6000.0 || s < 500.0)
      canvas->zoomScale(3000.0, true);
    canvas->clearCache();
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  return true;
}

double LayerOps::suggestCadastralScale(double currentScale, double target, double maxOk) {
  if (currentScale <= 0.0) return target;
  if (currentScale > maxOk) return target;
  return currentScale;
}

LayerOps::FieldBasemapPackResult LayerOps::prepareFieldBasemapPack(
    QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
    const QString& workCrsAuthId, QString* errorOut) {
  FieldBasemapPackResult r;
  if (!requireVworldKey(apiKey, errorOut)) return r;

  const QString work = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186")
                                                         : workCrsAuthId.trimmed();
  ensureOtfEnabled(project, canvas, work);

  QgsPointXY keepCenter;
  bool hadLocal = false;
  double keepScale = 10000.0;
  if (canvas) {
    const QgsRectangle e = canvas->extent();
    if (!e.isEmpty() && e.isFinite() && e.width() > 0 && canvas->scale() > 50.0 &&
        canvas->scale() < 500000.0) {
      keepCenter = e.center();
      hadLocal = true;
      keepScale = canvas->scale();
    }
  }

  QString satErr;
  r.satelliteOk = addVworldSatelliteMap(project, canvas, apiKey, &satErr);
  QString cadErr;
  r.cadastralOk = addVworldCadastralMap(project, canvas, apiKey, &cadErr);

  if (!r.satelliteOk && !r.cadastralOk) {
    if (errorOut) {
      *errorOut = satErr.isEmpty() ? cadErr : satErr;
      if (errorOut->isEmpty())
        *errorOut = QStringLiteral("현장 배경(위성·지적) 추가 실패");
    }
    return r;
  }

  if (canvas) {
    ensureOtfEnabled(project, canvas, work);
    syncMapCanvas(project, canvas, false);
    if (hadLocal) {
      canvas->setCenter(keepCenter);
      const double next = suggestCadastralScale(keepScale, 5000.0, 15000.0);
      canvas->zoomScale(next, true);
    } else {
      const double next = suggestCadastralScale(canvas->scale(), 4000.0, 5000.0);
      if (qAbs(next - canvas->scale()) > 1.0)
        canvas->zoomScale(next, true);
    }
    clampCanvasToKorea(canvas);
    canvas->refreshAllLayers();
    canvas->refresh();
  }
  if (errorOut && (!r.satelliteOk || !r.cadastralOk)) {
    QStringList parts;
    if (!r.satelliteOk && !satErr.isEmpty()) parts << satErr;
    if (!r.cadastralOk && !cadErr.isEmpty()) parts << cadErr;
    *errorOut = parts.join(QStringLiteral(" / "));
  }
  return r;
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
    if (legendTitlesMatch(n, name))
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

bool LayerOps::undoCommittedFeature(QgsVectorLayer* layer, qint64 featureId, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 되돌릴 수 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("되돌릴 도형을 찾지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->deleteFeature(fid)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 지우지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges(false)) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  if (startedHere)
    layer->startEditing();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
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

