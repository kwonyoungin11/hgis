#include "app/KaStatusBar.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QToolButton>

namespace {

QFrame* makeSeparator(QWidget* parent) {
  auto* sep = new QFrame(parent);
  sep->setObjectName(QStringLiteral("statusSep"));
  sep->setFrameShape(QFrame::VLine);
  sep->setFixedWidth(1);
  return sep;
}

QString shortCrs(const QString& authId) {
  QString s = authId.trimmed();
  if (s.startsWith(QLatin1String("EPSG:"), Qt::CaseInsensitive))
    s = s.mid(5);
  return s;
}

}  // namespace

KaStatusBar::KaStatusBar(QWidget* parent) : QStatusBar(parent) {
  setObjectName(QStringLiteral("kaStatusBar"));
  setSizeGripEnabled(false);

  m_xy = new QLabel(this);
  m_xy->setObjectName(QStringLiteral("xyReadout"));
  m_xy->setMinimumWidth(240);
  m_xy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_xy->setToolTip(QStringLiteral("지도 위 커서의 작업 좌표입니다."));
  clearCoordinate();
  addPermanentWidget(m_xy);

  addPermanentWidget(makeSeparator(this));

  auto* scaleLabel = new QLabel(QStringLiteral("축척 1:"), this);
  scaleLabel->setObjectName(QStringLiteral("scaleLabel"));
  addPermanentWidget(scaleLabel);

  m_scaleEdit = new QLineEdit(this);
  m_scaleEdit->setObjectName(QStringLiteral("scaleEdit"));
  m_scaleEdit->setPlaceholderText(QStringLiteral("예: 1000"));
  m_scaleEdit->setFixedWidth(96);
  m_scaleEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_scaleEdit->setToolTip(QStringLiteral("축척 분모를 입력하고 Enter를 누르세요. 1000이면 1:1000입니다."));
  addPermanentWidget(m_scaleEdit);

  m_scaleCombo = new QComboBox(this);
  m_scaleCombo->setObjectName(QStringLiteral("scaleCombo"));
  m_scaleCombo->setEditable(false);
  m_scaleCombo->setToolTip(QStringLiteral("자주 쓰는 축척"));
  const QList<int> presets = {500, 1000, 2000, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
  for (int s : presets)
    m_scaleCombo->addItem(QStringLiteral("1:%1").arg(s), s);
  m_scaleCombo->setCurrentIndex(5);
  addPermanentWidget(m_scaleCombo);

  addPermanentWidget(makeSeparator(this));

  m_crsButton = new QToolButton(this);
  m_crsButton->setObjectName(QStringLiteral("crsButton"));
  m_crsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_crsButton->setCursor(Qt::PointingHandCursor);
  connect(m_crsButton, &QToolButton::clicked, this, &KaStatusBar::crsClicked);
  addPermanentWidget(m_crsButton);

  m_uploadChip = new QLabel(this);
  m_uploadChip->setObjectName(QStringLiteral("uploadCrsChip"));
  addPermanentWidget(m_uploadChip);

  addPermanentWidget(makeSeparator(this));

  m_renderButton = new QToolButton(this);
  m_renderButton->setObjectName(QStringLiteral("renderToggle"));
  m_renderButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_renderButton->setCheckable(true);
  m_renderButton->setChecked(true);
  m_renderButton->setText(QStringLiteral("지도갱신 켜짐"));
  m_renderButton->setCursor(Qt::PointingHandCursor);
  m_renderButton->setToolTip(
      QStringLiteral("지도 다시 그리기를 잠시 멈춥니다. 레이어를 여러 개 켜고 끌 때 편합니다."));
  connect(m_renderButton, &QToolButton::toggled, this, [this](bool on) {
    m_renderButton->setText(on ? QStringLiteral("지도갱신 켜짐") : QStringLiteral("지도갱신 멈춤"));
    emit renderingToggled(on);
  });
  addPermanentWidget(m_renderButton);

  setWorkCrs(QStringLiteral("EPSG:5186"));
  setUploadCrs(QStringLiteral("EPSG:5179"));
}

void KaStatusBar::setCoordinate(double x, double y) {
  const QLocale loc;
  m_xy->setText(QStringLiteral("X %1    Y %2")
                    .arg(loc.toString(x, 'f', 3), loc.toString(y, 'f', 3)));
}

void KaStatusBar::clearCoordinate() {
  m_xy->setText(QStringLiteral("X —    Y —"));
}

void KaStatusBar::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
  refreshCrsText();
}

void KaStatusBar::setUploadCrs(const QString& authId) {
  m_uploadCrs = authId;
  refreshCrsText();
}

void KaStatusBar::refreshCrsText() {
  const QString work = shortCrs(m_workCrs);
  const QString upload = shortCrs(m_uploadCrs);
  m_crsButton->setText(QStringLiteral("작업 %1").arg(work));
  m_crsButton->setToolTip(
      QStringLiteral("지금 그리는 좌표계는 %1입니다. 눌러서 바꿀 수 있습니다.").arg(m_workCrs));
  m_uploadChip->setText(QStringLiteral("→ 제출 %1").arg(upload));
  m_uploadChip->setToolTip(
      QStringLiteral("제출용 파일은 작업 좌표계와 상관없이 항상 %1로 변환되어 나갑니다.")
          .arg(m_uploadCrs));
}

void KaStatusBar::setRenderingEnabled(bool on) {
  if (m_renderButton->isChecked() != on)
    m_renderButton->setChecked(on);
}

bool KaStatusBar::isRenderingEnabled() const {
  return m_renderButton->isChecked();
}
