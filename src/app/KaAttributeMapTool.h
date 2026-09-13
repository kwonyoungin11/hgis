#pragma once
#include <qgsmaptoolidentify.h>
#include <qgsfeature.h>

class QgsVectorLayer;

class KaAttributeMapTool : public QgsMapToolIdentify {
  Q_OBJECT
public:
  explicit KaAttributeMapTool(QgsMapCanvas* canvas);

  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;

  bool pickAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer, QgsFeature* outFeat);

signals:
  void featurePicked(QgsVectorLayer* layer, const QgsFeature& feature);
  void pickCanceled();
};
