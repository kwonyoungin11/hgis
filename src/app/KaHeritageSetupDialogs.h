#pragma once
#include <QDialog>

class QComboBox;

class KaHeritageAccountDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageAccountDialog(QWidget* parent = nullptr);
};

class KaHeritageRegionDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageRegionDialog(const QString& sido = {}, const QString& city = {},
                                 const QString& reason = {}, QWidget* parent = nullptr);
  QString sido() const;
  QString city() const;
private:
  QComboBox* m_sido = nullptr;
  QComboBox* m_city = nullptr;
};
