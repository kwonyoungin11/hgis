#pragma once
#include <QWidget>

class QLabel;
class QTableWidget;

class KaStartPage : public QWidget {
  Q_OBJECT
public:
  explicit KaStartPage(QWidget* parent = nullptr);
  void reload();

signals:
  void newSurveyRequested();
  void openRequested();
  void recentOpened(const QString& path);
  void forgetRequested(const QString& path);

private:
  void openRow(int row);
  void showRecentMenu(const QPoint& pos);
  QLabel* m_empty = nullptr;
  QTableWidget* m_recent = nullptr;
};
