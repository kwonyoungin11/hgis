#include "KaAboveLabelsOverlay.h"

#include <QPainter>

#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaprenderercustompainterjob.h>
#include <qgsmapsettings.h>

KaAboveLabelsOverlay::KaAboveLabelsOverlay(QgsMapCanvas* canvas) : QgsMapCanvasItem(canvas) {
  // 좌표격자(1000)보다 아래, 지도보다는 위. 격자와 눈금은 계속 맨 위에 남는다.
  setZValue(900);
  setVisible(false);
  // 지도가 실제로 다시 그려졌을 때만 덧그림도 다시 만든다.
  if (canvas) {
    QObject::connect(canvas, &QgsMapCanvas::renderComplete, canvas, [this](QPainter*) {
      m_dirty = true;
    });
  }
}

void KaAboveLabelsOverlay::setLayers(const QList<QgsMapLayer*>& layers) {
  m_layers.clear();
  for (QgsMapLayer* l : layers) {
    if (l) m_layers.append(QPointer<QgsMapLayer>(l));
  }
  setVisible(!m_layers.isEmpty());
  m_dirty = true;
  update();
}

void KaAboveLabelsOverlay::updatePosition() {
  if (!mMapCanvas) return;
  setRect(mMapCanvas->extent());
}

void KaAboveLabelsOverlay::paint(QPainter* painter) {
  if (!painter || !mMapCanvas || m_layers.isEmpty()) return;
  // 렌더 안에서 다시 이 항목이 그려지는 일이 없도록 막는다.
  if (m_painting) return;

  QList<QgsMapLayer*> live;
  for (const QPointer<QgsMapLayer>& p : m_layers) {
    if (p) live.append(p.data());
  }
  if (live.isEmpty()) return;

  const QSize logical = mMapCanvas->size();
  if (logical.width() < 2 || logical.height() < 2) return;
  const QgsRectangle ext = mMapCanvas->extent();

  // 범위·크기가 그대로고 지도도 다시 그려지지 않았으면 담아 둔 그림을 쓴다.
  // 이걸 안 하면 마우스가 움직일 때마다 지도를 통째로 다시 그려 화면이 멎는다.
  if (m_dirty || m_cache.isNull() || m_cacheSize != logical || m_cacheExtent != ext) {
    m_cacheSize = logical;
    m_cacheExtent = ext;
    rebuildCache();
    m_dirty = false;
  }
  if (!m_cache.isNull())
    painter->drawImage(0, 0, m_cache);
}

void KaAboveLabelsOverlay::rebuildCache() {
  m_cache = QImage();
  if (!mMapCanvas || m_layers.isEmpty()) return;
  QList<QgsMapLayer*> live;
  for (const QPointer<QgsMapLayer>& p : m_layers) {
    if (p) live.append(p.data());
  }
  if (live.isEmpty()) return;

  QgsMapSettings ms = mMapCanvas->mapSettings();
  ms.setLayers(live);
  ms.setBackgroundColor(Qt::transparent);
  // 이 패스는 도형만 그린다. 글자는 본 렌더가 이미 얹었다.
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, false);
  ms.setOutputSize(m_cacheSize);
  ms.setDevicePixelRatio(1.0);

  QImage img(m_cacheSize, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  m_painting = true;
  // 페인트 안에서는 start()+waitForFinished() 로 배경 스레드를 기다리면 안 된다.
  // renderSynchronously() 가 이 자리(주 스레드)에서 그리라고 만들어진 API 다.
  QgsMapRendererCustomPainterJob job(ms, &p);
  job.renderSynchronously();
  m_painting = false;
  p.end();
  m_cache = img;
}
