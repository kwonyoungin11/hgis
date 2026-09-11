#include "KaTopographicAccountDialog.h"
#include "core/TopographicSettings.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

KaTopographicAccountDialog::KaTopographicAccountDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("topographicAccountDialog"));
  setWindowTitle(QStringLiteral("수치지형도 아이디·비밀번호"));
  auto* layout = new QVBoxLayout(this);
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("topographicAccountScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* content = new QWidget(scroll);
  auto* contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  auto* description = new QLabel(QStringLiteral("국토지리정보원 계정을 이 PC에 저장하면 수치지형도를 받을 때 자동 로그인에 사용합니다. 두 항목을 모두 비우고 저장하면 저장된 자동로그인 계정을 해제합니다."), content);
  description->setWordWrap(true);
  description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  contentLayout->addWidget(description);
  auto* form = new QFormLayout;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  m_username = new QLineEdit(content);
  m_username->setMinimumWidth(0);
  m_username->setObjectName(QStringLiteral("topographicAccountUsername"));
  m_password = new QLineEdit(content);
  m_password->setMinimumWidth(0);
  m_password->setObjectName(QStringLiteral("topographicAccountPassword"));
  m_password->setEchoMode(QLineEdit::Password);
  form->addRow(QStringLiteral("아이디"), m_username);
  form->addRow(QStringLiteral("비밀번호"), m_password);
  contentLayout->addLayout(form);
  const auto credentials = TopographicSettings::credentials();
  m_username->setText(credentials.username); m_password->setText(credentials.password);
  m_error = new QLabel(content); m_error->setObjectName(QStringLiteral("topographicAccountError"));
  m_error->setWordWrap(true);
  m_error->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_error->hide(); contentLayout->addWidget(m_error);
  contentLayout->addStretch();
  scroll->setWidget(content); layout->addWidget(scroll, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("topographicAccountSave"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("topographicAccountCancel"));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this, scroll] {
    const TopographicSettings::Credentials entered{m_username->text().trimmed(), m_password->text()};
    QString error;
    if (entered.username.isEmpty() != entered.password.isEmpty())
      error = QStringLiteral("아이디와 비밀번호를 모두 입력하거나 두 항목을 모두 비워 주세요.");
    else if (TopographicSettings::saveCredentials(entered, &error)) { accept(); return; }
    m_error->setText(error); m_error->show(); scroll->ensureWidgetVisible(m_error);
  });
  QSize initial(480, 300);
  if (auto* currentScreen = screen())
    initial = initial.boundedTo(currentScreen->availableGeometry().size() - QSize(24, 24));
  resize(initial);
}
