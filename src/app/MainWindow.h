#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QPair>
#include <QVector>
#include "core/LocationSearch.h"
class QListWidget;
class QListWidgetItem;
class QLabel;
class QToolBar;
class QToolButton;
class QLineEdit;
class QComboBox;
class QEvent;
class ChecklistEngine;
#if KA_HGIS_HAS_QGIS
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsVectorLayer;
class QgsMapTool;
class KaCaptureMapTool;
class KaAttributeMapTool;
class KaDrawingStudio;
class QgsMapToolPan;
class QgsMapToolSelect;
class QgsGeometry;
class QgsFeature;
class QgsLayerTreeMapCanvasBridge;
#endif

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  bool openSurveyGpkg(const QString& gpkgPath);
  int domainLayerCount() const;
  QString workCrsAuthId() const { return m_workCrs; }
  void runChecklistPublic() { runChecklist(); }
  int seedDemoFieldData();
  int lastChecklistErrorCount() const;

private slots:
  void newSurvey();
  void openVectorLayer();
  void saveProject();
  void openProject();
  void startEditSurveyArea();
  void startEditFeaturePoly();
  void startEditFeatureLine();
  void startEditSectionLine();
  void startEditArtifact();
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
  void addBasemapVworld();
  void addBasemapVworldSat();
  void addBasemapVworldCadastral();
  void addBasemapOsm();
  void addBasemapGoogle();
  void removeSelectedLayers();
  void georefAssistant();
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
  void showSubToolsBasemap();
  void showSubToolsSubmit();
  void hideSubTools();
  void applySnapConfig();
  void openLayoutDesigner();
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
  bool tryAddDroppedUrls(const QList<QUrl>& urls);
  bool tryAddDroppedPaths(const QStringList& paths);
  QStringList selectedBrowserFiles() const;
  void loadSurveyLayers(const QString& gpkgOrStub);
  void applyStartupMap();
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
  void beginEdit(QgsVectorLayer* layer);
  void onGeometryCaptured(const QgsGeometry& geom);
  void stopCaptureTool();
  void ensureAttributeTool();
  void editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature);
  void editAttributesAtCanvasPos(const QPoint& canvasPos);
  static QString attributeFieldLabelKo(const QString& fieldName);
#endif

  void refreshLayerEmptyState();
  QLabel* m_layerEmpty = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_checkView = nullptr;
  QLabel* m_workHint = nullptr;
  QListWidget* m_workList = nullptr;
  QLineEdit* m_searchEdit = nullptr;
  QListWidget* m_fileBrowser = nullptr;
  QString m_browserPath;
  ChecklistEngine* m_checklist = nullptr;
  LocationSearch* m_locator = nullptr;
  QString m_surveyPath;
  QString m_workCrs = QStringLiteral("EPSG:5186");
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
  KaAttributeMapTool* m_attributeTool = nullptr;
  QgsMapToolPan* m_panTool = nullptr;
  QgsMapToolSelect* m_selectTool = nullptr;
  QgsVectorLayer* m_editLayer = nullptr;
  QgsLayerTreeMapCanvasBridge* m_bridge = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  bool m_scaleUiGuard = false;
  bool m_extentClampGuard = false;
  QToolBar* m_subToolbar = nullptr;
  QString m_subToolsMode;
  bool m_snapEnabled = true;
  KaDrawingStudio* m_drawingStudio = nullptr;
  QVector<QPair<QString, qint64>> m_committedUndo;
#endif
};
