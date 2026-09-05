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
  void toolbarCheckedIsUnderline();
  void primaryToolbarIconsUseInk();
  void noCheapSpinArrowBlock();
  void noCatchAllWidgetRules();
  void chromeFontIsFieldKorean();
  void beginnerChrome_questionLabels();
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
      "QWidget#beginnerRibbon",
      "QFrame#ribbonGroup",
      "QLabel#ribbonGroupCaption",
      "QWidget#regionLocator QToolButton#regionChip",
  };
  for (const char* sel : need) {
    QVERIFY2(qss.contains(QLatin1String(sel)), sel);
  }
}

void TestTheme::toolbarCheckedIsUnderline() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression solidMain(
      QStringLiteral(R"(QToolBar#mainToolbar QToolButton:checked\s*\{[^}]*background:\s*#1E67C6)"));
  QVERIFY2(!solidMain.match(qss).hasMatch(),
           "main toolbar :checked must not stay solid blue");
  const QRegularExpression washMain(
      QStringLiteral(R"(QToolBar#mainToolbar QToolButton:checked\s*\{[^}]*background:\s*#E8F0FA)"));
  QVERIFY2(!washMain.match(qss).hasMatch(),
           "B Underline: no pale-blue fill on main toolbar checked");
  QVERIFY2(qss.contains(QLatin1String("border-bottom: 2px solid #1E67C6")),
           "checked tools use a 2px underline, not a filled chip");
  const QRegularExpression solidPrimary(
      QStringLiteral(R"(QToolButton#btnPrimary\s*\{[^}]*background:\s*#1E67C6)"));
  QVERIFY2(!solidPrimary.match(qss).hasMatch(),
           "조사파일 단추는 항상 파란 면이면 안 됨");
}

void TestTheme::primaryToolbarIconsUseInk() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(!src.contains(QLatin1String("KaIcons::icon(iconId, QColor(255, 255, 255))")),
           "outlined 새조사/열기/저장 chips must not use white icons");
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

void TestTheme::beginnerChrome_questionLabels() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString main = QString::fromUtf8(mw.readAll());
  QVERIFY2(main.contains(QLatin1String("beginnerRibbon")) ||
               main.contains(QLatin1String("KaBeginnerRibbon")),
           "메인에 초보자 리본");
  QVERIFY2(main.contains(QString::fromUtf8("조사파일")), "조사파일 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("기록")), "기록 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("배경 지도를 깔아볼까?")), "배경 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("정합·분석")) ||
               main.contains(QString::fromUtf8("좌표없는 사진에 좌표를")),
           "정합 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("산출")) ||
               main.contains(QString::fromUtf8("내보내기")),
           "내보내기 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("찾기")), "찾기 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("만들까?")), "새조사 초보자 말");
  QVERIFY2(main.contains(QString::fromUtf8("도면 만들기")) ||
               (main.contains(QString::fromUtf8("도면")) && main.contains(QString::fromUtf8("만들까?"))),
           "도면 만들기");
  QVERIFY2(!main.contains(QLatin1String("addDockWidget(Qt::RightDockWidgetArea, checkDock)")),
           "도면 검수 칸을 지도에 붙이지 않음");
  QVERIFY2(main.contains(QString::fromUtf8("5179")), "제출은 5179");
  QVERIFY2(main.contains(QString::fromUtf8("유구 면을 그려볼까?")) ||
               main.contains(QString::fromUtf8("그려볼까?")),
           "그리기 말");
  QFile fb(QStringLiteral("src/app/KaFileBrowserPanel.cpp"));
  const QString panelCode = fb.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(fb.readAll()) : QString();
  QVERIFY2(main.contains(QString::fromUtf8("파일을 지도에 끌어 넣으면 레이어가 됩니다.")) ||
               panelCode.contains(QString::fromUtf8("파일을 지도에 끌어 넣으면 레이어가 됩니다.")),
           "파일함 끌어넣기 안내");
  QVERIFY2(!main.contains(QLatin1String("addIcon(QStringLiteral(\"out\"), QStringLiteral(\"terrain_3d\")")),
           "입체지형 리본 삭제");

  QFile rb(QStringLiteral("src/app/KaBeginnerRibbon.cpp"));
  QVERIFY2(rb.open(QIODevice::ReadOnly | QIODevice::Text), "KaBeginnerRibbon.cpp");
  const QString ribbon = QString::fromUtf8(rb.readAll());
  QVERIFY2(ribbon.contains(QLatin1String("ribbonGroupCaption")), "그룹 제목 라벨");
  QVERIFY2(ribbon.contains(QLatin1String("twoLine")) || ribbon.contains(QStringLiteral("\\n")),
           "리본 글자는 두 줄");

  QFile ds(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(ds.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString studio = QString::fromUtf8(ds.readAll());
  QVERIFY2(studio.contains(QString::fromUtf8("PDF로 내보낼까?")), "조판 PDF 초보자 말");
  QVERIFY2(studio.contains(QString::fromUtf8("무엇을 넣을까?")), "조판 오른쪽 안내");
  QVERIFY2(!studio.contains(QLatin1String("studioToolbar")), "조판 위 보기 툴바 없음 — 휠·드래그");
  QVERIFY2(!studio.contains(QString::fromUtf8("용지 전체를 볼까?")), "용지 맞춤 버튼 없음");
  QVERIFY2(!studio.contains(QString::fromUtf8("화면을 움직여볼까?")), "화면 이동 버튼 없음");
  QVERIFY2(!studio.contains(QLatin1String("addGroup(QStringLiteral(\"view\")")),
           "보기 그룹 없음");
  QVERIFY2(studio.contains(QLatin1String("savePdf")), "PDF 슬롯은 그대로");
  QVERIFY2(studio.contains(QLatin1String("&KaDrawingStudio::savePdf")),
           "조판 「PDF로 내보낼까?」가 savePdf에 연결되어 있어야 한다");
  QVERIFY2(!studio.contains(QLatin1String("addStudio(QStringLiteral(\"out\")")),
           "위 리본 PDF는 범례창과 중복이라 뺌");
}

QTEST_GUILESS_MAIN(TestTheme)
#include "test_theme.moc"
