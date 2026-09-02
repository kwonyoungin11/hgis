#include "KaDemClassDialog.h"

#include <QAbstractItemView>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgssinglebandpseudocolorrenderer.h>

KaDemClassDialog::KaDemClassDialog(QgsRasterLayer* layer, QWidget* parent)
    : QDialog(parent), m_layer(layer) {
  setWindowTitle(QStringLiteral("DEM 높이 구간"));
  setModal(false);
  setMinimumWidth(520);

  auto* root = new QVBoxLayout(this);
  auto* hint = new QLabel(
      QStringLiteral("칸 수·간격·색·이름을 여러 줄 한꺼번에 바꿀 수 있습니다. "
                     "맨 위 칸은 항상 ‘이상’이라 봉우리가 빠지지 않습니다."),
      this);
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color:#6E757D;"));
  root->addWidget(hint);

  auto* top = new QHBoxLayout();
  top->addWidget(new QLabel(QStringLiteral("칸 수"), this));
  m_count = new QSpinBox(this);
  m_count->setRange(2, 12);
  m_count->setValue(6);
  top->addWidget(m_count);
  top->addWidget(new QLabel(QStringLiteral("간격"), this));
  m_step = new QComboBox(this);
  m_step->addItem(QStringLiteral("자동"), 0.0);
  for (double s : {1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 25.0, 50.0, 100.0, 200.0})
    m_step->addItem(QStringLiteral("%1 m").arg(s, 0, 'f', 0), s);
  top->addWidget(m_step);
  auto* rebuild = new QPushButton(QStringLiteral("다시 나누기"), this);
  top->addWidget(rebuild);
  top->addStretch(1);
  root->addLayout(top);

  m_table = new QTableWidget(0, 4, this);
  m_table->setHorizontalHeaderLabels(
      {QStringLiteral("하한(m)"), QStringLiteral("상한(m)"), QStringLiteral("색"),
       QStringLiteral("이름")});
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->verticalHeader()->setVisible(false);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
  root->addWidget(m_table, 1);

  auto* btns = new QHBoxLayout();
  auto* apply = new QPushButton(QStringLiteral("적용"), this);
  apply->setDefault(true);
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  btns->addStretch(1);
  btns->addWidget(apply);
  btns->addWidget(close);
  root->addLayout(btns);

  connect(rebuild, &QPushButton::clicked, this, &KaDemClassDialog::rebuildRows);
  connect(apply, &QPushButton::clicked, this, &KaDemClassDialog::applyClasses);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int col) {
    if (col == 2) pickColor(row);
  });

  QList<LayerOps::DemElevationClass> classes = LayerOps::readDemElevationClasses(m_layer);
  if (classes.size() < 2) rebuildRows();
  else {
    m_count->setValue(classes.size());
    fillTable(classes);
  }
}

void KaDemClassDialog::fillTable(const QList<LayerOps::DemElevationClass>& classes) {
  m_table->setRowCount(classes.size());
  for (int i = 0; i < classes.size(); ++i) {
    const LayerOps::DemElevationClass& c = classes[i];
    auto* lo = new QTableWidgetItem(QString::number(c.lo, 'f', 0));
    auto* hi = new QTableWidgetItem(std::isfinite(c.hi) ? QString::number(c.hi, 'f', 0)
                                                        : QStringLiteral("이상"));
    auto* color = new QTableWidgetItem();
    color->setBackground(c.color);
    color->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    color->setToolTip(QStringLiteral("클릭하면 색을 바꿉니다"));
    auto* lab = new QTableWidgetItem(c.label);
    if (i == classes.size() - 1) {
      hi->setFlags(hi->flags() & ~Qt::ItemIsEditable);
      hi->setToolTip(QStringLiteral("마지막 칸은 열린 구간입니다"));
    }
    m_table->setItem(i, 0, lo);
    m_table->setItem(i, 1, hi);
    m_table->setItem(i, 2, color);
    m_table->setItem(i, 3, lab);
  }
}

QList<LayerOps::DemElevationClass> KaDemClassDialog::classesFromTable() const {
  QList<LayerOps::DemElevationClass> out;
  const int n = m_table->rowCount();
  for (int i = 0; i < n; ++i) {
    LayerOps::DemElevationClass c;
    const QTableWidgetItem* lo = m_table->item(i, 0);
    const QTableWidgetItem* hi = m_table->item(i, 1);
    const QTableWidgetItem* col = m_table->item(i, 2);
    const QTableWidgetItem* lab = m_table->item(i, 3);
    c.lo = lo ? lo->text().toDouble() : 0.0;
    const QString hs = hi ? hi->text().trimmed() : QString();
    c.hi = (i == n - 1 || hs == QStringLiteral("이상"))
               ? std::numeric_limits<double>::infinity()
               : hs.toDouble();
    c.color = col ? col->background().color() : QColor(128, 128, 128);
    c.label = lab ? lab->text() : QString();
    out.append(c);
  }
  return out;
}

void KaDemClassDialog::rebuildRows() {
  if (!m_layer) return;
  double zMin = 0.0;
  double zMax = 200.0;
  if (auto* rend = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(m_layer->renderer())) {
    if (std::isfinite(rend->classificationMin())) zMin = rend->classificationMin();
    if (std::isfinite(rend->classificationMax()) && rend->classificationMax() > zMin)
      zMax = rend->classificationMax();
  }
  const auto classes = LayerOps::buildDemElevationClasses(
      zMin, zMax, m_count->value(), m_step->currentData().toDouble());
  m_count->setValue(classes.size());
  fillTable(classes);
}

void KaDemClassDialog::applyClasses() {
  if (!m_layer) return;
  LayerOps::DemElevationStyle style;
  style.classes = classesFromTable();
  LayerOps::applyDemElevationStyle(m_layer, QgsRectangle(), style);
}

void KaDemClassDialog::pickColor(int row) {
  QTableWidgetItem* it = m_table->item(row, 2);
  if (!it) return;
  const QColor c = QColorDialog::getColor(it->background().color(), this, QStringLiteral("구간 색"));
  if (c.isValid()) it->setBackground(c);
}
