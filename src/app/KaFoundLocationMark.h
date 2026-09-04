#pragma once

#include <qgsmapcanvasitem.h>
#include <qgspointxy.h>

#include <QString>

// 주소 검색으로 찾은 자리를 지도 위에 핀 + 주소 이름으로 표시한다.
// 화면만 옮기면 어디를 찾았는지 알 수 없다는 현장 지적에서 나왔다.
//
// 조사 도메인이 아니다 — 레이어로 만들지 않고 범례에도 넣지 않는다.
// 캔버스 아이템이라 줌·팬에도 같은 지도 좌표에 붙어 있는다.
class KaFoundLocationMark : public QgsMapCanvasItem {
public:
  explicit KaFoundLocationMark(QgsMapCanvas* canvas);

  void setLocation(const QgsPointXY& mapPoint, const QString& title);
  QString title() const { return m_title; }

  void paint(QPainter* painter) override;
  QRectF boundingRect() const override;
  void updatePosition() override;

private:
  QgsPointXY m_point;
  QString m_title;
};
