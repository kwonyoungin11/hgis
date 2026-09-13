#pragma once

#include <QString>
#include <qgsrectangle.h>

class QgsRasterLayer;
class QgsMapCanvas;
class QgsCoordinateReferenceSystem;

// Shared display policy. Elevations remain in the original datasource.
namespace DemPresentation {
bool apply(QgsRasterLayer* layer, const QString& preset = QStringLiteral("national"),
           const QgsRectangle& extent = QgsRectangle());
bool restore(QgsRasterLayer* layer);
void followCanvas(QgsRasterLayer* layer, QgsMapCanvas* canvas);
QString reliefSource(QgsRasterLayer* layer, const QgsCoordinateReferenceSystem& workCrs,
                     QString* error);
}
