#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QPair>
#include <QVector>
#include <QHash>
#include <QSet>
#include <vector>
#include "core/LocationSearch.h"
#include "core/TrenchGridGenerator.h"
class QListWidget;
class QListWidgetItem;
class QAction;
class QLabel;
class QToolBar;
class QToolButton;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QShowEvent;
class QCloseEvent;
class QTabWidget;
class ChecklistEngine;
class KaStatusBar;
#if KA_HGIS_HAS_QGIS
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsMapLayer;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsMapTool;
class QgsMapToolEmitPoint;
class QgsPointXY;
class KaCaptureMapTool;
class KaAttributeMapTool;
class KaAlignMapTool;
class KaAlignPickTool;
class KaImageView;
class KaAlignLinkOverlay;
class KaDrawingStudio;
class KaSectionDrawingStudio;
class KaStartPage;
class KaCoordPointMapTool;
class KaMeasureMapTool;
class QSplitter;
class QListWidget;
class QTimer;
class QgsVertexMarker;
class QgsMapToolPan;
class QgsMapToolSelect;
class QgsGeometry;
class QgsFeature;
class QgsLayerTreeMapCanvasBridge;
class QgsMessageBar;
#endif

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  bool openSurveyGpkg(const QString& gpkgPath);
  int domainLayerCount() const;
  QString workCrsAuthId() const { return m_workCrs; }
  void runChecklistPublic() { runChecklist(); }
  int seedDemoFieldData();
  int lastChecklistErrorCount() const;

private slots:
  void newSurvey();
  void openVectorLayer();
  void importSoilShapefile();
  void downloadSoilTerrain();
  void downloadGeologyMap();
  void downloadRiverMap();
  void saveProject();
  void openProject();
  void startEditSurveyArea();
  void startEditFeaturePoly();
  void startEditFeatureLine();
  void startEditSectionLine();
  void startEditArtifact();
  void startEasyDraw();
  void mergeFeaturePolygons();
  void onWorkControlClicked(QListWidgetItem* item);
  void refreshWorkPanel();
  void saveEdits();
  void stopEdits();
  void startAttributeEditTool();
  void editCurrentLayerAttributes();
  void editCurrentLayerStyle();
  void addUserLayer();
  void addControlPoint();
  void importControlCsv();
  void runChecklist();
  void exportPdf();
  void exportShpPackage();
  void crsDefineOnly();
  void crsReproject();
  void setWorkCrs5186();
  void setWorkCrs5187();
  void convertSelectedTo5179();
  void convertSelected5186To5179();
  void convertSelected5187To5179();
  void convertShpFileTo5179();
  void startSelectTool();
  void startMeasureTool();
  void runDemHillshade();
  void startTrenchGrid();
  void placeTrenchGridAt(const QgsPointXY& origin);
  // Replaces the previous grid (clearLayer) and reports the excavation ratio
  // against the survey area when known. areaM2 <= 0 skips the ratio line.
  bool applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells, double areaM2);
  // 속성 창의 「적용」: 자동 채움이면 구역 재배치, 아니면 기존 격자 중심을
  // 유지한 채 회전·간격만 바꿔 재배치. 격자가 없으면 원점 클릭으로 넘어간다.
  void applyTrenchFromDialog();
  void beginTrenchOriginPick();
  void startTrenchGridMove();   // 전체 이동 모드
  void startTrenchGridEdit();   // 개별 편집 모드(그래픽식 선택·이동·삭제)
  void activateTrenchTool(bool single);
  void toggleMapGrid();
  void addBasemapVworld();
  void addBasemapVworldSat();
  void addBasemapVworldCadastral();
  void addBasemapOsm();
  void addBasemapGoogle();
  void removeSelectedLayers();
  void georefAssistant();
  void showSubToolsAlign();
  void showAbout();
  void runLocationSearch();
  void configureVworldKey();
  void rebuildLayouts();
  void onFileBrowserActivated(QListWidgetItem* item);
  void exportReportLayout();
  void browseDataFolder();
  void goFileBrowserRoot(const QString& path);
  static QString resolvedDesktopPath();
  void onMapContextMenu(const QPoint& pos);
  void onLayerTreeContextMenu(const QPoint& pos);
  void renameSelectedLayer();
  void onLayerTreeDoubleClicked(const QModelIndex& index);
  void zoomMapToFullMax();
  void zoomSelectedLayerMax();
  void onCanvasScaleChanged(double scale);
  void applyMapScaleFromUi();
  void refreshMapCanvasNow();
  void showSubToolsDraw();
  void showSubToolsBuffer();
  void showSubToolsBasemap();
  void showSubToolsSubmit();
  void hideSubTools();
  void startCoordPointTool();
  void runSiteBuffer500();
  void runSiteBuffer1000();
  void applySnapConfig();
  void openLayoutDesigner();
  void openSectionDesigner();
  void onViewTabCloseRequested(int index);
  void openRecentSurvey(const QString& path);
  void showHomePage();
  void showMapWorkspace();
  void rememberSurvey(const QString& path, const QString& name);
  void undoLastAction();

private:
  void buildUi();
  void buildMenus();
  void setupWorkPanel();
  void updateNextActionStatus();
  void setupFileBrowser();
  void clearSubToolbar();
  bool addVectorFromPath(const QString& path);
  bool addRasterFromPath(const QString& path);
  bool addSectionGeoTiffFromPath(const QString& path, const QString& crsAuthId);
  bool tryAddDroppedUrls(const QList<QUrl>& urls);
  bool tryAddDroppedPaths(const QStringList& paths);
  QStringList selectedBrowserFiles() const;
  void loadSurveyLayers(const QString& gpkgOrStub);
  void ensureDefaultBasemaps();
  void applyStartupMap();
  void ensureStartupViewReady();
  void scheduleMapDisplayRefresh();
  void bindMapDisplayScreen();
  void setWorkCrs(const QString& authId);
  void onLocationResults(const QVector<LocationHit>& hits);
  void onLocationFailed(const QString& message);
  void zoomToLocation(const LocationHit& hit);
  QJsonObject buildProjectState() const;
  QString rulesPath() const;
  QString vworldApiKeyOrPrompt();
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layerByKey(const QString& layerKey) const;
  QgsVectorLayer* ensureDomainLayerForEdit(const QString& layerKey, const QString& titleKo);
  void onLayerTreeRowsMoved();
  void moveSelectedLayer(int dir);
  void startAlignSession(QgsMapLayer* layer);
  void stopAlignSession();
  void ensureAlignSplit();
  void showAlignSplit();
  void hideAlignSplit();
  void refreshAlignUi();
  void updateAlignOverlay();
  void trackAlignPointer(const QPoint& globalPos);
  void deleteSelectedAlignPoint();
  void applyAlignMove();
  void beginEdit(QgsVectorLayer* layer);
  void onGeometryCaptured(const QgsGeometry& geom);
  void stopCaptureTool();
  void ensureAttributeTool();
  void editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature);
  void editAttributesAtCanvasPos(const QPoint& canvasPos);
  static QString attributeFieldLabelKo(const QString& fieldName);
#endif

  // Non-blocking feedback on the canvas. Reserve QMessageBox for questions and
  // for failures the user must acknowledge before anything else happens.
  enum class Notice { Info, Success, Warning, Critical };
  void notify(Notice level, const QString& title, const QString& text,
              const QString& details = QString());

  void refreshLayerEmptyState();
  QLabel* m_layerEmpty = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_checkView = nullptr;
  KaStatusBar* m_status = nullptr;
  QLabel* m_workHint = nullptr;
  QListWidget* m_workList = nullptr;
  QLineEdit* m_searchEdit = nullptr;
  QListWidget* m_fileBrowser = nullptr;
  QString m_browserPath;
  ChecklistEngine* m_checklist = nullptr;
  LocationSearch* m_locator = nullptr;
  QString m_surveyPath;
  QString m_workCrs = QStringLiteral("EPSG:5187");
#if KA_HGIS_HAS_QGIS
  void healTileLayer(QgsRasterLayer* layer);
#endif
  // 타일 자동 복구 상태: 레이어별 재시도 횟수(화면 이동 시 초기화)와 예약 중 표시.
  QHash<QString, int> m_tileHealCount;
  QSet<QString> m_tileHealPending;
  int m_stubSurveyArea = 0;
  int m_stubFeatures = 0;
  int m_stubGcp = 0;
  bool m_stubHasMeta = false;
  bool m_packageCreated = false;
  mutable int m_lastChecklistErrors = -1;
#if KA_HGIS_HAS_QGIS
  QgsMapCanvas* m_canvas = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  KaCaptureMapTool* m_captureTool = nullptr;
  KaCoordPointMapTool* m_coordPointTool = nullptr;
  KaMeasureMapTool* m_measureTool = nullptr;
  QAction* m_actMeasure = nullptr;
  QAction* m_actSelect = nullptr;
  QToolButton* m_btnDraw = nullptr;
  QgsMapToolEmitPoint* m_trenchOriginTool = nullptr;
  class KaTrenchMoveTool* m_trenchMoveTool = nullptr;
  class KaTrenchDialog* m_trenchDlg = nullptr;
  class KaCanvasGridOverlay* m_mapGrid = nullptr;
  QCheckBox* m_mapGridCheck = nullptr;
  QDoubleSpinBox* m_mapGridStep = nullptr;
  QDoubleSpinBox* m_mapGridRot = nullptr;
  QDoubleSpinBox* m_mapGridWidth = nullptr;
  QComboBox* m_mapGridDash = nullptr;
  KaAttributeMapTool* m_attributeTool = nullptr;
  KaAlignMapTool* m_alignTool = nullptr;
  KaAlignPickTool* m_alignPickTool = nullptr;
  KaImageView* m_alignImage = nullptr;
  QgsMapCanvas* m_alignLeftCanvas = nullptr;
  QWidget* m_alignLeftPane = nullptr;
  QLabel* m_alignLeftLabel = nullptr;
  QListWidget* m_alignPointList = nullptr;
  KaAlignLinkOverlay* m_alignOverlay = nullptr;
  QVector<QgsVertexMarker*> m_alignLeftMarks;
  QSplitter* m_mapSplitter = nullptr;
  bool m_alignApplied = false;
  bool m_alignCursorValid = false;
  bool m_alignLiveScreenValid = false;
  double m_alignCursorX = 0;
  double m_alignCursorY = 0;
  QPoint m_alignLiveScreen;
  QVector<QPoint> m_alignDstScreen;
  QTimer* m_alignCursorTimer = nullptr;
  QgsMapToolPan* m_panTool = nullptr;
  QgsMapToolSelect* m_selectTool = nullptr;
  QgsVectorLayer* m_editLayer = nullptr;
  QgsLayerTreeMapCanvasBridge* m_bridge = nullptr;
  QgsMessageBar* m_messageBar = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  bool m_scaleUiGuard = false;
  bool m_extentClampGuard = false;
  bool m_startupViewApplied = false;
  bool m_mapScreenBound = false;
  QTimer* m_displayRefresh = nullptr;
  QToolBar* m_subToolbar = nullptr;
  QString m_subToolsMode;
  bool m_snapEnabled = true;
  KaDrawingStudio* m_drawingStudio = nullptr;
  KaSectionDrawingStudio* m_sectionStudio = nullptr;
  QTabWidget* m_viewTabs = nullptr;
  KaStartPage* m_startPage = nullptr;
  QWidget* m_mapPage = nullptr;
  QVector<QPair<QString, qint64>> m_committedUndo;
#endif
};
