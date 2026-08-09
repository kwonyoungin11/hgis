#pragma once
#include <QString>
#include <QStringList>
class QgsProject;
class QgsVectorLayer;
class QgsMapLayer;
class QgsMapCanvas;
class QgsRectangle;
class QgsLayerTreeGroup;

class LayerOps {
public:
  static constexpr const char* kGroupSurveyData = "조사 데이터";
  static constexpr const char* kGroupReference = "참조 지도";
  static constexpr const char* kPropLayerKey = "ka_hgis/layer_key";
  static constexpr const char* kPropLayerRole = "ka_hgis/layer_role";
  static constexpr const char* kRoleSurvey = "survey";
  static constexpr const char* kRoleReference = "reference";

  enum class KoreaBasemap {
    VWorldBase,
    VWorldSatellite,
    VWorldHybrid,
    GoogleRoad,
    GoogleSatellite,
    Osm
  };

  static QString reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                      const QString& outPath, QgsProject* project, QString* errorOut = nullptr);

  static int ensureControlPointQualityFields(QgsVectorLayer* controlPoints);

  static bool applyFeaturePolyStyle(QgsVectorLayer* featurePoly);
  static bool applyDomainDrawStyle(QgsVectorLayer* layer, const QString& layerKey = {});
  static bool mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut = nullptr);

  static bool addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity);

  static bool toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible);

  static bool addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                              QString* errorOut = nullptr);

  static bool setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                         QString* errorOut = nullptr, bool zoomKorea = true);

  static bool ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId);

  static QgsRectangle koreaExtentForCrs(const QString& epsgAuthId);

  static void zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId);
  static void syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea = true);
  static void zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer);
  static void zoomToFullMax(QgsMapCanvas* canvas);
  static void applyKoreaMapLimits(QgsProject* project, QgsMapCanvas* canvas);
  static bool clampCanvasToKorea(QgsMapCanvas* canvas);

  static QString convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                  QgsProject* project, QString* errorOut = nullptr);

  static QString convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                      QgsProject* project, QString* errorOut = nullptr);

  static QString georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                         QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool hasVisibleReferenceLayer(QgsProject* project);

  static bool removeConfirmedLayers(QgsProject* project, QgsMapCanvas* canvas, const QStringList& layerIds);

  static int importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath,
                                    QString* errorOut = nullptr);

  static QgsLayerTreeGroup* ensureLegendGroup(QgsProject* project, const QString& groupName);
  static void placeInLegendGroup(QgsProject* project, QgsMapLayer* layer, const QString& groupName,
                                 bool insertAtBottom = false);
  static void markSurveyLayer(QgsMapLayer* layer, const QString& layerKey);
  static void markReferenceLayer(QgsMapLayer* layer);
  static QString layerKeyOf(const QgsMapLayer* layer);
  static bool isReferenceLayer(const QgsMapLayer* layer);
  static QgsVectorLayer* findByLayerKey(QgsProject* project, const QString& layerKey);
  static void removeSurveyDomainLayers(QgsProject* project);
  static QStringList domainLayerKeys();
  static void pruneEmptyLegendGroups(QgsProject* project);
  static QgsVectorLayer* ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                           const QString& layerKey, const QString& titleKo,
                                           QString* errorOut = nullptr);
  static void applyLegendCrsLabel(QgsMapLayer* layer);
};
