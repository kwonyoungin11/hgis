#include <QtTest>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include "app/KaTheme.h"

class TestTheme : public QObject {
  Q_OBJECT
private slots:
  void tokensMatchSpec();
  void embeddedNotEmpty();
  void diskMatchesEmbedded();
  void noUrl();
  void gisExcludePresent();
  void requiredSelectorsPresent();
  void noCatchAllWidgetRules();
};

void TestTheme::tokensMatchSpec() {
  QCOMPARE(KaTheme::tokens().sky1, QColor(94, 179, 228));
  QCOMPARE(KaTheme::tokens().ink, QColor(15, 23, 42));
  QCOMPARE(KaTheme::tokens().sky5, QColor(10, 74, 124));
  QCOMPARE(KaTheme::tokens().border, QColor(0, 0, 0));
  QCOMPARE(KaTheme::tokens().canvasNeutral, QColor(232, 238, 244));
  QCOMPARE(KaTheme::tokens().desk, QColor(229, 231, 235));
}

static QString normalize(QString s) { return s.replace(QLatin1String("\r\n"), QLatin1String("\n")); }

void TestTheme::embeddedNotEmpty() {
  QVERIFY2(!KaTheme::embeddedStyleSheet().trimmed().isEmpty(), "embedded QSS must compile in");
}

void TestTheme::diskMatchesEmbedded() {
  QFile f(QStringLiteral("data/theme/ka-hgis.qss"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run theme_qss with WORKING_DIRECTORY = source tree");
  const QString disk = normalize(QString::fromUtf8(f.readAll()));
  const QString emb = normalize(KaTheme::embeddedStyleSheet());
  QCOMPARE(emb, disk);
}

void TestTheme::noUrl() {
  QVERIFY2(!KaTheme::embeddedStyleSheet().contains(QLatin1String("url(")),
           "QSS must not use url()");
}

void TestTheme::gisExcludePresent() {
  const QString qss = KaTheme::embeddedStyleSheet();
  QVERIFY(qss.contains(QLatin1String("QgsMapCanvas")));
  QVERIFY(qss.contains(QLatin1String("QWidget#mapCanvas")));
  QVERIFY(qss.contains(QLatin1String("QgsLayoutView")));
  QVERIFY(qss.contains(QLatin1String("QWidget#layoutView")));
}

void TestTheme::requiredSelectorsPresent() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const char* need[] = {
      "QToolButton",
      "QToolBar#subToolbar",
      "QDockWidget",
      "QSplitter::handle",
      "QgsLayerTreeView",
      "QAbstractSpinBox::up-button",
      "QCheckBox",
      "QToolTip",
      "QPushButton:checked",
      "QPushButton#btnAdjustDone",
      "QToolButton.sampleTile",
  };
  for (const char* sel : need) {
    QVERIFY2(qss.contains(QLatin1String(sel)), sel);
  }
}

void TestTheme::noCatchAllWidgetRules() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression banned(
      QStringLiteral(R"((^|[\n{;])\s*(QWidget|QGraphicsView|QFrame)\s*\{)"));
  QVERIFY2(!banned.match(qss).hasMatch(),
           "no ID-less QWidget / QGraphicsView / QFrame rules");
}

QTEST_GUILESS_MAIN(TestTheme)
#include "test_theme.moc"
