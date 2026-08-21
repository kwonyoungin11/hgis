#pragma once

#include <qgsmaptool.h>
#include <qgspointxy.h>
#include <qgsmapmouseevent.h>
#include <qgsfeatureid.h>
#include <QPointer>

class QgsVectorLayer;
class QgsRubberBand;
class QKeyEvent;

// trial_trench 편집 도구. ArcGIS 그래픽 레이어처럼 다룬다:
//  - Single 모드(기본): 트렌치를 클릭해 선택 → 끌어서 하나만 이동,
//    Delete/Backspace 또는 우클릭 = 그 트렌치만 삭제
//  - Whole 모드: 모서리 찍고 놓을 곳 찍기 = 격자 전체 이동
class KaTrenchMoveTool : public QgsMapTool {
  Q_OBJECT
public:
  enum class Mode { Single, Whole };

  explicit KaTrenchMoveTool(QgsMapCanvas* canvas);
  ~KaTrenchMoveTool() override;
  void setLayer(QgsVectorLayer* layer);
  void setSnapMeters(double meters);
  void setGridOverlay(class KaCanvasGridOverlay* grid);
  void setMode(Mode mode);
  Mode mode() const { return m_mode; }

  Flags flags() const override { return Flags(); }
  void canvasPressEvent(QgsMapMouseEvent* e) override;
  void canvasMoveEvent(QgsMapMouseEvent* e) override;
  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;
  void deactivate() override;

signals:
  void statusMessage(const QString& text);

private:
  void rebuildRubber(const QgsPointXY& offset);
  void rebuildSingleRubber(QgsFeatureId fid, const QgsPointXY& offset);
  void clearSingleSelection();
  void applyTranslate(double dx, double dy);
  void applyTranslateOne(QgsFeatureId fid, double dx, double dy);
  QgsFeatureId hitTrench(const QgsPointXY& p, QString* nameOut = nullptr) const;
  void deleteTrench(QgsFeatureId fid, const QString& name);
  QgsPointXY snapPt(const QgsPointXY& p) const;
  QgsPointXY nearestVertex(const QgsPointXY& p) const;

  QPointer<QgsVectorLayer> m_layer;
  QgsRubberBand* m_rubber = nullptr;        // 전체 이동 고스트
  QgsRubberBand* m_singleRubber = nullptr;  // 선택/개별 이동 하이라이트
  Mode m_mode = Mode::Single;
  bool m_dragging = false;
  bool m_awaitDrop = false;
  bool m_dragSingle = false;
  QgsFeatureId m_selFid = -1;
  QString m_selName;
  QgsPointXY m_from;
  double m_snapM = 0.0;
  class KaCanvasGridOverlay* m_grid = nullptr;
};
