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
#include <qgslayoutexporter.h>
#include <qgsmasterlayoutinterface.h>
#include <qgspagesizeregistry.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutitempage.h>
#include <qgscoordinatereferencesystem.h>

QStringList LayoutService::defaultLayoutNames() {
  return {
    QStringLiteral("survey_area_map"),
    QStringLiteral("site_location"),
    QStringLiteral("feature_plan"),
    QStringLiteral("feature_detail"),
    QStringLiteral("section")
  };
}

int LayoutService::ensureDefaultLayouts(QgsProject* project) {
  if (!project) return 0;
  int created = 0;
  for (const QString& name : defaultLayoutNames()) {
    if (project->layoutManager()->layoutByName(name)) continue;
    auto* layout = new QgsPrintLayout(project);
    layout->initializeDefaults();
    layout->setName(name);

    // title
    auto* title = new QgsLayoutItemLabel(layout);
    title->setText(QStringLiteral("KA-HGIS / %1").arg(name));
    title->attemptSetSceneRect(QRectF(10, 5, 180, 10));
    title->setFont(QFont(QStringLiteral("Sans Serif"), 14, QFont::Bold));
    layout->addLayoutItem(title);

    // map
    auto* map = new QgsLayoutItemMap(layout);
    map->attemptSetSceneRect(QRectF(10, 20, 190, 150));
    map->setFrameEnabled(true);
    if (project->crs().isValid()) {
      // extent from project layers
      QgsRectangle ext;
      bool first = true;
      const auto layers = project->mapLayers().values();
      for (QgsMapLayer* ml : layers) {
        if (!ml) continue;
        if (first) { ext = ml->extent(); first = false; }
        else ext.combineExtentWith(ml->extent());
      }
      if (!first && !ext.isEmpty()) {
        map->setExtent(ext);
      }
    }
    layout->addLayoutItem(map);

    // CRS label
    auto* crs = new QgsLayoutItemLabel(layout);
    crs->setText(QStringLiteral("CRS: %1").arg(project->crs().authid()));
    crs->attemptSetSceneRect(QRectF(10, 175, 100, 8));
    layout->addLayoutItem(crs);

    // scale bar
    auto* sb = new QgsLayoutItemScaleBar(layout);
    sb->setLinkedMap(map);
    sb->attemptSetSceneRect(QRectF(120, 175, 70, 10));
    layout->addLayoutItem(sb);

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

