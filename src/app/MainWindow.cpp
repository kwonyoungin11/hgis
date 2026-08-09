#include "MainWindow.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"

#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QEventLoop>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>
#include <QSize>
#include <QListWidgetItem>
#include <QToolBar>
#include <QToolButton>
#include <QKeyEvent>
#include <QEvent>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>
#include <QVector>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QFileSystemModel>
#include <QTreeView>
#include <QSplitter>
#include <QFrame>
#include <QStandardPaths>
#include <QColor>
#include <QPalette>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QStorageInfo>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgslayertree.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmaptoolpan.h>
#include <qgssnappingconfig.h>
#include <qgsrubberband.h>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsrectangle.h>
#include <qgsexception.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("고고학 전용 HGIS"));
  resize(1440, 900);
  m_checklist = new ChecklistEngine(this);
  m_checklist->loadRules(rulesPath());
  m_locator = new LocationSearch(this);
  connect(m_locator, &LocationSearch::finished, this, [this](const QVector<LocationHit>& hits) {
    onLocationResults(hits);
  });
  connect(m_locator, &LocationSearch::failed, this, [this](const QString& msg) {
    onLocationFailed(msg);
  });
  buildMenus();
  buildUi();
  applyStartupMap();
  statusBar()->showMessage(
      QStringLiteral("작업 CRS %1 | 업로드 %2 | 규칙 %3개")
          .arg(m_workCrs, QString::fromUtf8(SurveyProjectFactory::uploadCrsAuthId()))
          .arg(m_checklist->ruleCount()),
      10000);
}

MainWindow::~MainWindow() = default;

QString MainWindow::rulesPath() const {
  const QStringList cands = {
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json"))
  };
  for (const QString& c : cands) if (QFile::exists(c)) return c;
  return cands.last();
}

void MainWindow::buildMenus() {
  auto add = [](QMenu* m, const QIcon& ic, const QString& text, auto slot) {
    QAction* a = m->addAction(ic, text, slot);
    a->setIconVisibleInMenu(true);
    return a;
  };

  auto* file = menuBar()->addMenu(KaIcons::icon(QStringLiteral("new")), QStringLiteral("파일"));
  add(file, KaIcons::icon(QStringLiteral("new")), QStringLiteral("새 조사 만들기…"),
      [this]() { newSurvey(); });
  add(file, KaIcons::icon(QStringLiteral("layer")), QStringLiteral("벡터 불러오기…"),
      [this]() { openVectorLayer(); });
  add(file, KaIcons::icon(QStringLiteral("open")), QStringLiteral("프로젝트 열기…"),
      [this]() { openProject(); });
  add(file, KaIcons::icon(QStringLiteral("save")), QStringLiteral("프로젝트 저장…"),
      [this]() { saveProject(); });

  auto* crs = menuBar()->addMenu(KaIcons::icon(QStringLiteral("crs")), QStringLiteral("좌표계"));
  add(crs, KaIcons::icon(QStringLiteral("crs")), QStringLiteral("작업 CRS → 5186 중부"),
      [this]() { setWorkCrs5186(); });
  add(crs, KaIcons::icon(QStringLiteral("crs")), QStringLiteral("작업 CRS → 5187 동부"),
      [this]() { setWorkCrs5187(); });
  crs->addSeparator();
  add(crs, KaIcons::icon(QStringLiteral("upload")), QStringLiteral("선택 레이어 → 5179 SHP (업로드용)"),
      [this]() { convertSelectedTo5179(); });
  add(crs, KaIcons::icon(QStringLiteral("upload")), QStringLiteral("SHP 파일 → 5179 SHP (업로드용)…"),
      [this]() { convertShpFileTo5179(); });
  crs->addSeparator();
  auto* aDef = add(crs, KaIcons::icon(QStringLiteral("crs")), QStringLiteral("이름만 지정(위험)"),
                   [this]() { crsDefineOnly(); });
  aDef->setObjectName(QStringLiteral("actionCrsDefineOnly"));
  auto* aRep = add(crs, KaIcons::icon(QStringLiteral("transform")), QStringLiteral("임의 좌표 변환(재투영)"),
                   [this]() { crsReproject(); });
  aRep->setObjectName(QStringLiteral("actionCrsReproject"));

  auto* bg = menuBar()->addMenu(KaIcons::icon(QStringLiteral("map")), QStringLiteral("배경지도"));
  add(bg, KaIcons::icon(QStringLiteral("map")), QStringLiteral("VWorld 배경"),
      [this]() { addBasemapVworld(); });
  add(bg, KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("VWorld 위성"),
      [this]() { addBasemapVworldSat(); });
  add(bg, KaIcons::icon(QStringLiteral("map")), QStringLiteral("VWorld 지적도"),
      [this]() { addBasemapVworldCadastral(); });
  add(bg, KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM"),
      [this]() { addBasemapOsm(); });
  add(bg, KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
      [this]() { addBasemapGoogle(); });

  auto* tools = menuBar()->addMenu(KaIcons::icon(QStringLiteral("check")), QStringLiteral("도구"));
  add(tools, KaIcons::icon(QStringLiteral("georef")), QStringLiteral("스캔 평면도 맞추기…"),
      [this]() { georefAssistant(); });
  add(tools, KaIcons::icon(QStringLiteral("palette")), QStringLiteral("유구 스타일(종류)"), [this]() {
#if KA_HGIS_HAS_QGIS
    if (auto* fp = layerByKey(QStringLiteral("feature_poly"))) {
      if (LayerOps::applyFeaturePolyStyle(fp)) {
        if (m_canvas) m_canvas->refresh();
        statusBar()->showMessage(QStringLiteral("유구 범주 스타일 적용"), 4000);
      }
    }
#endif
  });
  add(tools, KaIcons::icon(QStringLiteral("trash")), QStringLiteral("선택 레이어 삭제"),
      [this]() { removeSelectedLayers(); });
  add(tools, KaIcons::icon(QStringLiteral("check")), QStringLiteral("도면 검수"),
      [this]() { runChecklist(); });
  add(tools, KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("PDF 내보내기…"),
      [this]() { exportPdf(); });
  add(tools, KaIcons::icon(QStringLiteral("export")), QStringLiteral("제출 패키지(SHP)…"),
      [this]() { exportShpPackage(); });
  add(tools, KaIcons::icon(QStringLiteral("palette")), QStringLiteral("도면 조판 다시 만들기(범례·축척)"),
      [this]() { rebuildLayouts(); });

  auto* help = menuBar()->addMenu(KaIcons::icon(QStringLiteral("help")), QStringLiteral("도움말"));
  add(help, KaIcons::icon(QStringLiteral("help")), QStringLiteral("정보"),
      [this]() { showAbout(); });
  add(help, KaIcons::icon(QStringLiteral("search")), QStringLiteral("VWorld API 키 설정…"),
      [this]() { configureVworldKey(); });

  auto* searchTb = addToolBar(QStringLiteral("위치검색"));
  searchTb->setObjectName(QStringLiteral("searchToolbar"));
  searchTb->setIconSize(QSize(24, 24));
  searchTb->setMovable(false);
  auto* searchLabel = new QLabel(QStringLiteral(" 위치 "));
  searchTb->addWidget(searchLabel);
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setObjectName(QStringLiteral("locationSearch"));
  m_searchEdit->setPlaceholderText(QStringLiteral("주소 · 지번 · 지역 · 상호 검색 (예: 경주 황남동, 서울시청)"));
  m_searchEdit->setMinimumWidth(360);
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::runLocationSearch);
  searchTb->addWidget(m_searchEdit);
  searchTb->addAction(KaIcons::icon(QStringLiteral("search")), QStringLiteral("검색"),
                      this, &MainWindow::runLocationSearch);

  auto* mainTb = addToolBar(QStringLiteral("주요"));
  mainTb->setObjectName(QStringLiteral("mainToolbar"));
  mainTb->setIconSize(QSize(36, 36));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->setMovable(false);
  mainTb->addAction(KaIcons::icon(QStringLiteral("new")), QStringLiteral("새조사"), this, &MainWindow::newSurvey);
  mainTb->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("열기"), this, &MainWindow::openProject);
  mainTb->addAction(KaIcons::icon(QStringLiteral("save")), QStringLiteral("저장"), this, &MainWindow::saveProject);
  mainTb->addSeparator();

  auto* btnDraw = new QToolButton(mainTb);
  btnDraw->setObjectName(QStringLiteral("btnDraw"));
  btnDraw->setIcon(KaIcons::icon(QStringLiteral("draw_poly")));
  btnDraw->setText(QStringLiteral("그리기"));
  btnDraw->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnDraw->setCheckable(true);
  btnDraw->setToolTip(QStringLiteral("누르면 면·선·구역·묶기 세부 도구가 나타납니다"));
  connect(btnDraw, &QToolButton::clicked, this, [this, btnDraw](bool on) {
    if (on) showSubToolsDraw();
    else hideSubTools();
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
    btnDraw->setChecked(m_subToolbar && m_subToolbar->isVisible() && m_subToolsMode == QLatin1String("draw"));
  });
  mainTb->addWidget(btnDraw);

  auto* btnBg = new QToolButton(mainTb);
  btnBg->setObjectName(QStringLiteral("btnBasemap"));
  btnBg->setIcon(KaIcons::icon(QStringLiteral("satellite")));
  btnBg->setText(QStringLiteral("배경"));
  btnBg->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnBg->setCheckable(true);
  btnBg->setToolTip(QStringLiteral("위성·지적·OSM 배경"));
  connect(btnBg, &QToolButton::clicked, this, [this, btnBg](bool on) {
    if (on) showSubToolsBasemap();
    else hideSubTools();
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
    btnBg->setChecked(m_subToolbar && m_subToolbar->isVisible() && m_subToolsMode == QLatin1String("basemap"));
  });
  mainTb->addWidget(btnBg);

  auto* btnSubmit = new QToolButton(mainTb);
  btnSubmit->setObjectName(QStringLiteral("btnSubmit"));
  btnSubmit->setIcon(KaIcons::icon(QStringLiteral("export")));
  btnSubmit->setText(QStringLiteral("제출"));
  btnSubmit->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnSubmit->setCheckable(true);
  btnSubmit->setToolTip(QStringLiteral("검수·SHP·PDF·5179 변환"));
  connect(btnSubmit, &QToolButton::clicked, this, [this, btnSubmit](bool on) {
    if (on) showSubToolsSubmit();
    else hideSubTools();
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
    if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
    btnSubmit->setChecked(m_subToolbar && m_subToolbar->isVisible() && m_subToolsMode == QLatin1String("submit"));
  });
  mainTb->addWidget(btnSubmit);

  mainTb->addSeparator();
  mainTb->addAction(KaIcons::icon(QStringLiteral("save")), QStringLiteral("편집저장"), this, &MainWindow::saveEdits);
  mainTb->addAction(KaIcons::icon(QStringLiteral("trash")), QStringLiteral("삭제"), this, &MainWindow::removeSelectedLayers);

  m_subToolbar = addToolBar(QStringLiteral("세부도구"));
  m_subToolbar->setObjectName(QStringLiteral("subToolbar"));
  m_subToolbar->setIconSize(QSize(28, 28));
  m_subToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_subToolbar->setMovable(false);
  m_subToolbar->setVisible(false);
}

void MainWindow::clearSubToolbar() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  m_subToolbar->clear();
#endif
}

void MainWindow::hideSubTools() {
#if KA_HGIS_HAS_QGIS
  clearSubToolbar();
  if (m_subToolbar) m_subToolbar->setVisible(false);
  m_subToolsMode.clear();
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
#endif
}

void MainWindow::showSubToolsDraw() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("draw") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("draw");
  auto* lab = new QLabel(QStringLiteral("  그리기 › "));
  lab->setStyleSheet(QStringLiteral("color:#1e4d8c;font-weight:700;padding:0 4px;"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_poly")), QStringLiteral("면(폴리곤)"),
                          this, &MainWindow::startEditFeaturePoly);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_line")), QStringLiteral("선"),
                          this, &MainWindow::startEditFeatureLine);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_area")), QStringLiteral("구역"),
                          this, &MainWindow::startEditSurveyArea);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("gps")), QStringLiteral("GPS"),
                          this, &MainWindow::addControlPoint);
  m_subToolbar->addSeparator();
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("그리기종료"),
                          this, &MainWindow::stopEdits);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(QStringLiteral("그리기 도구: 면·선·구역 중 선택 · 여러 면은 「폴리곤 묶기」로 제출용 1개로 합침"), 8000);
#endif
}

void MainWindow::showSubToolsBasemap() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("basemap") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("basemap");
  auto* lab = new QLabel(QStringLiteral("  배경 › "));
  lab->setStyleSheet(QStringLiteral("color:#0f766e;font-weight:700;padding:0 4px;"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("위성"),
                          this, &MainWindow::addBasemapVworldSat);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("지적·번지"),
                          this, &MainWindow::addBasemapVworldCadastral);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("VWorld"),
                          this, &MainWindow::addBasemapVworld);
  m_subToolbar->addAction(QStringLiteral("OSM"), this, &MainWindow::addBasemapOsm);
  m_subToolbar->addAction(QStringLiteral("Google위성"), this, &MainWindow::addBasemapGoogle);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(QStringLiteral("배경 지도 선택 · 작업 CRS(5186/5187)에 OTF로 맞춰 표시"), 6000);
#endif
}

void MainWindow::showSubToolsSubmit() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("submit") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("submit");
  auto* lab = new QLabel(QStringLiteral("  제출 › "));
  lab->setStyleSheet(QStringLiteral("color:#b45309;font-weight:700;padding:0 4px;"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("도면검수"),
                          this, &MainWindow::runChecklist);
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("export")), QStringLiteral("SHP패키지(5179)"),
                          this, &MainWindow::exportShpPackage);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("조판PDF"),
                          this, &MainWindow::exportReportLayout);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("upload")), QStringLiteral("5179변환"),
                          this, &MainWindow::convertSelectedTo5179);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("제출: 필요 시 폴리곤 묶기 → SHP패키지(feature_poly.shp 한 파일, EPSG:5179) → 문화재 인트라넷 업로드"),
      12000);
#endif
}

void MainWindow::applyPhase1Theme() {
  QPalette light;
  light.setColor(QPalette::Window, QColor(0xe8, 0xf1, 0xfb));
  light.setColor(QPalette::WindowText, QColor(0x0f, 0x17, 0x2a));
  light.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
  light.setColor(QPalette::AlternateBase, QColor(0xf7, 0xfb, 0xff));
  light.setColor(QPalette::Text, QColor(0x0f, 0x17, 0x2a));
  light.setColor(QPalette::Button, QColor(0xe8, 0xf1, 0xfb));
  light.setColor(QPalette::ButtonText, QColor(0x0f, 0x17, 0x2a));
  light.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xee));
  light.setColor(QPalette::ToolTipText, QColor(0x0f, 0x17, 0x2a));
  light.setColor(QPalette::BrightText, QColor(0x0f, 0x17, 0x2a));
  light.setColor(QPalette::Highlight, QColor(0x2b, 0x6c, 0xb0));
  light.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  light.setColor(QPalette::PlaceholderText, QColor(0x64, 0x74, 0x8b));
  qApp->setPalette(light);
  setPalette(light);

  setStyleSheet(QStringLiteral(
      "* { color: #0f172a; }"
      "QMainWindow { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      " stop:0 #e8f1fb, stop:1 #c5d9f2); color: #0f172a; }"
      "QToolBar#mainToolbar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      " stop:0 #1e4d8c, stop:1 #2b6cb0); border: none; spacing: 8px; padding: 8px; }"
      "QToolBar#mainToolbar QToolButton { color: white; font-weight: 700; font-size: 12px;"
      " padding: 6px 12px; border-radius: 10px; background: transparent; min-width: 56px; }"
      "QToolBar#mainToolbar QToolButton:hover { background: rgba(255,255,255,0.18); color: white; }"
      "QToolBar#mainToolbar QToolButton:checked { background: rgba(255,255,255,0.28); color: white; }"
      "QToolBar#subToolbar { background: #0f172a; border: none; spacing: 6px; padding: 6px 10px; }"
      "QToolBar#subToolbar QToolButton, QToolBar#subToolbar QLabel {"
      " color: #f8fafc; font-weight: 600; font-size: 12px; padding: 4px 10px; border-radius: 8px; }"
      "QToolBar#subToolbar QToolButton { background: #1e293b; border: 1px solid #334155; }"
      "QToolBar#subToolbar QToolButton:hover { background: #334155; color: white; }"
      "QToolBar#searchToolbar, QToolBar#searchToolbar QLabel { color: #0f172a; background: #e8f1fb; }"
      "QToolBar#searchToolbar QToolButton { color: #0f172a; background: #dbeafe; border-radius: 6px; padding: 4px 8px; }"
      "QFrame#filesCard, QFrame#layersCard, QFrame#mapCard {"
      " background: #ffffff; border: 2px solid #7aa2c9;"
      " border-radius: 14px; color: #0f172a; }"
      "QFrame#filesCard { border-top: 5px solid #2563eb; }"
      "QFrame#layersCard { border-top: 5px solid #0d9488; }"
      "QFrame#mapCard { border-top: 5px solid #1e4d8c; }"
      "QFrame#filesCard QToolButton, QFrame#layersCard QToolButton, QFrame#mapCard QToolButton {"
      " color: #0f172a; background: #e8f1fb; border: 1px solid #9bb8d9;"
      " border-radius: 8px; padding: 5px 10px; font-weight: 600; min-height: 26px; }"
      "QFrame#filesCard QToolButton:hover, QFrame#layersCard QToolButton:hover, QFrame#mapCard QToolButton:hover {"
      " background: #d0e4f7; color: #0f172a; }"
      "QFrame#layersCard QToolButton:checked { background: #1e4d8c; color: white; }"
      "QLabel { color: #0f172a; background: transparent; }"
      "QLabel#cardCaption {"
      " color: #0f172a; font-weight: 800; font-size: 14px; padding: 8px 10px 6px 10px;"
      " background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #eff6ff, stop:1 #ffffff);"
      " border-radius: 8px; margin-bottom: 2px; }"
      "QLabel#cardCaptionFiles { color: #1d4ed8; }"
      "QLabel#cardCaptionLayers { color: #0f766e; }"
      "QFrame#filesInner, QFrame#layersInner {"
      " background: #f8fbff; border: 1px solid #cfe0f2; border-radius: 10px; }"
      "QStatusBar { background: #1e4d8c; color: white; font-weight: 600; }"
      "QStatusBar QLabel { color: white; }"
      "QTreeView, QTreeView::item, QgsLayerTreeView, QgsLayerTreeView::item {"
      " background: #f7fbff; color: #0f172a; border: none; border-radius: 8px;"
      " font-size: 13px; padding: 3px; }"
      "QTreeView::item:selected, QgsLayerTreeView::item:selected {"
      " background: #bfdbfe; color: #0f172a; }"
      "QTreeView::item:hover, QgsLayerTreeView::item:hover {"
      " background: #e0f2fe; color: #0f172a; }"
      "QHeaderView::section { background: #e8f1fb; color: #0f172a; }"
      "QLineEdit, QComboBox, QAbstractSpinBox {"
      " border: 1px solid #9bb8d9; border-radius: 8px; padding: 4px 8px;"
      " color: #0f172a; background: white; selection-background-color: #bfdbfe;"
      " selection-color: #0f172a; }"
      "QComboBox QAbstractItemView { color: #0f172a; background: white; selection-background-color: #bfdbfe; }"
      "QMenuBar { color: #0f172a; background: #e8f1fb; font-weight: 600; }"
      "QMenuBar::item { color: #0f172a; padding: 4px 10px; }"
      "QMenuBar::item:selected { background: #bfdbfe; color: #0f172a; }"
      "QMenu { color: #0f172a; background: white; }"
      "QMenu::item { color: #0f172a; padding: 6px 24px; }"
      "QMenu::item:selected { background: #bfdbfe; color: #0f172a; }"
      "QMessageBox, QMessageBox QLabel { color: #0f172a; background: #f8fafc; }"
      "QMessageBox QPushButton { color: #0f172a; background: #e2e8f0; border: 1px solid #94a3b8;"
      " border-radius: 6px; padding: 6px 16px; min-width: 64px; }"
      "QInputDialog, QDialog { color: #0f172a; background: #f8fafc; }"
      "QInputDialog QLabel, QDialog QLabel { color: #0f172a; }"
      "QToolTip { color: #0f172a; background: #fffbeb; border: 1px solid #f59e0b; }"));
}

void MainWindow::buildUi() {
  applyPhase1Theme();
  auto* central = new QWidget(this);
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  m_canvas->setCanvasColor(QColor(232, 241, 251));
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  m_canvas->setParallelRenderingEnabled(true);
  m_canvas->setMapUpdateInterval(60);
  m_canvas->setPreviewJobsEnabled(true);
  m_canvas->setSegmentationTolerance(2.0);
  const QgsCoordinateReferenceSystem crs(m_workCrs);
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_panTool = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_panTool);

  auto* treeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new QgsLayerTreeModel(treeRoot, this);
  model->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  m_layerTree = new QgsLayerTreeView(central);
  m_layerTree->setObjectName(QStringLiteral("layerTree"));
  m_layerTree->setModel(model);
  m_layerTree->setFocusPolicy(Qt::StrongFocus);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragDropMode(QAbstractItemView::DragDrop);
  m_layerTree->setStyleSheet(QStringLiteral(
      "QgsLayerTreeView, QTreeView { background: #f7fbff; color: #0f172a; border: none;"
      " border-radius: 8px; font-size: 13px; outline: none; }"
      "QgsLayerTreeView::item, QTreeView::item { color: #0f172a; padding: 3px 4px; min-height: 22px; }"
      "QgsLayerTreeView::item:selected, QTreeView::item:selected {"
      " background: #bfdbfe; color: #0f172a; }"
      "QgsLayerTreeView::item:hover, QTreeView::item:hover {"
      " background: #e0f2fe; color: #0f172a; }"));
  {
    QPalette pal = m_layerTree->palette();
    pal.setColor(QPalette::Base, QColor(0xf7, 0xfb, 0xff));
    pal.setColor(QPalette::Text, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::WindowText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::BrightText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::HighlightedText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::Highlight, QColor(0xbf, 0xdb, 0xfe));
    m_layerTree->setPalette(pal);
  }
  connect(model, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
    onLayerTreeRowsMoved();
  });
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_layerTree, &QWidget::customContextMenuRequested, this, &MainWindow::onLayerTreeContextMenu);
  m_bridge = new QgsLayerTreeMapCanvasBridge(treeRoot, m_canvas, this);
  m_bridge->setAutoSetupOnFirstLayer(true);

  m_canvas->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_canvas, &QWidget::customContextMenuRequested, this, &MainWindow::onMapContextMenu);
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, &MainWindow::onCanvasScaleChanged);
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_extentClampGuard || !m_canvas) return;
    m_extentClampGuard = true;
    if (LayerOps::clampCanvasToKorea(m_canvas))
      m_canvas->refresh();
    m_extentClampGuard = false;
  });
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); });
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); });
  });
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);

  setupFileBrowser();

  auto* delBtn = new QToolButton(central);
  delBtn->setObjectName(QStringLiteral("btnRemoveLayer"));
  delBtn->setIcon(KaIcons::icon(QStringLiteral("trash")));
  delBtn->setText(QStringLiteral("레이어 삭제"));
  delBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(delBtn, &QToolButton::clicked, this, &MainWindow::removeSelectedLayers);

  auto* btnCrs5186 = new QToolButton(central);
  btnCrs5186->setObjectName(QStringLiteral("btnCrs5186"));
  btnCrs5186->setText(QStringLiteral("5186"));
  btnCrs5186->setToolTip(QStringLiteral("작업 좌표계 EPSG:5186 (중부원점)"));
  btnCrs5186->setCheckable(true);
  btnCrs5186->setChecked(m_workCrs == QLatin1String("EPSG:5186"));
  connect(btnCrs5186, &QToolButton::clicked, this, [this, btnCrs5186]() {
    setWorkCrs5186();
    if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(true);
    if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(false);
    Q_UNUSED(btnCrs5186);
  });
  auto* btnCrs5187 = new QToolButton(central);
  btnCrs5187->setObjectName(QStringLiteral("btnCrs5187"));
  btnCrs5187->setText(QStringLiteral("5187"));
  btnCrs5187->setToolTip(QStringLiteral("작업 좌표계 EPSG:5187 (동부원점)"));
  btnCrs5187->setCheckable(true);
  btnCrs5187->setChecked(m_workCrs == QLatin1String("EPSG:5187"));
  connect(btnCrs5187, &QToolButton::clicked, this, [this]() {
    setWorkCrs5187();
    if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(false);
    if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(true);
  });
  auto* btnZoomMax = new QToolButton(central);
  btnZoomMax->setObjectName(QStringLiteral("btnZoomMax"));
  btnZoomMax->setText(QStringLiteral("전체보기"));
  btnZoomMax->setToolTip(QStringLiteral("지도를 최대 범위로 보기"));
  connect(btnZoomMax, &QToolButton::clicked, this, &MainWindow::zoomMapToFullMax);

  auto* filesCard = new QFrame(central);
  filesCard->setObjectName(QStringLiteral("filesCard"));
  auto* filesLay = new QVBoxLayout(filesCard);
  filesLay->setContentsMargins(10, 10, 10, 10);
  filesLay->setSpacing(8);
  auto* capFiles = new QLabel(QStringLiteral("① 조사 파일 경로 (SHP·GPKG)"), filesCard);
  capFiles->setObjectName(QStringLiteral("cardCaption"));
  capFiles->setProperty("class", QStringLiteral("cardCaptionFiles"));
  capFiles->setStyleSheet(QStringLiteral("QLabel { color: #1d4ed8; font-weight: 800; font-size: 14px; }"));
  auto* pathBar = new QHBoxLayout();
  pathBar->setSpacing(6);
  auto* btnPc = new QToolButton(filesCard);
  btnPc->setText(QStringLiteral("내PC"));
  btnPc->setObjectName(QStringLiteral("btnBrowsePc"));
  connect(btnPc, &QToolButton::clicked, this, [this]() { goFileBrowserRoot(QStringLiteral("")); });
  auto* btnC = new QToolButton(filesCard);
  btnC->setText(QStringLiteral("C:\\"));
  btnC->setObjectName(QStringLiteral("btnBrowseC"));
  connect(btnC, &QToolButton::clicked, this, [this]() { goFileBrowserRoot(QStringLiteral("C:/")); });
  auto* btnDocs = new QToolButton(filesCard);
  btnDocs->setText(QStringLiteral("문서"));
  btnDocs->setObjectName(QStringLiteral("btnBrowseDocs"));
  connect(btnDocs, &QToolButton::clicked, this, [this]() {
    goFileBrowserRoot(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  });
  auto* btnDesk = new QToolButton(filesCard);
  btnDesk->setText(QStringLiteral("바탕화면"));
  connect(btnDesk, &QToolButton::clicked, this, [this]() {
    goFileBrowserRoot(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
  });
  auto* btnPick = new QToolButton(filesCard);
  btnPick->setText(QStringLiteral("폴더…"));
  btnPick->setObjectName(QStringLiteral("btnBrowseFolder"));
  connect(btnPick, &QToolButton::clicked, this, &MainWindow::browseDataFolder);
  pathBar->addWidget(btnPc);
  pathBar->addWidget(btnC);
  pathBar->addWidget(btnDocs);
  pathBar->addWidget(btnDesk);
  pathBar->addWidget(btnPick);
  pathBar->addStretch(1);
  auto* filesInner = new QFrame(filesCard);
  filesInner->setObjectName(QStringLiteral("filesInner"));
  auto* filesInnerLay = new QVBoxLayout(filesInner);
  filesInnerLay->setContentsMargins(6, 6, 6, 6);
  filesInnerLay->addWidget(m_fileBrowser, 1);
  filesLay->addWidget(capFiles);
  filesLay->addLayout(pathBar);
  filesLay->addWidget(filesInner, 1);

  auto* layersCard = new QFrame(central);
  layersCard->setObjectName(QStringLiteral("layersCard"));
  auto* layersLay = new QVBoxLayout(layersCard);
  layersLay->setContentsMargins(10, 10, 10, 10);
  layersLay->setSpacing(8);
  auto* capLayers = new QLabel(QStringLiteral("② 지도 레이어 (체크=표시 · 드래그 추가)"), layersCard);
  capLayers->setObjectName(QStringLiteral("cardCaption"));
  capLayers->setStyleSheet(QStringLiteral("QLabel { color: #0f766e; font-weight: 800; font-size: 14px; }"));
  auto* layersInner = new QFrame(layersCard);
  layersInner->setObjectName(QStringLiteral("layersInner"));
  auto* layersInnerLay = new QVBoxLayout(layersInner);
  layersInnerLay->setContentsMargins(6, 6, 6, 6);
  layersInnerLay->addWidget(m_layerTree, 1);
  auto* bottomBar = new QHBoxLayout();
  bottomBar->setSpacing(6);
  bottomBar->addWidget(delBtn);
  bottomBar->addWidget(btnCrs5186);
  bottomBar->addWidget(btnCrs5187);
  bottomBar->addWidget(btnZoomMax);
  bottomBar->addStretch(1);
  layersLay->addWidget(capLayers);
  layersLay->addWidget(layersInner, 1);
  layersLay->addLayout(bottomBar);

  auto* leftSplit = new QSplitter(Qt::Vertical, central);
  leftSplit->setObjectName(QStringLiteral("leftSplit"));
  leftSplit->setHandleWidth(10);
  leftSplit->setChildrenCollapsible(false);
  leftSplit->addWidget(filesCard);
  leftSplit->addWidget(layersCard);
  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 4);
  leftSplit->setSizes({280, 340});
  leftSplit->setMinimumWidth(260);
  leftSplit->setMaximumWidth(400);
  leftSplit->setStyleSheet(QStringLiteral(
      "QSplitter::handle:vertical {"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #93c5fd, stop:0.5 #1e4d8c, stop:1 #5eead4);"
      " margin: 4px 16px; border-radius: 3px; height: 8px; }"
      "QSplitter::handle:vertical:hover { background: #2563eb; }"));

  auto* mapCard = new QFrame(central);
  mapCard->setObjectName(QStringLiteral("mapCard"));
  auto* mapLay = new QVBoxLayout(mapCard);
  mapLay->setContentsMargins(6, 6, 6, 6);
  auto* capMap = new QLabel(QStringLiteral("지도"), mapCard);
  capMap->setObjectName(QStringLiteral("cardCaption"));
  mapLay->addWidget(capMap);
  mapLay->addWidget(m_canvas, 1);

  auto* scaleBar = new QHBoxLayout();
  scaleBar->setSpacing(6);
  auto* scaleLbl = new QLabel(QStringLiteral("축척 1:"), mapCard);
  scaleLbl->setObjectName(QStringLiteral("scaleLabel"));
  m_scaleEdit = new QLineEdit(mapCard);
  m_scaleEdit->setObjectName(QStringLiteral("scaleEdit"));
  m_scaleEdit->setPlaceholderText(QStringLiteral("예: 1000"));
  m_scaleEdit->setMaximumWidth(120);
  m_scaleEdit->setClearButtonEnabled(true);
  connect(m_scaleEdit, &QLineEdit::returnPressed, this, &MainWindow::applyMapScaleFromUi);
  m_scaleCombo = new QComboBox(mapCard);
  m_scaleCombo->setObjectName(QStringLiteral("scaleCombo"));
  m_scaleCombo->setEditable(false);
  const QList<int> presets = {500, 1000, 2000, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
  for (int s : presets)
    m_scaleCombo->addItem(QStringLiteral("1:%1").arg(s), s);
  m_scaleCombo->setCurrentIndex(4);
  connect(m_scaleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
    if (!m_scaleCombo || !m_scaleEdit) return;
    const int s = m_scaleCombo->itemData(idx).toInt();
    if (s > 0) {
      m_scaleEdit->setText(QString::number(s));
      applyMapScaleFromUi();
    }
  });
  auto* btnScaleApply = new QToolButton(mapCard);
  btnScaleApply->setObjectName(QStringLiteral("btnScaleApply"));
  btnScaleApply->setText(QStringLiteral("축척적용"));
  connect(btnScaleApply, &QToolButton::clicked, this, &MainWindow::applyMapScaleFromUi);
  auto* btnMapRefresh = new QToolButton(mapCard);
  btnMapRefresh->setObjectName(QStringLiteral("btnMapRefresh"));
  btnMapRefresh->setText(QStringLiteral("지도새로고침"));
  connect(btnMapRefresh, &QToolButton::clicked, this, &MainWindow::refreshMapCanvasNow);
  scaleBar->addWidget(scaleLbl);
  scaleBar->addWidget(m_scaleEdit);
  scaleBar->addWidget(m_scaleCombo);
  scaleBar->addWidget(btnScaleApply);
  scaleBar->addWidget(btnMapRefresh);
  scaleBar->addStretch(1);
  mapLay->addLayout(scaleBar);

  root->addWidget(leftSplit, 0);
  root->addWidget(mapCard, 1);
#else
  root->addWidget(new QLabel(QStringLiteral("QGIS SDK 스텁 모드"), central), 1);
#endif
  setCentralWidget(central);
}









void MainWindow::rebuildLayouts() {
#if KA_HGIS_HAS_QGIS
  const int n = LayoutService::rebuildDefaultLayouts(QgsProject::instance());
  QMessageBox::information(
      this, QStringLiteral("조판"),
      QStringLiteral("도면 조판 %1종을 다시 만들었습니다.\n"
                     "포함: 제목칸 · 지도 · 진북 방위표 · 범례 · 축척자 · 도곽격자 · 작성요령\n"
                     "PDF 내보내기에서 선택하세요.")
          .arg(n > 0 ? n : LayoutService::defaultLayoutNames().size()));
  statusBar()->showMessage(QStringLiteral("조판 갱신 완료 (범례/축척/방위)"), 6000);
#endif
}





void MainWindow::newSurvey() {
  const QString name = QInputDialog::getText(this, QStringLiteral("새 조사"), QStringLiteral("조사명"));
  if (name.trimmed().isEmpty()) return;
  const QStringList crsChoices = {
      QStringLiteral("EPSG:5186 (중부원점 — 작업용)"),
      QStringLiteral("EPSG:5187 (동부원점 — 작업용)")
  };
  bool ok = false;
  const QString crsPick = QInputDialog::getItem(this, QStringLiteral("작업 좌표계"),
      QStringLiteral("수치지형도·지적·작도 CRS\n(문화재 인트라넷 업/다운로드는 5179로 변환)"),
      crsChoices, m_workCrs.contains(QLatin1String("5187")) ? 1 : 0, false, &ok);
  if (!ok) return;
  m_workCrs = crsPick.contains(QLatin1String("5187")) ? QStringLiteral("EPSG:5187")
                                                      : QStringLiteral("EPSG:5186");
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("저장 폴더"));
  if (dir.isEmpty()) return;
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, name, &err, m_workCrs);
  if (path.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("실패"), err);
    return;
  }
  m_surveyPath = path;
  m_stubSurveyArea = 0; m_stubFeatures = 0; m_stubGcp = 0; m_stubHasMeta = false;
  loadSurveyLayers(path);
#if KA_HGIS_HAS_QGIS
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
  applyStartupMap();
#endif
  setWindowTitle(QStringLiteral("고고학 전용 HGIS — %1 (%2)").arg(name, m_workCrs));
  statusBar()->showMessage(QStringLiteral("조사 저장소 준비. 그리기/GPS 시 레이어가 목록에 추가됩니다. | %1").arg(m_workCrs), 12000);
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr);
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (QgsProject::instance()->mapLayers().isEmpty()) {
    statusBar()->showMessage(
        QStringLiteral("OTF 켜짐 · 작업 CRS %1 · 도면은 좌표계가 달라도 이 지도에 맞춰 표시됩니다.")
            .arg(m_workCrs),
        15000);
  }
  LayerOps::zoomToKorea(m_canvas, m_workCrs);
  LayerOps::clampCanvasToKorea(m_canvas);
  if (m_canvas) {
    m_canvas->refreshAllLayers();
    m_canvas->refresh();
  }
#endif
}



void MainWindow::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
#if KA_HGIS_HAS_QGIS
  QString err;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, authId);
  if (!LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, authId, &err, true)) {
    QMessageBox::warning(this, QStringLiteral("CRS"), err);
    return;
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  LayerOps::clampCanvasToKorea(m_canvas);
  statusBar()->showMessage(
      QStringLiteral("작업 CRS = %1 · OTF(실시간 좌표변환) · 모든 레이어가 이 CRS로 표시")
          .arg(authId),
      10000);
#endif
}
void MainWindow::setWorkCrs5186() {
  setWorkCrs(QStringLiteral("EPSG:5186"));
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(true);
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(false);
}
void MainWindow::setWorkCrs5187() {
  setWorkCrs(QStringLiteral("EPSG:5187"));
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(false);
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(true);
}

void MainWindow::zoomMapToFullMax() {
#if KA_HGIS_HAS_QGIS
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::zoomToFullMax(m_canvas);
  LayerOps::clampCanvasToKorea(m_canvas);
  statusBar()->showMessage(QStringLiteral("한국 전체 범위"), 4000);
#endif
}

void MainWindow::zoomSelectedLayerMax() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !m_canvas) return;
  const QList<QgsMapLayer*> sel = m_layerTree->selectedLayers();
  if (sel.isEmpty()) {
    zoomMapToFullMax();
    return;
  }
  LayerOps::zoomToLayerMax(m_canvas, sel.first());
  statusBar()->showMessage(QStringLiteral("선택 레이어 최대 보기: %1").arg(sel.first()->name()), 4000);
#endif
}

void MainWindow::onMapContextMenu(const QPoint& pos) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  QMenu menu(this);
  menu.addAction(QStringLiteral("전체 최대 보기"), this, &MainWindow::zoomMapToFullMax);
  menu.addAction(QStringLiteral("한국 범위"), this, [this]() {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  });
  menu.addAction(QStringLiteral("선택 레이어 최대 보기"), this, &MainWindow::zoomSelectedLayerMax);
  menu.addSeparator();
  menu.addAction(QStringLiteral("새로고침"), this, [this]() {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  });
  menu.exec(m_canvas->mapToGlobal(pos));
#else
  Q_UNUSED(pos);
#endif
}

void MainWindow::onLayerTreeContextMenu(const QPoint& pos) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;
  const QModelIndex idx = m_layerTree->indexAt(pos);
  if (idx.isValid())
    m_layerTree->setCurrentIndex(idx);
  QMenu menu(this);
  menu.addAction(QStringLiteral("이 레이어 최대 크기로 보기"), this, &MainWindow::zoomSelectedLayerMax);
  menu.addAction(QStringLiteral("전체 최대 보기"), this, &MainWindow::zoomMapToFullMax);
  menu.addSeparator();
  menu.addAction(QStringLiteral("폴리곤 하나로 묶기 (제출용)"), this, &MainWindow::mergeFeaturePolygons);
  menu.addSeparator();
  menu.addAction(QStringLiteral("레이어 삭제"), this, &MainWindow::removeSelectedLayers);
  menu.exec(m_layerTree->viewport()->mapToGlobal(pos));
#else
  Q_UNUSED(pos);
#endif
}

void MainWindow::convertSelectedTo5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    QMessageBox::information(this, QStringLiteral("5179 변환"),
                             QStringLiteral("레이어 트리에서 변환할 벡터(폴리곤 등)를 선택하세요.\n"
                                            "5186/5187로 그린 도형 → 문화재 업로드용 EPSG:5179 SHP"));
    return;
  }
  const QString out = QFileDialog::getSaveFileName(
      this, QStringLiteral("업로드용 5179 SHP 저장"),
      vl->name() + QStringLiteral("_5179.shp"), QStringLiteral("SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::convertToShp5179(vl, out, QgsProject::instance(), &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("변환 실패"), err);
  else {
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("5179 SHP 생성(업로드용): %1").arg(out), 10000);
    QMessageBox::information(this, QStringLiteral("완료"),
                             QStringLiteral("EPSG:5179 SHP 저장됨.\n인트라넷 업로드에 이 파일을 사용하세요.\n%1").arg(out));
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::convertShpFileTo5179() {
#if KA_HGIS_HAS_QGIS
  const QString in = QFileDialog::getOpenFileName(
      this, QStringLiteral("5186/5187 SHP 선택"), QString(),
      QStringLiteral("Vector (*.shp *.gpkg *.geojson)"));
  if (in.isEmpty()) return;
  const QString out = QFileDialog::getSaveFileName(
      this, QStringLiteral("5179 SHP 저장"),
      QFileInfo(in).completeBaseName() + QStringLiteral("_5179.shp"),
      QStringLiteral("SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::convertFileToShp5179(in, out, QgsProject::instance(), &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("변환 실패"), err);
  else
    QMessageBox::information(this, QStringLiteral("완료"),
                             QStringLiteral("업로드용 EPSG:5179 SHP:\n%1").arg(out));
#endif
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
#if KA_HGIS_HAS_QGIS
  if (!event) return QMainWindow::eventFilter(watched, event);
  const bool onLayerDrop = m_layerTree &&
      (watched == m_layerTree || watched == m_layerTree->viewport());
  if (onLayerDrop) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
      auto* de = static_cast<QDragEnterEvent*>(event);
      if (de->mimeData() && de->mimeData()->hasUrls()) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::Drop) {
      auto* de = static_cast<QDropEvent*>(event);
      if (de->mimeData() && de->mimeData()->hasUrls()) {
        tryAddDroppedUrls(de->mimeData()->urls());
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::KeyPress) {
      auto* ke = static_cast<QKeyEvent*>(event);
      if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
        removeSelectedLayers();
        return true;
      }
    }
  }
#else
  Q_UNUSED(watched);
  Q_UNUSED(event);
#endif
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::removeSelectedLayers() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;

  QSet<QString> ids;
  QStringList names;

  QList<QgsMapLayer*> layers = m_layerTree->selectedLayers();
  if (layers.isEmpty()) {
    if (QgsMapLayer* cur = m_layerTree->currentLayer())
      layers.append(cur);
  }
  for (QgsMapLayer* l : layers) {
    if (!l) continue;
    ids.insert(l->id());
    names << l->name();
  }

  if (ids.isEmpty() && m_layerTree->selectionModel()) {
    const QModelIndexList idxs = m_layerTree->selectionModel()->selectedIndexes();
    auto* model = qobject_cast<QgsLayerTreeModel*>(m_layerTree->model());
    for (const QModelIndex& idx : idxs) {
      if (!idx.isValid() || !model) continue;
      QgsLayerTreeNode* node = model->index2node(idx);
      if (!node || !QgsLayerTree::isLayer(node)) continue;
      QgsMapLayer* l = QgsLayerTree::toLayer(node)->layer();
      if (!l) continue;
      ids.insert(l->id());
      names << l->name();
    }
  }

  if (ids.isEmpty()) {
    if (auto* node = m_layerTree->currentNode()) {
      if (QgsLayerTree::isLayer(node)) {
        if (QgsMapLayer* l = QgsLayerTree::toLayer(node)->layer()) {
          ids.insert(l->id());
          names << l->name();
        }
      }
    }
  }

  if (ids.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("삭제할 레이어를 먼저 클릭하세요"), 4000);
    return;
  }

  QgsProject::instance()->removeMapLayers(QList<QString>(ids.begin(), ids.end()));
  if (m_canvas) {
    m_canvas->refresh();
    m_canvas->redrawAllLayers();
  }
  statusBar()->showMessage(QStringLiteral("삭제: %1").arg(names.join(QStringLiteral(", "))), 5000);
#endif
}

QString MainWindow::vworldApiKeyOrPrompt() {
  QString key = VworldSettings::loadApiKey();
  if (!key.isEmpty()) return key;
  QMessageBox::information(this, QStringLiteral("VWorld API 키 필요"),
      QStringLiteral("VWorld 배경지도를 쓰려면 API 키가 필요합니다.\n도움말 → VWorld API 키 설정"));
  return {};
}

#if KA_HGIS_HAS_QGIS
static QStringList projectLayerNames() {
  QStringList names;
  for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
    if (l) names << l->name();
  }
  return names;
}

static void afterBasemapAdded(MainWindow* self, QgsMapCanvas* canvas, const QString& workCrs,
                              const QString& label) {
  if (!self || !canvas) return;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), canvas, workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), canvas, true);
  for (int i = 0; i < 30; ++i)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  QTimer::singleShot(300, self, [self, canvas, workCrs]() {
    if (!canvas) return;
    LayerOps::ensureOtfEnabled(QgsProject::instance(), canvas, workCrs);
    LayerOps::syncMapCanvas(QgsProject::instance(), canvas, false);
    canvas->refreshAllLayers();
    canvas->refresh();
  });
  const QStringList names = projectLayerNames();
  const int onMap = canvas->layers().size();
  self->statusBar()->showMessage(
      QStringLiteral("%1 추가 · OTF→%2 · 표시 %3 · 축척 1:%4")
          .arg(label, workCrs)
          .arg(onMap)
          .arg(canvas->scale(), 0, 'f', 0),
      12000);
  Q_UNUSED(names);
}
#endif

void MainWindow::onCanvasScaleChanged(double scale) {
#if KA_HGIS_HAS_QGIS
  if (m_scaleUiGuard || !m_scaleEdit) return;
  m_scaleUiGuard = true;
  m_scaleEdit->setText(QString::number(scale, 'f', 0));
  if (m_scaleCombo) {
    const int s = qRound(scale);
    int best = -1;
    int bestDiff = 0;
    for (int i = 0; i < m_scaleCombo->count(); ++i) {
      const int v = m_scaleCombo->itemData(i).toInt();
      const int d = qAbs(v - s);
      if (best < 0 || d < bestDiff) {
        best = i;
        bestDiff = d;
      }
    }
    if (best >= 0 && bestDiff <= qMax(50, s / 20))
      m_scaleCombo->setCurrentIndex(best);
  }
  m_scaleUiGuard = false;
#else
  Q_UNUSED(scale);
#endif
}

void MainWindow::applyMapScaleFromUi() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_scaleEdit) return;
  bool ok = false;
  double s = m_scaleEdit->text().trimmed().remove(QLatin1Char(',')).toDouble(&ok);
  if (!ok || s <= 0) {
    statusBar()->showMessage(QStringLiteral("축척 숫자를 입력하세요 (예: 1000 → 1:1000)"), 5000);
    return;
  }
  m_scaleUiGuard = true;
  m_canvas->zoomScale(s, true);
  m_canvas->refresh();
  m_scaleUiGuard = false;
  statusBar()->showMessage(QStringLiteral("축척 적용 1:%1").arg(s, 0, 'f', 0), 4000);
#endif
}

void MainWindow::refreshMapCanvasNow() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  for (int i = 0; i < 10; ++i)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
  m_canvas->refreshAllLayers();
  m_canvas->refresh();
  statusBar()->showMessage(
      QStringLiteral("지도 새로고침 · 표시 %1 · 축척 1:%2")
          .arg(m_canvas->layers().size())
          .arg(m_canvas->scale(), 0, 'f', 0),
      5000);
#endif
}

void MainWindow::addBasemapVworld() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldBaseMap(QgsProject::instance(), m_canvas, key, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("VWorld 배경"));
#endif
}

void MainWindow::addBasemapVworldSat() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldSatelliteMap(QgsProject::instance(), m_canvas, key, &err))
    QMessageBox::warning(this, QStringLiteral("위성"), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("VWorld 위성"));
#endif
}

void MainWindow::addBasemapVworldCadastral() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldCadastralMap(QgsProject::instance(), m_canvas, key, &err))
    QMessageBox::warning(this, QStringLiteral("지적도"), err);
  else {
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("VWorld 지적도"));
    if (m_canvas && m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(5000.0, true);
    statusBar()->showMessage(
        QStringLiteral("지적 번지는 축척 1:5000 이하에서 보입니다. 현재 1:%1")
            .arg(m_canvas ? m_canvas->scale() : 0, 0, 'f', 0),
        12000);
  }
#endif
}

void MainWindow::addBasemapOsm() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addOsmBasemap(QgsProject::instance(), m_canvas, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("OSM"));
#endif
}

void MainWindow::addBasemapGoogle() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::GoogleSatellite, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("Google 위성"));
#endif
}

void MainWindow::loadSurveyLayers(const QString& gpkgOrStub) {
#if KA_HGIS_HAS_QGIS
  if (gpkgOrStub.endsWith(QLatin1String(".stub"))) return;
  QgsProject* proj = QgsProject::instance();
  LayerOps::removeSurveyDomainLayers(proj);
  proj->setCrs(QgsCoordinateReferenceSystem(m_workCrs));
  if (m_canvas) m_canvas->setDestinationCrs(QgsCoordinateReferenceSystem(m_workCrs));
  m_surveyPath = gpkgOrStub;
  LayoutService::ensureDefaultLayouts(proj);
  LayerOps::pruneEmptyLegendGroups(proj);
  if (m_canvas) {
    LayerOps::zoomToKorea(m_canvas, m_workCrs);
    m_canvas->refresh();
  }
#else
  Q_UNUSED(gpkgOrStub);
#endif
}



#if KA_HGIS_HAS_QGIS
QgsVectorLayer* MainWindow::layerByKey(const QString& layerKey) const {
  return LayerOps::findByLayerKey(QgsProject::instance(), layerKey);
}

QgsVectorLayer* MainWindow::ensureDomainLayerForEdit(const QString& layerKey, const QString& titleKo) {
  QString err;
  auto* vl = LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath, layerKey, titleKo, &err);
  if (!vl) {
    const auto ans = QMessageBox::question(
        this, QStringLiteral("레이어"),
        QStringLiteral("%1\n\n지금 「새 조사」를 만들까요?")
            .arg(err.isEmpty() ? QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.") : err),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans == QMessageBox::Yes)
      newSurvey();
    vl = LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath, layerKey, titleKo, &err);
    if (!vl) return nullptr;
  }
  if (layerKey == QLatin1String("control_points"))
    LayerOps::ensureControlPointQualityFields(vl);
  if (layerKey == QLatin1String("feature_poly"))
    LayerOps::applyFeaturePolyStyle(vl);
  if (m_canvas) m_canvas->refresh();
  return vl;
}

void MainWindow::onLayerTreeRowsMoved() {
  if (!m_canvas) return;
  m_canvas->refreshAllLayers();
  m_canvas->refresh();
}

void MainWindow::stopCaptureTool() {
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    m_canvas->unsetMapTool(m_captureTool);
  if (m_panTool)
    m_canvas->setMapTool(m_panTool);
  if (m_captureTool)
    m_captureTool->resetSession();
}

void MainWindow::onGeometryCaptured(const QgsGeometry& geom) {
  try {
    QgsVectorLayer* layer = m_editLayer;
    if (!layer || !layer->isValid()) {
      statusBar()->showMessage(QStringLiteral("편집 레이어 없음 — 새 조사 후 다시 그리기"), 5000);
      return;
    }
    if (geom.isEmpty()) {
      statusBar()->showMessage(QStringLiteral("빈 도형 (면은 점 3개 이상, 선은 2개 이상)"), 5000);
      return;
    }
    if (!layer->isEditable()) {
      if (!layer->startEditing()) {
        QMessageBox::warning(this, QStringLiteral("편집"), QStringLiteral("편집 모드 실패"));
        return;
      }
    }

    QgsFeature feat(layer->fields());
    feat.setGeometry(geom);

    const QString layerKey = LayerOps::layerKeyOf(layer);
    if (layerKey == QLatin1String("feature_poly") || layerKey == QLatin1String("feature_line")) {
      bool ok = false;
      const QString kind = QInputDialog::getText(this, QStringLiteral("유구 속성"),
          QStringLiteral("종류(필수) 예: 수혈주거지"), QLineEdit::Normal, QString(), &ok);
      if (!ok || kind.trimmed().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("종류 미입력 — 도형 저장 안 함"), 4000);
        return;
      }
      const QString period = QInputDialog::getText(this, QStringLiteral("유구 속성"),
          QStringLiteral("시대(필수) 예: 청동기"), QLineEdit::Normal, QString(), &ok);
      if (!ok || period.trimmed().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("시대 미입력 — 도형 저장 안 함"), 4000);
        return;
      }
      const int ik = layer->fields().indexOf(QStringLiteral("kind"));
      const int ip = layer->fields().indexOf(QStringLiteral("period"));
      if (ik >= 0) feat.setAttribute(ik, kind.trimmed());
      if (ip >= 0) feat.setAttribute(ip, period.trimmed());
    } else if (layerKey == QLatin1String("survey_area")) {
      const QString sn = QInputDialog::getText(this, QStringLiteral("조사구역"), QStringLiteral("조사명(선택)"));
      const int isn = layer->fields().indexOf(QStringLiteral("survey_name"));
      if (isn >= 0 && !sn.trimmed().isEmpty()) feat.setAttribute(isn, sn.trimmed());
    }

    if (!layer->addFeature(feat)) {
      QMessageBox::warning(this, QStringLiteral("오류"),
                           QStringLiteral("피처 추가 실패\n%1").arg(layer->commitErrors().join(QLatin1Char('\n'))));
      return;
    }
    layer->triggerRepaint();
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("도형 추가됨 → 「편집저장」 누르세요. 계속 그리려면 지도 좌클릭."), 8000);
  } catch (const std::exception& ex) {
    QMessageBox::critical(this, QStringLiteral("그리기 오류"), QString::fromUtf8(ex.what()));
  } catch (...) {
    QMessageBox::critical(this, QStringLiteral("그리기 오류"), QStringLiteral("알 수 없는 오류"));
  }
}

void MainWindow::beginEdit(QgsVectorLayer* layer) {
  try {
    if (!layer || !layer->isValid()) {
      QMessageBox::warning(this, QStringLiteral("알림"),
                           QStringLiteral("먼저 「새 조사」로 프로젝트를 만드세요."));
      return;
    }
    if (!m_canvas) return;

    if (!layer->startEditing()) {
      if (!layer->isEditable()) {
        QMessageBox::warning(this, QStringLiteral("편집"),
                             QStringLiteral("편집 불가: %1").arg(layer->name()));
        return;
      }
    }

    m_editLayer = layer;

    QgsSnappingConfig snap = QgsProject::instance()->snappingConfig();
    snap.setEnabled(false);
    QgsProject::instance()->setSnappingConfig(snap);

    KaCaptureMapTool::Mode mode = KaCaptureMapTool::Mode::Polygon;
    const Qgis::GeometryType gt = layer->geometryType();
    if (gt == Qgis::GeometryType::Line) mode = KaCaptureMapTool::Mode::Line;
    else if (gt == Qgis::GeometryType::Point) mode = KaCaptureMapTool::Mode::Point;

    if (!m_captureTool) {
      m_captureTool = new KaCaptureMapTool(m_canvas);
      m_captureTool->setParent(this);
      connect(m_captureTool, &KaCaptureMapTool::geometryCaptured, this, &MainWindow::onGeometryCaptured,
              Qt::QueuedConnection);
      connect(m_captureTool, &KaCaptureMapTool::captureCanceled, this, [this]() {
        statusBar()->showMessage(QStringLiteral("그리기 취소 (점 부족 / ESC). 면≥3점, 선≥2점"), 5000);
      });
    }
    if (m_canvas->mapTool() == m_captureTool)
      m_canvas->unsetMapTool(m_captureTool);

    m_captureTool->setMode(mode);
    m_captureTool->setTargetLayer(layer);
    m_canvas->setMapTool(m_captureTool);
    m_canvas->setFocus(Qt::OtherFocusReason);

    const QString how = (mode == KaCaptureMapTool::Mode::Point)
                            ? QStringLiteral("클릭=점")
                            : QStringLiteral("좌클릭=점추가 / 우클릭·Enter=완료 / ESC=취소");
    statusBar()->showMessage(QStringLiteral("그리기: %1 | %2").arg(layer->name(), how), 0);
  } catch (const std::exception& ex) {
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QString::fromUtf8(ex.what()));
  } catch (...) {
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QStringLiteral("내부 오류"));
  }
}
#endif

void MainWindow::startEditSurveyArea() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("survey_area"), QStringLiteral("조사구역")));
#else
  m_stubSurveyArea++;
  statusBar()->showMessage(QStringLiteral("스텁: 조사구역 폴리곤 %1개").arg(m_stubSurveyArea));
#endif
}
void MainWindow::startEditFeaturePoly() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("유구면")));
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 유구 %1").arg(m_stubFeatures));
#endif
}
void MainWindow::startEditFeatureLine() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("feature_line"), QStringLiteral("유구선")));
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 선 %1").arg(m_stubFeatures));
#endif
}

void MainWindow::mergeFeaturePolygons() {
#if KA_HGIS_HAS_QGIS
  auto* fp = ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("유구면"));
  if (!fp) return;
  QString err;
  if (!LayerOps::mergePolygonFeatures(fp, &err)) {
    QMessageBox::warning(this, QStringLiteral("폴리곤 묶기"), err);
    return;
  }
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(
      QStringLiteral("유구면 폴리곤을 1개(멀티폴리곤)로 묶었습니다 · 제출 시 feature_poly.shp 한 파일"),
      10000);
  QMessageBox::information(
      this, QStringLiteral("폴리곤 묶기"),
      QStringLiteral("여러 폴리곤을 하나의 지오메트리로 합쳤습니다.\n"
                     "문화재 인트라넷 제출 시 「SHP내보내기」하면\n"
                     "feature_poly.shp 한 파일(EPSG:5179)로 등록하면 됩니다."));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 폴리곤 묶기"), 3000);
#endif
}
void MainWindow::saveEdits() {
#if KA_HGIS_HAS_QGIS
  int n = 0;
  for (auto* l : QgsProject::instance()->mapLayers()) {
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable()) {
        if (v->commitChanges()) ++n;
        else {
          QMessageBox::warning(this, QStringLiteral("저장 실패"),
                               QStringLiteral("%1: %2").arg(v->name(), v->commitErrors().join(QStringLiteral("; "))));
        }
      }
    }
  }
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("편집저장 완료 (%1개 레이어)").arg(n), 5000);
#else
  statusBar()->showMessage(QStringLiteral("스텁 저장"), 3000);
#endif
}
void MainWindow::stopEdits() {
#if KA_HGIS_HAS_QGIS
  stopCaptureTool();
  m_editLayer = nullptr;
#endif
  statusBar()->showMessage(QStringLiteral("그리기 종료. 미커밋은 「편집저장」"), 5000);
}

void MainWindow::addControlPoint() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("GPS 기준점"));
  auto* form = new QFormLayout(&dlg);
  auto* id = new QLineEdit(&dlg);
  auto* x = new QLineEdit(&dlg);
  auto* y = new QLineEdit(&dlg);
  auto* datum = new QLineEdit(QStringLiteral("세계측지계"), &dlg);
  auto* ell = new QLineEdit(QStringLiteral("GRS80"), &dlg);
  auto* proj = new QLineEdit(QStringLiteral("TM/UTM-K"), &dlg);
  auto* origin = new QLineEdit(&dlg);
  auto* acc = new QLineEdit(QStringLiteral("1.0"), &dlg);
  auto* pdop = new QLineEdit(QStringLiteral("1.5"), &dlg);
  auto* fix = new QLineEdit(QStringLiteral("RTK"), &dlg);
  form->addRow(QStringLiteral("점ID"), id);
  form->addRow(QStringLiteral("X"), x);
  form->addRow(QStringLiteral("Y"), y);
  form->addRow(QStringLiteral("측지기준계"), datum);
  form->addRow(QStringLiteral("타원체"), ell);
  form->addRow(QStringLiteral("투영"), proj);
  form->addRow(QStringLiteral("원점"), origin);
  form->addRow(QStringLiteral("accuracy_m"), acc);
  form->addRow(QStringLiteral("PDOP"), pdop);
  form->addRow(QStringLiteral("fix_type"), fix);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  if (id->text().isEmpty() || x->text().isEmpty() || y->text().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("입력"), QStringLiteral("점ID/X/Y 필수"));
    return;
  }
  m_stubHasMeta = !datum->text().isEmpty() && !ell->text().isEmpty() && !proj->text().isEmpty();
#if KA_HGIS_HAS_QGIS
  auto* layer = ensureDomainLayerForEdit(QStringLiteral("control_points"), QStringLiteral("GPS기준점"));
  if (layer && layer->startEditing()) {
    QgsFeature f(layer->fields());
    f.setAttribute(QStringLiteral("point_id"), id->text());
    f.setAttribute(QStringLiteral("x"), x->text().toDouble());
    f.setAttribute(QStringLiteral("y"), y->text().toDouble());
    f.setAttribute(QStringLiteral("datum"), datum->text());
    f.setAttribute(QStringLiteral("ellipsoid"), ell->text());
    f.setAttribute(QStringLiteral("projection"), proj->text());
    f.setAttribute(QStringLiteral("origin"), origin->text());
    f.setAttribute(QStringLiteral("accuracy"), acc->text());
    f.setAttribute(QStringLiteral("accuracy_m"), acc->text().toDouble());
    f.setAttribute(QStringLiteral("pdop"), pdop->text().toDouble());
    f.setAttribute(QStringLiteral("fix_type"), fix->text());
    QgsPointXY pt(x->text().toDouble(), y->text().toDouble());
    f.setGeometry(QgsGeometry::fromPointXY(pt));
    layer->addFeature(f);
    layer->commitChanges();
    m_stubGcp = layer->featureCount();
  } else
#endif
  { m_stubGcp++; }
  statusBar()->showMessage(QStringLiteral("기준점 등록 (총 추정 %1)").arg(m_stubGcp), 4000);
}

void MainWindow::importControlCsv() {
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("CSV"), QString(), QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) return;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QTextStream ts(&f);
  int n = 0;
  while (!ts.atEnd()) {
    const QString line = ts.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#')) continue;
    if (line.toLower().startsWith(QLatin1String("id")) || line.toLower().startsWith(QLatin1String("point"))) continue;
    const QStringList p = line.split(QRegularExpression(QStringLiteral("[,;\\t]")));
    if (p.size() >= 3) n++;
  }
  m_stubGcp += n;
  m_stubHasMeta = true;
  statusBar()->showMessage(QStringLiteral("CSV에서 %1점 가져옴 (합 %2)").arg(n).arg(m_stubGcp), 5000);
}

QJsonObject MainWindow::buildProjectState() const {
#if KA_HGIS_HAS_QGIS
  return ProjectStateBuilder::fromProject(QgsProject::instance());
#else
  QJsonObject st = ProjectStateBuilder::empty();
  st.insert(QStringLiteral("survey_area_count"), m_stubSurveyArea);
  st.insert(QStringLiteral("control_points_count"), m_stubGcp);
  st.insert(QStringLiteral("feature_poly_count"), m_stubFeatures);
  st.insert(QStringLiteral("project_crs_set"), true);
  st.insert(QStringLiteral("has_datum"), m_stubHasMeta);
  st.insert(QStringLiteral("has_ellipsoid"), m_stubHasMeta);
  st.insert(QStringLiteral("has_projection"), m_stubHasMeta);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("survey_is_polygon"), m_stubSurveyArea > 0);
  return st;
#endif
}

void MainWindow::runChecklist() {
  if (m_checklist->ruleCount() == 0) m_checklist->loadRules(rulesPath());
  const auto results = m_checklist->evaluate(buildProjectState());
  QString html = QStringLiteral("<b>검수 결과</b><br/>");
  int err = 0, warn = 0;
  for (const auto& r : results) {
    if (r.passed) continue;
    if (r.severity == QLatin1String("error")) err++; else warn++;
    html += QStringLiteral("<span style='color:%1'>[%2] %3</span><br/>")
              .arg(r.severity == QLatin1String("error") ? QStringLiteral("red") : QStringLiteral("orange"),
                   r.severity, r.messageKo);
  }
  if (err == 0 && warn == 0) html += QStringLiteral("<span style='color:green'>모두 통과</span>");
  m_checkView->setText(html);
  statusBar()->showMessage(QStringLiteral("검수: error %1 / warn %2").arg(err).arg(warn), 6000);
}

void MainWindow::exportPdf() {
#if KA_HGIS_HAS_QGIS
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
  const QString layoutName = QInputDialog::getItem(
      this, QStringLiteral("도면 PDF"), QStringLiteral("레이아웃"),
      LayoutService::defaultLayoutNames(), 0, false);
  if (layoutName.isEmpty()) return;
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("PDF"), layoutName + QStringLiteral(".pdf"), QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QString err;
  if (ExportService::writePdfViaLayout(QgsProject::instance(), layoutName, path, &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("PDF"), err);
  else
    statusBar()->showMessage(QStringLiteral("PDF: %1").arg(path), 5000);
#else
  QMessageBox::warning(this, QStringLiteral("PDF"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::exportShpPackage() {
  const auto results = m_checklist->evaluate(buildProjectState());
  bool hasErr = false;
  QString summary;
  for (const auto& r : results) {
    if (!r.passed) {
      summary += QStringLiteral("- [%1] %2\n").arg(r.severity, r.messageKo);
      if (r.severity == QLatin1String("error")) hasErr = true;
    }
  }
  if (summary.isEmpty()) summary = QStringLiteral("OK\n");
  const QString enc = QInputDialog::getItem(this, QStringLiteral("인코딩"), QStringLiteral("SHP 인코딩"),
                                      {QStringLiteral("UTF-8"), QStringLiteral("EUC-KR")}, 0, false);
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("제출 폴더"));
  if (dir.isEmpty()) return;
  QString err;
#if KA_HGIS_HAS_QGIS
  // Phase-1: do not block package export on checklist errors (tool, not gate).
  const QString out = ExportService::exportSubmissionPackage(
      QgsProject::instance(), dir, enc, summary, /*blockOnError=*/false, hasErr, &err);
#else
  const QString out;
  err = QStringLiteral("QGIS required");
#endif
  if (out.isEmpty())
    QMessageBox::warning(this, QStringLiteral("내보내기"), err);
  else {
    m_packageCreated = true;
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
  }
}

void MainWindow::crsDefineOnly() {
  QMessageBox::warning(this, QStringLiteral("위험"),
    QStringLiteral("「이름만 지정」은 좌표값을 바꾸지 않습니다.\n실제 이동이 필요하면 「좌표 변환」을 쓰세요."));
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* l = qobject_cast<QgsVectorLayer*>(cur);
  if (!l) {
    statusBar()->showMessage(QStringLiteral("CRS 이름만 지정 — 벡터 레이어를 선택하세요"), 5000);
    return;
  }
  const QString auth = QInputDialog::getText(this, QStringLiteral("CRS 이름만 지정"),
      QStringLiteral("EPSG 코드 (예: EPSG:5179)"), QLineEdit::Normal, QStringLiteral("EPSG:5179"));
  if (auth.isEmpty()) return;
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) {
    QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("잘못된 CRS"));
    return;
  }
  l->setCrs(crs);
  statusBar()->showMessage(QStringLiteral("CRS 라벨만 변경: %1 (좌표 미변환)").arg(auth), 6000);
#endif
}
void MainWindow::crsReproject() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    QMessageBox::information(this, QStringLiteral("좌표 변환"), QStringLiteral("레이어 트리에서 벡터 레이어를 선택하세요."));
    return;
  }
  const QString auth = QInputDialog::getText(this, QStringLiteral("좌표 변환(재투영)"),
      QStringLiteral("대상 CRS"), QLineEdit::Normal, QStringLiteral("EPSG:4326"));
  if (auth.isEmpty()) return;
  const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("재투영 저장"),
      vl->name() + QStringLiteral("_reproj.gpkg"), QStringLiteral("GPKG (*.gpkg);;SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::reprojectVectorLayer(vl, auth, out, QgsProject::instance(), &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("재투영 실패"), err);
  else {
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("재투영 완료: %1").arg(out), 6000);
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}
void MainWindow::georefAssistant() {
  const QString img = QFileDialog::getOpenFileName(this, QStringLiteral("스캔 평면도"), QString(),
      QStringLiteral("Images (*.png *.jpg *.jpeg *.tif *.tiff)"));
  if (img.isEmpty()) return;
#if KA_HGIS_HAS_QGIS
  auto* cp = layerByKey(QStringLiteral("control_points"));
  const int n = cp ? int(cp->featureCount()) : m_stubGcp;
  if (n < 2) {
    QMessageBox::warning(this, QStringLiteral("GCP"), QStringLiteral("기준점(GCP)이 2개 미만입니다. 먼저 등록하세요."));
    return;
  }
  QString err;
  const QString wf = LayerOps::georeferenceImageSimple(img, cp, QgsProject::instance(), m_canvas, &err);
  if (wf.isEmpty()) QMessageBox::warning(this, QStringLiteral("지오레퍼런스"), err);
  else statusBar()->showMessage(QStringLiteral("월드파일 작성: %1 (간단 아핀 — 정밀작업은 추가 GCP 권장)").arg(wf), 8000);
#else
  Q_UNUSED(img);
#endif
}

void MainWindow::openVectorLayer() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("SHP/벡터 추가"), QString(),
      QStringLiteral("Vector (*.shp *.gpkg *.geojson)"));
  if (path.isEmpty()) return;
  const QString title = QFileInfo(path).completeBaseName();
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    QMessageBox::warning(this, QStringLiteral("오류"),
                         QStringLiteral("열 수 없음: %1").arg(layer->error().message()));
    delete layer;
    return;
  }
  LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
  LayerOps::applyLegendCrsLabel(layer);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  QgsProject::instance()->addMapLayer(layer, true);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::zoomToLayerMax(m_canvas, layer);
  }
  statusBar()->showMessage(
      QStringLiteral("레이어 추가: %1 (원본 %2 → 화면 OTF %3)")
          .arg(title, layer->crs().authid(), m_workCrs),
      8000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("벡터 로드 시뮬레이션"));
#endif
}
void MainWindow::saveProject() {
#if KA_HGIS_HAS_QGIS
  saveEdits();
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("저장"), QString(), QStringLiteral("QGIS (*.qgz *.qgs)"));
  if (!path.isEmpty()) QgsProject::instance()->write(path);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 저장 시뮬레이션"));
#endif
}
void MainWindow::openProject() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("열기"), QString(), QStringLiteral("QGIS (*.qgz *.qgs)"));
  if (path.isEmpty()) return;
  if (!QgsProject::instance()->read(path)) {
    QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("프로젝트를 열 수 없습니다."));
    return;
  }
  if (auto* cp = layerByKey(QStringLiteral("control_points")))
    LayerOps::ensureControlPointQualityFields(cp);
  if (auto* fp = layerByKey(QStringLiteral("feature_poly")))
    LayerOps::applyFeaturePolyStyle(fp);
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(
      QStringLiteral("프로젝트 열림 · OTF · 작업 CRS %1").arg(m_workCrs), 8000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 열기 시뮬레이션"));
#endif
}
void MainWindow::runLocationSearch() {
  if (!m_locator || !m_searchEdit) return;
  const QString q = m_searchEdit->text().trimmed();
  if (q.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("주소·지번·지역·상호를 입력하세요"), 4000);
    return;
  }
  statusBar()->showMessage(QStringLiteral("위치 검색 중… %1").arg(q), 0);
  m_searchEdit->setEnabled(false);
  m_locator->search(q);
}

void MainWindow::onLocationFailed(const QString& message) {
  if (m_searchEdit) m_searchEdit->setEnabled(true);
  statusBar()->showMessage(message, 8000);
  QMessageBox::information(this, QStringLiteral("위치 검색"), message);
}

void MainWindow::onLocationResults(const QVector<LocationHit>& hits) {
  if (m_searchEdit) m_searchEdit->setEnabled(true);
  if (hits.isEmpty()) {
    onLocationFailed(QStringLiteral("검색 결과 없음"));
    return;
  }
  if (hits.size() == 1) {
    zoomToLocation(hits.first());
    return;
  }
  QStringList labels;
  for (const LocationHit& h : hits) {
    QString line = h.title;
    if (!h.detail.isEmpty()) line += QStringLiteral("  —  ") + h.detail;
    labels << line;
  }
  bool ok = false;
  const QString pick = QInputDialog::getItem(
      this, QStringLiteral("위치 선택"),
      QStringLiteral("검색 결과 %1건 — 이동할 위치를 선택하세요").arg(hits.size()),
      labels, 0, false, &ok);
  if (!ok) return;
  const int idx = labels.indexOf(pick);
  if (idx >= 0 && idx < hits.size())
    zoomToLocation(hits.at(idx));
}

void MainWindow::zoomToLocation(const LocationHit& hit) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest(m_workCrs);
  try {
    const QgsCoordinateTransform xf(wgs, dest, QgsProject::instance()
                                                   ? QgsProject::instance()->transformContext()
                                                   : QgsCoordinateTransformContext());
    if (hit.hasBbox) {
      QgsRectangle r(hit.west, hit.south, hit.east, hit.north);
      r = xf.transformBoundingBox(r);
      r.scale(1.4);
      m_canvas->setExtent(r);
    } else {
      const QgsPointXY p = xf.transform(QgsPointXY(hit.lon, hit.lat));
      const double pad = dest.authid().contains(QLatin1String("4326")) ? 0.01 : 400.0;
      m_canvas->setExtent(QgsRectangle(p.x() - pad, p.y() - pad, p.x() + pad, p.y() + pad));
    }
    m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("이동: %1").arg(hit.title), 8000);
  } catch (const QgsCsException& e) {
    QMessageBox::warning(this, QStringLiteral("좌표 변환"), e.what());
  } catch (...) {
    QMessageBox::warning(this, QStringLiteral("위치"), QStringLiteral("좌표 변환 실패"));
  }
#else
  Q_UNUSED(hit);
#endif
}

void MainWindow::configureVworldKey() {
  bool ok = false;
  const QString cur = VworldSettings::loadApiKey();
  const QString key = QInputDialog::getText(
      this, QStringLiteral("VWorld API 키"),
      QStringLiteral("vworld.kr 인증키 (SSOT: VWorld/ApiKey)\n배경지도·검색 공통"),
      QLineEdit::Normal, cur, &ok);
  if (!ok) return;
  VworldSettings::saveApiKey(key);
  statusBar()->showMessage(key.isEmpty()
      ? QStringLiteral("VWorld 키 삭제됨")
      : QStringLiteral("VWorld API 키 저장됨 — 다시 묻지 않습니다"), 6000);
}

void MainWindow::setupFileBrowser() {
  m_fsModel = new QFileSystemModel(this);
  m_fsModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Drives);
  m_fsModel->setNameFilters({QStringLiteral("*.shp"), QStringLiteral("*.gpkg"),
                             QStringLiteral("*.geojson"), QStringLiteral("*.json"),
                             QStringLiteral("*.tif"), QStringLiteral("*.tiff")});
  m_fsModel->setNameFilterDisables(false);
  m_fileBrowser = new QTreeView(this);
  m_fileBrowser->setObjectName(QStringLiteral("fileBrowser"));
  m_fileBrowser->setModel(m_fsModel);
  m_fileBrowser->setHeaderHidden(true);
  for (int c = 1; c < m_fsModel->columnCount(); ++c)
    m_fileBrowser->hideColumn(c);
  m_fileBrowser->setDragEnabled(true);
  m_fileBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_fileBrowser->setAnimated(false);
  m_fileBrowser->setIndentation(18);
  m_fileBrowser->setUniformRowHeights(true);
  m_fileBrowser->setTextElideMode(Qt::ElideMiddle);
  m_fileBrowser->setStyleSheet(QStringLiteral(
      "QTreeView { background: #f7fbff; color: #0f172a; border: none; border-radius: 8px;"
      " font-size: 13px; outline: none; }"
      "QTreeView::item { color: #0f172a; padding: 3px 4px; min-height: 22px; }"
      "QTreeView::item:selected { background: #bfdbfe; color: #0f172a; }"
      "QTreeView::item:hover { background: #e0f2fe; color: #0f172a; }"));
  {
    QPalette pal = m_fileBrowser->palette();
    pal.setColor(QPalette::Base, QColor(0xf7, 0xfb, 0xff));
    pal.setColor(QPalette::Text, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::WindowText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::BrightText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::HighlightedText, QColor(0x0f, 0x17, 0x2a));
    pal.setColor(QPalette::Highlight, QColor(0xbf, 0xdb, 0xfe));
    m_fileBrowser->setPalette(pal);
  }
  connect(m_fileBrowser, &QTreeView::doubleClicked, this, &MainWindow::onFileBrowserActivated);

  QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if (start.isEmpty() || !QDir(start).exists())
    start = QDir::homePath();
  if (start.isEmpty() || !QDir(start).exists())
    start = QStringLiteral("C:/");
  m_fsModel->setRootPath(start);
  goFileBrowserRoot(start);
}

void MainWindow::goFileBrowserRoot(const QString& path) {
  if (!m_fsModel || !m_fileBrowser) return;
  QString p = QDir::fromNativeSeparators(path.trimmed());
  if (p.isEmpty()) {
    m_fsModel->setRootPath(QStringLiteral(""));
    m_fileBrowser->setRootIndex(m_fsModel->index(QStringLiteral("")));
    statusBar()->showMessage(QStringLiteral("내 PC 드라이브 목록 — 폴더를 열어 SHP/GPKG를 찾으세요"), 8000);
    return;
  }
  if (p.length() == 2 && p[1] == QLatin1Char(':'))
    p += QLatin1Char('/');
  if (!QDir(p).exists())
    p = QStringLiteral("C:/");
  m_fsModel->setRootPath(p);
  const QModelIndex idx = m_fsModel->index(p);
  if (idx.isValid()) {
    m_fileBrowser->setRootIndex(idx);
    m_fileBrowser->expand(idx);
  }
  statusBar()->showMessage(
      QStringLiteral("경로: %1  |  SHP·GPKG 더블클릭 또는 아래「지도 레이어」로 드래그")
          .arg(QDir::toNativeSeparators(p)),
      10000);
}

void MainWindow::browseDataFolder() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("조사 데이터 폴더 선택"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  if (!dir.isEmpty())
    goFileBrowserRoot(dir);
}

void MainWindow::onFileBrowserActivated(const QModelIndex& index) {
  if (!m_fsModel) return;
  const QString path = m_fsModel->filePath(index);
  if (path.isEmpty()) return;
  if (m_fsModel->isDir(index)) {
    goFileBrowserRoot(path);
    return;
  }
  if (!addVectorFromPath(path))
    QMessageBox::warning(this, QStringLiteral("파일"),
                         QStringLiteral("지도 레이어로 열 수 없습니다 (SHP/GPKG/GeoJSON):\n%1").arg(path));
}

bool MainWindow::tryAddDroppedUrls(const QList<QUrl>& urls) {
  int n = 0;
  for (const QUrl& u : urls) {
    if (!u.isLocalFile()) continue;
    const QString path = u.toLocalFile();
    const QString low = path.toLower();
    if (!(low.endsWith(QLatin1String(".shp")) || low.endsWith(QLatin1String(".gpkg")) ||
          low.endsWith(QLatin1String(".geojson")) || low.endsWith(QLatin1String(".json")) ||
          low.endsWith(QLatin1String(".tif")) || low.endsWith(QLatin1String(".tiff"))))
      continue;
    if (addVectorFromPath(path))
      ++n;
  }
  if (n > 0)
    statusBar()->showMessage(QStringLiteral("레이어 %1개 추가됨 (파일→지도)").arg(n), 5000);
  return n > 0;
}

bool MainWindow::addVectorFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    delete layer;
    return false;
  }
  LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
  LayerOps::applyLegendCrsLabel(layer);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  QgsProject::instance()->addMapLayer(layer, true);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::zoomToLayerMax(m_canvas, layer);
  }
  statusBar()->showMessage(
      QStringLiteral("레이어 추가: %1 (원본 %2 → 화면 OTF %3)")
          .arg(title, layer->crs().authid(), m_workCrs),
      8000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

void MainWindow::exportReportLayout() {
#if KA_HGIS_HAS_QGIS
  const QStringList papers = {QStringLiteral("A4 세로"), QStringLiteral("A4 가로"),
                              QStringLiteral("A3 세로"), QStringLiteral("A3 가로")};
  bool ok = false;
  const QString pick = QInputDialog::getItem(this, QStringLiteral("조판 PDF"),
      QStringLiteral("용지 · 방향"), papers, 0, false, &ok);
  if (!ok || pick.isEmpty()) return;
  const QString title = QInputDialog::getText(this, QStringLiteral("조판 PDF"),
      QStringLiteral("도면 제목"), QLineEdit::Normal, QStringLiteral("조사도면"), &ok);
  if (!ok) return;
  LayoutService::Paper paper = LayoutService::Paper::A4;
  LayoutService::Orientation ori = LayoutService::Orientation::Portrait;
  if (pick.startsWith(QLatin1String("A3"))) paper = LayoutService::Paper::A3;
  if (pick.contains(QStringLiteral("가로"))) ori = LayoutService::Orientation::Landscape;
  QString err;
  const QString layoutName = LayoutService::createReportLayout(
      QgsProject::instance(), title.trimmed().isEmpty() ? QStringLiteral("조사도면") : title.trimmed(),
      paper, ori, &err);
  if (layoutName.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("조판"), err);
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("PDF 저장"), layoutName + QStringLiteral(".pdf"), QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  if (ExportService::writePdfViaLayout(QgsProject::instance(), layoutName, path, &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("PDF"), err);
  else
    statusBar()->showMessage(QStringLiteral("조판 PDF: %1").arg(path), 6000);
#else
  QMessageBox::warning(this, QStringLiteral("조판"), QStringLiteral("QGIS 빌드 필요"));
#endif
}


void MainWindow::showAbout() {
  QMessageBox::about(this, QStringLiteral("정보"),
    QStringLiteral("고고학 전용 HGIS (ka-hgis) 0.3.0\n"
                   "C++/Qt6 + QGIS 4.x libraries\n"
                   "작업 CRS: 5186/5187 | 업로드: 5179\n"
                   "위치검색: 주소·지번·지역·상호\n"
                   "License: GNU GPLv2 or later"));
}



