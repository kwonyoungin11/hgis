#pragma once

#include <QHash>
#include <QWidget>

class QAction;
class QFrame;
class QHBoxLayout;
class QToolButton;

// 초보자용 위 리본. 기존 QAction/QToolButton을 단계 그룹에만 옮긴다.
class KaBeginnerRibbon : public QWidget {
  Q_OBJECT
public:
  explicit KaBeginnerRibbon(QWidget* parent = nullptr);

  QFrame* addGroup(const QString& id, const QString& caption);
  QToolButton* addAction(const QString& groupId, QAction* action);
  void addWidget(const QString& groupId, QWidget* widget);
  QFrame* group(const QString& id) const;
  static QString twoLine(const QString& text);
  static void applyTwoLine(QToolButton* button);

private:
  QHBoxLayout* buttonRow(const QString& groupId) const;

  QHBoxLayout* m_row = nullptr;
  QHash<QString, QFrame*> m_groups;
  QHash<QString, QHBoxLayout*> m_btnRows;
};
