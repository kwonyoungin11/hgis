#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QColor>
class QgsProject;
class QgsVectorLayer;
class QgsRasterLayer;
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
  static constexpr const char* kPropAlignPending = "ka_hgis/align_pending";
  /// 토층·수계·지질: 가장 축소된 축척(분모). 더 축소하면 숨김.
  static constexpr double kThematicMinScaleDenom = 100000.0;
  static void applyThematicOverlayScaleRange(QgsMapLayer* layer);
  static bool clampCanvasToThematicScale(QgsMapCanvas* canvas);
  /// Grow a 5186 envelope around its center until the longer side equals
  /// maxSpanMeters. Too-large envelopes are returned unchanged (caller rejects).
  static QgsRectangle expandExtentToMaxSpan(const QgsRectangle& extent, double maxSpanMeters);

  static void setAlignPending(QgsMapLayer* layer, bool pending);
  static bool isAlignPending(const QgsMapLayer* layer);

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
  static bool applyAreaM2Labels(QgsVectorLayer* layer);
  static bool applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fill, const QColor& stroke,
                                     double strokeWidthMm, double markerSizeMm = 3.5,
                                     bool noFill = false, bool noStroke = false, bool dashed = false);
  static bool readSimpleVectorStyle(const QgsVectorLayer* layer, QColor* fill, QColor* stroke,
                                    double* strokeWidthMm, double* markerSizeMm,
                                    bool* noFill = nullptr, bool* noStroke = nullptr,
                                    bool* dashed = nullptr);
  static bool mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut = nullptr);

  struct FieldBasemapPackResult {
    bool satelliteOk = false;
    bool cadastralOk = false;
  };
  static double suggestCadastralScale(double currentScale, double target = 4000.0,
                                      double maxOk = 5000.0);
  static FieldBasemapPackResult prepareFieldBasemapPack(QgsProject* project, QgsMapCanvas* canvas,
                                                        const QString& apiKey,
                                                        const QString& workCrsAuthId,
                                                        QString* errorOut = nullptr);

  static bool addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut = nullptr);

  static bool addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);
  static void applyCanvasScreenDpi(QgsMapCanvas* canvas);
  static void refreshXyzBasemapTiles(QgsMapCanvas* canvas);
  // Show / DevicePixelRatioChange: 4K·혼합 DPI에서 XYZ 타일을 다시 받는다.
  static bool canvasDisplayEventNeedsTileRefresh(int eventType);

  static bool addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);
  // VWorld GetCapabilities only publish 4326 (and 900913/3857). 5186/5187/5179
  // make QGIS WMS fail with "Cannot calculate extent".
  static QStringList cadastralWmsCrsCandidates(const QString& workCrsAuthId = {});

  static bool addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  // 흙토람(농진청) 토양도 신청으로 내려받은 SHP를 참조 지도로 불러온다.
  // crsOverrideAuthId가 비어 있지 않으면 레이어 좌표계를 그 값으로 지정한다
  // (.prj 없는 배포본 대응 — 흙토람 고시 좌표계는 EPSG:2097 중부원점/Bessel).
  // categoryField가 있으면 그 필드 값별 반투명 색으로 구분한다(분포지형 등).
  // 성공 시 프로젝트에 추가된 레이어를 반환한다.
  static QgsVectorLayer* addSoilShapefile(QgsProject* project, QgsMapCanvas* canvas,
                                          const QString& path, const QString& crsOverrideAuthId,
                                          const QString& categoryField, QString* errorOut = nullptr);

  static bool setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity);

  static bool toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible);

  static bool addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                              QString* errorOut = nullptr);

  static bool setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                         QString* errorOut = nullptr, bool zoomKorea = true);

  static bool ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId);

  static QgsRectangle koreaExtentForCrs(const QString& epsgAuthId);
  // 3857 위성 커버를 작업 CRS로 옮긴 뒤, 타일이 화면을 메우는 안쪽 상자.
  static QgsRectangle satelliteFillExtentForCrs(const QString& epsgAuthId);

  static void zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId, bool refresh = true);
  static void syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea = true);
  static QList<QgsMapLayer*> visibleLayersPaintOrder(QgsProject* project);
  static bool zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer);
  static bool isolateAndZoomToLayer(QgsProject* project, QgsMapCanvas* canvas, QgsMapLayer* layer,
                                    bool keepReference = true);
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
  // RGB GeoTIFF 용지 흰색을 투명으로. 이미 적용돼 있으면 그대로 둔다.
  static void knockOutRasterPaper(QgsRasterLayer* layer);
  static void knockOutProjectRasterPaper(QgsProject* project);
  static QString layerKeyOf(const QgsMapLayer* layer);
  static bool isReferenceLayer(const QgsMapLayer* layer);
  static bool isBasemapLayer(const QgsMapLayer* layer);
  static QgsVectorLayer* findByLayerKey(QgsProject* project, const QString& layerKey);
  // Current layer is used only when its ka_hgis/layer_key equals requiredKey.
  // 유구면 must not fall back to survey_area just because it is the current polygon.
  static QgsVectorLayer* digitizeTargetLayer(QgsProject* project, QgsVectorLayer* current,
                                             const QString& requiredKey);
  static void removeSurveyDomainLayers(QgsProject* project);
  static QStringList domainLayerKeys();
  static void pruneEmptyLegendGroups(QgsProject* project);
  static QgsVectorLayer* ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                           const QString& layerKey, const QString& titleKo,
                                           QString* errorOut = nullptr);
  static QgsVectorLayer* createUserPolygonLayer(QgsProject* project, const QString& gpkgPath,
                                                const QString& titleKo, const QString& crsAuthId,
                                                QString* errorOut = nullptr);
  static void applyLegendCrsLabel(QgsMapLayer* layer);
  static bool undoCommittedFeature(QgsVectorLayer* layer, qint64 featureId, QString* errorOut = nullptr);
};
