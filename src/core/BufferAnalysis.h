#pragma once

#include <QString>

class QgsProject;
class QgsMapCanvas;
class QgsVectorLayer;

namespace BufferAnalysis {

bool addDistanceRing(QgsProject* project, QgsMapCanvas* canvas, QgsVectorLayer* source,
                     double meters, QString* errorOut = nullptr);

}
