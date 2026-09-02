#pragma once

#include <QDialog>
#include <QList>

#include "core/LayerOps.h"

class QComboBox;
class QSpinBox;
class QTableWidget;
class QgsRasterLayer;

// DEM 높이 구간을 여러 줄 한꺼번에 고친다(칸 수·간격·색·라벨).
class KaDemClassDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaDemClassDialog(QgsRasterLayer* layer, QWidget* parent = nullptr);

private:
  void fillTable(const QList<LayerOps::DemElevationClass>& classes);
  QList<LayerOps::DemElevationClass> classesFromTable() const;
  void rebuildRows();
  void applyClasses();
  void pickColor(int row);

  QgsRasterLayer* m_layer = nullptr;
  QSpinBox* m_count = nullptr;
  QComboBox* m_step = nullptr;
  QTableWidget* m_table = nullptr;
};
