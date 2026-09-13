#pragma once
#include <QMainWindow>
#include <QPointer>

class QComboBox;
class QLabel;
class QgsProject;
class QgsMapCanvas;
class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsLayoutViewToolPan;
class QgsLayoutViewToolZoom;
class QgsPrintLayout;
class QgsLayout;

class KaLayoutWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit KaLayoutWindow(QgsProject* project, QgsMapCanvas* mapCanvas, QWidget* parent = nullptr);
  void openLayoutByName(const QString& layoutName);
  void refreshLayoutList();

private slots:
  void onLayoutPicked(int index);
  void useSelectTool();
  void usePanTool();
  void useZoomTool();
  void zoomFull();
  void exportPdf();
  void refreshFromProject();

private:
  void buildUi();
  void setActiveLayout(QgsLayout* layout);
  static void ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas);

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_mapCanvas;
  QgsLayoutView* m_view = nullptr;
  QComboBox* m_layoutCombo = nullptr;
  QLabel* m_status = nullptr;
  QgsLayoutViewToolSelect* m_toolSelect = nullptr;
  QgsLayoutViewToolPan* m_toolPan = nullptr;
  QgsLayoutViewToolZoom* m_toolZoom = nullptr;
};
