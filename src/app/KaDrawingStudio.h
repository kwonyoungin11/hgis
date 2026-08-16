#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QRectF>
#include <QString>

class QAction;
class QCheckBox;
class QDoubleSpinBox;
class QDoubleSpinBox;
class QEvent;
class QFrame;
class QGraphicsRectItem;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QShowEvent;
class QSpinBox;
class QWidget;
class QgsProject;
class QgsMapCanvas;
class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsLayoutViewToolPan;
class QgsPrintLayout;
class QgsLayoutItem;
class QgsLayoutItemMap;
class QgsLayoutItemScaleBar;
class QgsLayerTreeView;
class QgsLayerTreeModel;
class QgsVectorLayer;
class QgsCoordinateReferenceSystem;
class QgsRectangle;
class KaLayoutMapDrawTool;
class KaLayoutMapAdjustTool;

class KaDrawingStudio : public QMainWindow {
  Q_OBJECT
public:
  enum class PlaceKind { MapFrame, Legend, North, ScaleBar, ScaleLabel, CrsLabel };

  explicit KaDrawingStudio(QgsProject* project, QgsMapCanvas* mapCanvas,
                           double paperWidthMm, double paperHeightMm,
                           QWidget* parent = nullptr);
  static bool promptPaper(QWidget* parent, double* widthMm, double* heightMm);
  void resetPaper(double widthMm, double heightMm);
  void refreshMapFromProject();
  bool isMapAdjusting() const { return m_adjustingMap; }
  bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
  void beginActivateMap();
  void endActivateMap();
  void centerSurveyInMap();

private slots:
  void beginDrawMapFrame();
  void useSelectTool();
  void usePanTool();
  void zoomFull();
  void zoomPaperVisible();
  void onRectDrawn(const QRectF& layoutRect);
  void syncMapFromLayers();
  void savePdf();
  void beginPlaceLegend();
  void beginPlaceNorth(const QString& svgRel);
  void beginPlaceScaleBar(const QString& style);
  void beginPlaceScaleLabel();
  void beginPlaceCrsLabel();
  void applyOnScreenScale();
  void applyLegendSettings();
  void onLayoutSelectionChanged(QgsLayoutItem* item);
  void deleteSelectedItems();
  void undoLastChange();
  void syncScaleDecorations();
  void focusGridSettings();

private:
  void buildUi();
  void ensureBlankLayout();
  void attachLayoutToView();
  QgsPrintLayout* layout() const;
  QgsLayoutItemMap* mapItem() const;
  void applyLayersToMap(QgsLayoutItemMap* map, bool includeLiveBasemap, bool refitExtent);
  QgsVectorLayer* blankMapLayer();
  static void ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas);
  void startPlace(PlaceKind kind);
  void createOrResizeMap(const QRectF& layoutRect);
  void placeLegend(const QRectF& layoutRect);
  void placeNorth(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleBar(const QRectF& layoutRect, bool selectAfter = true);
  void placeScaleLabel(const QRectF& layoutRect, bool selectAfter = true);
  void placeCrsLabel(const QRectF& layoutRect);
  void refreshScaleWidgets(bool readFromMap = false);
  void applyScaleBarNow();
  void applyNorthNow();
  void applyCrsLabelNow();
  void relinkDecorations();
  void ensureStandardDecorations();
  void applyStandardChromePositions();
  void snapMapScaleToNice();
  void applyNiceScaleBar(QgsLayoutItemScaleBar* sb);
  QRectF defaultItemRect(const char* id) const;
  QgsRectangle surveyExtentOnMap(QgsLayoutItemMap* map) const;
  void finishPlace();
  void selectPlacedItem();
  void updateInspector(QgsLayoutItem* item);
  void setDrawerCardActive(QFrame* card);
  void syncScaleChips();
  void applyCrsGrid(QgsLayoutItemMap* map);
  void clearGridCoordinateLabels();
  void placeGridCoordinateLabels(QgsLayoutItemMap* map);
  void autoPlaceDefaultSheet();
  QRectF defaultMapRect() const;
  void updatePageOutline();
  void showEvent(QShowEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  static int displayScale(double raw);

  QPointer<QgsProject> m_project;
  QPointer<QgsMapCanvas> m_mapCanvas;
  double m_paperW = 297.0;
  double m_paperH = 210.0;
  QgsLayoutView* m_view = nullptr;
  QgsLayoutViewToolSelect* m_toolSelect = nullptr;
  QgsLayoutViewToolPan* m_toolPan = nullptr;
  KaLayoutMapAdjustTool* m_toolMoveContent = nullptr;
  KaLayoutMapDrawTool* m_toolDrawMap = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  QgsLayerTreeModel* m_layerModel = nullptr;
  bool m_gridEnabled = false;
  bool m_gridShowNums = false;
  double m_gridIntervalM = 20.0;
  QLabel* m_status = nullptr;
  QFrame* m_adjustBar = nullptr;
  QFrame* m_scaleBar = nullptr;
  QAction* m_actEndAdjust = nullptr;
  QLineEdit* m_legendTitle = nullptr;
  QSpinBox* m_legendFont = nullptr;
  QCheckBox* m_legendBold = nullptr;
  QCheckBox* m_legendItalic = nullptr;
  QDoubleSpinBox* m_northSize = nullptr;
  QSpinBox* m_scaleSpin = nullptr;
  QWidget* m_inspector = nullptr;
  QLabel* m_inspectorCap = nullptr;
  QWidget* m_legendProps = nullptr;
  QWidget* m_scaleProps = nullptr;
  QFrame* m_cardLegend = nullptr;
  QFrame* m_cardNorth = nullptr;
  QFrame* m_cardScaleBar = nullptr;
  QFrame* m_cardScale = nullptr;
  PlaceKind m_placeKind = PlaceKind::MapFrame;
  QString m_pendingNorthSvg;
  QString m_pendingScaleBarStyle;
  QStringList m_placeUndo;
  bool m_adjustingMap = false;
  bool m_paperFitPending = true;
  QGraphicsRectItem* m_pageOutline = nullptr;
  QPointer<QgsVectorLayer> m_blankMapLayer;
};
