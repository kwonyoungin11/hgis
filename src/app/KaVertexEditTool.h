#pragma once

#include <qgsfeatureid.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptool.h>
#include <qgspointxy.h>

#include <QPointer>

class QgsVectorLayer;
class QgsRubberBand;
class QgsVertexMarker;
class QKeyEvent;

// 도형선택에서 꼭짓점을 끌어 도형을 고치는 도구.
//
// QGIS의 QgsVertexTool은 qgis_app 안에 있어 SDK가 내보내지 않는다(qgis_gui에
// 심볼이 없음). Architecture B에서 링크할 수 없으므로 조사 도메인에 필요한
// 최소 기능만 QgsMapTool 위에 직접 만든다:
//   - 도형 클릭 = 선택(꼭짓점이 동그라미로 보임)
//   - 꼭짓점을 끌면 그 점만 이동 → 놓으면 commit. 자석은 snapToMap.
//   - 선 위 우클릭 = 점추가 / 점삭제 메뉴
//   - 변 위를 더블클릭하면 그 자리에 꼭짓점 추가
//   - Delete = 그 꼭짓점 삭제
//     (면은 4점, 선은 2점 미만으로는 줄이지 않는다)
class KaVertexEditTool : public QgsMapTool {
  Q_OBJECT
public:
  explicit KaVertexEditTool(QgsMapCanvas* canvas);
  ~KaVertexEditTool() override;

  void setLayer(QgsVectorLayer* layer);
  QgsVectorLayer* layer() const { return m_layer; }
  void setSnapEnabled(bool on) { m_snapEnabled = on; }

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void canvasDoubleClickEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

signals:
  void statusMessage(const QString& text);

private:
  void selectAt(const QgsPointXY& mapPt);
  void clearSelection();
  void showVertexMarkers();
  void showLineVertexMenu(QgsMapMouseEvent* e);
  QgsPointXY snapMapPoint(QgsMapMouseEvent* e, bool* snapped = nullptr) const;
  // 화면에서 tolPx 안에 있는 꼭짓점 번호. 없으면 -1.
  int vertexNear(const QgsPointXY& mapPt, int tolPx = 20) const;
  // 화면에서 tolPx 안에 있는 변의 뒤쪽 꼭짓점 번호. 없으면 -1.
  int segmentNear(const QgsPointXY& mapPt, QgsPointXY* onLine, int tolPx = 16) const;
  double mapTolerance(int px) const;
  double layerTolerance(int px) const;
  // 도형은 레이어 CRS, 마우스는 지도 CRS. 섞으면 도형이 안 잡힌다.
  QgsPointXY toLayer(const QgsPointXY& mapPt) const;
  QgsPointXY toMap(const QgsPointXY& layerPt) const;
  bool moveVertexTo(int index, const QgsPointXY& to);
  bool deleteVertexAt(int index);
  bool insertVertexAt(int index, const QgsPointXY& at);
  QgsGeometry selectedGeometry() const;
  void refreshRubber(const QgsGeometry& geom);

  QPointer<QgsVectorLayer> m_layer;
  QgsFeatureId m_fid = -1;
  QgsRubberBand* m_outline = nullptr;
  QVector<QgsVertexMarker*> m_marks;
  int m_dragIndex = -1;
  bool m_dragging = false;
  bool m_snapEnabled = true;
  Qt::ContextMenuPolicy m_savedMenuPolicy = Qt::DefaultContextMenu;
};
