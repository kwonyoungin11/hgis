#include "KaTrenchDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>

KaTrenchDialog::KaTrenchDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("시굴격자 속성"));
  setModal(false);

  auto* form = new QFormLayout(this);

  m_auto = new QCheckBox(QStringLiteral("지금 그린(또는 선택한) 조사구역에 자동 배치"), this);
  form->addRow(m_auto);

  m_size = new QComboBox(this);
  m_size->addItem(QStringLiteral("2 × 20 m"), 20);
  m_size->addItem(QStringLiteral("2 × 10 m"), 10);
  form->addRow(QStringLiteral("규격"), m_size);

  m_balk = new QDoubleSpinBox(this);
  m_balk->setRange(0, 200);
  m_balk->setValue(10.0);
  m_balk->setSuffix(QStringLiteral(" m"));
  m_balk->setToolTip(QStringLiteral("트렌치 사이 간격입니다. 간격을 키우면 시굴 비율이 내려갑니다."));
  form->addRow(QStringLiteral("둑(간격)"), m_balk);

  m_rows = new QSpinBox(this);
  m_rows->setRange(1, 200);
  m_rows->setValue(2);
  form->addRow(QStringLiteral("행"), m_rows);

  m_cols = new QSpinBox(this);
  m_cols->setRange(1, 200);
  m_cols->setValue(2);
  form->addRow(QStringLiteral("열"), m_cols);

  m_az = new QDoubleSpinBox(this);
  m_az->setRange(0, 360);
  m_az->setValue(0);
  m_az->setSingleStep(1.0);
  m_az->setSuffix(QStringLiteral(" °"));
  m_az->setToolTip(QStringLiteral("격자 회전(북 기준 시계 방향). 「적용」을 누르면 회전된 격자로 다시 배치합니다."));
  form->addRow(QStringLiteral("회전(방위)"), m_az);

  m_prefix = new QLineEdit(QStringLiteral("Tr-"), this);
  form->addRow(QStringLiteral("이름 접두"), m_prefix);

  m_ratio = new QLabel(this);
  m_ratio->setWordWrap(true);
  form->addRow(m_ratio);

  auto* hint = new QLabel(
      QStringLiteral("깐 뒤 맵에서 격자를 끌어 옮기세요. 개별 편집은 트렌치 하나 이동·삭제입니다."),
      this);
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color:#6E757D;"));
  form->addRow(hint);

  auto* btnRow = new QHBoxLayout();
  auto* apply = new QPushButton(QStringLiteral("구역에 깔기"), this);
  apply->setDefault(true);
  apply->setToolTip(QStringLiteral("조사구역 위에 격자를 다시 깝니다. 깐 뒤 맵에서 끌어 옮기세요."));
  auto* manual = new QPushButton(QStringLiteral("맵에 찍기"), this);
  manual->setToolTip(QStringLiteral("맵을 한 번 찍어 그 점을 원점으로 놓습니다."));
  auto* editOne = new QPushButton(QStringLiteral("개별 편집"), this);
  editOne->setToolTip(QStringLiteral("그래픽처럼 트렌치를 하나씩 선택·이동·삭제합니다."));
  auto* move = new QPushButton(QStringLiteral("전체 이동"), this);
  move->setToolTip(QStringLiteral("모서리를 찍고 놓을 곳을 찍어 격자 전체를 옮깁니다."));
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  btnRow->addWidget(apply);
  btnRow->addWidget(manual);
  btnRow->addWidget(editOne);
  btnRow->addWidget(move);
  btnRow->addStretch(1);
  btnRow->addWidget(close);
  form->addRow(btnRow);

  connect(apply, &QPushButton::clicked, this, &KaTrenchDialog::applyRequested);
  connect(manual, &QPushButton::clicked, this, &KaTrenchDialog::manualPlaceRequested);
  connect(editOne, &QPushButton::clicked, this, &KaTrenchDialog::editSingleRequested);
  connect(move, &QPushButton::clicked, this, &KaTrenchDialog::moveRequested);
  connect(close, &QPushButton::clicked, this, &QDialog::hide);

  connect(m_size, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { refreshPlan(); });
  connect(m_balk, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { refreshPlan(); });
  connect(m_az, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { refreshPlan(); });
  connect(m_rows, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { refreshPlan(); });
  connect(m_cols, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int) { refreshPlan(); });
  connect(m_auto, &QCheckBox::toggled, this, [this](bool) { refreshPlan(); });
  refreshPlan();
}

void KaTrenchDialog::setArea(const QByteArray& wkb, double areaM2) {
  const bool hadArea = !m_areaWkb.isEmpty();
  m_areaWkb = wkb;
  m_areaM2 = areaM2;
  const bool has = !wkb.isEmpty();
  m_auto->setEnabled(has);
  if (!has)
    m_auto->setChecked(false);
  else if (!hadArea)
    m_auto->setChecked(true);
  m_auto->setToolTip(has
                         ? QStringLiteral(
                               "조사구역이 여러 개면 선택한 곳만, 선택이 없으면 마지막에 그린 곳만 깝니다.")
                         : QStringLiteral("조사구역을 먼저 그리면 그 구역 안에 자동 배치를 쓸 수 있습니다."));
  refreshPlan();
}

TrenchGridGenerator::Spec KaTrenchDialog::spec() const {
  TrenchGridGenerator::Spec sp;
  sp.trenchWidth = 2.0;
  sp.trenchLength = m_size->currentData().toDouble();
  sp.balkWidth = m_balk->value();
  sp.rows = m_rows->value();
  sp.cols = m_cols->value();
  sp.azimuthDeg = m_az->value();
  sp.namePrefix = m_prefix->text().trimmed();
  if (sp.namePrefix.isEmpty())
    sp.namePrefix = QStringLiteral("Tr-");
  return sp;
}

bool KaTrenchDialog::autoFill() const {
  return m_auto->isChecked() && !m_areaWkb.isEmpty();
}

void KaTrenchDialog::refreshPlan() {
  const bool fill = autoFill();
  m_rows->setEnabled(!fill);
  m_cols->setEnabled(!fill);
  if (!fill) {
    const TrenchGridGenerator::Spec sp = spec();
    const double t = sp.rows * sp.cols * sp.trenchWidth * sp.trenchLength;
    m_ratio->setStyleSheet(QString());
    m_ratio->setText(QStringLiteral("수동 배치: %1개 · 총 %2㎡ — 「원점 클릭 배치」로 맵에서 위치를 정하세요.")
                         .arg(sp.rows * sp.cols)
                         .arg(QLocale().toString(t, 'f', 0)));
    return;
  }
  const auto cells = TrenchGridGenerator::buildInArea(spec(), m_areaWkb);
  const double t = TrenchGridGenerator::totalArea(cells);
  const double pct = m_areaM2 > 0.0 ? t / m_areaM2 * 100.0 : 0.0;
  const bool over = pct > 10.0;
  m_ratio->setStyleSheet(over ? QStringLiteral("color:#A33A2E;font-weight:700;")
                              : QStringLiteral("color:#2E7D4F;font-weight:700;"));
  m_ratio->setText(QStringLiteral("트렌치 %1개 · 총 %2㎡ · 시굴 비율 %3% (기준: 시굴 10% · 표본 2%)%4")
                       .arg(cells.size())
                       .arg(QLocale().toString(t, 'f', 0))
                       .arg(QLocale().toString(pct, 'f', 1))
                       .arg(over ? QStringLiteral(" — 간격을 키우세요") : QString()));
}
