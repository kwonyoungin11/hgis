#include "KaDemClassDialog.h"
#include "core/DemPresentation.h"
#include "core/LayerOps.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <qgscoordinatetransform.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

KaDemClassDialog::KaDemClassDialog(QgsRasterLayer* layer, QWidget* parent, QgsMapCanvas* canvas)
    : QDialog(parent), m_layer(layer), m_canvas(canvas) {
  setWindowTitle(QStringLiteral("DEM 표현"));
  setModal(false);
  setMinimumWidth(380);
  auto* form = new QFormLayout(this);
  auto* hint = new QLabel(QStringLiteral("표고에 따라 색이 부드럽게 이어집니다. 색띠의 눈금은 선택한 표현과 함께 바뀝니다."), this);
  hint->setWordWrap(true);
  form->addRow(hint);
  m_preset = new QComboBox(this);
  m_preset->setObjectName(QStringLiteral("demPreset"));
  m_preset->addItem(QStringLiteral("전국 표준 (0–2000m)"), QStringLiteral("national"));
  m_preset->addItem(QStringLiteral("저지대 강조"), QStringLiteral("lowland"));
  m_preset->addItem(QStringLiteral("현재 화면 맞춤"), QStringLiteral("viewport"));
  const int selected = layer ? m_preset->findData(layer->customProperty(QStringLiteral("ka_hgis/dem_preset"), QStringLiteral("national"))) : 0;
  m_preset->setCurrentIndex(qMax(0, selected));
  form->addRow(QStringLiteral("색 표현"), m_preset);
  m_relief = new QCheckBox(QStringLiteral("지형 음영 합성"), this);
  m_relief->setObjectName(QStringLiteral("demReliefEnabled"));
  m_relief->setChecked(!layer || layer->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), true).toBool());
  form->addRow(m_relief);
  m_exaggeration = new QDoubleSpinBox(this);
  m_exaggeration->setObjectName(QStringLiteral("demExaggeration"));
  m_exaggeration->setRange(.1, 5.);
  m_exaggeration->setSingleStep(.1);
  m_exaggeration->setSuffix(QStringLiteral(" 배"));
  m_exaggeration->setValue(layer ? layer->customProperty(QStringLiteral("ka_hgis/dem_z_factor"), 1.).toDouble() : 1.);
  form->addRow(QStringLiteral("수직과장"), m_exaggeration);
  m_strength = new QSpinBox(this);
  m_strength->setObjectName(QStringLiteral("demReliefStrength"));
  m_strength->setRange(0, 80);
  m_strength->setSuffix(QStringLiteral(" %"));
  m_strength->setValue(layer ? qRound(layer->customProperty(QStringLiteral("ka_hgis/dem_relief_strength"), .30).toDouble() * 100.) : 30);
  form->addRow(QStringLiteral("음영 세기"), m_strength);
  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  form->addRow(m_status);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
  buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
  connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &KaDemClassDialog::applyStyle);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  form->addRow(buttons);
  if (layer) connect(layer, &QObject::destroyed, this, &QDialog::reject);
  DemPresentation::followCanvas(layer, canvas);
}

void KaDemClassDialog::applyStyle() {
  if (!m_layer) return;
  const QString preset = m_preset->currentData().toString();
  QgsRectangle extent;
  if (preset == QLatin1String("viewport")) {
    if (!m_canvas) { m_status->setText(QStringLiteral("지도 화면에서 DEM 표현을 다시 열어 주세요.")); return; }
    try {
      const QgsCoordinateTransform transform(m_canvas->mapSettings().destinationCrs(), m_layer->crs(), m_canvas->mapSettings().transformContext());
      extent = transform.transformBoundingBox(m_canvas->extent());
    } catch (const QgsCsException&) {
      m_status->setText(QStringLiteral("화면 좌표를 변환하지 못했습니다. 작업 좌표계를 확인하세요.")); return;
    }
  }
  if (!DemPresentation::apply(m_layer, preset, extent)) {
    m_status->setText(QStringLiteral("이 화면에서 표고값을 읽지 못했습니다. DEM이 있는 곳으로 이동한 뒤 적용하세요.")); return;
  }
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), m_relief->isChecked());
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_z_factor"), m_exaggeration->value());
  m_layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_strength"), m_strength->value() / 100.);
  auto* project = QgsProject::instance();
  auto* shade = LayerOps::ensureDemRelief(project, m_layer);
  if (m_canvas) LayerOps::syncMapCanvas(project, m_canvas, false);
  project->setDirty(true);
  m_status->setText(m_relief->isChecked() && !shade
      ? m_layer->customProperty(QStringLiteral("ka_hgis/dem_relief_error")).toString()
      : QStringLiteral("적용했습니다. 색띠로 표고를 확인하세요. 음영은 지형 방향에 따라 밝기를 바꿉니다."));
}
