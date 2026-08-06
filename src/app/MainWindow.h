#pragma once
#include <QMainWindow>
#include <QJsonObject>
class QListWidget;
class QListWidgetItem;
class QLabel;
class QToolBar;
class QEvent;
class ChecklistEngine;
#if KA_HGIS_HAS_QGIS
class QgsMapCanvas;
class QgsLayerTreeView;
class QgsVectorLayer;
class QgsMapTool;
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

private:
  void buildUi();
  void buildMenus();
  void setStepTools(int step);
  void loadSurveyLayers(const QString& gpkgOrStub);
  void applyStartupMap();
  void setWorkCrs(const QString& authId);
  QJsonObject buildProjectState() const;
  QString rulesPath() const;
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layerByName(const QString& name) const;
  void beginEdit(QgsVectorLayer* layer);
#endif

  QListWidget* m_steps = nullptr;
  QLabel* m_help = nullptr;
  QLabel* m_checkView = nullptr;
  QToolBar* m_stepTools = nullptr;
  ChecklistEngine* m_checklist = nullptr;
  QString m_surveyPath;
  QString m_workCrs = QStringLiteral("EPSG:5186");
  int m_stubSurveyArea = 0;
  int m_stubFeatures = 0;
  int m_stubGcp = 0;
  bool m_stubHasMeta = false;
#if KA_HGIS_HAS_QGIS
  QgsMapCanvas* m_canvas = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
#endif
};
