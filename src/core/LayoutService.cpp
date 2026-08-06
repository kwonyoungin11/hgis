#include "LayoutService.h"
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QRectF>
#include <QColor>
#include <QDir>
#include <QCoreApplication>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgsproject.h>
#include <qgsprintlayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemshape.h>
#include <qgslayoutitemmapgrid.h>
#include <qgslayoutexporter.h>
#include <qgsmasterlayoutinterface.h>
#include <qgscoordinatereferencesystem.h>
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

QString LayoutService::koreanTitle(const QString& name) {
  if (name == QLatin1String("survey_area_map")) return QStringLiteral("조사구역도");
  if (name == QLatin1String("site_location")) return QStringLiteral("유적위치도");
  if (name == QLatin1String("feature_plan")) return QStringLiteral("유구배치도");
  if (name == QLatin1String("feature_detail")) return QStringLiteral("개별유구실측도");
  if (name == QLatin1String("section")) return QStringLiteral("층위도");
  return name;
}

static QString northArrowSvg() {
  const QStringList cands = {
    QStringLiteral("D:/OSGeo4W/apps/qgis-dev/svg/arrows/NorthArrow_02.svg"),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../OSGeo4W/apps/qgis-dev/svg/arrows/NorthArrow_02.svg")),
    qEnvironmentVariable("OSGEO4W_ROOT") + QStringLiteral("/apps/qgis-dev/svg/arrows/NorthArrow_02.svg"),
  };
  for (const QString& p : cands) {
    if (!p.isEmpty() && QFile::exists(p)) return p;
  }
  return cands.first();
}

static QgsRectangle projectExtent(QgsProject* project) {
  QgsRectangle ext;
  bool first = true;
  const auto layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml || ml->extent().isEmpty()) continue;
    const QString n = ml->name();
    if (n.contains(QStringLiteral("VWorld")) || n.contains(QStringLiteral("Google"))
        || n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("배경")))
      continue;
    if (first) {
      ext = ml->extent();
      first = false;
    } else {
      ext.combineExtentWith(ml->extent());
    }
  }
  if (first || ext.isEmpty()) {
    for (QgsMapLayer* ml : layers) {
      if (!ml || ml->extent().isEmpty()) continue;
      if (first) {
        ext = ml->extent();
        first = false;
      } else {
        ext.combineExtentWith(ml->extent());
      }
    }
  }
  if (first || ext.isEmpty())
    ext = QgsRectangle(900000, 1800000, 1200000, 2100000);
  else
    ext.scale(1.2);
  return ext;
}

static QList<QgsMapLayer*> legendLayers(QgsProject* project) {
  QList<QgsMapLayer*> out;
  const auto layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml) continue;
    const QString n = ml->name();
    if (n.contains(QStringLiteral("VWorld")) || n.contains(QStringLiteral("Google"))
        || n.contains(QStringLiteral("OSM")))
      continue;
    out.append(ml);
  }
  if (out.isEmpty()) {
    for (QgsMapLayer* ml : layers)
      if (ml) out.append(ml);
  }
  return out;
}

static QList<QgsMapLayer*> allMapLayers(QgsProject* project) {
  QList<QgsMapLayer*> out;
  for (QgsMapLayer* ml : project->mapLayers().values())
    if (ml) out.append(ml);
  return out;
}

static void addTitleBlock(QgsPrintLayout* layout, const QString& sheetKo, const QString& crsAuth) {
  auto* title = new QgsLayoutItemLabel(layout);
  title->setText(QStringLiteral("【%1】").arg(sheetKo));
  title->attemptSetSceneRect(QRectF(10, 4, 150, 9));
  title->setFont(QFont(QStringLiteral("Malgun Gothic"), 16, QFont::Bold));
  title->setFrameEnabled(true);
  layout->addLayoutItem(title);

  auto* meta = new QgsLayoutItemLabel(layout);
  meta->setText(QStringLiteral("고고학 전용 HGIS  |  CRS %1  |  방위: 진북  |  단위: m  |  범례·축척자 포함")
                    .arg(crsAuth));
  meta->attemptSetSceneRect(QRectF(10, 13, 190, 6));
  meta->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
  layout->addLayoutItem(meta);
}

static void createOneLayout(QgsProject* project, const QString& name, const QgsRectangle& ext) {
  auto* layout = new QgsPrintLayout(project);
  layout->initializeDefaults();
  layout->setName(name);
  layout->setUnits(Qgis::LayoutUnit::Millimeters);

  const QString sheetKo = LayoutService::koreanTitle(name);
  const QString crsAuth = project->crs().authid().isEmpty()
                              ? QStringLiteral("EPSG:5186")
                              : project->crs().authid();

  addTitleBlock(layout, sheetKo, crsAuth);

  auto* map = new QgsLayoutItemMap(layout);
  map->attemptSetSceneRect(QRectF(10, 22, 145, 148));
  map->setFrameEnabled(true);
  map->setCrs(project->crs());
  map->setExtent(ext);
  map->setLayers(allMapLayers(project));
  map->setKeepLayerSet(false);
  map->setMapRotation(0.0);
  map->setFrameStrokeWidth(QgsLayoutMeasurement(0.5, Qgis::LayoutUnit::Millimeters));

  if (map->grids()) {
    auto* grid = new QgsLayoutItemMapGrid(QStringLiteral("grid"), map);
    grid->setEnabled(true);
    grid->setStyle(Qgis::MapGridStyle::Lines);
    grid->setIntervalX(100);
    grid->setIntervalY(100);
    map->grids()->addGrid(grid);
  }
  layout->addLayoutItem(map);

  const QString svg = northArrowSvg();
  if (QFile::exists(svg)) {
    auto* north = new QgsLayoutItemPicture(layout);
    north->setPicturePath(svg, Qgis::PictureFormat::SVG);
    north->setLinkedMap(map);
    north->setNorthMode(QgsLayoutItemPicture::TrueNorth);
    north->attemptSetSceneRect(QRectF(160, 22, 28, 28));
    north->setFrameEnabled(false);
    layout->addLayoutItem(north);
  } else {
    auto* north = new QgsLayoutItemLabel(layout);
    north->setText(QStringLiteral("N\n↑\n진북"));
    north->setHAlign(Qt::AlignHCenter);
    north->attemptSetSceneRect(QRectF(160, 22, 28, 24));
    north->setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    north->setFrameEnabled(true);
    layout->addLayoutItem(north);
  }

  auto* northLbl = new QgsLayoutItemLabel(layout);
  northLbl->setText(QStringLiteral("방위표 (진북)"));
  northLbl->setHAlign(Qt::AlignHCenter);
  northLbl->attemptSetSceneRect(QRectF(158, 50, 32, 6));
  northLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  layout->addLayoutItem(northLbl);

  auto* legend = new QgsLayoutItemLegend(layout);
  legend->setTitle(QStringLiteral("범 례"));
  legend->setLinkedMap(map);
  legend->setLegendFilterByMapEnabled(true);
  legend->setAutoUpdateModel(true);
  legend->attemptSetSceneRect(QRectF(158, 58, 42, 75));
  legend->setFrameEnabled(true);
  legend->setBackgroundEnabled(true);
  layout->addLayoutItem(legend);

  auto* sb = new QgsLayoutItemScaleBar(layout);
  sb->setLinkedMap(map);
  sb->setStyle(QStringLiteral("Double Box"));
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  sb->setNumberOfSegments(4);
  sb->setNumberOfSegmentsLeft(0);
  sb->applyDefaultSize(Qgis::DistanceUnit::Meters);
  sb->setHeight(3.0);
  sb->attemptSetSceneRect(QRectF(12, 172, 90, 14));
  layout->addLayoutItem(sb);

  auto* scaleLbl = new QgsLayoutItemLabel(layout);
  const int sc = map->scale() > 0 ? int(map->scale()) : 0;
  scaleLbl->setText(sc > 0 ? QStringLiteral("축척 1 : %1").arg(sc)
                           : QStringLiteral("축척: 축척자 참조"));
  scaleLbl->setId(QStringLiteral("scale_label"));
  scaleLbl->attemptSetSceneRect(QRectF(105, 174, 50, 8));
  scaleLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
  layout->addLayoutItem(scaleLbl);

  auto* note = new QgsLayoutItemLabel(layout);
  note->setText(QStringLiteral(
      "작성요령: 측량 폴리곤 기준 | 점·원 심볼 금지 | GCP≥2 | 업로드 CRS=EPSG:5179 | "
      "축척자·범례·방위표 포함"));
  note->attemptSetSceneRect(QRectF(10, 188, 190, 8));
  note->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  layout->addLayoutItem(note);

  auto* stamp = new QgsLayoutItemLabel(layout);
  stamp->setText(QStringLiteral("도엽코드: %1\n작성: ka-hgis").arg(name));
  stamp->attemptSetSceneRect(QRectF(158, 138, 42, 28));
  stamp->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
  stamp->setFrameEnabled(true);
  layout->addLayoutItem(stamp);

  project->layoutManager()->addLayout(layout);
}

int LayoutService::rebuildDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  for (const QString& name : defaultLayoutNames()) {
    if (QgsMasterLayoutInterface* old = project->layoutManager()->layoutByName(name))
      project->layoutManager()->removeLayout(old);
  }
  return ensureDefaultLayouts(project);
}

int LayoutService::ensureDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  int created = 0;
  const QgsRectangle ext = projectExtent(project);
  for (const QString& name : defaultLayoutNames()) {
    if (project->layoutManager()->layoutByName(name)) continue;
    createOneLayout(project, name, ext);
    ++created;
  }
  return created;
}

QString LayoutService::exportLayoutPdf(QgsProject* project, const QString& layoutName,
                                       const QString& pdfPath, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return {};
  }
  ensureDefaultLayouts(project);
  QgsMasterLayoutInterface* master = project->layoutManager()->layoutByName(layoutName);
  if (!master) {
    if (errorOut) *errorOut = QStringLiteral("Layout not found: %1").arg(layoutName);
    return {};
  }
  auto* layout = dynamic_cast<QgsPrintLayout*>(master);
  if (!layout) {
    if (errorOut) *errorOut = QStringLiteral("Not a print layout");
    return {};
  }

  const QgsRectangle ext = projectExtent(project);
  const QList<QgsMapLayer*> layers = allMapLayers(project);
  for (QGraphicsItem* gi : layout->items()) {
    if (auto* map = dynamic_cast<QgsLayoutItemMap*>(gi)) {
      map->setExtent(ext);
      map->setLayers(layers);
      map->setCrs(project->crs());
    }
    if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(gi)) {
      if (lbl->id() == QLatin1String("scale_label")) {
        for (QGraphicsItem* gj : layout->items()) {
          if (auto* map = dynamic_cast<QgsLayoutItemMap*>(gj)) {
            const int sc = map->scale() > 0 ? int(map->scale()) : 0;
            if (sc > 0) lbl->setText(QStringLiteral("축척 1 : %1").arg(sc));
            break;
          }
        }
      }
    }
  }

  QgsLayoutExporter exporter(layout);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  settings.forceVectorOutput = true;
  const auto r = exporter.exportToPdf(pdfPath, settings);
  if (r != QgsLayoutExporter::Success) {
    if (errorOut) *errorOut = QStringLiteral("PDF export failed code %1").arg(int(r));
    return {};
  }
  if (!QFile::exists(pdfPath) || QFileInfo(pdfPath).size() < 500) {
    if (errorOut) *errorOut = QStringLiteral("PDF missing or too small");
    return {};
  }
  return pdfPath;
}
