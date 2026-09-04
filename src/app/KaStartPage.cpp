#include "KaStartPage.h"
#include "KaIcons.h"
#include "core/RecentSurveys.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

static void applyCardElevation(QWidget* w, int blur = 14, int yOffset = 3, int alpha = 35) {
  if (!w) return;
  auto* shadow = new QGraphicsDropShadowEffect(w);
  shadow->setBlurRadius(blur);
  shadow->setOffset(0, yOffset);
  shadow->setColor(QColor(0, 0, 0, alpha));
  w->setGraphicsEffect(shadow);
}

KaStartPage::KaStartPage(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("startPage"));
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(12);

  auto* rail = new QWidget(this);
  rail->setObjectName(QStringLiteral("startRail"));
  rail->setFixedWidth(168);
  rail->setStyleSheet(QStringLiteral("QWidget#startRail { background:#232830; }"));
  auto* railLay = new QVBoxLayout(rail);
  railLay->setContentsMargins(16, 20, 16, 20);
  railLay->setSpacing(10);
  auto* homeLab = new QLabel(QStringLiteral("홈"), rail);
  homeLab->setObjectName(QStringLiteral("startRailHome"));
  homeLab->setStyleSheet(QStringLiteral("font-weight:800;font-size:16px;color:#EFF2F5;"));
  railLay->addWidget(homeLab);
  auto* railHint = new QLabel(QStringLiteral("최근 조사를\n여기서 다시 엽니다."), rail);
  railHint->setWordWrap(true);
  railHint->setStyleSheet(QStringLiteral("color:#A9B1BA;font-size:12px;"));
  railLay->addWidget(railHint);
  railLay->addStretch(1);
  root->addWidget(rail);

  auto* main = new QWidget(this);
  main->setObjectName(QStringLiteral("startMain"));
  auto* mainLay = new QVBoxLayout(main);
  mainLay->setContentsMargins(28, 22, 28, 22);
  mainLay->setSpacing(14);

  auto* title = new QLabel(QStringLiteral("홈"), main);
  title->setStyleSheet(QStringLiteral("font-weight:800;font-size:22px;"));
  mainLay->addWidget(title);

  auto* actions = new QHBoxLayout;
  actions->setSpacing(12);
  auto* btnNew = new QPushButton(KaIcons::icon(QStringLiteral("new")), QStringLiteral("새 조사"),
                                 main);
  btnNew->setObjectName(QStringLiteral("startNewBtn"));
  btnNew->setMinimumSize(168, 80);
  auto* btnOpen = new QPushButton(KaIcons::icon(QStringLiteral("open")),
                                  QStringLiteral("열기"), main);
  btnOpen->setObjectName(QStringLiteral("startOpenBtn"));
  btnOpen->setMinimumSize(168, 80);
  connect(btnNew, &QPushButton::clicked, this, &KaStartPage::newSurveyRequested);
  connect(btnOpen, &QPushButton::clicked, this, &KaStartPage::openRequested);
  applyCardElevation(btnNew, 14, 3, 40);
  applyCardElevation(btnOpen, 14, 3, 40);
  actions->addWidget(btnNew);
  actions->addWidget(btnOpen);
  actions->addStretch(1);
  mainLay->addLayout(actions);

  auto* recCap = new QLabel(QStringLiteral("최근 조사"), main);
  recCap->setStyleSheet(QStringLiteral("font-weight:800;font-size:15px;"));
  mainLay->addWidget(recCap);

  m_empty = new QLabel(QStringLiteral("아직 최근 조사가 없습니다. 새 조사 또는 열기로 시작하세요."),
                       main);
  m_empty->setObjectName(QStringLiteral("recentEmptyHint"));
  m_empty->setWordWrap(true);
  mainLay->addWidget(m_empty);

  m_recent = new QTableWidget(0, 3, main);
  m_recent->setObjectName(QStringLiteral("recentSurveyList"));
  applyCardElevation(m_recent, 16, 4, 30);
  m_recent->setHorizontalHeaderLabels(
      {QStringLiteral("이름"), QStringLiteral("최근 열림"), QStringLiteral("위치")});
  m_recent->verticalHeader()->setVisible(false);
  m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_recent->setSelectionMode(QAbstractItemView::SingleSelection);
  m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_recent->setShowGrid(false);
  m_recent->setAlternatingRowColors(true);
  m_recent->setFocusPolicy(Qt::StrongFocus);
  m_recent->setContextMenuPolicy(Qt::CustomContextMenu);
  m_recent->horizontalHeader()->setStretchLastSection(true);
  m_recent->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_recent->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_recent->setColumnWidth(0, 220);
  connect(m_recent, &QTableWidget::cellActivated, this, [this](int row, int) { openRow(row); });
  connect(m_recent, &QTableWidget::cellClicked, this, [this](int row, int) { openRow(row); });
  connect(m_recent, &QTableWidget::customContextMenuRequested, this, &KaStartPage::showRecentMenu);
  mainLay->addWidget(m_recent, 1);

  root->addWidget(main, 1);
  reload();
}

void KaStartPage::reload() {
  if (!m_recent) return;
  m_recent->setRowCount(0);
  QSettings st = RecentSurveys::userSettings();
  const auto items = RecentSurveys::load(st);
  if (m_empty) m_empty->setVisible(items.isEmpty());
  m_recent->setVisible(!items.isEmpty());
  int row = 0;
  for (const RecentSurveys::Item& it : items) {
    const QString when = it.lastOpenedMs > 0
                             ? QDateTime::fromMSecsSinceEpoch(it.lastOpenedMs)
                                   .toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                             : QString();
    m_recent->insertRow(row);
    auto* name = new QTableWidgetItem(KaIcons::icon(QStringLiteral("open")), it.name);
    name->setData(Qt::UserRole, it.path);
    name->setToolTip(it.path);
    auto* date = new QTableWidgetItem(when);
    date->setToolTip(when);
    auto* loc = new QTableWidgetItem(it.path);
    loc->setToolTip(it.path);
    m_recent->setItem(row, 0, name);
    m_recent->setItem(row, 1, date);
    m_recent->setItem(row, 2, loc);
    m_recent->setRowHeight(row, 36);
    ++row;
  }
}

void KaStartPage::openRow(int row) {
  if (!m_recent || row < 0) return;
  auto* it = m_recent->item(row, 0);
  if (!it) return;
  const QString path = it->data(Qt::UserRole).toString();
  if (path.isEmpty()) return;
  emit recentOpened(path);
}

void KaStartPage::showRecentMenu(const QPoint& pos) {
  if (!m_recent) return;
  auto* it = m_recent->itemAt(pos);
  if (!it) return;
  const int row = it->row();
  auto* name = m_recent->item(row, 0);
  if (!name) return;
  const QString path = name->data(Qt::UserRole).toString();
  if (path.isEmpty()) return;
  QMenu menu(this);
  QAction* forget = menu.addAction(QStringLiteral("목록에서 제거"));
  if (menu.exec(m_recent->viewport()->mapToGlobal(pos)) == forget)
    emit forgetRequested(path);
}
