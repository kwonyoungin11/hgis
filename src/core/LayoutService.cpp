#include "LayoutService.h"
#include "LayerOps.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QRectF>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <exception>

#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgslayout.h>
#include <qgslayoutexporter.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemmapgrid.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutmeasurement.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutsize.h>
#include <qgsmaplayer.h>
#include <qgsmasterlayoutinterface.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

QStringList LayoutService::defaultLayoutNames() {
  return {
    QStringLiteral("survey_area_map"),
    QStringLiteral("site_location"),
    QStringLiteral("feature_plan"),
    QStringLiteral("feature_detail"),
    QStringLiteral("section")
  };
}

QString LayoutService::layoutId(DrawingKind kind) {
  switch (kind) {
    case DrawingKind::SurveyAreaMap: return QStringLiteral("survey_area_map");
    case DrawingKind::SiteLocation: return QStringLiteral("site_location");
    case DrawingKind::FeaturePlan: return QStringLiteral("feature_plan");
    case DrawingKind::FeatureDetail: return QStringLiteral("feature_detail");
    case DrawingKind::Section: return QStringLiteral("section");
  }
  return QStringLiteral("survey_area_map");
}

LayoutService::DrawingKind LayoutService::kindFromLayoutId(const QString& layoutId) {
  if (layoutId == QLatin1String("site_location")) return DrawingKind::SiteLocation;
  if (layoutId == QLatin1String("feature_plan")) return DrawingKind::FeaturePlan;
  if (layoutId == QLatin1String("feature_detail")) return DrawingKind::FeatureDetail;
  if (layoutId == QLatin1String("section")) return DrawingKind::Section;
  return DrawingKind::SurveyAreaMap;
}

QString LayoutService::koreanTitle(const QString& name) {
  if (name == QLatin1String("survey_area_map")) return QStringLiteral("조사구역도");
  if (name == QLatin1String("site_location")) return QStringLiteral("유적위치도");
  if (name == QLatin1String("feature_plan")) return QStringLiteral("유구배치도");
  if (name == QLatin1String("feature_detail")) return QStringLiteral("개별유구실측도");
  if (name == QLatin1String("section")) return QStringLiteral("층위도");
  return name;
}

LayoutService::DrawingRecipe LayoutService::recipe(DrawingKind kind) {
  DrawingRecipe r;
  r.kind = kind;
  r.layoutId = layoutId(kind);
  r.titleKo = koreanTitle(r.layoutId);
  switch (kind) {
    case DrawingKind::SurveyAreaMap:
      r.purposeKo = QStringLiteral("발굴·조사 범위를 1:5,000 기준으로 보여 줍니다.");
      r.defaultScale = 5000.0;
      r.gridIntervalM = 100.0;
      r.includeBasemap = true;
      r.layerKeys = {QStringLiteral("survey_area"), QStringLiteral("control_points")};
      r.emptyHintKo = QStringLiteral("먼저 「그리기 → 구역」으로 조사구역을 그리세요.");
      r.scaleChoices = {2500.0, 5000.0, 10000.0};
      break;
    case DrawingKind::SiteLocation:
      r.purposeKo = QStringLiteral("주변 지형에서 유적 위치를 넓게 보여 줍니다.");
      r.defaultScale = 25000.0;
      r.gridIntervalM = 1000.0;
      r.includeBasemap = true;
      r.layerKeys = {QStringLiteral("survey_area")};
      r.emptyHintKo = QStringLiteral("먼저 조사구역을 그리면 위치도에 표시됩니다.");
      r.scaleChoices = {10000.0, 25000.0, 50000.0};
      break;
    case DrawingKind::FeaturePlan:
      r.purposeKo = QStringLiteral("조사구역 안의 유구 배치와 종류·시대를 보여 줍니다.");
      r.defaultScale = 1000.0;
      r.gridIntervalM = 20.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                     QStringLiteral("feature_line"), QStringLiteral("control_points")};
      r.emptyHintKo = QStringLiteral("유구 면·선을 그린 뒤 종류·시대를 넣으면 범례가 채워집니다.");
      r.scaleChoices = {500.0, 1000.0, 2000.0};
      break;
    case DrawingKind::FeatureDetail:
      r.purposeKo = QStringLiteral("선택한 유구 하나를 확대해 실측도로 만듭니다.");
      r.defaultScale = 100.0;
      r.gridIntervalM = 2.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("feature_poly"), QStringLiteral("feature_line")};
      r.emptyHintKo = QStringLiteral("유구를 그린 뒤, 왼쪽에서 실측할 유구를 고르세요.");
      r.scaleChoices = {50.0, 100.0, 200.0};
      break;
    case DrawingKind::Section:
      r.purposeKo = QStringLiteral("단면·층위 기준선을 도면으로 출력합니다.");
      r.defaultScale = 200.0;
      r.gridIntervalM = 0.0;
      r.includeBasemap = false;
      r.layerKeys = {QStringLiteral("section_line"), QStringLiteral("survey_area")};
      r.emptyHintKo = QStringLiteral("먼저 「그리기 → 단면」으로 단면선을 그리세요.");
      r.scaleChoices = {100.0, 200.0, 500.0};
      break;
  }
  return r;
}

QList<LayoutService::DrawingRecipe> LayoutService::allRecipes() {
  return {
    recipe(DrawingKind::SurveyAreaMap),
    recipe(DrawingKind::SiteLocation),
    recipe(DrawingKind::FeaturePlan),
    recipe(DrawingKind::FeatureDetail),
    recipe(DrawingKind::Section),
  };
}

static QgsRectangle featuresExtent(QgsVectorLayer* layer, QgsFeatureId onlyId = FID_NULL) {
  QgsRectangle ext;
  bool first = true;
  if (!layer || layer->featureCount() <= 0) return ext;
  QgsFeatureRequest req;
  if (onlyId != FID_NULL)
    req.setFilterFid(onlyId);
  QgsFeatureIterator it = layer->getFeatures(req);
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QgsGeometry g = f.geometry();
    if (g.isNull() || g.isEmpty()) continue;
    if (first) {
      ext = g.boundingBox();
      first = false;
    } else {
      ext.combineExtentWith(g.boundingBox());
    }
  }
  return ext;
}

static QString firstAttr(QgsVectorLayer* layer, const QString& field) {
  if (!layer) return {};
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  if (!it.nextFeature(f)) return {};
  return f.attribute(field).toString().trimmed();
}

static QList<QgsMapLayer*> layersForRecipe(QgsProject* project, const LayoutService::DrawingRecipe& rec) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  for (const QString& key : rec.layerKeys) {
    if (auto* vl = LayerOps::findByLayerKey(project, key))
      out.append(vl);
  }
  Q_UNUSED(rec.includeBasemap);
  return out;
}

static QString koreanLegendText(const QList<QgsMapLayer*>& layers) {
  QStringList lines;
  lines << QStringLiteral("범례");
  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl) continue;
    const QString key = LayerOps::layerKeyOf(vl);
    QString name = LayoutService::koreanTitle(key);
    if (name == key || name.isEmpty()) {
      if (key == QLatin1String("survey_area") || vl->name().contains(QStringLiteral("조사")))
        name = QStringLiteral("조사구역");
      else if (key == QLatin1String("feature_poly") || vl->name().contains(QStringLiteral("유구면")))
        name = QStringLiteral("유구 면");
      else if (key == QLatin1String("feature_line") || vl->name().contains(QStringLiteral("유구선")))
        name = QStringLiteral("유구 선");
      else if (key == QLatin1String("section_line") || vl->name().contains(QStringLiteral("단면")))
        name = QStringLiteral("단면선");
      else if (key == QLatin1String("control_points") || vl->name().contains(QStringLiteral("GPS")))
        name = QStringLiteral("기준점");
      else
        name = vl->name();
    }
    QString extra;
    if (vl->fields().indexOf(QStringLiteral("kind")) >= 0) {
      QStringList kinds;
      QgsFeature f;
      QgsFeatureIterator it = vl->getFeatures();
      while (it.nextFeature(f)) {
        const QString k = f.attribute(QStringLiteral("kind")).toString().trimmed();
        const QString p = f.attribute(QStringLiteral("period")).toString().trimmed();
        QString row = k;
        if (!p.isEmpty()) row += QStringLiteral(" / %1").arg(p);
        if (!row.isEmpty() && !kinds.contains(row))
          kinds << row;
      }
      if (!kinds.isEmpty())
        extra = QStringLiteral("\n  · ") + kinds.join(QStringLiteral("\n  · "));
    }
    lines << name + extra;
  }
  if (lines.size() == 1)
    lines << QStringLiteral("(그린 도형 없음)");
  return lines.join(QLatin1Char('\n'));
}

static bool hasDrawableContent(const QList<QgsMapLayer*>& layers, const LayoutService::DrawingRecipe& rec,
                               QgsFeatureId featureId) {
  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || LayerOps::isReferenceLayer(vl)) continue;
    if (rec.kind == LayoutService::DrawingKind::FeatureDetail) {
      if (featureId != FID_NULL && vl->getFeature(featureId).isValid())
        return true;
      if (vl->featureCount() > 0) return true;
    } else if (vl->featureCount() > 0) {
      return true;
    }
  }
  return false;
}

static QgsRectangle resolveExtent(QgsProject* project, const LayoutService::DrawingRecipe& rec,
                                  const LayoutService::DrawingOptions& opt) {
  if (opt.extentMode == LayoutService::ExtentMode::Canvas && !opt.canvasExtent.isEmpty())
    return opt.canvasExtent;

  auto* sa = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
  auto* fp = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
  auto* fl = LayerOps::findByLayerKey(project, QStringLiteral("feature_line"));
  auto* sl = LayerOps::findByLayerKey(project, QStringLiteral("section_line"));

  if (opt.extentMode == LayoutService::ExtentMode::SelectedFeature ||
      rec.kind == LayoutService::DrawingKind::FeatureDetail) {
    QgsRectangle e = featuresExtent(fp, opt.featureId);
    if (e.isEmpty()) e = featuresExtent(fl, opt.featureId);
    if (!e.isEmpty()) {
      e.scale(1.35);
      return e;
    }
  }

  if (rec.kind == LayoutService::DrawingKind::Section) {
    QgsRectangle e = featuresExtent(sl);
    if (e.isEmpty()) e = featuresExtent(sa);
    if (!e.isEmpty()) {
      e.scale(1.4);
      return e;
    }
  }

  QgsRectangle survey = featuresExtent(sa);
  if (opt.extentMode == LayoutService::ExtentMode::WideContext ||
      rec.kind == LayoutService::DrawingKind::SiteLocation) {
    if (!survey.isEmpty()) {
      survey.scale(8.0);
      return survey;
    }
  }

  if (!survey.isEmpty()) {
    survey.scale(rec.kind == LayoutService::DrawingKind::FeaturePlan ? 1.08 : 1.15);
    return survey;
  }

  QgsRectangle feats = featuresExtent(fp);
  if (!feats.isEmpty()) {
    const QgsRectangle lineExt = featuresExtent(fl);
    if (!lineExt.isEmpty()) feats.combineExtentWith(lineExt);
    feats.scale(1.2);
    return feats;
  }
  return {};
}

static void applyPageSize(QgsPrintLayout* layout, LayoutService::Paper paper,
                          LayoutService::Orientation orientation) {
  double pageW = (paper == LayoutService::Paper::A3) ? 297.0 : 210.0;
  double pageH = (paper == LayoutService::Paper::A3) ? 420.0 : 297.0;
  if (orientation == LayoutService::Orientation::Landscape)
    std::swap(pageW, pageH);
  if (layout->pageCollection() && layout->pageCollection()->pageCount() > 0) {
    if (QgsLayoutItemPage* page = layout->pageCollection()->page(0))
      page->setPageSize(QgsLayoutSize(pageW, pageH, Qgis::LayoutUnit::Millimeters));
  }
}

static void fillLayout(QgsPrintLayout* layout, QgsProject* project,
                       const LayoutService::DrawingRecipe& rec,
                       const LayoutService::DrawingOptions& opt,
                       LayoutService::DrawingBuildResult* result) {
  const double pageW = (opt.paper == LayoutService::Paper::A3) ? 297.0 : 210.0;
  const double pageH = (opt.paper == LayoutService::Paper::A3) ? 420.0 : 297.0;
  const bool land = opt.orientation == LayoutService::Orientation::Landscape;
  const double W = land ? std::max(pageW, pageH) : std::min(pageW, pageH);
  const double H = land ? std::min(pageW, pageH) : std::max(pageW, pageH);

  applyPageSize(layout, opt.paper, opt.orientation);

  auto* sa = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
  const QString surveyName = !opt.surveyName.isEmpty() ? opt.surveyName : firstAttr(sa, QStringLiteral("survey_name"));
  const QString siteName = !opt.siteName.isEmpty() ? opt.siteName : firstAttr(sa, QStringLiteral("site_name"));
  const QString sheet = !opt.titleKo.isEmpty() ? opt.titleKo : rec.titleKo;
  const QString crsAuth = project && project->crs().isValid()
                              ? project->crs().authid()
                              : QStringLiteral("EPSG:5186");

  const double margin = 10.0;
  const double sideW = 46.0;
  const double mapW = W - margin * 2 - sideW - 4.0;
  const double mapH = H - margin * 2 - 28.0;

  auto* title = new QgsLayoutItemLabel(layout);
  title->setText(QStringLiteral("【%1】").arg(sheet));
  title->attemptSetSceneRect(QRectF(margin, 4, mapW, 9));
  title->setFont(QFont(QStringLiteral("Malgun Gothic"), 16, QFont::Bold));
  title->setFrameEnabled(true);
  layout->addLayoutItem(title);

  QString meta = surveyName.isEmpty() ? QStringLiteral("고고학 전용 HGIS")
                                      : QStringLiteral("조사명: %1").arg(surveyName);
  if (!siteName.isEmpty())
    meta += QStringLiteral("  |  유적명: %1").arg(siteName);
  meta += QStringLiteral("  |  진북  |  %1").arg(crsAuth);
  auto* metaLbl = new QgsLayoutItemLabel(layout);
  metaLbl->setText(meta);
  metaLbl->attemptSetSceneRect(QRectF(margin, 13.5, mapW, 6));
  metaLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  layout->addLayoutItem(metaLbl);

  const QList<QgsMapLayer*> mapLayers = layersForRecipe(project, rec);
  const bool content = hasDrawableContent(mapLayers, rec, opt.featureId);
  if (result) result->hasMapContent = content;

  auto* map = new QgsLayoutItemMap(layout);
  map->attemptSetSceneRect(QRectF(margin, 21, mapW, mapH));
  map->setFrameEnabled(true);
  map->setFrameStrokeWidth(QgsLayoutMeasurement(0.5, Qgis::LayoutUnit::Millimeters));
  layout->addLayoutItem(map);
  if (project && project->crs().isValid())
    map->setCrs(project->crs());
  map->setLayers(mapLayers);
  map->setKeepLayerSet(true);
  map->setMapRotation(0.0);

  QgsRectangle ext = resolveExtent(project, rec, opt);
  if (!ext.isEmpty())
    map->setExtent(ext);

  const double scale = opt.scaleOverride > 0.0 ? opt.scaleOverride : rec.defaultScale;
  if (scale > 0.0 && content && !ext.isEmpty())
    map->setScale(scale, true);

  if (rec.gridIntervalM > 0.0 && map->grids()) {
    auto* grid = new QgsLayoutItemMapGrid(QStringLiteral("grid"), map);
    grid->setEnabled(true);
    grid->setStyle(Qgis::MapGridStyle::Lines);
    grid->setIntervalX(rec.gridIntervalM);
    grid->setIntervalY(rec.gridIntervalM);
    map->grids()->addGrid(grid);
  }

  if (result) {
    result->appliedScale = map->scale();
    result->appliedExtent = map->extent();
  }

  const double sideX = margin + mapW + 4.0;
  auto* north = new QgsLayoutItemLabel(layout);
  north->setText(QStringLiteral("N\n↑\n진북"));
  north->setHAlign(Qt::AlignHCenter);
  north->attemptSetSceneRect(QRectF(sideX + 8, 21, 28, 24));
  north->setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
  north->setFrameEnabled(true);
  layout->addLayoutItem(north);

  auto* northLbl = new QgsLayoutItemLabel(layout);
  northLbl->setText(QStringLiteral("방위표 (진북)"));
  northLbl->setHAlign(Qt::AlignHCenter);
  northLbl->attemptSetSceneRect(QRectF(sideX, 50, sideW, 6));
  northLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  layout->addLayoutItem(northLbl);

  auto* legend = new QgsLayoutItemLabel(layout);
  legend->setText(koreanLegendText(mapLayers));
  legend->attemptSetSceneRect(QRectF(sideX, 58, sideW, std::max(40.0, mapH - 90.0)));
  legend->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  legend->setFrameEnabled(true);
  legend->setBackgroundEnabled(true);
  layout->addLayoutItem(legend);

  auto* sb = new QgsLayoutItemScaleBar(layout);
  layout->addLayoutItem(sb);
  sb->setLinkedMap(map);
  sb->setStyle(QStringLiteral("Double Box"));
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  sb->setNumberOfSegments(4);
  sb->setNumberOfSegmentsLeft(0);
  if (content && !ext.isEmpty())
    sb->applyDefaultSize(Qgis::DistanceUnit::Meters);
  sb->setHeight(3.0);
  sb->attemptSetSceneRect(QRectF(margin, H - 16, 88, 12));

  auto* scaleLbl = new QgsLayoutItemLabel(layout);
  const int sc = map->scale() > 0 ? int(std::lround(map->scale())) : 0;
  scaleLbl->setText(sc > 0 ? QStringLiteral("축척 1 : %1").arg(sc)
                           : QStringLiteral("축척: 축척자 참조"));
  scaleLbl->setId(QStringLiteral("scale_label"));
  scaleLbl->attemptSetSceneRect(QRectF(margin + 90, H - 14, 55, 8));
  scaleLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
  layout->addLayoutItem(scaleLbl);

  auto* stamp = new QgsLayoutItemLabel(layout);
  stamp->setText(QStringLiteral("업로드 CRS: EPSG:5179\n작성: ka-hgis\n%1")
                     .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
  stamp->attemptSetSceneRect(QRectF(sideX, H - 40, sideW, 24));
  stamp->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  stamp->setFrameEnabled(true);
  layout->addLayoutItem(stamp);

  if (!content) {
    auto* hint = new QgsLayoutItemLabel(layout);
    hint->setText(rec.emptyHintKo);
    hint->setHAlign(Qt::AlignHCenter);
    hint->setVAlign(Qt::AlignVCenter);
    hint->attemptSetSceneRect(QRectF(margin + 8, 21 + mapH * 0.38, mapW - 16, 18));
    hint->setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    hint->setBackgroundEnabled(true);
    hint->setFrameEnabled(true);
    hint->setId(QStringLiteral("empty_hint"));
    layout->addLayoutItem(hint);
    if (result) result->warningKo = rec.emptyHintKo;
  }
}

QgsRectangle LayoutService::extentForPaperScale(const QgsRectangle& currentExtent,
                                                double mapWidthMm, double scaleDenominator) {
  if (mapWidthMm <= 0.0 || scaleDenominator <= 0.0)
    return currentExtent;
  const double groundWidth = scaleDenominator * (mapWidthMm / 1000.0);
  QgsPointXY center(0.0, 0.0);
  double ratio = 1.0;
  if (currentExtent.isFinite() && currentExtent.width() > 0.0) {
    center = currentExtent.center();
    if (currentExtent.height() > 0.0)
      ratio = currentExtent.height() / currentExtent.width();
  }
  const double groundHeight = groundWidth * ratio;
  return QgsRectangle(center.x() - groundWidth * 0.5, center.y() - groundHeight * 0.5,
                      center.x() + groundWidth * 0.5, center.y() + groundHeight * 0.5);
}

QString LayoutService::createBlankSheet(QgsProject* project, double widthMm, double heightMm,
                                        const QString& name, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  if (widthMm < 20.0 || heightMm < 20.0) {
    if (errorOut) *errorOut = QStringLiteral("용지가 너무 작습니다.");
    return {};
  }
  if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
    project->layoutManager()->removeLayout(old);
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(name);
  layout->setUnits(Qgis::LayoutUnit::Millimeters);
  if (layout->pageCollection() && layout->pageCollection()->pageCount() > 0) {
    if (QgsLayoutItemPage* page = layout->pageCollection()->page(0))
      page->setPageSize(QgsLayoutSize(widthMm, heightMm, Qgis::LayoutUnit::Millimeters));
  }
  project->layoutManager()->addLayout(layout);
  return name;
}

static QgsPrintLayout* replaceLayout(QgsProject* project, const QString& name) {
  if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
    project->layoutManager()->removeLayout(old);
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(name);
  layout->setUnits(Qgis::LayoutUnit::Millimeters);
  project->layoutManager()->addLayout(layout);
  return layout;
}

LayoutService::DrawingBuildResult LayoutService::buildDrawing(QgsProject* project, DrawingKind kind,
                                                              const DrawingOptions& options,
                                                              QString* errorOut) {
  DrawingBuildResult result;
  result.layoutId = layoutId(kind);
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    result.warningKo = QStringLiteral("프로젝트가 없습니다.");
    return result;
  }
  const DrawingRecipe rec = recipe(kind);
  if (kind == DrawingKind::FeatureDetail && options.featureId == FID_NULL) {
    auto* fp = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    if (!fp || fp->featureCount() <= 0) {
      result.warningKo = rec.emptyHintKo;
      result.hasMapContent = false;
    }
  }
  try {
    QgsPrintLayout* layout = replaceLayout(project, result.layoutId);
    fillLayout(layout, project, rec, options, &result);
  } catch (const std::exception& ex) {
    result.warningKo = QString::fromUtf8(ex.what());
    if (errorOut) *errorOut = result.warningKo;
  } catch (...) {
    result.warningKo = QStringLiteral("도면을 만드는 중 오류가 났습니다.");
    if (errorOut) *errorOut = result.warningKo;
  }
  return result;
}

int LayoutService::ensureDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  int created = 0;
  DrawingOptions opt;
  opt.orientation = Orientation::Landscape;
  for (const DrawingRecipe& rec : allRecipes()) {
    if (project->layoutManager()->layoutByName(rec.layoutId)) continue;
    buildDrawing(project, rec.kind, opt, nullptr);
    ++created;
  }
  return created;
}

int LayoutService::rebuildDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  for (const QString& name : defaultLayoutNames()) {
    if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
      project->layoutManager()->removeLayout(old);
  }
  DrawingOptions opt;
  opt.orientation = Orientation::Landscape;
  int n = 0;
  for (const DrawingRecipe& rec : allRecipes()) {
    buildDrawing(project, rec.kind, opt, nullptr);
    ++n;
  }
  return n;
}

QString LayoutService::createReportLayout(QgsProject* project, const QString& titleKo,
                                          Paper paper, Orientation orientation,
                                          QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  const QString name = QStringLiteral("report_%1_%2")
                           .arg(paper == Paper::A3 ? QStringLiteral("a3") : QStringLiteral("a4"),
                                orientation == Orientation::Landscape ? QStringLiteral("land")
                                                                      : QStringLiteral("port"));
  DrawingOptions opt;
  opt.titleKo = titleKo.isEmpty() ? QStringLiteral("조사도면") : titleKo;
  opt.paper = paper;
  opt.orientation = orientation;
  const DrawingRecipe rec = recipe(DrawingKind::FeaturePlan);
  QgsPrintLayout* layout = replaceLayout(project, name);
  DrawingBuildResult dummy;
  fillLayout(layout, project, rec, opt, &dummy);
  return name;
}

QString LayoutService::exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                       const QString& pdfPath, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  ensureDefaultLayouts(project);
  QgsMasterLayoutInterface* master = project->layoutManager()->layoutByName(layoutName);
  if (!master) {
    if (errorOut) *errorOut = QStringLiteral("도면을 찾을 수 없습니다: %1").arg(koreanTitle(layoutName));
    return {};
  }
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  if (!layout) {
    if (errorOut) *errorOut = QStringLiteral("인쇄 도면이 아닙니다.");
    return {};
  }

  QgsLayoutExporter exporter(layout);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  settings.forceVectorOutput = true;
  const auto r = exporter.exportToPdf(pdfPath, settings);
  if (r != QgsLayoutExporter::Success) {
    if (errorOut) *errorOut = QStringLiteral("PDF 내보내기 실패 (코드 %1)").arg(int(r));
    return {};
  }
  if (!QFile::exists(pdfPath) || QFileInfo(pdfPath).size() < 500) {
    if (errorOut) *errorOut = QStringLiteral("PDF 파일이 비었거나 너무 작습니다.");
    return {};
  }
  return pdfPath;
}

QImage LayoutService::renderPreview(QgsProject* project, const QString& layoutName,
                                    const QSize& imageSize, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return {};
  }
  QgsMasterLayoutInterface* master = project->layoutManager()->layoutByName(layoutName);
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  if (!layout) {
    if (errorOut) *errorOut = QStringLiteral("미리볼 도면이 없습니다.");
    return {};
  }
  try {
    QgsLayoutExporter exporter(layout);
    const QSize sz = imageSize.isEmpty() ? QSize(720, 510) : imageSize;
    QImage img = exporter.renderPageToImage(0, sz, 96.0);
    if (img.isNull()) {
      if (errorOut) *errorOut = QStringLiteral("미리보기를 만들지 못했습니다.");
      return {};
    }
    return img;
  } catch (const std::exception& ex) {
    if (errorOut) *errorOut = QString::fromUtf8(ex.what());
    return {};
  } catch (...) {
    if (errorOut) *errorOut = QStringLiteral("미리보기를 그리는 중 오류가 났습니다.");
    return {};
  }
}

int LayoutService::exportDrawingPdfs(QgsProject* project, const QString& outDir, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return 0;
  }
  QDir dir(outDir);
  if (!dir.exists() && !QDir().mkpath(outDir)) {
    if (errorOut) *errorOut = QStringLiteral("폴더를 만들 수 없습니다.");
    return 0;
  }
  rebuildDefaultLayouts(project);
  int n = 0;
  QString lastErr;
  for (const DrawingRecipe& rec : allRecipes()) {
    const QString path = dir.filePath(rec.titleKo + QStringLiteral(".pdf"));
    QString err;
    if (!exportLayoutPdf(project, rec.layoutId, path, &err).isEmpty())
      ++n;
    else
      lastErr = err;
  }
  if (n == 0 && errorOut)
    *errorOut = lastErr.isEmpty() ? QStringLiteral("도면 PDF를 만들지 못했습니다.") : lastErr;
  return n;
}
