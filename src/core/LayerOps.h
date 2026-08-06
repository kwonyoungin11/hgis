#pragma once
#include <QString>
class QgsProject;
class QgsVectorLayer;
class QgsMapCanvas;

class LayerOps {
public:
  // Reproject active vector layer to target CRS auth id, write new file, add to project.
  static QString reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                      const QString& outPath, QgsProject* project, QString* errorOut = nullptr);

  // Ensure GNSS quality fields exist on control_points (migration).
  static int ensureControlPointQualityFields(QgsVectorLayer* controlPoints);

  // Apply categorized renderer on feature_poly by "kind" (fallback "period").
  static bool applyFeaturePolyStyle(QgsVectorLayer* featurePoly);

  // Add OSM XYZ basemap raster layer.
  static bool addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  // Georeference image using >=2 control_points: write .pgw/.jgw worldfile (affine from first 2 pts)
  // and load as raster. Simplified: maps image corners using GCP pixel assumption if only geo coords.
  // Better path: user provides image; GCPs are map coords; we write world file placing image
  // with origin at GCP1 and pixel size from GCP1-GCP2 distance / image width.
  static QString georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                         QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);
};
