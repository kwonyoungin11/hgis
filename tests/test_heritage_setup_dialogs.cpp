#include <QtTest>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QUuid>
#include "app/KaHeritageSetupDialogs.h"
#include "core/HeritageIntranetSettings.h"

class HeritageSetupDialogsTest : public QObject {
  Q_OBJECT
  QString settingsDirectory;
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    const int fontId = QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath(QStringLiteral("Fonts/malgun.ttf")));
    QVERIFY(fontId >= 0);
    QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 9));
#endif
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis-qa-heritage"));
    QCoreApplication::setApplicationName(QStringLiteral("setup-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    settingsDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QVERIFY(!QFileInfo::exists(settingsDirectory));
    QString error;
    QVERIFY2(HeritageIntranetSettings::saveCredentials({}, &error), qPrintable(error));
  }
  void missingAccountCanBeSavedAndCancelledWithoutChanges() {
    KaHeritageAccountDialog dialog;
    dialog.show();
    QTest::qWait(20);
    QDir().mkpath(QStringLiteral("build/qa/heritage-startup-20260915/dialogs"));
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/heritage-startup-20260915/dialogs/account.png")));
    auto* id = dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountUsername")); QVERIFY(id);
    auto* pw = dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword")); QVERIFY(pw);
    QCOMPARE(pw->echoMode(), QLineEdit::Password);
    auto* buttons = dialog.findChild<QDialogButtonBox*>(); QVERIFY(buttons);
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    id->setText(QStringLiteral("fixture-user"));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(accepted.count(), 0);
    QVERIFY(!HeritageIntranetSettings::hasCredentials());
    pw->setText(QStringLiteral(" test;@\\password "));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(accepted.count(), 1);
    QCOMPARE(HeritageIntranetSettings::credentials().password, pw->text());
    KaHeritageAccountDialog cancelled;
    cancelled.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword"))->setText(QStringLiteral("unsaved"));
    cancelled.reject();
    QCOMPARE(HeritageIntranetSettings::credentials().password, pw->text());
  }
  void failedLookupRequiresExplicitCityAndResetsAcrossProvinces() {
    KaHeritageRegionDialog dialog({}, {}, QStringLiteral("좌표로 시·군을 찾지 못했습니다."));
    dialog.show();
    QTest::qWait(20);
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/heritage-startup-20260915/dialogs/region.png")));
    auto* sido = dialog.findChild<QComboBox*>(QStringLiteral("heritageSido")); QVERIFY(sido);
    auto* city = dialog.findChild<QComboBox*>(QStringLiteral("heritageCity")); QVERIFY(city);
    auto* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(!ok->isEnabled());
    sido->setCurrentIndex(sido->findData(QStringLiteral("경상북도")));
    QVERIFY(!ok->isEnabled());
    city->setCurrentIndex(city->findData(QStringLiteral("안동시")));
    QVERIFY(ok->isEnabled());
    QCOMPARE(dialog.city(), QStringLiteral("안동시"));
    sido->setCurrentIndex(sido->findData(QStringLiteral("경기도")));
    QVERIFY(dialog.city().isEmpty());
    QVERIFY(!ok->isEnabled());
  }
  void detectedRegionAndSejongCanBeConfirmed() {
    KaHeritageRegionDialog detected(QStringLiteral("경상북도"), QStringLiteral("안동시"));
    QCOMPARE(detected.sido(), QStringLiteral("경상북도"));
    QCOMPARE(detected.city(), QStringLiteral("안동시"));
    KaHeritageRegionDialog sejong(QStringLiteral("세종특별자치시"), QStringLiteral("세종시"));
    auto* ok = sejong.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    QVERIFY(ok->isEnabled());
    QCOMPARE(sejong.sido(), QStringLiteral("세종특별자치시"));
    QCOMPARE(sejong.city(), QStringLiteral("세종시"));
  }
  void clearingAccountKeepsPersonalEmptyOverride() {
    KaHeritageAccountDialog dialog;
    dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountUsername"))->clear();
    dialog.findChild<QLineEdit*>(QStringLiteral("heritageAccountPassword"))->clear();
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QVERIFY(!HeritageIntranetSettings::hasCredentials());
    QVERIFY(QFileInfo::exists(QDir(settingsDirectory).filePath(QStringLiteral("heritage-account.ini"))));
  }
  void cleanupTestCase() {
    QVERIFY(settingsDirectory == QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    QVERIFY(QFileInfo(settingsDirectory).fileName().startsWith(QStringLiteral("setup-")));
    QVERIFY(QDir(settingsDirectory).removeRecursively());
  }
};
QTEST_MAIN(HeritageSetupDialogsTest)
#include "test_heritage_setup_dialogs.moc"
