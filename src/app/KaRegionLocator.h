#pragma once

#include <QWidget>
#include <QString>

class QComboBox;
class QLineEdit;
class QLabel;
class QFrame;
class QButtonGroup;
class QAbstractButton;

class KaRegionLocator : public QWidget {
  Q_OBJECT
public:
  explicit KaRegionLocator(QWidget* parent = nullptr);
  QSize sizeHint() const override;

signals:
  void searchRequested(const QString& query);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void openAddressPopup(const QString& sido);
  void fillDongs();
  void emitSearch();
  // 팝업을 닫고 시·도 칩도 함께 해제한다. 팝업 열림 == 칩 눌림이 유일한 규칙.
  void closePanel();

  QButtonGroup* m_group = nullptr;
  QFrame* m_popup = nullptr;
  QLabel* m_sidoLabel = nullptr;
  QComboBox* m_city = nullptr;
  QComboBox* m_dong = nullptr;
  QLineEdit* m_lot = nullptr;
  QString m_sido;
  QAbstractButton* m_activeChip = nullptr;
};
