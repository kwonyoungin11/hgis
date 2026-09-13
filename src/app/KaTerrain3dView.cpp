#include "KaTerrain3dView.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

KaTerrain3dView::KaTerrain3dView(QWidget* parent) : QWidget(parent) {
  setMinimumSize(320, 240);
  setAutoFillBackground(false);
  setMouseTracking(false);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setCursor(Qt::OpenHandCursor);
}

void KaTerrain3dView::setMesh(const Terrain3dService::Mesh& mesh, const QImage& texture) {
  m_mesh = mesh;
  m_texture = texture;
  const double span = std::max(mesh.groundWidthM, mesh.groundHeightM);
  const double rise = std::max(20.0, static_cast<double>(mesh.zMax));
  m_dist = static_cast<float>(std::max(40.0, std::hypot(span, rise) * 1.25));
  update();
  emit viewChanged();
}

void KaTerrain3dView::setTexture(const QImage& texture) {
  m_texture = texture;
  update();
  emit viewChanged();
}

void KaTerrain3dView::setDistance(float distance) {
  m_dist = std::clamp(distance, 20.0f, 20000.0f);
  update();
  emit viewChanged();
}

QImage KaTerrain3dView::renderView(int w, int h) const {
  return Terrain3dService::renderPerspective(m_mesh, m_texture, w, h, m_yaw, m_pitch, m_dist);
}

void KaTerrain3dView::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const int rw = m_dragging ? std::max(160, width() / 2) : width();
  const int rh = m_dragging ? std::max(120, height() / 2) : height();
  const QImage img = renderView(rw, rh);
  p.drawImage(rect(), img);
}

void KaTerrain3dView::mousePressEvent(QMouseEvent* event) {
  if (!event) return;
  m_last = event->pos();
  m_dragging = true;
  setCursor(Qt::ClosedHandCursor);
  setFocus(Qt::MouseFocusReason);
  event->accept();
}

void KaTerrain3dView::mouseMoveEvent(QMouseEvent* event) {
  if (!event || !(event->buttons() & (Qt::LeftButton | Qt::RightButton | Qt::MiddleButton)))
    return;
  const QPoint d = event->pos() - m_last;
  m_last = event->pos();
  m_yaw += d.x() * 0.4f;
  m_pitch = std::clamp(m_pitch + d.y() * 0.3f, 8.0f, 80.0f);
  event->accept();
  update();
  emit viewChanged();
}

void KaTerrain3dView::mouseReleaseEvent(QMouseEvent* event) {
  m_dragging = false;
  setCursor(Qt::OpenHandCursor);
  if (event)
    event->accept();
  update();
}

void KaTerrain3dView::wheelEvent(QWheelEvent* event) {
  if (!event) return;
  const float step = event->angleDelta().y() > 0 ? 0.9f : 1.1f;
  m_dist = std::clamp(m_dist * step, 20.0f, 20000.0f);
  event->accept();
  update();
  emit viewChanged();
}
