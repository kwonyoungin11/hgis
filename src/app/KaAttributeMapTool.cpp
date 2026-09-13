#include "KaAttributeMapTool.h"

#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsvectorlayer.h>
#include <qgsmaplayer.h>

#include <QKeyEvent>
#include <QStatusTipEvent>
#include <QApplication>

KaAttributeMapTool::KaAttributeMapTool(QgsMapCanvas* canvas) : QgsMapToolIdentify(canvas) {
  setCursor(Qt::PointingHandCursor);
}

void KaAttributeMapTool::activate() {
  QgsMapToolIdentify::activate();
  setCursor(Qt::PointingHandCursor);
}

void KaAttributeMapTool::keyPressEvent(QKeyEvent* e) {
  if (e && e->key() == Qt::Key_Escape) {
    emit pickCanceled();
    e->accept();
    return;
  }
  QgsMapToolIdentify::keyPressEvent(e);
}

bool KaAttributeMapTool::pickAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer,
                                      QgsFeature* outFeat) {
  if (!mCanvas || !outLayer || !outFeat) return false;
  *outLayer = nullptr;
  *outFeat = QgsFeature();

  const QList<IdentifyResult> results = identify(
      screenPos.x(), screenPos.y(),
      QgsMapToolIdentify::TopDownStopAtFirst,
      QgsMapToolIdentify::VectorLayer);

  if (results.isEmpty()) return false;

  auto* vl = qobject_cast<QgsVectorLayer*>(results.first().mLayer);
  if (!vl || !results.first().mFeature.isValid()) return false;

  *outLayer = vl;
  *outFeat = results.first().mFeature;
  return true;
}

void KaAttributeMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !mCanvas) return;
  if (e->button() != Qt::LeftButton) {
    QgsMapToolIdentify::canvasReleaseEvent(e);
    return;
  }

  QgsVectorLayer* layer = nullptr;
  QgsFeature feat;
  if (!pickAtScreen(e->pos(), &layer, &feat) || !layer) {
    if (mCanvas) {
      mCanvas->setStatusTip(QStringLiteral("이 위치에 도형 없음 — 조사 데이터 레이어 도형을 클릭하세요"));
      QApplication::sendEvent(mCanvas, new QStatusTipEvent(
          QStringLiteral("이 위치에 도형 없음 — 조사 데이터 레이어 도형을 클릭하세요")));
    }
    return;
  }
  emit featurePicked(layer, feat);
}
