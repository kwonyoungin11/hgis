#include "KaBeginnerRibbon.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

KaBeginnerRibbon::KaBeginnerRibbon(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("beginnerRibbon"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_row = new QHBoxLayout(this);
  m_row->setContentsMargins(4, 2, 4, 2);
  m_row->setSpacing(2);
}

QFrame* KaBeginnerRibbon::addGroup(const QString& id, const QString& caption) {
  if (m_groups.contains(id))
    return m_groups.value(id);
  auto* fr = new QFrame(this);
  fr->setObjectName(QStringLiteral("ribbonGroup"));
  auto* vl = new QVBoxLayout(fr);
  vl->setContentsMargins(4, 2, 4, 2);
  vl->setSpacing(2);
  auto* cap = new QLabel(caption, fr);
  cap->setObjectName(QStringLiteral("ribbonGroupCaption"));
  cap->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  cap->setWordWrap(false);
  auto* btns = new QHBoxLayout();
  btns->setContentsMargins(0, 0, 0, 0);
  btns->setSpacing(3);
  vl->addWidget(cap);
  vl->addLayout(btns, 1);
  m_row->addWidget(fr, 0);
  m_groups.insert(id, fr);
  m_btnRows.insert(id, btns);
  return fr;
}

QHBoxLayout* KaBeginnerRibbon::buttonRow(const QString& groupId) const {
  return m_btnRows.value(groupId, nullptr);
}

QToolButton* KaBeginnerRibbon::addAction(const QString& groupId, QAction* action) {
  QHBoxLayout* row = buttonRow(groupId);
  if (!row || !action)
    return nullptr;
  auto* b = new QToolButton(m_groups.value(groupId));
  if (action)
    action->setText(twoLine(action->text()));
  b->setDefaultAction(action);
  applyTwoLine(b);
  row->addWidget(b);
  return b;
}

void KaBeginnerRibbon::addWidget(const QString& groupId, QWidget* widget) {
  QHBoxLayout* row = buttonRow(groupId);
  if (!row || !widget)
    return;
  if (auto* b = qobject_cast<QToolButton*>(widget)) {
    b->setText(twoLine(b->text()));
    applyTwoLine(b);
  }
  row->addWidget(widget);
}

QString KaBeginnerRibbon::twoLine(const QString& text) {
  const QString t = text.trimmed();
  if (t.isEmpty() || t.contains(QLatin1Char('\n')))
    return t;
  if (t.contains(QLatin1Char('('))) {
    const int paren = t.indexOf(QLatin1Char('('));
    if (paren > 0)
      return t.left(paren).trimmed() + QLatin1Char('\n') + t.mid(paren).trimmed();
  }
  if (t.size() <= 4)
    return t;
  int cut = t.lastIndexOf(QChar::Space, t.size() / 2 + 2);
  if (cut < 1)
    cut = t.indexOf(QChar::Space);
  if (cut < 1)
    cut = t.size() / 2;
  const QString a = t.left(cut).trimmed();
  const QString b = t.mid(cut).trimmed();
  if (a.isEmpty() || b.isEmpty())
    return t;
  return a + QLatin1Char('\n') + b;
}

void KaBeginnerRibbon::applyTwoLine(QToolButton* button) {
  if (!button)
    return;
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setAutoRaise(false);
  button->setFocusPolicy(Qt::TabFocus);
  button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  button->setFixedSize(64, 58);
}

QFrame* KaBeginnerRibbon::group(const QString& id) const {
  return m_groups.value(id, nullptr);
}
