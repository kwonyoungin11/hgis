#include "HeritageSiteLegend.h"

#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayertreenode.h>
#include <qgsproject.h>
#include <qgsrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

#include <memory>

HeritageSiteLegend::HeritageSiteLegend(QgsVectorLayer* layer) : m_layer(layer) {
  if (layer)
    connect(layer, &QgsMapLayer::styleChanged, this, &QgsMapLayerLegend::itemsChanged);
}

bool HeritageSiteLegend::isInstalled(const QgsVectorLayer* layer) {
  return layer && dynamic_cast<const HeritageSiteLegend*>(layer->legend()) != nullptr;
}

bool HeritageSiteLegend::install(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (isInstalled(layer)) return true;
  layer->setLegend(new HeritageSiteLegend(layer));
  return true;
}

bool HeritageSiteLegend::isPanelNode(QgsLayerTreeLayer* node, const QgsVectorLayer* layer) {
  if (!node) return false;
  QgsLayerTreeNode* root = node;
  while (root->parent()) root = root->parent();

  // 레이어가 붙어 있는 프로젝트의 트리와 같으면 레이어창이다.
  const QgsProject* project = layer ? layer->project() : nullptr;
  if (project && project->layerTreeRoot())
    return root == static_cast<QgsLayerTreeNode*>(project->layerTreeRoot());

  // 프로젝트를 모르면 도면 범례로 본다. 유적명이 빠지는 쪽보다 낫다.
  return false;
}

QList<QgsLayerTreeModelLegendNode*> HeritageSiteLegend::createLayerTreeModelLegendNodes(
    QgsLayerTreeLayer* node) {
  if (!node || !m_layer) return {};

  if (!isPanelNode(node, m_layer)) {
    // 도면 범례. 기본 구현에 그대로 넘긴다 — 기본 노드에 붙은 렌더러 rule key 가 있어야
    // setLegendFilterByMapEnabled(true) 의 인쇄범위 걸러내기가 동작한다.
    std::unique_ptr<QgsMapLayerLegend> def(QgsMapLayerLegend::defaultVectorLegend(m_layer));
    if (!def) return {};
    return def->createLayerTreeModelLegendNodes(node);
  }

  // 레이어창. 종류 색 견본 하나만 만들어 레이어 이름 줄에 붙인다.
  QgsFeatureRenderer* renderer = m_layer->renderer();
  if (!renderer) return {};
  const QgsLegendSymbolList items = renderer->legendSymbolItems();
  QgsSymbol* first = nullptr;
  for (const QgsLegendSymbolItem& it : items) {
    if (it.symbol()) {
      first = it.symbol();
      break;
    }
  }
  if (!first) return {};

  // 라벨은 비운다. 레이어 이름이 곧 종류 이름이다.
  QgsLegendSymbolItem swatch(first, QString(), QString());
  auto* symbolNode = new QgsSymbolLegendNode(node, swatch);
  symbolNode->setEmbeddedInParent(true);
  symbolNode->setUserLabel(QString());
  return {symbolNode};
}
