#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include "core/LocationSearch.h"
class QListWidget;
class QListWidgetItem;
class QLabel;
class QToolBar;
class QLineEdit;
class QEvent;
class ChecklistEngine;
#if KA_HGIS_HAS_QGIS
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsVectorLayer;
class QgsMapTool;
class KaCaptureMapTool;
class QgsMapToolPan;
class QgsGeometry;
#endif

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  void onStepChanged(int row);
  void newSurvey();
  void openVectorLayer();
  void saveProject();
  void openProject();
  void startEditSurveyArea();
  void startEditFeaturePoly();
  void startEditFeatureLine();
  void saveEdits();
  void stopEdits();
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
  void convertShpFileTo5179();
  void addBasemapVworld();
  void addBasemapVworldSat();
  void addBasemapGoogle();
  void removeSelectedLayers();
  void georefAssistant();
  void showAbout();
  void runLocationSearch();
  void configureVworldKey();
  void goNextStep();
  void rebuildLayouts();
  void showBeginnerGuide();

private:
  void buildUi();
  void buildMenus();
  void setStepTools(int step);
  void updateBeginnerGuide(int step);
  void refreshStepProgress();
  bool isStepComplete(int step) const;
  void loadSurveyLayers(const QString& gpkgOrStub);
  void applyStartupMap();
  void setWorkCrs(const QString& authId);
  void onLocationResults(const QVector<LocationHit>& hits);
  void onLocationFailed(const QString& message);
  void zoomToLocation(const LocationHit& hit);
  QJsonObject buildProjectState() const;
  QString rulesPath() const;
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layerByName(const QString& name) const;
  void beginEdit(QgsVectorLayer* layer);
  void onGeometryCaptured(const QgsGeometry& geom);
  void stopCaptureTool();
#endif

  QListWidget* m_steps = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_guideNow = nullptr;
  QLabel* m_checkView = nullptr;
  QToolBar* m_stepTools = nullptr;
  QLineEdit* m_searchEdit = nullptr;
  class QPushButton* m_nextBtn = nullptr;
  ChecklistEngine* m_checklist = nullptr;
  LocationSearch* m_locator = nullptr;
  QString m_surveyPath;
  QString m_workCrs = QStringLiteral("EPSG:5186");
  int m_stubSurveyArea = 0;
  int m_stubFeatures = 0;
  int m_stubGcp = 0;
  bool m_stubHasMeta = false;
  int m_lastChecklistErrors = -1;
  bool m_exportedOnce = false;
  QStringList m_stepBaseLabels;
#if KA_HGIS_HAS_QGIS
  QgsMapCanvas* m_canvas = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  KaCaptureMapTool* m_captureTool = nullptr;
  QgsMapToolPan* m_panTool = nullptr;
  QgsVectorLayer* m_editLayer = nullptr;
#endif
};
