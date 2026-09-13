#pragma once

#include <QPointer>
#include <qgsmaplayerlegend.h>

class QgsRasterLayer;

// One legend implementation is shared by the layer tree and layout legends.
// The raster layer owns this object; nodes own independent shader snapshots.
class DemColorRampLegend final : public QgsMapLayerLegend {
public:
  static bool install(QgsRasterLayer* layer);
  QList<QgsLayerTreeModelLegendNode*> createLayerTreeModelLegendNodes(QgsLayerTreeLayer* node) override;

private:
  explicit DemColorRampLegend(QgsRasterLayer* layer);
  QPointer<QgsRasterLayer> m_layer;
};
