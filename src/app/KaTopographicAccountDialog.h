#pragma once
#include <QDialog>

class QLabel;
class QLineEdit;

class KaTopographicAccountDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaTopographicAccountDialog(QWidget* parent = nullptr);

private:
  QLineEdit* m_username = nullptr;
  QLineEdit* m_password = nullptr;
  QLabel* m_error = nullptr;
};
