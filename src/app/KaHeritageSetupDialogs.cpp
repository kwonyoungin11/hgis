#include "KaHeritageSetupDialogs.h"
#include "core/HeritageIntranetSettings.h"
#include "core/KoreaRegionCatalog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

KaHeritageAccountDialog::KaHeritageAccountDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("heritageAccountDialog"));
  setWindowTitle(QStringLiteral("국가유산 인트라넷 아이디·비밀번호"));
  auto* layout = new QVBoxLayout(this);
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* content = new QWidget(scroll);
  auto* inner = new QVBoxLayout(content);
  auto* description = new QLabel(QStringLiteral(
      "주변유적을 받을 때 사용할 국가유산 공간정보 인트라넷 계정을 이 PC에 저장합니다. "
      "수치지형도 계정과는 별개입니다. 두 항목을 모두 비우고 저장하면 자동로그인 계정을 해제합니다."), content);
  description->setWordWrap(true);
  description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  inner->addWidget(description);
  auto* form = new QFormLayout;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  auto* username = new QLineEdit(content);
  username->setObjectName(QStringLiteral("heritageAccountUsername"));
  auto* password = new QLineEdit(content);
  password->setObjectName(QStringLiteral("heritageAccountPassword"));
  password->setEchoMode(QLineEdit::Password);
  const auto saved = HeritageIntranetSettings::credentials();
  username->setText(saved.username);
  password->setText(saved.password);
  form->addRow(QStringLiteral("아이디"), username);
  form->addRow(QStringLiteral("비밀번호"), password);
  inner->addLayout(form);
  auto* error = new QLabel(content);
  error->setObjectName(QStringLiteral("heritageAccountError"));
  error->setWordWrap(true);
  error->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  error->hide();
  inner->addWidget(error);
  inner->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this, username, password, error, scroll] {
    const HeritageIntranetSettings::Credentials entered{username->text().trimmed(), password->text()};
    QString why;
    if (entered.username.isEmpty() != entered.password.isEmpty())
      why = QStringLiteral("아이디와 비밀번호를 모두 입력하거나 두 항목을 모두 비워 주세요.");
    else if (HeritageIntranetSettings::saveCredentials(entered, &why)) { accept(); return; }
    error->setText(why);
    error->show();
    scroll->ensureWidgetVisible(error);
  });
  QSize initial(480, 300);
  if (auto* currentScreen = screen())
    initial = initial.boundedTo(currentScreen->availableGeometry().size() - QSize(24, 24));
  resize(initial);
}

KaHeritageRegionDialog::KaHeritageRegionDialog(const QString& initialSido, const QString& initialCity,
                                             const QString& reason, QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("heritageRegionDialog"));
  setWindowTitle(QStringLiteral("주변유적 받을 시·군 선택"));
  auto* layout = new QVBoxLayout(this);
  auto* description = new QLabel(reason.isEmpty()
      ? QStringLiteral("조사구역이 속한 시·군을 확인하세요. 자료는 선택한 시·군 단위로만 받습니다.")
      : reason + QStringLiteral("\n조사구역이 속한 시·군을 아래에서 직접 선택하면 계속 받을 수 있습니다."), this);
  description->setWordWrap(true);
  description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  layout->addWidget(description);
  auto* form = new QFormLayout;
  m_sido = new QComboBox(this);
  m_sido->setObjectName(QStringLiteral("heritageSido"));
  m_city = new QComboBox(this);
  m_city->setObjectName(QStringLiteral("heritageCity"));
  m_sido->addItem(QStringLiteral("시·도 선택"), QString());
  for (const auto& name : KoreaRegionCatalog::sidoNames()) m_sido->addItem(name, name);
  form->addRow(QStringLiteral("시·도"), m_sido);
  form->addRow(QStringLiteral("시·군·구"), m_city);
  layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  auto* ok = buttons->button(QDialogButtonBox::Ok);
  ok->setText(QStringLiteral("이 지역 받기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  layout->addWidget(buttons);
  auto updateCities = [this] {
    m_city->clear();
    m_city->addItem(QStringLiteral("시·군·구 선택"), QString());
    for (const auto& name : KoreaRegionCatalog::citiesOf(sido())) m_city->addItem(name, name);
  };
  auto updateEnabled = [this, ok] { ok->setEnabled(!sido().isEmpty() && !city().isEmpty()); };
  connect(m_sido, &QComboBox::currentIndexChanged, this, updateCities);
  connect(m_city, &QComboBox::currentIndexChanged, this, updateEnabled);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    if (!sido().isEmpty() && !city().isEmpty()) accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  updateCities();
  const int province = m_sido->findData(KoreaRegionCatalog::canonicalSido(initialSido));
  if (province > 0) m_sido->setCurrentIndex(province);
  const int municipality = m_city->findData(initialCity);
  if (municipality > 0) m_city->setCurrentIndex(municipality);
  updateEnabled();
  resize(500, 220);
}

QString KaHeritageRegionDialog::sido() const { return m_sido->currentData().toString(); }
QString KaHeritageRegionDialog::city() const { return m_city->currentData().toString(); }
