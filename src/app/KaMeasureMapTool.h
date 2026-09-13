#pragma once

#include <qgsmaptool.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <QPointer>
#include <QVector>

class QgsRubberBand;
class QgsVertexMarker;
class QgsMapCanvas;
class QFrame;
class QLabel;
class QToolButton;

// Map-canvas tape: distance + area. Rubber-band only — no domain layer.
class KaMeasureMapTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Distance, Area };

  explicit KaMeasureMapTool(QgsMapCanvas* canvas);
  ~KaMeasureMapTool() override;

  void setMode(Mode mode);
  Mode mode() const { return m_mode; }
  void setSnapEnabled(bool on);
  void resetSession();

  Flags flags() const override;
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

signals:
  void statusMessage(const QString& text);

private:
  void ensureHud();
  void placeHud();
  void rebuildRubber(const QgsPointXY* cursorOrNull);
  void destroyGraphics();
  void updateSnapMarker(const QgsPointXY& mapPt, bool snapped);
  bool mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snapped = nullptr);
  void addVertex(const QgsPointXY& pt);
  void undoVertex();
  void finish();
  void showTapeMenu(const QPoint& globalPos);
  void refreshHud(const QgsPointXY* cursorOrNull);
  QString copyText() const;
  QgsCoordinateReferenceSystem measureCrs() const;
  QgsCoordinateTransformContext transformContext() const;

  Mode m_mode = Mode::Distance;
  QgsRubberBand* m_rubber = nullptr;
  QgsVertexMarker* m_snapMark = nullptr;
  QVector<QgsVertexMarker*> m_vertexMarks;
  QVector<QgsPointXY> m_points;
  bool m_snapEnabled = true;
  bool m_finished = false;
  Qt::ContextMenuPolicy m_savedMenuPolicy = Qt::DefaultContextMenu;

  QPointer<QFrame> m_hud;
  QToolButton* m_btnDist = nullptr;
  QToolButton* m_btnArea = nullptr;
  QLabel* m_total = nullptr;
  QLabel* m_detail = nullptr;
  QLabel* m_hint = nullptr;
};
