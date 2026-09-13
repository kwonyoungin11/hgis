#pragma once
#include <QStatusBar>

class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;

// Instrument panel along the bottom of the main window: live cursor position,
// map scale entry, work/upload CRS and the render switch. Subclasses QStatusBar
// so existing showMessage() call sites keep working unchanged.
class KaStatusBar : public QStatusBar {
  Q_OBJECT
public:
  explicit KaStatusBar(QWidget* parent = nullptr);

  QLineEdit* scaleEdit() const { return m_scaleEdit; }
  QComboBox* scaleCombo() const { return m_scaleCombo; }

  void setCoordinate(double x, double y);
  void clearCoordinate();
  void setWorkCrs(const QString& authId);
  void setUploadCrs(const QString& authId);
  void setRenderingEnabled(bool on);
  bool isRenderingEnabled() const;

signals:
  void crsClicked();
  void renderingToggled(bool on);

private:
  void refreshCrsText();

  QLabel* m_xy = nullptr;
  QLineEdit* m_scaleEdit = nullptr;
  QComboBox* m_scaleCombo = nullptr;
  QToolButton* m_crsButton = nullptr;
  QLabel* m_uploadChip = nullptr;
  QToolButton* m_renderButton = nullptr;
  QString m_workCrs;
  QString m_uploadCrs;
};
