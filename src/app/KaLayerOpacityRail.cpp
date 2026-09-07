#include "KaLayerOpacityRail.h"

#include <QEvent>
#include <QLabel>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

KaLayerOpacityRail::KaLayerOpacityRail(QWidget* host) : QFrame(host), m_host(host) {
  setObjectName(QStringLiteral("layerOpacityRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::NoFocus);
  setFixedWidth(44);

  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(4, 6, 4, 6);
  lay->setSpacing(4);

  m_title = new QLabel(QStringLiteral("투\n명\n도"), this);
  m_title->setObjectName(QStringLiteral("layerOpacityTitle"));
  m_title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  m_title->setWordWrap(false);

  m_slider = new QSlider(Qt::Vertical, this);
  m_slider->setObjectName(QStringLiteral("layerOpacitySlider"));
  m_slider->setRange(0, 100);
  m_slider->setValue(100);
  m_slider->setEnabled(false);
  m_slider->setInvertedAppearance(false);
  m_slider->setTickPosition(QSlider::NoTicks);
  m_slider->setToolTip(QStringLiteral("선택한 배경지도의 투명도. 위로 올리면 진해집니다."));
  m_slider->setFocusPolicy(Qt::ClickFocus);

  m_value = new QLabel(QStringLiteral("-"), this);
  m_value->setObjectName(QStringLiteral("layerOpacityValue"));
  m_value->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  m_value->setMinimumWidth(32);

  lay->addWidget(m_title, 0, Qt::AlignHCenter);
  lay->addWidget(m_slider, 1, Qt::AlignHCenter);
  lay->addWidget(m_value, 0, Qt::AlignHCenter);

  connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
    if (m_value && m_slider && m_slider->isEnabled())
      m_value->setText(QStringLiteral("%1%").arg(value));
    emit percentChanged(value);
  });

  if (m_host) {
    m_host->installEventFilter(this);
    reposition();
    raise();
  }
}

void KaLayerOpacityRail::setPercent(int percent, bool enabled) {
  const int val = qBound(0, percent, 100);
  if (m_slider) {
    m_slider->blockSignals(true);
    m_slider->setEnabled(enabled);
    m_slider->setValue(enabled ? val : 100);
    m_slider->blockSignals(false);
  }
  if (m_value) {
    m_value->setEnabled(enabled);
    m_value->setText(enabled ? QStringLiteral("%1%").arg(val) : QStringLiteral("-"));
  }
  if (m_title)
    m_title->setEnabled(enabled);
}

int KaLayerOpacityRail::percent() const {
  return m_slider ? m_slider->value() : 100;
}

bool KaLayerOpacityRail::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_host && event &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
       event->type() == QEvent::LayoutRequest)) {
    reposition();
  }
  return QFrame::eventFilter(watched, event);
}

void KaLayerOpacityRail::showEvent(QShowEvent* event) {
  QFrame::showEvent(event);
  reposition();
}

void KaLayerOpacityRail::reposition() {
  if (!m_host)
    return;
  const int w = 44;
  const int h = qBound(168, m_host->height() - 20, 240);
  move(6, 8);
  resize(w, h);
  raise();
}
