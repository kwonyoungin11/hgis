#include "KaImageView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsItemGroup>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QPixmap>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPainter>
#include <cmath>
#include <QPaintEvent>
#include <QRect>
#include <QPolygon>
#include <QPolygonF>

KaAlignLinkOverlay::KaAlignLinkOverlay(QWidget* parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
}

void KaAlignLinkOverlay::setLinks(const QVector<QLine>& done, const QLine& live, bool hasLive) {
  m_done = done;
  m_live = live;
  m_hasLive = hasLive;
  update();
}

void KaAlignLinkOverlay::drawArrow(QPainter& p, const QLine& ln) {
  const QPointF d = QPointF(ln.p2()) - QPointF(ln.p1());
  const double len = std::hypot(d.x(), d.y());
  if (len < 4) return;
  p.drawLine(ln);
  const QPointF u(d.x() / len, d.y() / len);
  const QPointF n(-u.y(), u.x());
  QPolygonF head;
  head << ln.p2() << (ln.p2() - u * 12 + n * 5) << (ln.p2() - u * 12 - n * 5);
  p.drawPolygon(head);
}

void KaAlignLinkOverlay::drawNumber(QPainter& p, const QPoint& at, int n, const QColor& ring) {
  const int r = 8;
  p.setPen(QPen(ring, 2));
  p.setBrush(QColor(255, 255, 255, 240));
  p.drawEllipse(at, r, r);
  p.setPen(QColor(15, 23, 42));
  QFont f = p.font();
  f.setPointSize(8);
  f.setBold(true);
  p.setFont(f);
  p.drawText(QRect(at.x() - r, at.y() - r, r * 2, r * 2), Qt::AlignCenter, QString::number(n));
}

void KaAlignLinkOverlay::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor(15, 118, 110, 240), 2.4));
  p.setBrush(QColor(15, 118, 110, 240));
  for (int i = 0; i < m_done.size(); ++i) {
    drawArrow(p, m_done[i]);
    drawNumber(p, m_done[i].p1(), i + 1, QColor(15, 118, 110));
    drawNumber(p, m_done[i].p2(), i + 1, QColor(15, 118, 110));
  }
  if (m_hasLive) {
    QPen live(QColor(234, 88, 12, 240), 2.6, Qt::DashLine);
    live.setDashPattern({6, 4});
    p.setPen(live);
    p.setBrush(QColor(234, 88, 12, 240));
    drawArrow(p, m_live);
    drawNumber(p, m_live.p1(), m_done.size() + 1, QColor(234, 88, 12));
  }
}

KaImageView::KaImageView(QWidget* parent)
    : QGraphicsView(parent) {
  setScene(new QGraphicsScene(this));
  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
  setRenderHint(QPainter::SmoothPixmapTransform, true);
  setBackgroundBrush(QColor(246, 241, 232));
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
}

bool KaImageView::loadPath(const QString& path) {
  clearMarks();
  scene()->clear();
  m_pix = nullptr;
  m_fitted = false;
  QPixmap pm(path);
  if (pm.isNull()) return false;
  m_pix = scene()->addPixmap(pm);
  m_pix->setZValue(0);
  scene()->setSceneRect(QRectF(pm.rect()).adjusted(-20, -20, 20, 20));
  fitImage();
  return true;
}

void KaImageView::clearMarks() {
  for (auto* m : m_marks) {
    if (m && scene()) scene()->removeItem(m);
    delete m;
  }
  m_marks.clear();
}

void KaImageView::addMarkItem(double pixelX, double pixelY, int number, const QColor& ring) {
  if (!scene()) return;
  auto* g = new QGraphicsItemGroup();
  g->setFlag(QGraphicsItem::ItemIgnoresTransformations);
  const qreal r = 9;
  auto* fill = new QGraphicsEllipseItem(-r, -r, r * 2, r * 2);
  fill->setPen(QPen(ring, 2.4));
  fill->setBrush(QBrush(QColor(255, 255, 255, 240)));
  auto* h = new QGraphicsLineItem(-r - 4, 0, r + 4, 0);
  auto* v = new QGraphicsLineItem(0, -r - 4, 0, r + 4);
  h->setPen(QPen(ring, 2));
  v->setPen(QPen(ring, 2));
  auto* tx = new QGraphicsSimpleTextItem(QString::number(number));
  QFont f = tx->font();
  f.setPointSize(9);
  f.setBold(true);
  tx->setFont(f);
  tx->setBrush(QColor(15, 23, 42));
  const QRectF br = tx->boundingRect();
  tx->setPos(-br.width() * 0.5, -r - br.height() - 1);
  g->addToGroup(fill);
  g->addToGroup(h);
  g->addToGroup(v);
  g->addToGroup(tx);
  g->setPos(pixelX, pixelY);
  g->setZValue(50);
  scene()->addItem(g);
  m_marks.append(g);
}

void KaImageView::setMarks(const QVector<QPointF>& pts, const QPointF* pending) {
  clearMarks();
  for (int i = 0; i < pts.size(); ++i)
    addMarkItem(pts[i].x(), pts[i].y(), i + 1, QColor(220, 38, 38));
  if (pending)
    addMarkItem(pending->x(), pending->y(), pts.size() + 1, QColor(234, 179, 8));
}

QPoint KaImageView::viewPosForPixel(double pixelX, double pixelY) const {
  return mapFromScene(QPointF(pixelX, pixelY));
}

void KaImageView::fitImage() {
  if (!m_pix) return;
  fitInView(m_pix, Qt::KeepAspectRatio);
  m_fitted = true;
}

void KaImageView::wheelEvent(QWheelEvent* e) {
  const double s = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
  scale(s, s);
  m_fitted = false;
}

void KaImageView::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::MiddleButton) {
    m_panning = true;
    m_lastPan = e->pos();
    setCursor(Qt::ClosedHandCursor);
    e->accept();
    return;
  }
  if (e->button() == Qt::LeftButton && m_pix) {
    const QPointF sc = mapToScene(e->pos());
    emit pixelClicked(sc.x(), sc.y());
    e->accept();
    return;
  }
  QGraphicsView::mousePressEvent(e);
}

void KaImageView::mouseMoveEvent(QMouseEvent* e) {
  if (m_panning) {
    const QPoint d = e->pos() - m_lastPan;
    m_lastPan = e->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
    e->accept();
    return;
  }
  QGraphicsView::mouseMoveEvent(e);
}

void KaImageView::mouseReleaseEvent(QMouseEvent* e) {
  if (m_panning && e->button() == Qt::MiddleButton) {
    m_panning = false;
    setCursor(Qt::CrossCursor);
    e->accept();
    return;
  }
  QGraphicsView::mouseReleaseEvent(e);
}

void KaImageView::resizeEvent(QResizeEvent* e) {
  QGraphicsView::resizeEvent(e);
  if (m_fitted) fitImage();
}
