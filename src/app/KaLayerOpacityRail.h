#pragma once

#include <QFrame>
#include <QPointer>

class QLabel;
class QSlider;
class QShowEvent;

// 맵·도면만들기 캔버스 안쪽 왼쪽 위에 붙는 세로 투명도 막대.
// 레이어 카드 밖, 레이어 창 바로 오른쪽(캔버스 좌상)에 둔다.
class KaLayerOpacityRail : public QFrame {
  Q_OBJECT
public:
  explicit KaLayerOpacityRail(QWidget* host);

  QSlider* slider() const { return m_slider; }
  void setPercent(int percent, bool enabled);
  int percent() const;

signals:
  void percentChanged(int percent);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void reposition();

  QPointer<QWidget> m_host;
  QSlider* m_slider = nullptr;
  QLabel* m_title = nullptr;
  QLabel* m_value = nullptr;
};
