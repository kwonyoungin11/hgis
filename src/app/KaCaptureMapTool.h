#pragma once
#include <qgsmaptool.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <QVector>
#include <QPointer>

class QgsRubberBand;
class QgsVectorLayer;
class QgsMapCanvas;

class KaCaptureMapTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Polygon, Line, Point };

  explicit KaCaptureMapTool(QgsMapCanvas* canvas);
  ~KaCaptureMapTool() override;

  void setMode(Mode mode);
  void setTargetLayer(QgsVectorLayer* layer);
  void resetSession();
  Mode mode() const { return m_mode; }
  int pointCount() const { return m_points.size(); }

  Flags flags() const override;
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

signals:
  void geometryCaptured(const QgsGeometry& geom);
  void captureCanceled();

private:
  void finish();
  void cancel();
  void rebuildRubber(const QgsPointXY* cursorOrNull);
  void destroyRubber();
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out);

  Mode m_mode = Mode::Polygon;
  QPointer<QgsVectorLayer> m_layer;
  QgsRubberBand* m_rubber = nullptr;
  QVector<QgsPointXY> m_points;
  bool m_finishing = false;
};
