#pragma once

#include <qgsmaptool.h>
#include <qgsmapmouseevent.h>
#include <qgspointxy.h>
#include <qgsfeatureid.h>

#include <QPointer>
#include <QList>

class QgsMapCanvas;
class QgsVectorLayer;
class QgsRubberBand;

class KaFeatureSelectTool : public QgsMapTool {
  Q_OBJECT
public:
  struct SelectedItem {
    QPointer<QgsVectorLayer> layer;
    QgsFeatureId fid = -1;
  };

  explicit KaFeatureSelectTool(QgsMapCanvas* canvas);
  ~KaFeatureSelectTool() override;

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void activate() override;
  void deactivate() override;

  static QList<SelectedItem> allSelectedFeatures(QgsMapCanvas* canvas);

signals:
  void selectionChanged(int totalSelected);
  void statusMessage(const QString& msg);
  void requestMerge();
  void requestSplit();
  void requestClip();

private:
  void handleContextMenu(QgsMapMouseEvent* e);
  void selectAtPoint(const QgsPointXY& mapPt, bool addToSelection);
  void selectInRect(const QgsRectangle& mapRect, bool addToSelection);

  bool m_dragging = false;
  QPoint m_pressPos;
  QgsPointXY m_pressMapPt;
  QgsRubberBand* m_rubberBand = nullptr;
};
