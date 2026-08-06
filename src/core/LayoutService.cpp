#include "LayoutService.h"
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QRectF>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgsproject.h>
#include <qgsprintlayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitemlegend.h>
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

static QString koreanTitle(const QString& name) {
  if (name == QLatin1String("survey_area_map")) return QStringLiteral("조사구역도");
  if (name == QLatin1String("site_location")) return QStringLiteral("유적위치도");
  if (name == QLatin1String("feature_plan")) return QStringLiteral("유구배치도");
  if (name == QLatin1String("feature_detail")) return QStringLiteral("개별유구실측도");
  if (name == QLatin1String("section")) return QStringLiteral("층위도");
  return name;
}

static QgsRectangle projectExtent(QgsProject* project) {
  QgsRectangle ext;
  bool first = true;
  const auto layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (!ml || ml->extent().isEmpty()) continue;
    if (first) {
      ext = ml->extent();
      first = false;
    } else {
      ext.combineExtentWith(ml->extent());
    }
  }
  if (first || ext.isEmpty()) {
    // Korea approx in EPSG:5179-ish meters fallback
    ext = QgsRectangle(900000, 1800000, 1200000, 2100000);
  } else {
    ext.scale(1.15);
  }
  return ext;
}

static QList<QgsMapLayer*> projectLayersList(QgsProject* project) {
  QList<QgsMapLayer*> out;
  const auto layers = project->mapLayers().values();
  for (QgsMapLayer* ml : layers) {
    if (ml) out.append(ml);
  }
  return out;
}

int LayoutService::ensureDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  int created = 0;
  const QgsRectangle ext = projectExtent(project);
  const QList<QgsMapLayer*> mapLayers = projectLayersList(project);

  for (const QString& name : defaultLayoutNames()) {
    if (project->layoutManager()->layoutByName(name)) continue;

    auto* layout = new QgsPrintLayout(project);
    layout->initializeDefaults();
    layout->setName(name);

    // Title block
    auto* title = new QgsLayoutItemLabel(layout);
    title->setText(QStringLiteral("고고학 전용 HGIS — %1").arg(koreanTitle(name)));
    title->attemptSetSceneRect(QRectF(10, 4, 150, 10));
    title->setFont(QFont(QStringLiteral("Malgun Gothic"), 14, QFont::Bold));
    layout->addLayoutItem(title);

    // Subtitle: scale note + sheet
    auto* sub = new QgsLayoutItemLabel(layout);
    sub->setText(QStringLiteral("권장축척 참고: 위치 1:50,000~1:5,000 / 배치·실측 상세축척 | 도엽: %1").arg(name));
    sub->attemptSetSceneRect(QRectF(10, 14, 190, 6));
    sub->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
    layout->addLayoutItem(sub);

    // Map
    auto* map = new QgsLayoutItemMap(layout);
    map->attemptSetSceneRect(QRectF(10, 22, 150, 145));
    map->setFrameEnabled(true);
    map->setCrs(project->crs());
    map->setExtent(ext);
    if (!mapLayers.isEmpty()) {
      map->setLayers(mapLayers);
      map->setKeepLayerSet(true);
    }
    map->setMapRotation(0.0); // 진북 기준 0°
    layout->addLayoutItem(map);

    // North arrow (text-based, always available)
    auto* north = new QgsLayoutItemLabel(layout);
    north->setText(QStringLiteral("N\n↑\n진북"));
    north->setHAlign(Qt::AlignHCenter);
    north->attemptSetSceneRect(QRectF(165, 22, 25, 22));
    north->setFont(QFont(QStringLiteral("Malgun Gothic"), 10, QFont::Bold));
    north->setFrameEnabled(true);
    layout->addLayoutItem(north);

    // Legend
    auto* legend = new QgsLayoutItemLegend(layout);
    legend->setTitle(QStringLiteral("범례"));
    legend->setLinkedMap(map);
    legend->setAutoUpdateModel(true);
    legend->attemptSetSceneRect(QRectF(165, 48, 35, 70));
    legend->setFrameEnabled(true);
    layout->addLayoutItem(legend);

    // Scale bar
    auto* sb = new QgsLayoutItemScaleBar(layout);
    sb->setLinkedMap(map);
    sb->setUnits(Qgis::DistanceUnit::Meters);
    sb->applyDefaultSize(Qgis::DistanceUnit::Meters);
    sb->setNumberOfSegments(4);
    sb->attemptSetSceneRect(QRectF(10, 170, 80, 12));
    layout->addLayoutItem(sb);

    // Scale denominator label
    auto* scaleLbl = new QgsLayoutItemLabel(layout);
    const double scale = map->scale();
    scaleLbl->setText(scale > 0
                          ? QStringLiteral("축척 약 1:%1").arg(int(scale))
                          : QStringLiteral("축척: 지도 참조"));
    scaleLbl->attemptSetSceneRect(QRectF(95, 172, 50, 8));
    scaleLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8));
    layout->addLayoutItem(scaleLbl);

    // CRS + metadata footer
    auto* footer = new QgsLayoutItemLabel(layout);
    footer->setText(QStringLiteral("CRS: %1 | 작성: ka-hgis | 방위: 진북(지도 회전 0°) | 범례: 레이어 스타일 기준")
                        .arg(project->crs().authid().isEmpty() ? QStringLiteral("EPSG:5179") : project->crs().authid()));
    footer->attemptSetSceneRect(QRectF(10, 185, 190, 8));
    footer->setFont(QFont(QStringLiteral("Malgun Gothic"), 7));
    layout->addLayoutItem(footer);

    project->layoutManager()->addLayout(layout);
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
    if (errorOut) *errorOut = QStringLiteral("Not a print layout: %1").arg(layoutName);
    return {};
  }

  // refresh map extents before export
  const QgsRectangle ext = projectExtent(project);
  const auto items = layout->items();
  for (QGraphicsItem* gi : items) {
    if (auto* map = dynamic_cast<QgsLayoutItemMap*>(gi)) {
      map->setExtent(ext);
      map->setLayers(projectLayersList(project));
    }
  }

  QgsLayoutExporter exporter(layout);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  settings.forceVectorOutput = true;
  const QgsLayoutExporter::ExportResult r = exporter.exportToPdf(pdfPath, settings);
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
