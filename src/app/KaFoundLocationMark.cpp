#include "KaFoundLocationMark.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <qgsmapcanvas.h>

namespace {
constexpr int kPinHeight = 34;   // 핀 꼭짓점에서 머리까지
constexpr int kPinRadius = 11;   // 머리 반지름
constexpr int kLabelGap = 6;
const QColor kInk(214, 61, 43);
}  // namespace

KaFoundLocationMark::KaFoundLocationMark(QgsMapCanvas* canvas) : QgsMapCanvasItem(canvas) {
  setZValue(200);  // 배경지도·조사 도형 위
}

void KaFoundLocationMark::setLocation(const QgsPointXY& mapPoint, const QString& title) {
  m_point = mapPoint;
  m_title = title;
  updatePosition();
  update();
}

void KaFoundLocationMark::updatePosition() {
  // 아이템 원점을 찾은 지도 좌표에 붙인다. 줌·팬해도 같은 자리에 남는다.
  prepareGeometryChange();
  setPos(toCanvasCoordinates(m_point));
  update();
}

QRectF KaFoundLocationMark::boundingRect() const {
  QFont f;
  f.setFamily(QStringLiteral("Malgun Gothic"));
  f.setPointSize(9);
  f.setBold(true);
  const QFontMetrics fm(f);
  const int textW = m_title.isEmpty() ? 0 : fm.horizontalAdvance(m_title) + 14;
  const int textH = m_title.isEmpty() ? 0 : fm.height() + 8;
  // 원점(0,0)이 핀 끝. 위로 핀, 그 위 오른쪽으로 라벨.
  const qreal left = -kPinRadius - 2;
  const qreal top = -kPinHeight - kPinRadius - textH - kLabelGap - 2;
  const qreal width = std::max<qreal>(kPinRadius * 2 + 4, textW + kPinRadius + 8);
  const qreal height = kPinHeight + kPinRadius + textH + kLabelGap + 6;
  return QRectF(left, top, width, height);
}

void KaFoundLocationMark::paint(QPainter* painter) {
  if (!painter) return;
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  // 핀: 아래 꼭짓점이 정확한 좌표.
  QPainterPath pin;
  pin.moveTo(0, 0);
  pin.lineTo(-kPinRadius * 0.62, -kPinHeight + kPinRadius * 0.6);
  pin.lineTo(kPinRadius * 0.62, -kPinHeight + kPinRadius * 0.6);
  pin.closeSubpath();
  painter->setPen(Qt::NoPen);
  painter->setBrush(kInk);
  painter->drawPath(pin);

  const QPointF head(0, -kPinHeight);
  painter->setBrush(kInk);
  painter->drawEllipse(head, kPinRadius, kPinRadius);
  painter->setBrush(QColor(255, 255, 255));
  painter->drawEllipse(head, kPinRadius * 0.42, kPinRadius * 0.42);

  if (m_title.isEmpty()) {
    painter->restore();
    return;
  }

  QFont f;
  f.setFamily(QStringLiteral("Malgun Gothic"));
  f.setPointSize(9);
  f.setBold(true);
  painter->setFont(f);
  const QFontMetrics fm(f);
  const int textW = fm.horizontalAdvance(m_title);
  const int textH = fm.height();
  const QRectF box(-kPinRadius, -kPinHeight - kPinRadius - kLabelGap - textH - 8, textW + 14,
                   textH + 8);

  painter->setPen(QPen(kInk, 1.5));
  painter->setBrush(QColor(255, 255, 255, 235));
  painter->drawRoundedRect(box, 5, 5);
  painter->setPen(QColor(31, 35, 40));
  painter->drawText(box, Qt::AlignCenter, m_title);
  painter->restore();
}
