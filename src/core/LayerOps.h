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
  static QString detectNameField(const QgsVectorLayer* layer);
  static bool applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName = QString(),
                                       double fontSizePt = 5.0, bool showArea = false);
  static double labelFontSize(const QgsVectorLayer* layer, double defaultSize = 5.0);
  static bool labelShowArea(const QgsVectorLayer* layer, bool defaultShow = false);
  static QString currentLabelField(const QgsVectorLayer* layer);
  // 외부에서 받은 SHP 파일(.cpg 없는 경우)의 한글 깨짐을 방지하기 위한 인코딩 준비 및 변경
  static QString prepareShapefileEncoding(const QString& shpPath);
  static bool setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding);
  // Vector labeling only. Cadastral WMS/XYZ text is baked into tiles.
  static bool hasToggleableLabels(const QgsMapLayer* layer);
  static bool labelsVisible(const QgsMapLayer* layer);
  static bool setLabelsVisible(QgsMapLayer* layer, bool on);
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
  // XYZ/WMS 타일: 브라우저형 User-Agent. 입체지형 Google drape도 호출한다.
  static void ensureTileNetworkIdentity();
  // Show / DevicePixelRatioChange: 4K·혼합 DPI에서 XYZ 타일을 다시 받는다.
  static bool canvasDisplayEventNeedsTileRefresh(int eventType);

  static bool addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);
  // VWorld GetCapabilities only publish 4326 (and 900913/3857). 5186/5187/5179
  // make QGIS WMS fail with "Cannot calculate extent".
  static QStringList cadastralWmsCrsCandidates(const QString& workCrsAuthId = {});

  static bool addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  static bool addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut = nullptr);

  // 지형맵: OpenTopoMap XYZ only (EPSG:3857). Never VWorld WMS GetMap —
  // WMS + OTF(QgsRasterProjector) + pan aborts TileDownloadManager (Windows AV).
  // 작업 CRS(5186/5187)는 OTF destinationCrs — URI에 5186/5179를 넣지 않는다.
  static bool addElevationHillshadeMap(QgsProject* project, QgsMapCanvas* canvas,
                                       const QString& apiKey, QString* errorOut = nullptr);

  // DEM click: Copernicus GLO-30 COG (single-band, meter legend) when the
  // canvas has an extent; NASA GIBS color tiles as fallback. Never VWorld WMS.
  static bool addDemColorReliefMap(QgsProject* project, QgsMapCanvas* canvas,
                                   QString* errorOut = nullptr);
  // Copernicus GLO-30 COG (/vsicurl). lat/lon in WGS84 degrees.
  static QString copernicusCogUriForWgs84(double latDeg, double lonDeg);

  // 국토지리원 공개DEM(.img) / GeoTIFF. Single-band elevation + meter ramp.
  static bool addDemElevationRaster(QgsProject* project, QgsMapCanvas* canvas,
                                    const QString& path, QString* errorOut = nullptr);

  // Color-ramp renderer so 조판 범례 lists height in meters.
  struct DemElevationClass {
    double lo = 0.0;
    double hi = 0.0;  // last class is +inf
    QColor color;
    QString label;
  };
  struct DemElevationStyle {
    int classCount = 0;      // 0 = auto (2–8)
    double stepMeters = 0.0; // 0 = auto nice step
    QList<DemElevationClass> classes;  // if set, used as-is (last forced +inf)
  };
  static bool applyDemElevationStyle(QgsRasterLayer* layer);
  static bool applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent);
  static bool applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent,
                                     const DemElevationStyle& style);
  static QList<DemElevationClass> buildDemElevationClasses(double zMin, double zMax, int classCount,
                                                           double stepMeters);
  static QList<DemElevationClass> readDemElevationClasses(const QgsRasterLayer* layer);
  // Equal-interval step (m) so alluvial sites get 1–5 m classes, not one -2…1155 band.
  static double demElevationClassStep(double zMin, double zMax);
  // Ensure a high-resolution shaded relief layer (Hillshade) is blended over the DEM (Multiply blend).
  static QgsRasterLayer* ensureDemRelief(QgsProject* project, QgsRasterLayer* demLayer);

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
  static bool isLayerVisible(QgsProject* project, const QString& name);
  // refresh() only when no WMS/XYZ job is in flight (provider_wms deleteLater AV).
  static void refreshCanvasIfIdle(QgsMapCanvas* canvas);

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
  // 미리 받아 둔 MBTiles 타일팩을 참조 지도로 올린다. 원격 XYZ와 같은 자리에
  // 쓰이지만 네트워크를 타지 않는다(TilePackService가 만든 파일).
  static bool addTilePackBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                 const QString& name, QString* errorOut);
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
  // Deletes every saved feature and writes GPKG. After this, ensureDomainLayer
  // must reopen the same table empty — legend-only remove leaves the polygons.
  static bool purgeCommittedFeatures(QgsVectorLayer* layer, QString* errorOut = nullptr);
  // Moves one vertex of a saved feature. Does not commit; caller writes GPKG.
  static bool moveFeatureVertex(QgsVectorLayer* layer, qint64 featureId, int vertex,
                                double x, double y, QString* errorOut = nullptr);
};
