#pragma once

#include <QList>
#include <QPointer>
#include <qgsmaplayerlegend.h>

class QgsVectorLayer;
class QgsLayerTreeLayer;
class QgsLayerTreeModelLegendNode;

// 주변유적 참조 레이어의 범례.
//
// 레이어창에는 종류 이름 한 줄만 나오고(색 견본은 그 줄에 붙는다),
// 도면 범례에는 유적명이 한 줄씩 나온다.
//
// QGIS는 레이어 하나에 범례 구현이 하나다 — 레이어창과 도면 범례가 같은 QgsMapLayerLegend 를
// 쓴다(DemColorRampLegend 주석 참고). 그래서 여기서 갈라야 한다.
// 가르는 근거: LayoutService::tuneSheetLegend() 가 도면 범례를 Manual 동기화로 돌리고
// 레이어 트리를 복제한다. 그래서 도면 범례 노드의 최상위 루트는 프로젝트 트리가 아니다.
class HeritageSiteLegend final : public QgsMapLayerLegend {
  Q_OBJECT
public:
  static bool install(QgsVectorLayer* layer);
  static bool isInstalled(const QgsVectorLayer* layer);

  // 이 노드가 레이어창(프로젝트 레이어 트리)에 속하는가.
  static bool isPanelNode(QgsLayerTreeLayer* node, const QgsVectorLayer* layer);

  QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes(
      QgsLayerTreeLayer* node) override;

private:
  explicit HeritageSiteLegend(QgsVectorLayer* layer);
  QPointer<QgsVectorLayer> m_layer;
};
