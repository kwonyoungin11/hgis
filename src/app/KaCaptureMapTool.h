#pragma once
#include <qgsmaptool.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <qgsfeatureid.h>
#include <QVector>
#include <QPointer>

class QgsRubberBand;
class QgsVertexMarker;
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
  void setSnapEnabled(bool on);
  void setEasyDraw(bool on);
  bool easyDraw() const { return m_easyDraw; }
  void resetSession();
  bool undoLastVertex();
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
  void vertexMoved();

private:
  void finish();
  void cancel();
  void rebuildRubber(const QgsPointXY* cursorOrNull);
  void destroyRubber();
  void updateSnapMarker(const QgsPointXY& mapPt, bool snapped);
  void destroySnapMarker();
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snapped = nullptr);
  bool nearPoint(const QgsPointXY& a, const QgsPointXY& b) const;
  int indexOfSketchVertex(const QgsPointXY& pt) const;
  bool hitSavedVertex(const QgsPointXY& mapPt, QgsFeatureId* fid, int* vertex) const;
  void previewMovedVertex(const QgsPointXY& mapPt);
  void finishVertexDrag(const QgsPointXY& mapPt);
  void cancelVertexDrag();

  Mode m_mode = Mode::Polygon;
  QPointer<QgsVectorLayer> m_layer;
  QgsRubberBand* m_rubber = nullptr;
  QgsVertexMarker* m_snapMark = nullptr;
  QVector<QgsPointXY> m_points;
  bool m_finishing = false;
  bool m_snapEnabled = true;
  bool m_easyDraw = false;
  bool m_draggingVertex = false;
  QgsFeatureId m_dragFid = -1;
  int m_dragVertex = -1;
  Qt::ContextMenuPolicy m_savedMenuPolicy = Qt::DefaultContextMenu;
};
