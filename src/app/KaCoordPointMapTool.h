#pragma once
#include <qgsmaptool.h>
#include <qgspointxy.h>
#include <qgsgeometry.h>
#include <QVector>
#include <QString>

class QgsRubberBand;
class QgsVertexMarker;
class QgsMapMouseEvent;
class QgsMapCanvasItem;

class KaCoordPointMapTool : public QgsMapTool {
  Q_OBJECT
public:
  explicit KaCoordPointMapTool(QgsMapCanvas* canvas);
  ~KaCoordPointMapTool() override;

  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  Flags flags() const override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void activate() override;
  void deactivate() override;
  void clearAll();

  QVector<QgsPointXY> points() const { return m_points; }
  QVector<QString> letters() const { return m_letters; }
  QVector<QString> texts() const { return m_texts; }
  QgsGeometry frameGeometry() const { return m_frameGeom; }

signals:
  void statusMessage(const QString& text);

private:
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, QgsGeometry* hitGeom);
  void addCallout(const QgsPointXY& mapPt, const QgsGeometry& hitGeom);
  void updateFrame(const QgsGeometry& hitGeom);
  void destroyFrame();

  QgsRubberBand* m_frame = nullptr;
  QgsVertexMarker* m_snapMark = nullptr;
  QgsMapCanvasItem* m_labels = nullptr;
  QVector<QgsPointXY> m_points;
  QVector<QString> m_letters;
  QVector<QString> m_texts;
  QgsGeometry m_frameGeom;
  void clearVectorSelections();
};
