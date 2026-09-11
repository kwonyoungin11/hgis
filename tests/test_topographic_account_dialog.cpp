#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QUuid>
#include "app/KaTopographicAccountDialog.h"
#include "core/TopographicSettings.h"

class TopographicAccountDialogTest : public QObject {
  Q_OBJECT
  QString m_settingsDirectory;
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    const int fontId = QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath(QStringLiteral("Fonts/malgun.ttf")));
    QVERIFY(fontId >= 0);
    const auto families = QFontDatabase::applicationFontFamilies(fontId); QVERIFY(!families.isEmpty());
    QApplication::setFont(QFont(families.first(), 9));
#endif
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-qa-account"));
    QCoreApplication::setApplicationName(QStringLiteral("account-ui-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QVERIFY(!QFileInfo::exists(m_settingsDirectory));
    QString error;
    // Create the isolated personal override before opening the dialog. Tests
    // never read the developer's bundled/repository login configuration.
    QVERIFY2(TopographicSettings::saveCredentials({QStringLiteral("fixture-user"), QStringLiteral("fixture-password")}, &error), qPrintable(error));
  }
  void narrowWindowKeepsActionsAndInputsReachable_data() {
    QTest::addColumn<int>("fontPoints");
    QTest::newRow("normal-font") << 9;
    QTest::newRow("large-font") << 18;
  }
  void narrowWindowKeepsActionsAndInputsReachable() {
    QFETCH(int, fontPoints);
    KaTopographicAccountDialog dialog;
    QFont font = dialog.font(); font.setPointSize(fontPoints); dialog.setFont(font);
    dialog.resize(280, 180); dialog.show(); QTest::qWait(30);
    QCOMPARE(dialog.width(), 280);
    auto* save = dialog.findChild<QPushButton*>(QStringLiteral("topographicAccountSave")); QVERIFY(save);
    auto* cancel = dialog.findChild<QPushButton*>(QStringLiteral("topographicAccountCancel")); QVERIFY(cancel);
    for (auto* action : {save, cancel})
      QVERIFY(dialog.rect().contains(QRect(action->mapTo(&dialog, QPoint()), action->size())));
    QVERIFY(!QRect(save->mapTo(&dialog, QPoint()), save->size()).intersects(
        QRect(cancel->mapTo(&dialog, QPoint()), cancel->size())));
    auto* area = dialog.findChild<QScrollArea*>(QStringLiteral("topographicAccountScroll")); QVERIFY(area);
    for (const auto& name : {QStringLiteral("topographicAccountUsername"), QStringLiteral("topographicAccountPassword")}) {
      auto* edit = dialog.findChild<QLineEdit*>(name); QVERIFY(edit);
      area->ensureWidgetVisible(edit); QTest::qWait(10);
      QVERIFY(area->viewport()->rect().contains(QRect(edit->mapTo(area->viewport(), QPoint()), edit->size())));
    }
    QDir().mkpath(QStringLiteral("build/qa/account-unit"));
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/account-unit/dialog-280x180-%1pt.png").arg(fontPoints)));
  }
  void savedChangesAndEmptyOverrideRemainEditable() {
    KaTopographicAccountDialog dialog;
    auto* username = dialog.findChild<QLineEdit*>(QStringLiteral("topographicAccountUsername")); QVERIFY(username);
    auto* password = dialog.findChild<QLineEdit*>(QStringLiteral("topographicAccountPassword")); QVERIFY(password);
    QCOMPARE(password->echoMode(), QLineEdit::Password);
    username->setText(QStringLiteral("fixture-replacement")); password->setText(QStringLiteral("fixture-new-password"));
    dialog.findChild<QPushButton*>(QStringLiteral("topographicAccountSave"))->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(TopographicSettings::credentials().username, QStringLiteral("fixture-replacement"));
    KaTopographicAccountDialog clear;
    clear.findChild<QLineEdit*>(QStringLiteral("topographicAccountUsername"))->clear();
    clear.findChild<QLineEdit*>(QStringLiteral("topographicAccountPassword"))->clear();
    clear.findChild<QPushButton*>(QStringLiteral("topographicAccountSave"))->click();
    QCOMPARE(clear.result(), int(QDialog::Accepted));
    QVERIFY(TopographicSettings::credentials().username.isEmpty());
    QVERIFY(TopographicSettings::credentials().password.isEmpty());
  }
  void cleanupTestCase() {
    QVERIFY(m_settingsDirectory == QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    QVERIFY(QFileInfo(m_settingsDirectory).fileName().startsWith(QStringLiteral("account-ui-")));
    QVERIFY(QDir(m_settingsDirectory).removeRecursively());
  }
};
QTEST_MAIN(TopographicAccountDialogTest)
#include "test_topographic_account_dialog.moc"
