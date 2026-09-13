#pragma once
#include <QDialog>
#include <QPointer>
class QgsRasterLayer;
class QgsMapCanvas;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class KaDemClassDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaDemClassDialog(QgsRasterLayer* layer, QWidget* parent = nullptr, QgsMapCanvas* canvas = nullptr);
private:
  void applyStyle();
  QPointer<QgsRasterLayer> m_layer;
  QPointer<QgsMapCanvas> m_canvas;
  QComboBox* m_preset = nullptr;
  QCheckBox* m_relief = nullptr;
  QDoubleSpinBox* m_exaggeration = nullptr;
  QSpinBox* m_strength = nullptr;
  QLabel* m_status = nullptr;
};
