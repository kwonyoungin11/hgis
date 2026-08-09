#pragma once
#include <qgsmaptool.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <QVector>
#include <QPointer>

class QgsRubberBand;
class QgsVectorLayer;

class KaCaptureMapTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Polygon, Line, Point };

  explicit KaCaptureMapTool(QgsMapCanvas* canvas);
  ~KaCaptureMapTool() override;

  void setMode(Mode mode);
  Mode mode() const { return m_mode; }
  void setTargetLayer(QgsVectorLayer* layer);
  void resetSession();
  int pointCount() const { return m_points.size(); }

  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;
  Flags flags() const override;

signals:
  void geometryCaptured(const QgsGeometry& geom);
  void captureCanceled();
  void vertexAdded(int count);

private:
  void addVertex(const QgsPointXY& mapPt);
  void finish();
  void cancel();
  void rebuildRubber(const QgsPointXY* cursorOrNull);
  void destroyRubber();
  QgsPointXY eventMapPoint(QgsMapMouseEvent* e);

  Mode m_mode = Mode::Polygon;
  QPointer<QgsVectorLayer> m_layer;
  QgsRubberBand* m_rubber = nullptr;
  QVector<QgsPointXY> m_points;
  bool m_finishing = false;
};
