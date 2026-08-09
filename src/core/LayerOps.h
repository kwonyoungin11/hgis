#pragma once
#include <QString>
class QgsProject;
class QgsVectorLayer;
class QgsMapCanvas;
class QgsRectangle;

class LayerOps {
public:
  enum class KoreaBasemap {
    VWorldBase,
    VWorldSatellite,
    VWorldHybrid,
    VWorldParcel,
    GoogleRoad,
    GoogleSatellite,
    Osm
  };

  static QString reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                      const QString& outPath, QgsProject* project, QString* errorOut = nullptr);

  static int ensureControlPointQualityFields(QgsVectorLayer* controlPoints);

  static bool applyFeaturePolyStyle(QgsVectorLayer* featurePoly);

  static bool applyReferenceVectorStyle(QgsVectorLayer* layer, const QString& role = QString());

  static bool addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                              QString* errorOut = nullptr);

  static bool addKoreaBasemapWithParcel(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                                        QString* errorOut = nullptr);

  static bool addVworldParcelOverlay(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                         QString* errorOut = nullptr);

  static QgsRectangle koreaExtentForCrs(const QString& epsgAuthId);

  static void zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId);

  // Convert any vector (SHP/GPKG layer) drawn in 5186/5187/etc → EPSG:5179 SHP for intranet upload.
  static QString convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                  QgsProject* project, QString* errorOut = nullptr);

  static QString convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                      QgsProject* project, QString* errorOut = nullptr);

  static QString georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                         QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);
};
