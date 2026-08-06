#include "MainWindow.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/LocationSearch.h"
#include "core/FeatureWriteService.h"

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
  setStepTools(0);
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
  add(bg, KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
      [this]() { addBasemapGoogle(); });
  add(bg, KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM"), [this]() {
#if KA_HGIS_HAS_QGIS
    QString err;
    if (!LayerOps::addOsmBasemap(QgsProject::instance(), m_canvas, &err))
      QMessageBox::warning(this, QStringLiteral("배경"), err);
    else if (m_canvas) LayerOps::zoomToKorea(m_canvas, m_workCrs);
#endif
  });

  auto* tools = menuBar()->addMenu(KaIcons::icon(QStringLiteral("check")), QStringLiteral("도구"));
  add(tools, KaIcons::icon(QStringLiteral("georef")), QStringLiteral("스캔 평면도 맞추기…"),
      [this]() { georefAssistant(); });
  add(tools, KaIcons::icon(QStringLiteral("palette")), QStringLiteral("유구 스타일(종류)"), [this]() {
#if KA_HGIS_HAS_QGIS
    if (auto* fp = layerByName(QStringLiteral("feature_poly"))) {
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
  add(help, KaIcons::icon(QStringLiteral("help")), QStringLiteral("초보자 가이드"),
      [this]() { showBeginnerGuide(); });
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
  mainTb->setIconSize(QSize(28, 28));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->addAction(KaIcons::icon(QStringLiteral("new")), QStringLiteral("새 조사"), this, &MainWindow::newSurvey);
  mainTb->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("열기"), this, &MainWindow::openProject);
  mainTb->addAction(KaIcons::icon(QStringLiteral("save")), QStringLiteral("저장"), this, &MainWindow::saveProject);
  mainTb->addSeparator();
  mainTb->addAction(KaIcons::icon(QStringLiteral("layer")), QStringLiteral("벡터"), this, &MainWindow::openVectorLayer);
  mainTb->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("VWorld"), this, &MainWindow::addBasemapVworld);
  mainTb->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("위성"), this, &MainWindow::addBasemapGoogle);
  mainTb->addSeparator();
  mainTb->addAction(KaIcons::icon(QStringLiteral("draw_area")), QStringLiteral("구역그리기"), this, &MainWindow::startEditSurveyArea);
  mainTb->addAction(KaIcons::icon(QStringLiteral("draw_poly")), QStringLiteral("유구면그리기"), this, &MainWindow::startEditFeaturePoly);
  mainTb->addAction(KaIcons::icon(QStringLiteral("draw_line")), QStringLiteral("선그리기"), this, &MainWindow::startEditFeatureLine);
  mainTb->addAction(KaIcons::icon(QStringLiteral("gps")), QStringLiteral("GPS"), this, &MainWindow::addControlPoint);
  mainTb->addSeparator();
  mainTb->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("검수"), this, &MainWindow::runChecklist);
  mainTb->addAction(KaIcons::icon(QStringLiteral("upload")), QStringLiteral("5179변환"), this, &MainWindow::convertSelectedTo5179);
  mainTb->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("PDF"), this, &MainWindow::exportPdf);
  mainTb->addAction(KaIcons::icon(QStringLiteral("export")), QStringLiteral("제출"), this, &MainWindow::exportShpPackage);
  mainTb->addAction(KaIcons::icon(QStringLiteral("trash")), QStringLiteral("삭제"), this, &MainWindow::removeSelectedLayers);
}

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  auto* layout = new QHBoxLayout(central);

  m_steps = new QListWidget(central);
  m_steps->setObjectName(QStringLiteral("stepRail"));
  m_steps->setIconSize(QSize(32, 32));
  m_steps->setSpacing(4);
  const QStringList stepLabels = {
    QStringLiteral("새 조사 만들기"),
    QStringLiteral("배경·지적 불러오기"),
    QStringLiteral("조사구역 그리기"),
    QStringLiteral("유구 / 단면선"),
    QStringLiteral("GPS 기준점"),
    QStringLiteral("도면 검수"),
    QStringLiteral("제출 패키지")
  };
  for (int i = 0; i < stepLabels.size(); ++i) {
    auto* it = new QListWidgetItem(KaIcons::step(i + 1), stepLabels[i]);
    it->setSizeHint(QSize(200, 44));
    m_steps->addItem(it);
  }
  m_steps->setFixedWidth(230);
  m_steps->setCurrentRow(0);
  connect(m_steps, &QListWidget::currentRowChanged, this, &MainWindow::onStepChanged);
  layout->addWidget(m_steps);

  auto* centerCol = new QVBoxLayout();
  m_stepTools = new QToolBar(QStringLiteral("단계도구"), central);
  m_stepTools->setObjectName(QStringLiteral("stepTools"));
  m_stepTools->setIconSize(QSize(28, 28));
  m_stepTools->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  centerCol->addWidget(m_stepTools);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  m_canvas->setCanvasColor(QColor(245, 247, 250));
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  m_canvas->setParallelRenderingEnabled(true);
  m_canvas->setMapUpdateInterval(120);
  m_canvas->setPreviewJobsEnabled(true);
  m_canvas->setSegmentationTolerance(2.0);
  const QgsCoordinateReferenceSystem crs(m_workCrs);
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_panTool = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_panTool);

  auto* treeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new QgsLayerTreeModel(treeRoot, this);
  m_layerTree = new QgsLayerTreeView(central);
  m_layerTree->setObjectName(QStringLiteral("layerTree"));
  m_layerTree->setModel(model);
  m_layerTree->setFixedWidth(220);
  m_layerTree->setFocusPolicy(Qt::StrongFocus);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragDropMode(QAbstractItemView::InternalMove);
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  new QgsLayerTreeMapCanvasBridge(treeRoot, m_canvas, this);

  auto* delAct = new QAction(KaIcons::icon(QStringLiteral("trash")), QStringLiteral("레이어 삭제"), this);
  delAct->setObjectName(QStringLiteral("actionRemoveLayer"));
  delAct->setShortcut(QKeySequence::Delete);
  delAct->setShortcutContext(Qt::WindowShortcut);
  connect(delAct, &QAction::triggered, this, &MainWindow::removeSelectedLayers);
  addAction(delAct);

  auto* delSc = new QShortcut(QKeySequence(Qt::Key_Delete), this);
  delSc->setContext(Qt::ApplicationShortcut);
  connect(delSc, &QShortcut::activated, this, &MainWindow::removeSelectedLayers);
  auto* bsSc = new QShortcut(QKeySequence(Qt::Key_Backspace), m_layerTree);
  bsSc->setContext(Qt::WidgetWithChildrenShortcut);
  connect(bsSc, &QShortcut::activated, this, &MainWindow::removeSelectedLayers);

  auto* layerCol = new QVBoxLayout();
  auto* layerTitleRow = new QHBoxLayout();
  auto* layerIcon = new QLabel(central);
  layerIcon->setPixmap(KaIcons::icon(QStringLiteral("layer")).pixmap(20, 20));
  auto* layerTitle = new QLabel(QStringLiteral("레이어"), central);
  layerTitle->setObjectName(QStringLiteral("layerTreeTitle"));
  auto* delBtn = new QToolButton(central);
  delBtn->setObjectName(QStringLiteral("btnRemoveLayer"));
  delBtn->setIcon(KaIcons::icon(QStringLiteral("trash")));
  delBtn->setText(QStringLiteral("삭제"));
  delBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  delBtn->setToolTip(QStringLiteral("선택 레이어 삭제 (Delete)"));
  delBtn->setAutoRaise(true);
  connect(delBtn, &QToolButton::clicked, this, &MainWindow::removeSelectedLayers);
  layerTitleRow->addWidget(layerIcon);
  layerTitleRow->addWidget(layerTitle, 1);
  layerTitleRow->addWidget(delBtn);
  layerCol->addLayout(layerTitleRow);
  layerCol->addWidget(m_layerTree, 1);

  auto* mapRow = new QHBoxLayout();
  mapRow->addLayout(layerCol);
  mapRow->addWidget(m_canvas, 1);
  centerCol->addLayout(mapRow, 1);
#else
  auto* stub = new QLabel(QStringLiteral("QGIS SDK 스텁 모드\n(디지타이즈는 카운터로 시뮬레이션)"), central);
  stub->setAlignment(Qt::AlignCenter);
  stub->setObjectName(QStringLiteral("mapStub"));
  centerCol->addWidget(stub, 1);
#endif
  layout->addLayout(centerCol, 1);

  auto* right = new QVBoxLayout();
  auto* guideTitle = new QLabel(QStringLiteral("지금 할 일"), central);
  guideTitle->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;color:#0f172a;"));
  m_guideNow = new QLabel(central);
  m_guideNow->setObjectName(QStringLiteral("guideNow"));
  m_guideNow->setWordWrap(true);
  m_guideNow->setFixedWidth(260);
  m_guideNow->setStyleSheet(
      QStringLiteral("background:#eff6ff;border:1px solid #93c5fd;border-radius:8px;padding:10px;"
                     "color:#1e3a8a;font-size:12px;"));
  m_nextBtn = new QPushButton(QStringLiteral("다음 단계 →"), central);
  m_nextBtn->setObjectName(QStringLiteral("btnNextStep"));
  m_nextBtn->setMinimumHeight(36);
  m_nextBtn->setStyleSheet(
      QStringLiteral("QPushButton{background:#2563eb;color:white;font-weight:700;border-radius:6px;padding:8px;}"
                     "QPushButton:hover{background:#1d4ed8;}"));
  connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::goNextStep);

  m_help = new QLabel(central);
  m_help->setWordWrap(true);
  m_help->setFixedWidth(260);
  m_help->setObjectName(QStringLiteral("helpPanel"));
  m_help->setTextFormat(Qt::RichText);
  m_checkView = new QLabel(QStringLiteral("검수 결과 없음"), central);
  m_checkView->setWordWrap(true);
  m_checkView->setObjectName(QStringLiteral("checkView"));
  m_checkView->setTextFormat(Qt::RichText);
  right->addWidget(guideTitle);
  right->addWidget(m_guideNow);
  right->addWidget(m_nextBtn);
  right->addSpacing(8);
  right->addWidget(new QLabel(QStringLiteral("자세히"), central));
  right->addWidget(m_help);
  right->addWidget(new QLabel(QStringLiteral("도면 검수"), central));
  right->addWidget(m_checkView, 1);
  layout->addLayout(right);

  setCentralWidget(central);

  if (!QSettings().value(QStringLiteral("ui/beginnerTipShown")).toBool()) {
    QTimer::singleShot(600, this, &MainWindow::showBeginnerGuide);
    QSettings().setValue(QStringLiteral("ui/beginnerTipShown"), true);
  }
}

void MainWindow::onStepChanged(int row) {
  setStepTools(row);
  updateBeginnerGuide(row);
}

void MainWindow::updateBeginnerGuide(int step) {
  if (!m_guideNow) return;
  const QString now[] = {
    QStringLiteral("<b>① 새 조사 만들기</b><br/>"
                   "위 파란 버튼 <b>「새 조사」</b> 또는 아래 <b>새 조사 만들기</b>를 누르세요.<br/>"
                   "조사 이름 → <b>중부(5186)</b> 또는 <b>동부(5187)</b> 선택 → 폴더 지정."),
    QStringLiteral("<b>② 배경·지적 넣기</b><br/>"
                   "이미 VWorld 지도가 깔려 있습니다.<br/>"
                   "수치지형도·지적 SHP는 <b>「벡터」</b>로 불러오세요.<br/>"
                   "위치가 안 보이면 위 <b>위치 검색</b>에 주소/지번 입력."),
    QStringLiteral("<b>③ 조사구역 그리기</b><br/>"
                   "<b>「구역 그리기」</b> → 지도에서 점 찍기 → 우클릭 완료 → <b>저장</b>.<br/>"
                   "⚠ 점·원 표시 금지, <b>폴리곤(면)</b>만."),
    QStringLiteral("<b>④ 유구 그리기</b><br/>"
                   "<b>유구 면</b> 또는 <b>선</b> → 그리고 → 종류·시대 입력 → <b>저장</b>."),
    QStringLiteral("<b>⑤ GPS 기준점</b><br/>"
                   "기준점 <b>최소 2개</b>. 점ID·X·Y·측지계 입력.<br/>"
                   "CSV가 있으면 가져오기."),
    QStringLiteral("<b>⑥ 도면 검수</b><br/>"
                   "<b>「검수」</b> 버튼. 빨간 error가 0개여야 제출 가능."),
    QStringLiteral("<b>⑦ 제출</b><br/>"
                   "1) <b>5179변환</b> (업로드용 SHP)<br/>"
                   "2) <b>PDF</b> (범례·축척자·방위 포함 조판)<br/>"
                   "3) <b>제출</b> 패키지")
  };
  if (step >= 0 && step < 7) m_guideNow->setText(now[step]);
  if (m_nextBtn) {
    m_nextBtn->setText(step >= 6 ? QStringLiteral("처음으로") : QStringLiteral("다음 단계 →"));
  }
}

void MainWindow::goNextStep() {
  if (!m_steps) return;
  const int r = m_steps->currentRow();
  if (r < 0) m_steps->setCurrentRow(0);
  else if (r >= 6) m_steps->setCurrentRow(0);
  else m_steps->setCurrentRow(r + 1);
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

void MainWindow::showBeginnerGuide() {
  QMessageBox::information(
      this, QStringLiteral("초보자 가이드 — 3분 흐름"),
      QStringLiteral(
          "현장 작업 순서 (왼쪽 1→7을 위에서 아래로)\n\n"
          "1. 새 조사 — 작업좌표계 5186(중부) 또는 5187(동부)\n"
          "2. 지적·지형 불러오기 + 위치검색으로 현장 이동\n"
          "3. 조사구역을 면(폴리곤)으로 그리기\n"
          "4. 유구 면/선 + 종류·시대\n"
          "5. GPS 기준점 2개 이상\n"
          "6. 검수 (빨간 오류 0)\n"
          "7. 5179 SHP 변환 → PDF(범례·축척) → 제출\n\n"
          "오른쪽 「지금 할 일」만 따라 하면 됩니다.\n"
          "막히면 「다음 단계」 버튼을 누르세요."));
}

void MainWindow::setStepTools(int step) {
  m_stepTools->clear();
  updateBeginnerGuide(step);
  const QString helps[] = {
    QStringLiteral("<b>완료 조건</b><br/>조사용 GPKG가 만들어짐<br/><b>CRS</b> 작업=5186/5187, 업로드=5179"),
    QStringLiteral("<b>완료 조건</b><br/>지적·지형이 레이어에 보임<br/>위치검색으로 현장 확대"),
    QStringLiteral("<b>완료 조건</b><br/>조사구역 <b>폴리곤</b> 1개 이상 저장<br/>점/원 심볼 금지"),
    QStringLiteral("<b>완료 조건</b><br/>유구에 <b>종류·시대</b> 입력 후 저장"),
    QStringLiteral("<b>완료 조건</b><br/>기준점 ≥2 + 측지 메타"),
    QStringLiteral("<b>완료 조건</b><br/>검수 error = 0"),
    QStringLiteral("<b>완료 조건</b><br/>5179 SHP + PDF(범례·축척자) + 제출폴더")
  };
  if (step >= 0 && step < 7) m_help->setText(helps[step]);

  auto add = [&](const QString& iconId, const QString& t, void (MainWindow::*slot)()) {
    m_stepTools->addAction(KaIcons::icon(iconId), t, this, slot);
  };
  switch (step) {
  case 0: add(QStringLiteral("new"), QStringLiteral("새 조사 만들기"), &MainWindow::newSurvey); break;
  case 1:
    add(QStringLiteral("layer"), QStringLiteral("지형·지적 불러오기"), &MainWindow::openVectorLayer);
    add(QStringLiteral("map"), QStringLiteral("VWorld 배경"), &MainWindow::addBasemapVworld);
    add(QStringLiteral("satellite"), QStringLiteral("위성"), &MainWindow::addBasemapGoogle);
    break;
  case 2:
    add(QStringLiteral("draw_area"), QStringLiteral("구역 그리기"), &MainWindow::startEditSurveyArea);
    add(QStringLiteral("saveedit"), QStringLiteral("저장"), &MainWindow::saveEdits);
    add(QStringLiteral("stop"), QStringLiteral("그리기 종료"), &MainWindow::stopEdits);
    break;
  case 3:
    add(QStringLiteral("draw_poly"), QStringLiteral("유구 면 그리기"), &MainWindow::startEditFeaturePoly);
    add(QStringLiteral("draw_line"), QStringLiteral("선 그리기"), &MainWindow::startEditFeatureLine);
    add(QStringLiteral("saveedit"), QStringLiteral("저장"), &MainWindow::saveEdits);
    add(QStringLiteral("stop"), QStringLiteral("그리기 종료"), &MainWindow::stopEdits);
    break;
  case 4:
    add(QStringLiteral("gps"), QStringLiteral("기준점 추가"), &MainWindow::addControlPoint);
    add(QStringLiteral("import"), QStringLiteral("CSV 가져오기"), &MainWindow::importControlCsv);
    break;
  case 5: add(QStringLiteral("check"), QStringLiteral("검수 실행"), &MainWindow::runChecklist); break;
  case 6:
    add(QStringLiteral("upload"), QStringLiteral("5179 SHP 변환"), &MainWindow::convertSelectedTo5179);
    add(QStringLiteral("palette"), QStringLiteral("조판 만들기"), &MainWindow::rebuildLayouts);
    add(QStringLiteral("pdf"), QStringLiteral("PDF(범례·축척)"), &MainWindow::exportPdf);
    add(QStringLiteral("export"), QStringLiteral("SHP 패키지"), &MainWindow::exportShpPackage);
    break;
  default: break;
  }
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
  statusBar()->showMessage(QStringLiteral("조사 생성 %1 | 작업 %2 | 업로드시 5179 변환")
                               .arg(path, m_workCrs),
                           10000);
  m_steps->setCurrentRow(1);
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr);
  const bool hasVworld = !QgsProject::instance()->mapLayersByName(QStringLiteral("VWorld 배경")).isEmpty();
  if (!hasVworld) {
    QString err;
    if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::VWorldBase, &err)) {
      statusBar()->showMessage(QStringLiteral("VWorld 실패, OSM 시도: %1").arg(err), 8000);
    }
  }
  LayerOps::zoomToKorea(m_canvas, m_workCrs);
#endif
}

void MainWindow::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, authId, &err)) {
    QMessageBox::warning(this, QStringLiteral("CRS"), err);
    return;
  }
  statusBar()->showMessage(QStringLiteral("작업 CRS = %1 (타일·지형 중첩 OTF)").arg(authId), 8000);
#endif
}
void MainWindow::setWorkCrs5186() { setWorkCrs(QStringLiteral("EPSG:5186")); }
void MainWindow::setWorkCrs5187() { setWorkCrs(QStringLiteral("EPSG:5187")); }

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
  if (m_layerTree && event && event->type() == QEvent::KeyPress) {
    const bool onTree = (watched == m_layerTree || watched == m_layerTree->viewport());
    if (onTree) {
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

void MainWindow::addBasemapVworld() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::VWorldBase, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else {
    LayerOps::zoomToKorea(m_canvas, m_workCrs);
    statusBar()->showMessage(QStringLiteral("VWorld 배경 (%1)").arg(m_workCrs), 5000);
  }
#endif
}
void MainWindow::addBasemapVworldSat() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::VWorldSatellite, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else LayerOps::zoomToKorea(m_canvas, m_workCrs);
#endif
}
void MainWindow::addBasemapGoogle() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::GoogleSatellite, &err))
    QMessageBox::warning(this, QStringLiteral("배경"), err);
  else LayerOps::zoomToKorea(m_canvas, m_workCrs);
#endif
}

void MainWindow::loadSurveyLayers(const QString& gpkgOrStub) {
#if KA_HGIS_HAS_QGIS
  if (gpkgOrStub.endsWith(QLatin1String(".stub"))) return;
  QgsProject::instance()->removeAllMapLayers();
  QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(m_workCrs));
  if (m_canvas) m_canvas->setDestinationCrs(QgsCoordinateReferenceSystem(m_workCrs));
  const char* names[] = {"survey_area","feature_poly","feature_line","section_line","control_points"};
  for (const char* n : names) {
    auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgOrStub, n), n, QStringLiteral("ogr"));
    if (vl->isValid()) QgsProject::instance()->addMapLayer(vl); else delete vl;
  }
  if (auto* cp = layerByName(QStringLiteral("control_points")))
    LayerOps::ensureControlPointQualityFields(cp);
  if (auto* fp = layerByName(QStringLiteral("feature_poly")))
    LayerOps::applyFeaturePolyStyle(fp);
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
  if (m_canvas) m_canvas->refresh();
#else
  Q_UNUSED(gpkgOrStub);
#endif
}

#if KA_HGIS_HAS_QGIS
QgsVectorLayer* MainWindow::layerByName(const QString& name) const {
  const auto layers = QgsProject::instance()->mapLayersByName(name);
  if (layers.isEmpty()) return nullptr;
  return qobject_cast<QgsVectorLayer*>(layers.first());
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

    QVariantMap attrs;
    if (layer->name() == QLatin1String("feature_poly") || layer->name() == QLatin1String("feature_line")) {
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
      attrs.insert(QStringLiteral("kind"), kind.trimmed());
      attrs.insert(QStringLiteral("period"), period.trimmed());
    } else if (layer->name() == QLatin1String("survey_area")) {
      const QString sn = QInputDialog::getText(this, QStringLiteral("조사구역"), QStringLiteral("조사명(선택)"));
      if (!sn.trimmed().isEmpty()) attrs.insert(QStringLiteral("survey_name"), sn.trimmed());
    }

    const FeatureWriteResult wr = FeatureWriteService::addFeature(layer, geom, attrs);
    if (!wr.ok) {
      QMessageBox::warning(this, QStringLiteral("도형 저장"), wr.errorKo);
      statusBar()->showMessage(wr.errorKo, 6000);
      return;
    }
    layer->triggerRepaint();
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("도형 추가됨 → 「저장」 누르세요. 계속 그리려면 지도 좌클릭."), 8000);
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
  beginEdit(layerByName(QStringLiteral("survey_area")));
#else
  m_stubSurveyArea++;
  statusBar()->showMessage(QStringLiteral("스텁: 조사구역 폴리곤 %1개").arg(m_stubSurveyArea));
#endif
}
void MainWindow::startEditFeaturePoly() {
#if KA_HGIS_HAS_QGIS
  beginEdit(layerByName(QStringLiteral("feature_poly")));
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 유구 %1").arg(m_stubFeatures));
#endif
}
void MainWindow::startEditFeatureLine() {
#if KA_HGIS_HAS_QGIS
  beginEdit(layerByName(QStringLiteral("feature_line")));
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 선 %1").arg(m_stubFeatures));
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
  statusBar()->showMessage(QStringLiteral("저장 완료 (%1개 레이어)").arg(n), 5000);
#else
  statusBar()->showMessage(QStringLiteral("스텁 저장"), 3000);
#endif
}
void MainWindow::stopEdits() {
#if KA_HGIS_HAS_QGIS
  stopCaptureTool();
  m_editLayer = nullptr;
#endif
  statusBar()->showMessage(QStringLiteral("그리기 종료 (이동 모드). 저장은 「저장」버튼"), 4000);
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
  auto* layer = layerByName(QStringLiteral("control_points"));
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
  const QString out = ExportService::exportSubmissionPackage(
      QgsProject::instance(), dir, enc, summary, true, hasErr, &err);
#else
  const QString out;
  err = QStringLiteral("QGIS required");
#endif
  if (out.isEmpty())
    QMessageBox::warning(this, QStringLiteral("제출 차단"), err);
  else
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
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
  auto* cp = layerByName(QStringLiteral("control_points"));
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
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("벡터"), QString(), QStringLiteral("Vector (*.gpkg *.shp *.geojson)"));
  if (path.isEmpty()) return;
  auto* layer = new QgsVectorLayer(path, QFileInfo(path).completeBaseName(), QStringLiteral("ogr"));
  if (!layer->isValid()) { QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("열 수 없음")); delete layer; return; }
  QgsProject::instance()->addMapLayer(layer);
  m_canvas->setExtent(layer->extent());
  m_canvas->refresh();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("배경 레이어 로드 시뮬레이션"));
#endif
}
void MainWindow::saveProject() {
#if KA_HGIS_HAS_QGIS
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
  if (auto* cp = layerByName(QStringLiteral("control_points")))
    LayerOps::ensureControlPointQualityFields(cp);
  if (auto* fp = layerByName(QStringLiteral("feature_poly")))
    LayerOps::applyFeaturePolyStyle(fp);
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("프로젝트 열림 + 마이그레이션/레이아웃 적용"), 5000);
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
  const QString cur = LocationSearch::vworldApiKey();
  const QString key = QInputDialog::getText(
      this, QStringLiteral("VWorld API 키"),
      QStringLiteral("vworld.kr 에서 발급받은 인증키 (비우면 OSM Nominatim 사용)\n"
                     "지번·도로명 정확도는 VWorld 키가 더 좋습니다."),
      QLineEdit::Normal, cur, &ok);
  if (!ok) return;
  LocationSearch::setVworldApiKey(key);
  statusBar()->showMessage(key.isEmpty()
                               ? QStringLiteral("검색: OSM Nominatim (한국)")
                               : QStringLiteral("검색: VWorld API 키 저장됨"),
                           6000);
}

void MainWindow::showAbout() {
  QMessageBox::about(this, QStringLiteral("정보"),
    QStringLiteral("고고학 전용 HGIS (ka-hgis) 0.3.0\n"
                   "C++/Qt6 + QGIS 4.x libraries\n"
                   "작업 CRS: 5186/5187 | 업로드: 5179\n"
                   "위치검색: 주소·지번·지역·상호\n"
                   "License: GNU GPLv2 or later"));
}



