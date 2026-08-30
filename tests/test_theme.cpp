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
  void noCheapSpinArrowBlock();
  void noCatchAllWidgetRules();
  void chromeFontIsFieldKorean();
};

void TestTheme::tokensMatchSpec() {
  // Ivory paper + terracotta accent (Anthropic-inspired palette).
  QCOMPARE(KaTheme::tokens().sky1, QColor(30, 103, 198));
  QCOMPARE(KaTheme::tokens().ink, QColor(31, 35, 40));
  QCOMPARE(KaTheme::tokens().sky5, QColor(30, 103, 198));
  QCOMPARE(KaTheme::tokens().border, QColor(213, 217, 222));
  QCOMPARE(KaTheme::tokens().canvasNeutral, QColor(255, 255, 255));
  QCOMPARE(KaTheme::tokens().desk, QColor(242, 243, 245));
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
      "QToolButton.sampleTile:checked",
      "QLabel#emptyState",
      "QToolBar#mainToolbar::separator",
      "QTableWidget",
      "QDialogButtonBox",
      "QWidget#startPage",
  };
  for (const char* sel : need) {
    QVERIFY2(qss.contains(QLatin1String(sel)), sel);
  }
}

void TestTheme::noCheapSpinArrowBlock() {
  const QString qss = KaTheme::embeddedStyleSheet();
  QVERIFY2(!qss.contains(QLatin1String("QAbstractSpinBox::up-arrow")),
           "spin arrows must be drawn by ChromeStyle, not a QSS black square");
}

void TestTheme::noCatchAllWidgetRules() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression banned(
      QStringLiteral(R"((^|[\n{;])\s*(QWidget|QGraphicsView|QFrame)\s*\{)"));
  QVERIFY2(!banned.match(qss).hasMatch(),
           "no ID-less QWidget / QGraphicsView / QFrame rules");
}

void TestTheme::chromeFontIsFieldKorean() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression face(
      QStringLiteral(R"(QMainWindow,[\s\S]*?font-family:\s*"Malgun Gothic")"));
  QVERIFY2(face.match(qss).hasMatch(),
           "window face must lead with Malgun Gothic (field Korean UI)");
  QVERIFY2(!qss.contains(QLatin1String("font-family: \"Pretendard GOV\"")),
           "do not lead chrome with an unshipped Pretendard family");
  QVERIFY2(!qss.contains(QLatin1String("QToolBar#studioToolRail QToolButton")) ||
               !QRegularExpression(QStringLiteral(
                    R"(QToolBar#studioToolRail QToolButton[\s\S]*?font-size:\s*10px)"))
                    .match(qss)
                    .hasMatch(),
           "studio tool rail must not use 10px type");
}

QTEST_GUILESS_MAIN(TestTheme)
#include "test_theme.moc"
