#pragma once

#include <QWidget>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class QFrame;
class QButtonGroup;

class KaRegionLocator : public QWidget {
  Q_OBJECT
public:
  explicit KaRegionLocator(QWidget* parent = nullptr);
  QSize sizeHint() const override;

signals:
  void searchRequested(const QString& query);

private:
  void openAddressPopup(const QString& sido);
  void fillDongs();
  void emitSearch();
  void hidePanel();

  QButtonGroup* m_group = nullptr;
  QFrame* m_popup = nullptr;
  QLabel* m_sidoLabel = nullptr;
  QComboBox* m_city = nullptr;
  QComboBox* m_dong = nullptr;
  QLineEdit* m_lot = nullptr;
  QString m_sido;
};
