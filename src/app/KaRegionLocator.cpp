#include "KaRegionLocator.h"
#include "core/KoreaRegionCatalog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFont>
#include <QSignalBlocker>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

KaRegionLocator::KaRegionLocator(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("regionLocator"));
  setMinimumWidth(360);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setToolTip(QStringLiteral("시·도를 누르면 시·동·번지로 찾습니다"));

  auto* grid = new QGridLayout(this);
  grid->setContentsMargins(6, 4, 6, 4);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(3);
  m_group = new QButtonGroup(this);
  m_group->setExclusive(true);

  const QStringList labels = {QStringLiteral("인천"), QStringLiteral("서울"), QStringLiteral("경기"),
                              QStringLiteral("강원"), QStringLiteral("충북"), QStringLiteral("경북"),
                              QStringLiteral("세종"), QStringLiteral("대전"), QStringLiteral("충남"),
                              QStringLiteral("전북"), QStringLiteral("광주"), QStringLiteral("대구"),
                              QStringLiteral("전남"), QStringLiteral("제주"), QStringLiteral("경남"),
                              QStringLiteral("울산"), QStringLiteral("부산")};
  for (int i = 0; i < labels.size(); ++i) {
    auto* b = new QToolButton(this);
    b->setObjectName(QStringLiteral("regionChip"));
    b->setText(labels.at(i));
    b->setCheckable(true);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setFixedHeight(24);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setCursor(Qt::PointingHandCursor);
    m_group->addButton(b, i);
    grid->addWidget(b, i / 9, i % 9);
    const QString sido = KoreaRegionCatalog::canonicalSido(labels.at(i));
    connect(b, &QToolButton::clicked, this, [this, sido]() { openAddressPopup(sido); });
  }
}

QSize KaRegionLocator::sizeHint() const { return QSize(420, 60); }

void KaRegionLocator::hidePanel() {
  if (m_popup) m_popup->hide();
}

void KaRegionLocator::openAddressPopup(const QString& sido) {
  m_sido = sido;
  QWidget* host = window();
  if (!m_popup) {
    m_popup = new QFrame(host ? host : this);
    m_popup->setObjectName(QStringLiteral("regionAddressPopup"));
    // A transient grab window steals the Windows IME; Tool is a real window so Hangul composes.
    m_popup->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    m_popup->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_popup->setAutoFillBackground(true);
    auto* col = new QVBoxLayout(m_popup);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(8);
    m_sidoLabel = new QLabel(m_popup);
    m_sidoLabel->setObjectName(QStringLiteral("regionSidoTitle"));
    auto* row = new QHBoxLayout();
    row->setSpacing(6);
    const QFont hangul(QStringLiteral("Malgun Gothic"), 10);
    m_city = new QComboBox(m_popup);
    m_city->setMinimumWidth(120);
    m_city->setFont(hangul);
    m_dong = new QComboBox(m_popup);
    m_dong->setObjectName(QStringLiteral("regionDong"));
    m_dong->setEditable(true);
    m_dong->setInsertPolicy(QComboBox::NoInsert);
    m_dong->setCompleter(nullptr);
    m_dong->setMinimumWidth(110);
    m_dong->setFont(hangul);
    m_dong->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_dong->setInputMethodHints(Qt::ImhNone);
    if (QLineEdit* dongEdit = m_dong->lineEdit()) {
      dongEdit->setPlaceholderText(QStringLiteral("동·읍·면"));
      dongEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
      dongEdit->setInputMethodHints(Qt::ImhNone);
      dongEdit->setFont(hangul);
    }
    m_lot = new QLineEdit(m_popup);
    m_lot->setPlaceholderText(QStringLiteral("번지"));
    m_lot->setMaximumWidth(96);
    m_lot->setFont(hangul);
    m_lot->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_lot->setInputMethodHints(Qt::ImhNone);
    auto* go = new QPushButton(QStringLiteral("찾기"), m_popup);
    go->setDefault(true);
    row->addWidget(m_city, 0);
    row->addWidget(m_dong, 1);
    row->addWidget(m_lot, 0);
    row->addWidget(go, 0);
    col->addWidget(m_sidoLabel);
    col->addLayout(row);
    connect(go, &QPushButton::clicked, this, &KaRegionLocator::emitSearch);
    connect(m_lot, &QLineEdit::returnPressed, this, &KaRegionLocator::emitSearch);
    connect(m_dong->lineEdit(), &QLineEdit::returnPressed, this, &KaRegionLocator::emitSearch);
    connect(m_city, &QComboBox::currentIndexChanged, this, &KaRegionLocator::fillDongs);
  } else if (m_popup->parentWidget() != host && host) {
    m_popup->setParent(host);
    m_popup->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  }
  m_sidoLabel->setText(sido);
  const QSignalBlocker block(m_city);
  m_city->clear();
  m_city->addItem(QStringLiteral("시·군·구"));
  for (const QString& c : KoreaRegionCatalog::citiesOf(sido)) m_city->addItem(c);
  fillDongs();
  m_lot->clear();
  m_popup->adjustSize();
  m_popup->move(mapToGlobal(QPoint(0, height())));
  m_popup->show();
  m_popup->raise();
  m_popup->activateWindow();
  m_dong->setFocus();
}

void KaRegionLocator::fillDongs() {
  if (!m_dong || !m_city) return;
  const QString city = m_city->currentIndex() > 0 ? m_city->currentText() : QString();
  m_dong->clear();
  m_dong->addItem(QStringLiteral("동·읍·면"));
  for (const QString& d : KoreaRegionCatalog::dongsOf(m_sido, city)) m_dong->addItem(d);
  if (m_dong->lineEdit()) m_dong->lineEdit()->clear();
}

void KaRegionLocator::emitSearch() {
  const QString city = (m_city && m_city->currentIndex() > 0) ? m_city->currentText() : QString();
  QString dong;
  if (m_dong) {
    dong = m_dong->currentText().trimmed();
    if (dong == QStringLiteral("동·읍·면")) dong.clear();
  }
  const QString q = KoreaRegionCatalog::composeAddress(
      m_sido, city, dong, m_lot ? m_lot->text() : QString());
  if (q.isEmpty()) return;
  hidePanel();
  emit searchRequested(q);
}
