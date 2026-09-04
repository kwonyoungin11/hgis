#include "MainWindow.h"
#include "KaTheme.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaAlignMapTool.h"
#include "KaImageView.h"
#include "KaDrawingStudio.h"
#include "KaSectionDrawingStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaStartPage.h"
#include "KaCoordPointMapTool.h"
#include "KaMeasureMapTool.h"
#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaVertexEditTool.h"
#include "KaFoundLocationMark.h"
#include "KaStatusBar.h"
#include "KaBeginnerRibbon.h"
#include "KaCrashGuard.h"
#include <QElapsedTimer>
#include "KaTrenchDialog.h"
#include "KaDemClassDialog.h"
#include "core/RecentSurveys.h"
#include "core/GeorefService.h"
#include "core/BufferAnalysis.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"
#include "core/Terrain3dLayoutService.h"
#include "core/LayerOps.h"
#include "core/SoilMapService.h"
#include "core/PaleoLandformService.h"
#include "core/GeologyMapService.h"
#include "core/RiverMapService.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"
#include "KaRegionLocator.h"
#include "core/WorkflowGuide.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QApplication>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
#include <QTimer>
#include <QPointer>
#include <QCursor>
#include <QAction>
#include <QDockWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
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
#include <QKeySequence>
#include <QAbstractItemView>
#include <QMenu>
#include <QPixmap>
#include <QImage>
#include <QAction>
#include <QSize>
#include <QListWidgetItem>
#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QKeyEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>
#include <QVector>
#include <QPair>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QSettings>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QFrame>
#include <QStandardPaths>
#include <QColor>
#include <QPalette>
#include <QUrl>
#include <QDesktopServices>
#include <QSizePolicy>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QStorageInfo>
#include <QHash>
#include <QMetaType>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QLocale>
#include <QLayout>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgsproject.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmaptool.h>
#include <qgsmaptoolemitpoint.h>
#include <qgspointxy.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolselect.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgsrubberband.h>
#include <qgsapplication.h>
#include <qgsmessagelog.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsrectangle.h>
#include <qgsvertexmarker.h>
#include <qgsmaptopixel.h>
#include <qgspoint.h>
#include <qgspointlocator.h>
#include <qgsexception.h>
#include <qgsproviderregistry.h>
#include <qgsprovidersublayerdetails.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("필드고고학GIS"));
  setWindowIcon(KaIcons::appIcon());
  resize(1280, 900);
  m_status = new KaStatusBar(this);
  setStatusBar(m_status);
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
  refreshWorkPanel();
  updateNextActionStatus();
  auto* undoAct = new QAction(QStringLiteral("되돌리기"), this);
  undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setShortcutContext(Qt::WindowShortcut);
  connect(undoAct, &QAction::triggered, this, &MainWindow::undoLastAction);
  addAction(undoAct);
  auto* fullAct = new QAction(QStringLiteral("전체 화면"), this);
  fullAct->setShortcut(Qt::Key_F11);
  fullAct->setShortcutContext(Qt::WindowShortcut);
  connect(fullAct, &QAction::triggered, this, [this]() {
    if (isFullScreen())
      showMaximized();
    else
      showFullScreen();
  });
  addAction(fullAct);
  {
    QSettings st = RecentSurveys::userSettings();
    const QByteArray geo = st.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    if (!geo.isEmpty())
      restoreGeometry(geo);
    const QByteArray dockState = st.value(QStringLiteral("MainWindow/state")).toByteArray();
    if (!dockState.isEmpty())
      restoreState(dockState);
    const QByteArray split = st.value(QStringLiteral("MainWindow/mainSplit")).toByteArray();
    if (m_mainSplit && !split.isEmpty()) {
      m_mainSplit->restoreState(split);
      const int totalW = m_mainSplit->width();
      if (totalW > 300) {
        const QList<int> sz = m_mainSplit->sizes();
        if (!sz.isEmpty() && sz.at(0) > totalW * 0.35) {
          const int leftW = qBound(200, int(totalW * 0.22), 360);
          m_mainSplit->setSizes({leftW, totalW - leftW});
        }
      }
    }
  }
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
  persistSurveyWork();
  QSettings st = RecentSurveys::userSettings();
  st.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
  st.setValue(QStringLiteral("MainWindow/state"), saveState());
  if (m_mainSplit)
    st.setValue(QStringLiteral("MainWindow/mainSplit"), m_mainSplit->saveState());
  QMainWindow::closeEvent(event);
}

bool MainWindow::openSurveyGpkg(const QString& gpkgPath) {
  if (gpkgPath.isEmpty() || !QFile::exists(gpkgPath)) return false;
  QElapsedTimer t;
  t.start();
  m_surveyPath = gpkgPath;
#if KA_HGIS_HAS_QGIS
  // 동반되는 QGIS 프로젝트(.qgz)가 있으면 외부 SHP, 라벨 5pt, 스타일 등 작업 레이어를 온전히 복원한다.
  const QString qgz = QFileInfo(gpkgPath).dir().filePath(QFileInfo(gpkgPath).completeBaseName() + QStringLiteral(".qgz"));
  if (QFile::exists(qgz)) {
    if (QgsProject::instance()->read(qgz)) {
      m_surveyPath = gpkgPath;
      if (QgsProject::instance()->crs().isValid())
        m_workCrs = QgsProject::instance()->crs().authid();
      LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
      LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
      if (m_canvas) m_canvas->refresh();
      ensureDefaultBasemaps();
      rememberSurvey(gpkgPath, QFileInfo(gpkgPath).completeBaseName());
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(gpkgPath).completeBaseName()));
      showMapWorkspace();
      updateNextActionStatus();
      KaCrashGuard::logLine(
          QStringLiteral("[open] 조사 프로젝트 열기 %1 ms — %2").arg(t.elapsed()).arg(qgz));
      return true;
    }
  }
#endif
  loadSurveyLayers(gpkgPath);
#if KA_HGIS_HAS_QGIS
  applyStartupMap();
  QgsProject* proj = QgsProject::instance();
  for (QgsMapLayer* ml : proj->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM") && ml->isValid()) {
      if (auto* rl = qobject_cast<QgsRasterLayer*>(ml)) {
        if (LayerOps::isLayerVisible(proj, QStringLiteral("DEM"))) {
          LayerOps::ensureDemRelief(proj, rl);
        }
      }
      break;
    }
  }
#endif
  rememberSurvey(gpkgPath, QFileInfo(gpkgPath).completeBaseName());
  showMapWorkspace();
  KaCrashGuard::logLine(
      QStringLiteral("[open] 조사 열기 %1 ms — %2").arg(t.elapsed()).arg(gpkgPath));
  return true;
}

int MainWindow::domainLayerCount() const {
#if KA_HGIS_HAS_QGIS
  int n = 0;
  for (const QString& k : LayerOps::domainLayerKeys()) {
    if (LayerOps::findByLayerKey(QgsProject::instance(), k)) ++n;
  }
  return n;
#else
  return 0;
#endif
}

int MainWindow::lastChecklistErrorCount() const {
  if (m_lastChecklistErrors >= 0) return m_lastChecklistErrors;
  if (!m_checklist) return -1;
  int err = 0;
  for (const auto& r : m_checklist->evaluate(buildProjectState())) {
    if (!r.passed && r.severity == QLatin1String("error")) ++err;
  }
  return err;
}

int MainWindow::seedDemoFieldData() {
#if !KA_HGIS_HAS_QGIS
  return 0;
#else
  int added = 0;
  auto commitLayer = [&](QgsVectorLayer* vl) -> bool {
    if (!vl || !vl->isValid()) return false;
    if (!vl->isEditable() && !vl->startEditing()) return false;
    if (!vl->commitChanges()) {
      vl->rollBack();
      return false;
    }
    return true;
  };
  auto ensure = [&](const char* key, const char* title) -> QgsVectorLayer* {
    QString err;
    return LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath,
                                       QString::fromUtf8(key), QString::fromUtf8(title), &err);
  };

  if (auto* sa = ensure("survey_area", "조사구역")) {
    if (sa->featureCount() == 0 && sa->startEditing()) {
      QgsFeature f(sa->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(198000, 451000) << QgsPointXY(202000, 451000)
           << QgsPointXY(202000, 454000) << QgsPointXY(198000, 454000)
           << QgsPointXY(198000, 451000);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      const int isn = sa->fields().indexOf(QStringLiteral("survey_name"));
      if (isn >= 0) f.setAttribute(isn, QStringLiteral("demo_verify"));
      if (sa->addFeature(f) && commitLayer(sa)) ++added;
      else sa->rollBack();
    }
  }

  if (auto* fp = ensure("feature_poly", "유구면")) {
    if (fp->featureCount() == 0 && fp->startEditing()) {
      QgsFeature f(fp->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(199200, 452000) << QgsPointXY(200800, 452000)
           << QgsPointXY(200800, 453200) << QgsPointXY(199200, 453200)
           << QgsPointXY(199200, 452000);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      const int ik = fp->fields().indexOf(QStringLiteral("kind"));
      const int ip = fp->fields().indexOf(QStringLiteral("period"));
      if (ik >= 0) f.setAttribute(ik, QStringLiteral("수혈주거지"));
      if (ip >= 0) f.setAttribute(ip, QStringLiteral("청동기"));
      if (fp->addFeature(f) && commitLayer(fp)) ++added;
      else fp->rollBack();
    }
  }

  if (auto* cp = ensure("control_points", "GPS기준점")) {
    LayerOps::ensureControlPointQualityFields(cp);
    if (cp->featureCount() < 2 && cp->startEditing()) {
      auto addPt = [&](const QString& id, double x, double y) {
        QgsFeature f(cp->fields());
        f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(x, y)));
        auto set = [&](const char* name, const QVariant& v) {
          const int i = cp->fields().indexOf(QString::fromUtf8(name));
          if (i >= 0) f.setAttribute(i, v);
        };
        set("point_id", id);
        set("x", x);
        set("y", y);
        set("datum", QStringLiteral("세계측지계"));
        set("ellipsoid", QStringLiteral("GRS80"));
        set("projection", QStringLiteral("TM/중부원점"));
        set("origin", QStringLiteral("중부"));
        set("accuracy", QStringLiteral("0.05m"));
        set("accuracy_m", 0.05);
        set("fix_type", QStringLiteral("RTK"));
        if (cp->addFeature(f)) ++added;
      };
      addPt(QStringLiteral("GCP1"), 198100, 451100);
      addPt(QStringLiteral("GCP2"), 201900, 453900);
      if (!commitLayer(cp)) cp->rollBack();
    }
  }

  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    if (auto* sa = layerByKey(QStringLiteral("survey_area")))
      LayerOps::zoomToLayerMax(m_canvas, sa);
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("데모 시드: 피처 %1건 추가").arg(added), 8000);
  return added;
#endif
}

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
  menuBar()->setNativeMenuBar(false);
  menuBar()->hide();

  auto* mainTb = addToolBar(QStringLiteral("주요"));
  mainTb->setObjectName(QStringLiteral("mainToolbar"));
  mainTb->setIconSize(QSize(25, 25));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->setMovable(false);
  mainTb->setFloatable(false);
  mainTb->setAttribute(Qt::WA_InputMethodEnabled, true);

  auto* ribbon = new KaBeginnerRibbon(mainTb);
  m_ribbon = ribbon;
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도를 깔아볼까?"));
  ribbon->addGroup(QStringLiteral("align"), QStringLiteral("정합·분석"));
  ribbon->addGroup(QStringLiteral("out"), QStringLiteral("산출"));
  ribbon->addGroup(QStringLiteral("find"), QStringLiteral("찾기"));

  auto addIcon = [this, ribbon](const QString& group, const QString& iconId, const QString& text,
                                const QString& tip, auto slot) -> QPair<QAction*, QToolButton*> {
    auto* a = new QAction(KaIcons::icon(iconId), text, this);
    a->setToolTip(tip);
    connect(a, &QAction::triggered, this, slot);
    addAction(a);
    QToolButton* b = ribbon->addAction(group, a);
    return {a, b};
  };
  auto paintPrimary = [](QToolButton* b, const QString& iconId) {
    if (!b) return;
    b->setObjectName(QStringLiteral("btnPrimary"));
    b->setIcon(KaIcons::icon(iconId, QColor(0x12, 0x4B, 0x94)));
  };
  auto [actNew, btnNew] = addIcon(QStringLiteral("survey"), QStringLiteral("new"),
                                  QStringLiteral("새로 만들까?"),
                                  QStringLiteral("오늘 현장 조사를 새로 만듭니다"),
                                  &MainWindow::newSurvey);
  auto [actOpen, btnOpen] = addIcon(QStringLiteral("survey"), QStringLiteral("open"),
                                    QStringLiteral("저장한 거 열까?"),
                                    QStringLiteral("저장한 조사를 엽니다"), &MainWindow::openProject);
  auto [actSave, btnSave] = addIcon(QStringLiteral("survey"), QStringLiteral("save"),
                                    QStringLiteral("여기까지 저장"),
                                    QStringLiteral("지금 조사를 저장합니다 (Ctrl+S)"), &MainWindow::saveProject);
  actSave->setShortcut(QKeySequence::Save);
  auto [actSaveAs, btnSaveAs] = addIcon(QStringLiteral("survey"), QStringLiteral("save_as"),
                                        QStringLiteral("다른이름 저장"),
                                        QStringLiteral("작업 중인 모든 레이어를 다른 이름으로 저장합니다 (Ctrl+Shift+S)"),
                                        &MainWindow::saveProjectAs);
  actSaveAs->setShortcut(QKeySequence::SaveAs);
  Q_UNUSED(actNew);
  Q_UNUSED(actOpen);
  Q_UNUSED(actSave);
  Q_UNUSED(actSaveAs);
  paintPrimary(btnNew, QStringLiteral("new"));
  paintPrimary(btnOpen, QStringLiteral("open"));
  paintPrimary(btnSave, QStringLiteral("save"));
  paintPrimary(btnSaveAs, QStringLiteral("save_as"));

  auto [actSelect, btnSelect] = addIcon(
      QStringLiteral("record"), QStringLiteral("select"), QStringLiteral("고를까?"),
      QStringLiteral("그린 도형을 고릅니다. 다시 누르면 이동으로 돌아갑니다"),
      &MainWindow::startSelectTool);
  m_actSelect = actSelect;
  Q_UNUSED(btnSelect);
  m_actSelect->setCheckable(true);
  auto [actMeasure, btnMeasure] = addIcon(
      QStringLiteral("record"), QStringLiteral("measure"), QStringLiteral("거리 재볼까?"),
      QStringLiteral("지도에서 거리와 면적을 잽니다. 다시 누르면 끝납니다"),
      &MainWindow::startMeasureTool);
  m_actMeasure = actMeasure;
  Q_UNUSED(btnMeasure);
  if (m_actMeasure)
    m_actMeasure->setCheckable(true);

  m_btnDraw = new QToolButton(ribbon);
  m_btnDraw->setObjectName(QStringLiteral("btnDraw"));
  m_btnDraw->setIcon(KaIcons::icon(QStringLiteral("draw_poly")));
  m_btnDraw->setText(QStringLiteral("그려볼까?"));
  m_btnDraw->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDraw->setCheckable(true);
  m_btnDraw->setToolTip(QStringLiteral("조사구역·유구 면과 선을 그립니다. 다시 누르면 도구를 닫고 이동합니다"));
  connect(m_btnDraw, &QToolButton::clicked, this, [this](bool on) {
    if (on) showSubToolsDraw();
    else hideSubTools();
    if (m_btnDraw)
      m_btnDraw->setChecked(m_subToolbar && m_subToolbar->isVisible() &&
                            m_subToolsMode == QLatin1String("draw"));
  });
  ribbon->addWidget(QStringLiteral("record"), m_btnDraw);

  addIcon(QStringLiteral("record"), QStringLiteral("trench_grid"), QStringLiteral("시굴격자"),
          QStringLiteral("조사구역이 있으면 바로 깔고, 없으면 맵을 찍어 놓습니다. 깐 뒤에는 끌어 옮깁니다"),
          &MainWindow::startTrenchGrid);

  m_btnTerrain = new QToolButton(ribbon);
  m_btnTerrain->setObjectName(QStringLiteral("btnTerrain"));
  m_btnTerrain->setIcon(KaIcons::icon(QStringLiteral("contour")));
  m_btnTerrain->setText(QStringLiteral("지형맵"));
  m_btnTerrain->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnTerrain->setCheckable(true);
  m_btnTerrain->setToolTip(
      QStringLiteral("등고·음영 지형도를 켭니다. 다시 누르면 숨깁니다"));
  connect(m_btnTerrain, &QToolButton::clicked, this, &MainWindow::toggleTerrainMap);
  connect(m_btnTerrain, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnTerrain);
  m_btnDem = new QToolButton(ribbon);
  m_btnDem->setObjectName(QStringLiteral("btnDem"));
  m_btnDem->setIcon(KaIcons::icon(QStringLiteral("dem")));
  m_btnDem->setText(QStringLiteral("DEM"));
  m_btnDem->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDem->setCheckable(true);
  m_btnDem->setToolTip(
      QStringLiteral("고도를 색으로 보여 줍니다. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* demMenu = new QMenu(m_btnDem);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")),
                      QStringLiteral("국토지리원 DEM 불러오기(.img)…"), this,
                      &MainWindow::importDemElevationRaster);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("DEM 파일로 음영 만들기…"),
                     this, &MainWindow::runDemHillshade);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("높이 구간 바꾸기…"), this,
                     &MainWindow::editDemElevationClasses);
  m_btnDem->setMenu(demMenu);
  m_btnDem->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnDem, &QToolButton::customContextMenuRequested, this,
          [this, demMenu](const QPoint& pos) { demMenu->exec(m_btnDem->mapToGlobal(pos)); });
  connect(m_btnDem, &QToolButton::clicked, this, &MainWindow::toggleDemMap);
  connect(m_btnDem, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnDem);
  m_btnSoil = new QToolButton(ribbon);
  m_btnSoil->setObjectName(QStringLiteral("btnSoil"));
  m_btnSoil->setIcon(KaIcons::icon(QStringLiteral("soil")));
  m_btnSoil->setText(QStringLiteral("토양도"));
  m_btnSoil->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnSoil->setCheckable(true);
  m_btnSoil->setPopupMode(QToolButton::MenuButtonPopup);
  m_btnSoil->setToolTip(
      QStringLiteral("흙토람 토양도를 겹칩니다. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* soilMenu = new QMenu(m_btnSoil);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("분포지형 내려받기 — 현재 화면 범위"), this,
                      &MainWindow::downloadSoilTerrain);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("open")),
                      QStringLiteral("토양도 파일 불러오기(SHP·GPKG)"), this,
                      &MainWindow::importSoilShapefile);
  m_btnSoil->setMenu(soilMenu);
  m_btnSoil->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnSoil, &QToolButton::customContextMenuRequested, this,
          [this, soilMenu](const QPoint& pos) { soilMenu->exec(m_btnSoil->mapToGlobal(pos)); });
  connect(m_btnSoil, &QToolButton::clicked, this, &MainWindow::downloadSoilTerrain);
  connect(m_btnSoil, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnSoil);
  m_btnPaleo = new QToolButton(ribbon);
  m_btnPaleo->setObjectName(QStringLiteral("btnPaleo"));
  m_btnPaleo->setIcon(KaIcons::icon(QStringLiteral("paleo")));
  m_btnPaleo->setText(QStringLiteral("고지형"));
  m_btnPaleo->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnPaleo->setToolTip(
      QStringLiteral("흙토람 입지 후보를 강조하고 구하도·자연제방 등 가설을 그립니다. 확정이 아닙니다"));
  connect(m_btnPaleo, &QToolButton::clicked, this, &MainWindow::startPaleoLandform);
  ribbon->addWidget(QStringLiteral("basemap"), m_btnPaleo);
  auto [actGeology, btnGeology] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("geology"), QStringLiteral("지질도"),
      QStringLiteral("KIGAM 1:5만 지질 색 위에 지형 음영을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadGeologyMap);
  m_actGeology = actGeology;
  Q_UNUSED(btnGeology);
  if (m_actGeology) m_actGeology->setCheckable(true);
  auto [actRiver, btnRiver] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("river"), QStringLiteral("수계도"),
      QStringLiteral("하천망을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadRiverMap);
  m_actRiver = actRiver;
  Q_UNUSED(btnRiver);
  if (m_actRiver) m_actRiver->setCheckable(true);
  for (QAction* thematic : {m_actGeology, m_actRiver}) {
    if (!thematic) continue;
    connect(thematic, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
    });
  }
  addIcon(QStringLiteral("align"), QStringLiteral("georef"), QStringLiteral("사진·캐드 맞출까?"),
          QStringLiteral("사진과 캐드 도면을 배경 지도에 맞춥니다"), &MainWindow::georefAssistant);

  auto* btnBuffer = new QToolButton(ribbon);
  btnBuffer->setObjectName(QStringLiteral("btnBuffer"));
  btnBuffer->setIcon(KaIcons::icon(QStringLiteral("buffer")));
  btnBuffer->setText(QStringLiteral("주변 유적"));
  btnBuffer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnBuffer->setCheckable(true);
  btnBuffer->setToolTip(QStringLiteral("선택한 면 둘레에 주변 유적 거리 경계를 그립니다"));
  connect(btnBuffer, &QToolButton::clicked, this, [this, btnBuffer](bool on) {
    if (on) showSubToolsBuffer();
    else hideSubTools();
    btnBuffer->setChecked(m_subToolbar && m_subToolbar->isVisible()
                          && m_subToolsMode == QLatin1String("buffer"));
  });
  ribbon->addWidget(QStringLiteral("align"), btnBuffer);

  addIcon(QStringLiteral("out"), QStringLiteral("pdf"), QStringLiteral("도면 만들까?"),
          QStringLiteral("종이에 지도를 올려 도면을 만듭니다"), &MainWindow::openLayoutDesigner);
  addIcon(QStringLiteral("out"), QStringLiteral("section"), QStringLiteral("단면도"),
          QStringLiteral("단면 GeoTIFF로 표고·거리 눈금 도면을 만듭니다"),
          &MainWindow::openSectionDesigner);
  addIcon(QStringLiteral("out"), QStringLiteral("transform"), QStringLiteral("제출(5179)"),
          QStringLiteral("레이어 목록에서 고른 레이어를 제출용 EPSG:5179 SHP로 저장합니다"),
          &MainWindow::convertSelectedTo5179);

  auto* region = new KaRegionLocator(ribbon);
  region->setObjectName(QStringLiteral("regionLocator"));
  connect(region, &KaRegionLocator::searchRequested, this,
          [this](const QString& q) { searchLocation(q); });
  ribbon->addWidget(QStringLiteral("find"), region);

  auto* webBtn = new QToolButton(ribbon);
  webBtn->setObjectName(QStringLiteral("btnWeb"));
  webBtn->setIcon(KaIcons::icon(QStringLiteral("map")));
  webBtn->setText(QStringLiteral("웹자료"));
  webBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  webBtn->setToolTip(QStringLiteral("인트라넷·토양도·지적도·지형도 웹 자료를 엽니다"));
  webBtn->setPopupMode(QToolButton::InstantPopup);
  auto* webMenu = new QMenu(webBtn);
  auto addWeb = [this, webMenu](const QString& iconId, const QString& text, const QString& url) {
    QAction* a = webMenu->addAction(KaIcons::icon(iconId), text);
    QObject::connect(a, &QAction::triggered, this, [url]() {
      QDesktopServices::openUrl(QUrl(url));
    });
  };
  addWeb(QStringLiteral("upload"), QStringLiteral("문화재 GIS 인트라넷"),
         QStringLiteral("https://intranet.gis-heritage.go.kr/"));
  addWeb(QStringLiteral("layer"), QStringLiteral("농진청 토양도"),
         QStringLiteral("http://soil.rda.go.kr/geoweb/soilmain.do"));
  addWeb(QStringLiteral("cadastral"), QStringLiteral("VWorld 지적도 자료"),
         QStringLiteral("https://www.vworld.kr/dtmk/dtmk_ntads_s002.do?dsId=30563"));
  addWeb(QStringLiteral("contour"), QStringLiteral("국토정보맵 지형도"),
         QStringLiteral("https://map.ngii.go.kr/ms/map/NlipMap.do"));
  webMenu->addSeparator();
  webMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                     QStringLiteral("토양도 SHP 불러오기(흙토람 다운로드)…"), this,
                     &MainWindow::importSoilShapefile);
  webBtn->setMenu(webMenu);

  auto* more = new QToolButton(ribbon);
  more->setIcon(KaIcons::icon(QStringLiteral("more")));
  more->setText(QStringLiteral("더보기"));
  more->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  more->setToolTip(QStringLiteral("가끔 쓰는 기능"));
  auto* moreMenu = new QMenu(more);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("벡터 불러오기"),
                      this, &MainWindow::openVectorLayer);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("토양도 SHP 불러오기"), this,
                      &MainWindow::importSoilShapefile);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("import")), QStringLiteral("CSV 기준점"),
                      this, &MainWindow::importControlCsv);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("georef")), QStringLiteral("맞추기"),
                      this, &MainWindow::georefAssistant);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM 배경"),
                      this, &MainWindow::addBasemapOsm);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
                      this, &MainWindow::addBasemapGoogle);
  moreMenu->addSeparator();
  moreMenu->addAction(QStringLiteral("파일함 보이기/숨기기"), this, [this]() {
    if (m_filesCard)
      m_filesCard->setVisible(!m_filesCard->isVisible());
  });
  moreMenu->addAction(QStringLiteral("작업 목록"), this, [this]() {
    if (auto* d = findChild<QDockWidget*>(QStringLiteral("workDock"))) {
      d->setVisible(!d->isVisible());
      if (d->isVisible()) d->raise();
    }
  });
  moreMenu->addAction(QStringLiteral("VWorld API 키"), this, &MainWindow::configureVworldKey);
  moreMenu->addAction(QStringLiteral("정보"), this, &MainWindow::showAbout);
  more->setMenu(moreMenu);
  more->setPopupMode(QToolButton::InstantPopup);
  ribbon->addWidget(QStringLiteral("find"), webBtn);
  ribbon->addWidget(QStringLiteral("find"), more);
  mainTb->addWidget(ribbon);

  m_subToolbar = addToolBar(QStringLiteral("세부도구"));
  m_subToolbar->setObjectName(QStringLiteral("subToolbar"));
  m_subToolbar->setIconSize(QSize(20, 20));
  m_subToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_subToolbar->setMovable(false);
  m_subToolbar->setVisible(false);
  insertToolBarBreak(m_subToolbar);
}

void MainWindow::clearSubToolbar() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  m_subToolbar->clear();
#endif
}

void MainWindow::applySnapConfig() {
#if KA_HGIS_HAS_QGIS
  if (!QgsProject::instance()) return;
  QgsSnappingConfig cfg = QgsProject::instance()->snappingConfig();
  cfg.setEnabled(m_snapEnabled);
  cfg.setMode(Qgis::SnappingMode::AllLayers);
  cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  cfg.setIntersectionSnapping(true); // 선과 선이 교차하는 지점에도 자석(스냅) 적용
  cfg.setSelfSnapping(true);         // 현재 그리고 있는 도형 자체의 교차·꼭짓점 스냅 적용
  cfg.setTolerance(16.0);
  cfg.setUnits(Qgis::MapToolUnit::Pixels);
  QgsProject::instance()->setSnappingConfig(cfg);
  if (m_canvas && m_canvas->snappingUtils())
    m_canvas->snappingUtils()->setConfig(cfg);
  if (m_alignLeftCanvas && m_alignLeftCanvas->snappingUtils())
    m_alignLeftCanvas->snappingUtils()->setConfig(cfg);
  if (m_captureTool)
    m_captureTool->setSnapEnabled(m_snapEnabled);
  if (m_measureTool)
    m_measureTool->setSnapEnabled(m_snapEnabled);
  if (m_vertexTool)
    m_vertexTool->setSnapEnabled(m_snapEnabled);
#endif
}

void MainWindow::hideSubTools() {
#if KA_HGIS_HAS_QGIS
  if (m_subToolsMode == QLatin1String("align"))
    stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw"))
    stopCaptureTool();
  clearSubToolbar();
  if (m_subToolbar) m_subToolbar->setVisible(false);
  m_subToolsMode.clear();
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBuffer"))) b->setChecked(false);
#endif
}

void MainWindow::showSubToolsBuffer() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("buffer") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("buffer");
  auto* lab = new QLabel(QStringLiteral("  주변유적경계 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("500m"),
                          this, &MainWindow::runSiteBuffer500);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("1000m"),
                          this, &MainWindow::runSiteBuffer1000);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("면 레이어를 고른 뒤 500m 또는 1000m를 누르세요. 점선은 도형 색에서 바꿉니다."),
      8000);
#endif
}

void MainWindow::runSiteBuffer500() {
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())
                                      : nullptr;
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, layer, 500.0, &err)) {
    notify(Notice::Warning, QStringLiteral("주변유적경계"),
           QStringLiteral("500m 경계를 그리지 못했습니다."), err);
    return;
  }
  statusBar()->showMessage(QStringLiteral("주변 500m 경계를 그렸습니다"), 6000);
#endif
}

void MainWindow::runSiteBuffer1000() {
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())
                                      : nullptr;
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, layer, 1000.0, &err)) {
    notify(Notice::Warning, QStringLiteral("주변유적경계"),
           QStringLiteral("1000m 경계를 그리지 못했습니다."), err);
    return;
  }
  statusBar()->showMessage(QStringLiteral("주변 1000m 경계를 그렸습니다"), 6000);
#endif
}

void MainWindow::showSubToolsDraw() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  if (m_viewTabs && m_mapPage && m_viewTabs->currentWidget() != m_mapPage)
    showMapWorkspace();
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("draw");
  auto* lab = new QLabel(QStringLiteral("  그리기 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("select")), QStringLiteral("도형선택"),
                          this, &MainWindow::startSelectTool);
  auto* snapAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("snap")), QStringLiteral("자석 켜짐"));
  snapAct->setCheckable(true);
  snapAct->setChecked(m_snapEnabled);
  snapAct->setToolTip(QStringLiteral(
      "켜면 조사구역·유구·불러온 SHP·CAD의 모서리와 선에 붙습니다. 위성·지적 그림에는 붙지 않습니다"));
  connect(snapAct, &QAction::toggled, this, [this](bool on) {
    m_snapEnabled = on;
    applySnapConfig();
    statusBar()->showMessage(on ? QStringLiteral("자석 켜짐 — 선·꼭짓점에 붙습니다. 위성·지적 그림은 제외")
                                : QStringLiteral("자석 꺼짐"),
                             4000);
  });
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("easy_draw")), QStringLiteral("쉽게 그려볼까?"),
                          this, &MainWindow::startEasyDraw);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_area")), QStringLiteral("조사구역"),
                          this, &MainWindow::startEditSurveyArea);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_poly")), QStringLiteral("유구 면을 그려볼까?"),
                          this, &MainWindow::startEditFeaturePoly);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_line")), QStringLiteral("유구 선"),
                          this, &MainWindow::startEditFeatureLine);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("artifact")), QStringLiteral("유물 찍을까?"),
                          this, &MainWindow::startEditArtifact);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("line")), QStringLiteral("단면선"),
                          this, &MainWindow::startEditSectionLine);
  m_subToolbar->addAction(QStringLiteral("속성"), this, &MainWindow::startAttributeEditTool);
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("유구 면을 그리는 중 — 점을 찍고 Enter로 닫기. 작업 좌표계 → 제출 5179."), 8000);
#endif
}

void MainWindow::showSubToolsBasemap() {
#if KA_HGIS_HAS_QGIS
  hideSubTools();
  ensureDefaultBasemaps();
  showMapWorkspace();
  statusBar()->showMessage(QStringLiteral("위성과 지적도는 시작할 때 자동으로 올라옵니다."), 6000);
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
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("도면검수"),
                          this, &MainWindow::runChecklist);
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("export")), QStringLiteral("SHP패키지(5179)"),
                          this, &MainWindow::exportShpPackage);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("도면만들기"),
                          this, &MainWindow::openLayoutDesigner);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("도면PDF"),
                          this, &MainWindow::exportReportLayout);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("upload")), QStringLiteral("5179변환"),
                          this, &MainWindow::convertSelectedTo5179);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("다 그렸으면 도면을 만들고, 필요할 때만 업로드용으로 보내세요."),
      12000);
#endif
}


void MainWindow::updateNextActionStatus() {
  QString msg;
  if (m_surveyPath.isEmpty()) {
    msg = QStringLiteral("먼저 「새로 만들까?」로 오늘 현장을 만드세요.");
  } else {
#if KA_HGIS_HAS_QGIS
    const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
    const bool hasDraw = domainLayerCount() > 0;
#else
    const bool hasBg = false;
    const bool hasDraw = (m_stubSurveyArea + m_stubFeatures) > 0;
#endif
    if (!hasBg)
      msg = QStringLiteral("위성·지적이 없습니다. 더보기 → VWorld API 키를 확인하세요.");
    else if (!hasDraw)
      msg = QStringLiteral("「그려볼까?」로 구역을 그리세요.");
    else
      msg = QStringLiteral("다 그렸으면 「도면 만들까?」로 종이에 옮기세요.");
  }
  statusBar()->showMessage(msg);
}

void MainWindow::notify(Notice level, const QString& title, const QString& text,
                        const QString& details) {
#if KA_HGIS_HAS_QGIS
  if (m_messageBar) {
    Qgis::MessageLevel lv = Qgis::MessageLevel::Info;
    switch (level) {
      case Notice::Success:
        lv = Qgis::MessageLevel::Success;
        break;
      case Notice::Warning:
        lv = Qgis::MessageLevel::Warning;
        break;
      case Notice::Critical:
        lv = Qgis::MessageLevel::Critical;
        break;
      case Notice::Info:
        break;
    }
    if (details.isEmpty())
      m_messageBar->pushMessage(title, text, lv);
    else
      m_messageBar->pushMessage(title, text, details, lv);
    return;
  }
#endif
  const QString body = details.isEmpty() ? text : text + QLatin1Char('\n') + details;
  if (level == Notice::Warning || level == Notice::Critical)
    QMessageBox::warning(this, title, body);
  else
    QMessageBox::information(this, title, body);
}

#if KA_HGIS_HAS_QGIS
// 위성·배경 타일이 일부 실패해 반만 그려진 채 캐시에 굳는 문제의 자동 복구.
// 실패가 감지된 타일 레이어만 1.8초 뒤 다시 그린다(같은 화면에서 레이어당 최대 3회).
void MainWindow::healTileLayer(QgsRasterLayer* layer) {
  if (!layer || layer->providerType() != QLatin1String("wms")) return;
  const QString id = layer->id();
  if (m_tileHealCount.value(id) >= 3 || m_tileHealPending.contains(id)) return;
  const int attempt = ++m_tileHealCount[id];
  m_tileHealPending.insert(id);
  QPointer<QgsRasterLayer> guard(layer);
  QTimer::singleShot(1800, this, [this, guard, id, attempt]() {
    m_tileHealPending.remove(id);
    if (!guard) return;
    if (m_canvas && m_canvas->isDrawing()) return;
    KaCrashGuard::logLine(QStringLiteral("[render] '%1' 빠진 타일 자동 재시도 %2/3")
                              .arg(guard->name())
                              .arg(attempt));
    guard->triggerRepaint();
  });
}
#endif

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("centralRoot"));
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  KaTheme::excludeMapSurface(m_canvas);
  m_canvas->setCanvasColor(KaTheme::tokens().canvasNeutral);
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  // 병렬 렌더는 꺼 둔다. ParallelJob이 provider_wms의 중첩 이벤트 루프와 겹치면
  // deleteLater가 ACCESS_VIOLATION을 낸다(현장 덤프 2026-08-31). KaApplication의
  // qgis/parallel_rendering=false와 같은 값이어야 서로 싸우지 않는다.
  m_canvas->setParallelRenderingEnabled(false);
  // 미리보기 작업은 켠다 — 이게 꺼져 있으면 화면을 끄는 동안 캔버스가 비어
  // 흰 화면이 보인다. 스레드 풀이 2개 이상이라야 실제로 겹쳐서 돈다.
  m_canvas->setPreviewJobsEnabled(true);
  // 렌더가 끝날 때까지 기다리지 말고 받은 타일부터 화면에 올린다.
  // 한 장 그리는 데 2.4초가 걸리므로, 이 값이 크면 그 동안 화면이 비어 보인다.
  // 작을수록 위성이 타일 단위로 차오르며 채워져 깜빡임이 덜 보인다.
  m_canvas->setMapUpdateInterval(80);
  m_canvas->setAcceptDrops(true);
  m_canvas->setSegmentationTolerance(2.0);
  const QgsCoordinateReferenceSystem crs(m_workCrs);
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_panTool = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_panTool);
  m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  m_mapGrid->setEnabled(false);
  // 팬·줌에서 지도가 하얗게 비는 원인을 좁히기 위한 계측.
  // KA_HGIS_TRACE_RENDER=1 로 켠다. 렌더가 몇 번 시작·취소되는지, 레이어 목록이
  // 팬 도중에 갈리는지가 로그에 남는다.
  if (!qgetenv("KA_HGIS_TRACE_RENDER").isEmpty()) {
    auto* seq = new int(0);
    connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this, seq]() {
      const QgsRectangle e = m_canvas->extent();
      KaCrashGuard::logLine(
          QStringLiteral("[trace %1] extentsChanged scale=%2 draw=%3 size=%4x%5 dpr=%6 dpi=%7")
              .arg(++*seq)
              .arg(m_canvas->scale(), 0, 'f', 0)
              .arg(m_canvas->isDrawing() ? 1 : 0)
              .arg(m_canvas->width())
              .arg(m_canvas->height())
              .arg(m_canvas->mapSettings().devicePixelRatio(), 0, 'f', 2)
              .arg(m_canvas->mapSettings().outputDpi(), 0, 'f', 1));
    });
    connect(m_canvas, &QgsMapCanvas::renderStarting, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] renderStarting").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::mapCanvasRefreshed, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] refreshed").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::layersChanged, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] layersChanged  n=%2")
                                .arg(++*seq)
                                .arg(m_canvas->layers().size()));
    });
  }
  // 타일 일부 실패(요청 제한·순간 네트워크 오류)로 위성지도가 반만 보이면
  // 해당 레이어만 자동으로 다시 그린다. 화면을 움직이면 재시도 카운터 초기화.
  connect(m_canvas, &QgsMapCanvas::renderErrorOccurred, this,
          [this](const QString& err, QgsMapLayer* layer) {
            KaCrashGuard::logLine(
                QStringLiteral("[render] 렌더 오류(%1): %2")
                    .arg(layer ? layer->name() : QStringLiteral("-"), err.left(160)));
            healTileLayer(qobject_cast<QgsRasterLayer*>(layer));
          });
  connect(QgsApplication::messageLog(),
          qOverload<const QString&, const QString&, Qgis::MessageLevel>(
              &QgsMessageLog::messageReceived),
          this, [this](const QString& message, const QString& tag, Qgis::MessageLevel level) {
            if (level != Qgis::MessageLevel::Warning && level != Qgis::MessageLevel::Critical)
              return;
            const bool tileIssue = tag.contains(QLatin1String("WMS"), Qt::CaseInsensitive) ||
                                   message.contains(QLatin1String("tile"), Qt::CaseInsensitive) ||
                                   message.contains(QStringLiteral("타일"));
            if (!tileIssue || !m_canvas) return;
            const QList<QgsMapLayer*> visible = m_canvas->layers();
            for (QgsMapLayer* ml : visible) healTileLayer(qobject_cast<QgsRasterLayer*>(ml));
          });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    m_tileHealCount.clear();
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* tool, QgsMapTool*) {
    if (m_actMeasure)
      m_actMeasure->setChecked(m_measureTool && tool == m_measureTool);
  });

  auto* treeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new QgsLayerTreeModel(treeRoot, this);
  model->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeRename, true);
  m_layerTree = new QgsLayerTreeView(central);
  m_layerTree->setObjectName(QStringLiteral("layerTree"));
  m_layerTree->setModel(model);
  m_layerTree->setFocusPolicy(Qt::StrongFocus);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragDropMode(QAbstractItemView::DragDrop);
  connect(model, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
    onLayerTreeRowsMoved();
  });
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_layerTree, &QWidget::customContextMenuRequested, this, &MainWindow::onLayerTreeContextMenu);
  connect(m_layerTree, &QTreeView::doubleClicked, this, &MainWindow::onLayerTreeDoubleClicked);
  m_bridge = new QgsLayerTreeMapCanvasBridge(treeRoot, m_canvas, this);
  m_bridge->setAutoSetupOnFirstLayer(false);

  m_canvas->setContextMenuPolicy(Qt::CustomContextMenu);
  m_canvas->installEventFilter(this);
  if (m_canvas->viewport())
    m_canvas->viewport()->installEventFilter(this);
  connect(m_canvas, &QWidget::customContextMenuRequested, this, &MainWindow::onMapContextMenu);
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, &MainWindow::onCanvasScaleChanged);
  connect(m_canvas, &QgsMapCanvas::xyCoordinates, this, [this](const QgsPointXY& p) {
    if (m_status) m_status->setCoordinate(p.x(), p.y());
  });
  m_status->setWorkCrs(m_workCrs);
  connect(m_status, &KaStatusBar::crsClicked, this, [this]() {
    QMenu menu(this);
    QAction* a86 = menu.addAction(QStringLiteral("중부원점 (EPSG:5186)"));
    QAction* a87 = menu.addAction(QStringLiteral("동부원점 (EPSG:5187)"));
    a86->setCheckable(true);
    a87->setCheckable(true);
    a86->setChecked(m_workCrs == QLatin1String("EPSG:5186"));
    a87->setChecked(m_workCrs == QLatin1String("EPSG:5187"));
    menu.addSeparator();
    QAction* note = menu.addAction(QStringLiteral("제출용 파일은 항상 EPSG:5179로 나갑니다"));
    note->setEnabled(false);
    const QAction* picked = menu.exec(QCursor::pos());
    if (picked == a86)
      setWorkCrs5186();
    else if (picked == a87)
      setWorkCrs5187();
  });
  connect(m_status, &KaStatusBar::renderingToggled, this, [this](bool on) {
    if (!m_canvas) return;
    m_canvas->setRenderFlag(on);
    if (on) m_canvas->refresh();
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* newTool, QgsMapTool*) {
    if (m_actSelect)
      m_actSelect->setChecked(newTool && m_selectTool && newTool == m_selectTool);
    if (m_actMeasure)
      m_actMeasure->setChecked(newTool && m_measureTool && newTool == m_measureTool);
    if (m_btnDraw) {
      const bool drawing = newTool && m_captureTool && newTool == m_captureTool;
      const bool subOpen = m_subToolbar && m_subToolbar->isVisible() &&
                           m_subToolsMode == QLatin1String("draw");
      m_btnDraw->setChecked(drawing || subOpen);
    }
  });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
    if (m_extentClampGuard || !m_canvas) return;
    if (m_canvas->isDrawing()) return;
    m_extentClampGuard = true;
    LayerOps::clampCanvasToKorea(m_canvas);
    m_extentClampGuard = false;
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  connect(m_canvas, &QgsMapCanvas::renderComplete, this, [this](QPainter*) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); syncThematicButtons(); });
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); syncThematicButtons(); });
  });
  // 레이어의 체크를 끄고 켜는 것도 아이콘에 그대로 따라와야 한다.
  if (QgsLayerTree* legendRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(legendRoot, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode*) { syncThematicButtons(); });
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);

  setupFileBrowser();

  // 1. 레이어 패널 (m_layersCard) - 상단 배치
  auto* layersCard = new QFrame(central);
  layersCard->setObjectName(QStringLiteral("layersCard"));
  m_layersCard = layersCard;
  auto* layersLay = new QVBoxLayout(layersCard);
  layersLay->setContentsMargins(6, 6, 6, 6);
  layersLay->setSpacing(6);

  auto* capLayers = new QLabel(QStringLiteral("레이어"), layersCard);
  capLayers->setObjectName(QStringLiteral("cardCaption"));

  auto* layersInner = new QFrame(layersCard);
  layersInner->setObjectName(QStringLiteral("layersInner"));
  auto* layersInnerLay = new QVBoxLayout(layersInner);
  layersInnerLay->setContentsMargins(4, 4, 4, 4);
  layersInnerLay->addWidget(m_layerTree, 1);
  m_layerEmpty = new QLabel(
      QStringLiteral("레이어가 없습니다.\n파일함에서 SHP·DXF·DWG를 끌어 넣거나\n위성·지적 배경을 올리세요."),
      layersInner);
  m_layerEmpty->setObjectName(QStringLiteral("emptyState"));
  m_layerEmpty->setAlignment(Qt::AlignCenter);
  m_layerEmpty->setWordWrap(true);
  layersInnerLay->addWidget(m_layerEmpty, 1);
#if KA_HGIS_HAS_QGIS
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    refreshLayerEmptyState();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    refreshLayerEmptyState();
  });
#endif
  refreshLayerEmptyState();

  auto* addBtn = new QToolButton(layersCard);
  addBtn->setObjectName(QStringLiteral("btnAddLayer"));
  addBtn->setText(QStringLiteral("+ 추가"));
  addBtn->setToolTip(QStringLiteral("SHP·DXF·GPKG 등 파일을 도면 레이어로 불러옵니다"));
  addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(addBtn, &QToolButton::clicked, this, &MainWindow::addUserLayer);

  auto* delBtn = new QToolButton(layersCard);
  delBtn->setObjectName(QStringLiteral("btnRemoveLayer"));
  delBtn->setIcon(KaIcons::icon(QStringLiteral("trash")));
  delBtn->setText(QStringLiteral("삭제"));
  delBtn->setToolTip(QStringLiteral("선택한 레이어를 목록에서 뺍니다 (파일은 남습니다)"));
  delBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(delBtn, &QToolButton::clicked, this, &MainWindow::removeSelectedLayers);

  auto* upBtn = new QToolButton(layersCard);
  upBtn->setObjectName(QStringLiteral("btnLayerUp"));
  upBtn->setText(QStringLiteral("▲ 위로"));
  upBtn->setToolTip(QStringLiteral("선택한 레이어 순서를 위로 올립니다 (지도에서도 위)"));
  connect(upBtn, &QToolButton::clicked, this, [this]() { moveSelectedLayer(-1); });

  auto* downBtn = new QToolButton(layersCard);
  downBtn->setObjectName(QStringLiteral("btnLayerDown"));
  downBtn->setText(QStringLiteral("▼ 아래로"));
  downBtn->setToolTip(QStringLiteral("선택한 레이어 순서를 아래로 내립니다 (지도에서도 아래)"));
  connect(downBtn, &QToolButton::clicked, this, [this]() { moveSelectedLayer(1); });

  auto* btnZoomMax = new QToolButton(layersCard);
  btnZoomMax->setObjectName(QStringLiteral("btnZoomMax"));
  btnZoomMax->setText(QStringLiteral("🔍 전체보기"));
  btnZoomMax->setToolTip(QStringLiteral("지도를 최대 전체 범위로 맞춤"));
  connect(btnZoomMax, &QToolButton::clicked, this, &MainWindow::zoomMapToFullMax);

  addBtn->setAutoRaise(true);
  delBtn->setAutoRaise(true);
  upBtn->setAutoRaise(true);
  downBtn->setAutoRaise(true);
  btnZoomMax->setAutoRaise(true);

  auto* layerBottomBar1 = new QHBoxLayout();
  layerBottomBar1->setSpacing(4);
  layerBottomBar1->addWidget(addBtn);
  layerBottomBar1->addWidget(delBtn);
  layerBottomBar1->addStretch(1);

  auto* layerBottomBar2 = new QHBoxLayout();
  layerBottomBar2->setSpacing(4);
  layerBottomBar2->addWidget(upBtn);
  layerBottomBar2->addWidget(downBtn);
  layerBottomBar2->addWidget(btnZoomMax);
  layerBottomBar2->addStretch(1);

  layersLay->addWidget(capLayers);
  layersLay->addWidget(layersInner, 1);
  layersLay->addLayout(layerBottomBar1);
  layersLay->addLayout(layerBottomBar2);

  // 2. 파일함 패널 (m_filesCard) - 하단 배치
  auto* filesCard = new QFrame(central);
  filesCard->setObjectName(QStringLiteral("filesCard"));
  m_filesCard = filesCard;
  auto* filesLay = new QVBoxLayout(filesCard);
  filesLay->setContentsMargins(6, 6, 6, 6);
  filesLay->setSpacing(6);

  auto* capFiles = new QLabel(QStringLiteral("파일함"), filesCard);
  capFiles->setObjectName(QStringLiteral("cardCaption"));
  capFiles->setProperty("class", QStringLiteral("cardCaptionFiles"));

  auto* pathBar = new QHBoxLayout();
  pathBar->setSpacing(4);
  auto* btnPc = new QToolButton(filesCard);
  btnPc->setText(QStringLiteral("내PC"));
  btnPc->setToolTip(QStringLiteral("내 컴퓨터 드라이브 목록으로 이동"));
  btnPc->setObjectName(QStringLiteral("btnBrowsePc"));
  connect(btnPc, &QToolButton::clicked, this, [this]() { goFileBrowserRoot(QString()); });

  auto* btnUp = new QToolButton(filesCard);
  btnUp->setText(QStringLiteral("⬆ 상위"));
  btnUp->setToolTip(QStringLiteral("상위 폴더(..)로 한 단계 이동"));
  btnUp->setObjectName(QStringLiteral("btnBrowseUp"));
  connect(btnUp, &QToolButton::clicked, this, [this]() {
    if (m_browserPath.isEmpty()) {
      goFileBrowserRoot(QString());
      return;
    }
    const QDir d(m_browserPath);
    const QString parent = QDir::cleanPath(d.absolutePath() + QStringLiteral("/.."));
    if (parent == QDir::cleanPath(m_browserPath) || parent.length() < 3)
      goFileBrowserRoot(QString());
    else
      goFileBrowserRoot(parent);
  });

  auto* btnC = new QToolButton(filesCard);
  btnC->setText(QStringLiteral("C:\\"));
  btnC->setToolTip(QStringLiteral("C 드라이브로 바로 이동"));
  btnC->setObjectName(QStringLiteral("btnBrowseC"));
  connect(btnC, &QToolButton::clicked, this, [this]() { goFileBrowserRoot(QStringLiteral("C:/")); });
  pathBar->addWidget(btnPc);
  pathBar->addWidget(btnUp);
  pathBar->addWidget(btnC);
  pathBar->addStretch(1);

  auto* pathBar2 = new QHBoxLayout();
  pathBar2->setSpacing(4);
  auto* btnDocs = new QToolButton(filesCard);
  btnDocs->setText(QStringLiteral("문서"));
  btnDocs->setToolTip(QStringLiteral("문서 폴더로 바로 이동"));
  btnDocs->setObjectName(QStringLiteral("btnBrowseDocs"));
  connect(btnDocs, &QToolButton::clicked, this, [this]() {
    goFileBrowserRoot(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  });

  auto* btnDesk = new QToolButton(filesCard);
  btnDesk->setText(QStringLiteral("바탕화면"));
  btnDesk->setToolTip(QStringLiteral("바탕화면 폴더로 바로 이동"));
  btnDesk->setObjectName(QStringLiteral("btnBrowseDesktop"));
  connect(btnDesk, &QToolButton::clicked, this, [this]() {
    goFileBrowserRoot(resolvedDesktopPath());
  });

  auto* btnPick = new QToolButton(filesCard);
  btnPick->setText(QStringLiteral("폴더…"));
  btnPick->setToolTip(QStringLiteral("조사 데이터 폴더 직접 선택"));
  btnPick->setObjectName(QStringLiteral("btnBrowseFolder"));
  connect(btnPick, &QToolButton::clicked, this, &MainWindow::browseDataFolder);
  pathBar2->addWidget(btnDocs);
  pathBar2->addWidget(btnDesk);
  pathBar2->addWidget(btnPick);
  pathBar2->addStretch(1);

  auto* filesInner = new QFrame(filesCard);
  filesInner->setObjectName(QStringLiteral("filesInner"));
  auto* filesInnerLay = new QVBoxLayout(filesInner);
  filesInnerLay->setContentsMargins(4, 4, 4, 4);
  filesInnerLay->addWidget(m_fileBrowser, 1);

  auto* filesHint = new QLabel(QStringLiteral("파일을 지도에 끌어 넣으면 레이어가 됩니다."), filesCard);
  filesHint->setObjectName(QStringLiteral("emptyState"));
  filesHint->setWordWrap(true);

  filesLay->addWidget(capFiles);
  filesLay->addLayout(pathBar);
  filesLay->addLayout(pathBar2);
  filesLay->addWidget(filesInner, 1);
  filesLay->addWidget(filesHint);

  // 3. 좌측 패널 수직 분할: 레이어(위) + 파일함(아래) 동시 노출
  auto* leftSplit = new QSplitter(Qt::Vertical, central);
  leftSplit->setObjectName(QStringLiteral("leftSplit"));
  m_leftSplit = leftSplit;
  leftSplit->setHandleWidth(8);
  leftSplit->setChildrenCollapsible(false);
  leftSplit->addWidget(layersCard);
  leftSplit->addWidget(filesCard);
  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 2);
  leftSplit->setSizes({380, 260});
  leftSplit->setMinimumWidth(220);
  leftSplit->setMaximumWidth(720);
  leftSplit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  auto* mapCard = new QFrame(central);
  mapCard->setObjectName(QStringLiteral("mapCard"));
  auto* mapLay = new QVBoxLayout(mapCard);
  mapLay->setContentsMargins(4, 4, 4, 4);
  mapLay->setSpacing(4);
  m_messageBar = new QgsMessageBar(mapCard);
  m_messageBar->setObjectName(QStringLiteral("mapMessageBar"));
  mapLay->addWidget(m_messageBar, 0);
  mapLay->addWidget(m_canvas, 1);

  auto* scaleBar = new QHBoxLayout();
  scaleBar->setSpacing(4);
  m_scaleEdit = m_status->scaleEdit();
  m_scaleCombo = m_status->scaleCombo();
  connect(m_scaleEdit, &QLineEdit::returnPressed, this, &MainWindow::applyMapScaleFromUi);
  connect(m_scaleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
    if (!m_scaleCombo || !m_scaleEdit) return;
    const int s = m_scaleCombo->itemData(idx).toInt();
    if (s > 0) {
      m_scaleEdit->setText(QString::number(s));
      applyMapScaleFromUi();
    }
  });
  m_mapGridCheck = new QCheckBox(QStringLiteral("좌표격자"), mapCard);
  m_mapGridCheck->setObjectName(QStringLiteral("mapGridCheck"));
  m_mapGridCheck->setToolTip(QStringLiteral("맵에 좌표 격자를 켭니다. Shift를 누른 채 켜면 경위도입니다."));
  connect(m_mapGridCheck, &QCheckBox::toggled, this, &MainWindow::toggleMapGrid);
  m_mapGridStep = new QDoubleSpinBox(mapCard);
  m_mapGridStep->setObjectName(QStringLiteral("mapGridStep"));
  m_mapGridStep->setRange(1.0, 10000.0);
  m_mapGridStep->setDecimals(0);
  m_mapGridStep->setSuffix(QStringLiteral(" m"));
  m_mapGridStep->setValue(20);
  m_mapGridStep->setMaximumWidth(80);
  m_mapGridStep->setToolTip(QStringLiteral("격자 간격(미터). 시굴격자 이동 때도 이 간격에 붙습니다."));
  connect(m_mapGridStep, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridRot = new QDoubleSpinBox(mapCard);
  m_mapGridRot->setRange(0.0, 360.0);
  m_mapGridRot->setDecimals(1);
  m_mapGridRot->setSuffix(QStringLiteral(" °"));
  m_mapGridRot->setValue(0);
  m_mapGridRot->setMaximumWidth(70);
  m_mapGridRot->setToolTip(QStringLiteral("격자 회전 (동쪽 기준 시계 방향)"));
  connect(m_mapGridRot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridWidth = new QDoubleSpinBox(mapCard);
  m_mapGridWidth->setRange(0.5, 5.0);
  m_mapGridWidth->setDecimals(1);
  m_mapGridWidth->setSingleStep(0.1);
  m_mapGridWidth->setValue(1.2);
  m_mapGridWidth->setMaximumWidth(60);
  m_mapGridWidth->setToolTip(QStringLiteral("격자 선 굵기"));
  connect(m_mapGridWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridDash = new QComboBox(mapCard);
  m_mapGridDash->addItem(QStringLiteral("실선"), static_cast<int>(Qt::SolidLine));
  m_mapGridDash->addItem(QStringLiteral("점선"), static_cast<int>(Qt::DashLine));
  m_mapGridDash->addItem(QStringLiteral("점"), static_cast<int>(Qt::DotLine));
  m_mapGridDash->setCurrentIndex(1);
  m_mapGridDash->setMaximumWidth(70);
  m_mapGridDash->setToolTip(QStringLiteral("격자 선 모양"));
  connect(m_mapGridDash, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  auto* gridDetail = new QWidget(mapCard);
  gridDetail->setObjectName(QStringLiteral("gridDetail"));
  auto* gridDetailLay = new QHBoxLayout(gridDetail);
  gridDetailLay->setContentsMargins(0, 0, 0, 0);
  gridDetailLay->setSpacing(3);
  gridDetailLay->addWidget(m_mapGridStep);
  gridDetailLay->addWidget(new QLabel(QStringLiteral("회전"), gridDetail));
  gridDetailLay->addWidget(m_mapGridRot);
  gridDetailLay->addWidget(new QLabel(QStringLiteral("굵기"), gridDetail));
  gridDetailLay->addWidget(m_mapGridWidth);
  gridDetailLay->addWidget(m_mapGridDash);
  gridDetail->setVisible(false);
  connect(m_mapGridCheck, &QCheckBox::toggled, gridDetail, &QWidget::setVisible);
  scaleBar->addWidget(m_mapGridCheck);
  scaleBar->addWidget(gridDetail);
  scaleBar->addStretch(1);
  mapLay->addLayout(scaleBar);

  // 왼쪽 패널 ↔ 지도 사이를 끌어서 나눌 수 있게 한다. 나눈 폭은 창 상태와
  // 같이 저장돼 다음에 열 때 그대로 온다(MainWindow/mainSplit).
  m_mainSplit = new QSplitter(Qt::Horizontal, central);
  m_mainSplit->setObjectName(QStringLiteral("mainSplit"));
  m_mainSplit->setHandleWidth(10);
  m_mainSplit->setChildrenCollapsible(false);
  m_mainSplit->addWidget(leftSplit);
  m_mainSplit->addWidget(mapCard);
  m_mainSplit->setStretchFactor(0, 0);
  m_mainSplit->setStretchFactor(1, 1);
  m_mainSplit->setSizes({268, 1012});
  root->addWidget(m_mainSplit, 1);
#else
  root->addWidget(new QLabel(QStringLiteral("QGIS SDK 스텁 모드"), central), 1);
  setCentralWidget(central);
#endif
#if KA_HGIS_HAS_QGIS
  m_viewTabs = new QTabWidget(this);
  m_viewTabs->setObjectName(QStringLiteral("viewTabs"));
  m_viewTabs->setDocumentMode(true);
  m_viewTabs->setTabsClosable(true);
  m_viewTabs->setMovable(false);
  m_startPage = new KaStartPage(m_viewTabs);
  connect(m_startPage, &KaStartPage::newSurveyRequested, this, &MainWindow::newSurvey);
  connect(m_startPage, &KaStartPage::openRequested, this, &MainWindow::openProject);
  connect(m_startPage, &KaStartPage::recentOpened, this, &MainWindow::openRecentSurvey);
  connect(m_startPage, &KaStartPage::forgetRequested, this, [this](const QString& path) {
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, path);
    if (m_startPage) m_startPage->reload();
  });
  m_mapPage = central;
  const int homeIdx = m_viewTabs->addTab(m_startPage, KaIcons::icon(QStringLiteral("new")),
                                         QStringLiteral("홈"));
  const int mapIdx = m_viewTabs->addTab(central, KaIcons::icon(QStringLiteral("map")),
                                        QStringLiteral("지도"));
  if (QTabBar* bar = m_viewTabs->tabBar()) {
    bar->setTabButton(homeIdx, QTabBar::RightSide, nullptr);
    bar->setTabButton(mapIdx, QTabBar::RightSide, nullptr);
  }
  connect(m_viewTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onViewTabCloseRequested);
  connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int i) {
    if (!m_viewTabs) return;
    QWidget* page = m_viewTabs->widget(i);
    if (m_startPage && page == m_startPage)
      m_startPage->reload();
    if (m_mapPage && page == m_mapPage) {
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
    } else {
      hideSubTools();
    }
    if (m_drawingStudio && page == m_drawingStudio)
      QTimer::singleShot(0, this, [this]() {
        if (!m_drawingStudio) return;
        m_drawingStudio->refreshMapFromProject();
        m_drawingStudio->centerOnMapCanvas();
      });
    if (m_sectionStudio && page == m_sectionStudio)
      QTimer::singleShot(0, this, [this]() {
        if (m_sectionStudio) m_sectionStudio->refreshLayers();
      });
  });
  m_viewTabs->setCurrentWidget(m_startPage);
  setCentralWidget(m_viewTabs);
  m_autosaveTimer = new QTimer(this);
  m_autosaveTimer->setInterval(20000);
  connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::persistSurveyWork);
  m_autosaveTimer->start();
  QTimer::singleShot(0, this, &MainWindow::restoreLastSurvey);
#endif

}

void MainWindow::setupWorkPanel() {
  auto* dock = new QDockWidget(QStringLiteral("작업 제어"), this);
  dock->setObjectName(QStringLiteral("workDock"));
  dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  auto* box = new QWidget(dock);
  auto* lay = new QVBoxLayout(box);
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("원하는 작업을 누르세요"), box);
  m_workHint = new QLabel(box);
  m_workHint->setObjectName(QStringLiteral("workHint"));
  m_workHint->setWordWrap(true);
  m_workList = new QListWidget(box);
  m_workList->setObjectName(QStringLiteral("workControlList"));
  m_workList->setSpacing(3);
  connect(m_workList, &QListWidget::itemClicked, this, &MainWindow::onWorkControlClicked);
  lay->addWidget(title);
  lay->addWidget(m_workHint);
  lay->addWidget(m_workList, 1);
  dock->setWidget(box);
  addDockWidget(Qt::RightDockWidgetArea, dock);
  dock->setMinimumWidth(220);
  dock->hide();
  refreshWorkPanel();
}

void MainWindow::refreshWorkPanel() {
  if (!m_workList) return;
  const QJsonObject st = buildProjectState();
#if KA_HGIS_HAS_QGIS
  const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
#else
  const bool hasBg = false;
#endif
  const int errCount = lastChecklistErrorCount();
  const bool surveyReady = !m_surveyPath.isEmpty() ||
                           st.value(QStringLiteral("survey_area_count")).toInt() > 0;
  QJsonObject st2 = st;
  if (surveyReady && st2.value(QStringLiteral("survey_area_count")).toInt() == 0)
    st2.insert(QStringLiteral("layer_count"), 1);
  const auto steps = WorkflowGuide::evaluate(st2, hasBg, errCount, m_packageCreated);

  struct Extra {
    QString id;
    QString title;
    QString hint;
    bool done;
  };
  const QList<Extra> extras = {
      {QStringLiteral("action_edit_attrs"), QStringLiteral("속성 고치기"),
       QStringLiteral("그린 도형을 클릭해 종류·시대를 넣습니다."),
       st.value(QStringLiteral("has_kind_period")).toBool() &&
           st.value(QStringLiteral("feature_poly_count")).toInt() > 0},
      {QStringLiteral("action_section"), QStringLiteral("단면선 그리기"),
       QStringLiteral("층위·단면 기준선을 그립니다."), false},
      {QStringLiteral("action_import_csv"), QStringLiteral("CSV 기준점"),
       QStringLiteral("GPS CSV를 가져와 기준점을 채웁니다."),
       st.value(QStringLiteral("control_points_count")).toInt() >= 2},
      {QStringLiteral("action_drawing_studio"), QStringLiteral("도면 만들기"),
       QStringLiteral("조사구역도 등 5종 PDF를 미리보고 저장합니다."), false},
  };

  const QString cur = m_workList->currentItem()
                          ? m_workList->currentItem()->data(Qt::UserRole).toString()
                          : QString();
  m_workList->clear();
  auto addItem = [&](const QString& id, const QString& title, const QString& hint, bool done) {
    auto* it = new QListWidgetItem(
        QStringLiteral("%1  %2\n    %3")
            .arg(done ? QStringLiteral("완료") : QStringLiteral("실행"), title, hint),
        m_workList);
    it->setData(Qt::UserRole, id);
    it->setToolTip(hint);
  };
  for (const auto& s : steps)
    addItem(s.actionId, s.title, s.completionHint, s.complete);
  for (const auto& e : extras)
    addItem(e.id, e.title, e.hint, e.done);

  if (!cur.isEmpty()) {
    for (int i = 0; i < m_workList->count(); ++i) {
      if (m_workList->item(i)->data(Qt::UserRole).toString() == cur) {
        m_workList->setCurrentRow(i);
        break;
      }
    }
  }
  if (m_workHint) {
    QString next = QStringLiteral("아무 항목이나 눌러 바로 실행합니다.");
    for (const auto& s : steps) {
      if (!s.complete) {
        next = QStringLiteral("다음: %1 — %2").arg(s.title, s.completionHint);
        break;
      }
    }
    m_workHint->setText(next);
  }
}

void MainWindow::onWorkControlClicked(QListWidgetItem* item) {
  if (!item) return;
  const QString id = item->data(Qt::UserRole).toString();
  if (id == QLatin1String("action_new_survey"))
    newSurvey();
  else if (id == QLatin1String("action_add_basemap"))
    showSubToolsBasemap();
  else if (id == QLatin1String("action_digitize_area"))
    startEditSurveyArea();
  else if (id == QLatin1String("action_digitize_feature"))
    startEditFeaturePoly();
  else if (id == QLatin1String("action_add_control_point"))
    addControlPoint();
  else if (id == QLatin1String("action_run_checklist"))
    runChecklist();
  else if (id == QLatin1String("action_export_package"))
    exportShpPackage();
  else if (id == QLatin1String("action_edit_attrs"))
    startAttributeEditTool();
  else if (id == QLatin1String("action_section"))
    startEditSectionLine();
  else if (id == QLatin1String("action_import_csv"))
    importControlCsv();
  else if (id == QLatin1String("action_drawing_studio"))
    openLayoutDesigner();
  refreshWorkPanel();
}









void MainWindow::rebuildLayouts() {
#if KA_HGIS_HAS_QGIS
  const int n = LayoutService::rebuildDefaultLayouts(QgsProject::instance());
  statusBar()->showMessage(QStringLiteral("도면 5종을 다시 만들었습니다 (%1)").arg(n), 6000);
  openLayoutDesigner();
#endif
}

void MainWindow::openLayoutDesigner() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_terrain3dStudio && m_viewTabs->currentWidget() == m_terrain3dStudio) {
    placeTerrain3dOnSheet();
    return;
  }
  if (m_drawingStudio && m_viewTabs->indexOf(m_drawingStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_drawingStudio);
    hideSubTools();
    m_drawingStudio->refreshMapFromProject();
    m_drawingStudio->centerOnMapCanvas();
    return;
  }
  double w = 297.0, h = 210.0;
  if (!KaDrawingStudio::promptPaper(this, &w, &h))
    return;
  if (!m_drawingStudio) {
    m_drawingStudio = new KaDrawingStudio(QgsProject::instance(), m_canvas, w, h, this);
    m_drawingStudio->setAttribute(Qt::WA_DeleteOnClose, false);
  } else {
    m_drawingStudio->resetPaper(w, h);
  }
  m_drawingStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_drawingStudio) < 0)
    m_viewTabs->addTab(m_drawingStudio, KaIcons::icon(QStringLiteral("pdf")),
                       QStringLiteral("레이아웃"));
  m_viewTabs->setCurrentWidget(m_drawingStudio);
  hideSubTools();
  m_drawingStudio->refreshMapFromProject();
  m_drawingStudio->centerOnMapCanvas();
  statusBar()->showMessage(QStringLiteral("조판입니다. 좌표점은 용지 아래 아이콘으로 찍습니다."), 6000);
#endif
}

void MainWindow::placeTerrain3dOnSheet() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs || !m_terrain3dStudio)
    return;
  if (!m_terrain3dStudio->hasScene()) {
    QMessageBox::information(this, QStringLiteral("입체지형 도면출력"),
                             QStringLiteral("먼저 「화면을 입체로」로 지금 지도를 만드세요."));
    return;
  }
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png)) {
    QMessageBox::warning(this, QStringLiteral("입체지형 도면출력"),
                         QStringLiteral("입체지형 그림을 만들지 못했습니다."));
    return;
  }
  Terrain3dLayoutService::SheetSpec spec;
  spec.pngPath = png;
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (m_terrain3dStudio->groundExtent(&x0, &y0, &x1, &y1))
    spec.groundExtent = QgsRectangle(x0, y0, x1, y1);
  spec.crs.createFromUserInput(m_terrain3dStudio->workCrsLabel());
  spec.visibleWidthM = m_terrain3dStudio->visibleWidthM(1600, 1000);
  spec.yawDegFromNorth = m_terrain3dStudio->northYawDeg();
  spec.crsLabel = m_terrain3dStudio->workCrsLabel();
  spec.demName = m_terrain3dStudio->demDisplayName();
  spec.zMin = m_terrain3dStudio->zMin();
  spec.zMax = m_terrain3dStudio->zMax();
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->detachSheet();
  QString err;
  if (Terrain3dLayoutService::buildSheet(QgsProject::instance(), spec, &err).isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("입체지형 도면출력"),
                         err.isEmpty() ? QStringLiteral("입체지형 조판을 만들지 못했습니다.") : err);
    return;
  }
  openTerrain3dLayout();
  statusBar()->showMessage(QStringLiteral("입체지형 조판입니다. 범례·방위·축척이 있습니다."), 6000);
#endif
}

void MainWindow::openTerrain3dLayout() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (!m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio = new KaTerrain3dLayoutStudio(QgsProject::instance(), this);
    m_terrain3dLayoutStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_terrain3dLayoutStudio, &KaTerrain3dLayoutStudio::requestScale, this,
            &MainWindow::applyTerrain3dSheetScale);
    connect(m_terrain3dLayoutStudio, &KaTerrain3dLayoutStudio::overlaysChanged, this,
            &MainWindow::refreshTerrain3dDrapeAndSheet);
  }
  m_terrain3dLayoutStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_terrain3dLayoutStudio) < 0)
    m_viewTabs->addTab(m_terrain3dLayoutStudio, KaIcons::icon(QStringLiteral("terrain_3d")),
                       QStringLiteral("입체지형 조판"));
  m_viewTabs->setCurrentWidget(m_terrain3dLayoutStudio);
  hideSubTools();
  m_terrain3dLayoutStudio->attachSheet();
#endif
}

QString MainWindow::terrain3dSheetPngPath() const {
  QString dir = m_surveyPath.isEmpty() ? QString() : QFileInfo(m_surveyPath).absolutePath();
#if KA_HGIS_HAS_QGIS
  if (dir.isEmpty() && QgsProject::instance())
    dir = QgsProject::instance()->homePath();
#endif
  if (dir.isEmpty())
    dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  return QDir(dir).filePath(QStringLiteral("입체지형_조판.png"));
}

void MainWindow::applyTerrain3dSheetScale(int denominator) {
#if KA_HGIS_HAS_QGIS
  if (!m_terrain3dStudio || !m_terrain3dStudio->hasScene())
    return;
  const int denom = std::max(10, denominator);
  double widthMm = Terrain3dLayoutService::pictureWidthMm(QgsProject::instance());
  if (widthMm < 8.0)
    widthMm = 300.0;
  const double targetGroundM = static_cast<double>(denom) * (widthMm / 1000.0);
  m_terrain3dStudio->setVisibleWidthM(targetGroundM, 1600, 1000);
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png)) {
    statusBar()->showMessage(QStringLiteral("입체지형 그림을 다시 만들지 못했습니다."), 5000);
    return;
  }
  QString err;
  if (!Terrain3dLayoutService::replacePicture(QgsProject::instance(), png, &err)) {
    statusBar()->showMessage(err.isEmpty() ? QStringLiteral("그림을 바꾸지 못했습니다.") : err, 5000);
    return;
  }
  if (!Terrain3dLayoutService::applyScale(QgsProject::instance(), denom, &err)) {
    statusBar()->showMessage(err.isEmpty() ? QStringLiteral("축척을 맞추지 못했습니다.") : err, 5000);
    return;
  }
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->attachSheet();
  statusBar()->showMessage(QStringLiteral("입체지형을 축척 1 : %1에 맞췄습니다.").arg(denom), 5000);
#else
  Q_UNUSED(denominator);
#endif
}

void MainWindow::refreshTerrain3dDrapeAndSheet() {
#if KA_HGIS_HAS_QGIS
  if (!m_terrain3dStudio || !m_terrain3dStudio->hasScene())
    return;
  m_terrain3dStudio->refreshDrape();
  if (!m_terrain3dLayoutStudio)
    return;
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png))
    return;
  QString err;
  Terrain3dLayoutService::replacePicture(QgsProject::instance(), png, &err);
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->attachSheet();
#endif
}

void MainWindow::openSectionDesigner() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_sectionStudio && m_viewTabs->indexOf(m_sectionStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_sectionStudio);
    hideSubTools();
    m_sectionStudio->refreshLayers();
    return;
  }
  if (!m_sectionStudio) {
    m_sectionStudio = new KaSectionDrawingStudio(QgsProject::instance(), this);
    m_sectionStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_sectionStudio, &KaSectionDrawingStudio::geoTiffAddRequested, this,
            [this](const QString& path) {
              if (path.isEmpty()) return;
              const QString crs = m_sectionStudio
                  ? m_sectionStudio->selectedCrsAuthId()
                  : QStringLiteral("EPSG:5187");
              if (!addSectionGeoTiffFromPath(path, crs)) {
                QMessageBox::warning(this, QStringLiteral("GeoTIFF 추가"),
                                     QStringLiteral("단면 GeoTIFF를 열지 못했습니다.\n%1").arg(path));
                return;
              }
            });
  }
  m_sectionStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_sectionStudio) < 0)
    m_viewTabs->addTab(m_sectionStudio, KaIcons::icon(QStringLiteral("section")),
                       QStringLiteral("단면도"));
  m_viewTabs->setCurrentWidget(m_sectionStudio);
  hideSubTools();
  m_sectionStudio->refreshLayers();
  statusBar()->showMessage(QStringLiteral("용지 눈금이 준비되었습니다. GeoTIFF 추가로 단면을 맞추세요."), 6000);
#endif
}

void MainWindow::openTerrain3dStudio() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_terrain3dStudio && m_viewTabs->indexOf(m_terrain3dStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_terrain3dStudio);
    hideSubTools();
    return;
  }
  if (!m_terrain3dStudio) {
    m_terrain3dStudio = new KaTerrain3dStudio(QgsProject::instance(), m_canvas, this);
    m_terrain3dStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_terrain3dStudio, &KaTerrain3dStudio::requestDrawingStudio, this,
            &MainWindow::placeTerrain3dOnSheet);
  }
  m_terrain3dStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_terrain3dStudio) < 0)
    m_viewTabs->addTab(m_terrain3dStudio, KaIcons::icon(QStringLiteral("terrain_3d")),
                       QStringLiteral("입체지형"));
  m_viewTabs->setCurrentWidget(m_terrain3dStudio);
  hideSubTools();
  statusBar()->showMessage(QStringLiteral("지금 지도 화면을 고해상 입체로 만듭니다."), 6000);
#endif
}

void MainWindow::rememberSurvey(const QString& path, const QString& name) {
  QSettings st = RecentSurveys::userSettings();
  RecentSurveys::remember(st, path, name);
  if (m_startPage)
    m_startPage->reload();
}

void MainWindow::showHomePage() {
#if KA_HGIS_HAS_QGIS
  if (m_viewTabs && m_startPage) {
    m_startPage->reload();
    m_viewTabs->setCurrentWidget(m_startPage);
  }
#endif
}

void MainWindow::showMapWorkspace() {
#if KA_HGIS_HAS_QGIS
  if (m_viewTabs && m_mapPage)
    m_viewTabs->setCurrentWidget(m_mapPage);
  QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
#endif
}

void MainWindow::openRecentSurvey(const QString& path) {
  if (path.isEmpty() || !QFile::exists(path)) {
    QMessageBox::warning(this, QStringLiteral("최근 조사"),
                         QStringLiteral("파일이 없습니다.\n%1").arg(path));
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, path);
    if (m_startPage) m_startPage->reload();
    return;
  }
  const QString ext = QFileInfo(path).suffix().toLower();
  if (ext == QLatin1String("gpkg")) {
    if (openSurveyGpkg(path)) {
      rememberSurvey(path, QFileInfo(path).completeBaseName());
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
      showMapWorkspace();
    }
    return;
  }
#if KA_HGIS_HAS_QGIS
  if (!QgsProject::instance()->read(path)) {
    QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("프로젝트를 열 수 없습니다."));
    return;
  }
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  if (m_canvas) m_canvas->refresh();
  ensureDefaultBasemaps();
  rememberSurvey(path, QFileInfo(path).completeBaseName());
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
  showMapWorkspace();
  updateNextActionStatus();
#endif
}

void MainWindow::onViewTabCloseRequested(int index) {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs || index < 0)
    return;
  QWidget* w = m_viewTabs->widget(index);
  if (!w || (w != m_drawingStudio && w != m_sectionStudio && w != m_terrain3dStudio &&
             w != m_terrain3dLayoutStudio))
    return;
  m_viewTabs->removeTab(index);
  w->hide();
  if (m_mapPage)
    m_viewTabs->setCurrentWidget(m_mapPage);
  else
    m_viewTabs->setCurrentIndex(0);
#else
  Q_UNUSED(index);
#endif
}





void MainWindow::newSurvey() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("새 조사"));
  dlg.setMinimumWidth(420);
  auto* form = new QFormLayout(&dlg);
  form->setSpacing(12);
  form->setContentsMargins(20, 20, 20, 16);
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("예: 병산동"));
  nameEdit->setMinimumHeight(36);
  auto* crsRow = new QHBoxLayout();
  auto* btn5186 = new QPushButton(QStringLiteral("5186  중부원점"), &dlg);
  auto* btn5187 = new QPushButton(QStringLiteral("5187  동부원점"), &dlg);
  for (auto* b : {btn5186, btn5187}) {
    b->setCheckable(true);
    b->setMinimumHeight(40);
    b->setCursor(Qt::PointingHandCursor);
  }
  const bool use5187 = m_workCrs.contains(QLatin1String("5187"));
  btn5186->setChecked(!use5187);
  btn5187->setChecked(use5187);
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
  });
  crsRow->addWidget(btn5186, 1);
  crsRow->addWidget(btn5187, 1);
  auto* tip = new QLabel(QStringLiteral("나중에 「도면만들기」옆에서 업로드용으로 바꿀 수 있습니다."), &dlg);
  tip->setWordWrap(true);
  form->addRow(QStringLiteral("조사명"), nameEdit);
  form->addRow(QStringLiteral("작업 좌표계"), crsRow);
  form->addRow(tip);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("다음"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString name = nameEdit->text().trimmed();
  if (name.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("새 조사"), QStringLiteral("조사명을 입력하세요."));
    return;
  }
  m_workCrs = btn5187->isChecked() ? QStringLiteral("EPSG:5187") : QStringLiteral("EPSG:5186");
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("저장 폴더"));
  if (dir.isEmpty()) return;
#if KA_HGIS_HAS_QGIS
  // Drop the previous survey from the legend before creating the GPKG so
  // open layers do not lock the old file, and 새 조사 always starts clean.
  hideSubTools();
  stopAlignSession();
  stopCaptureTool();
  m_editLayer = nullptr;
  m_committedUndo.clear();
  LayerOps::removeSurveyDomainLayers(QgsProject::instance());
  refreshLayerEmptyState();
#endif
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
  applyStartupMap();
#endif
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186")))
    b86->setChecked(!m_workCrs.contains(QLatin1String("5187")));
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187")))
    b87->setChecked(m_workCrs.contains(QLatin1String("5187")));
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(name));
  rememberSurvey(path, name);
  showMapWorkspace();
  updateNextActionStatus();
  refreshWorkPanel();
}

void MainWindow::refreshLayerEmptyState() {
#if KA_HGIS_HAS_QGIS
  const bool empty = !QgsProject::instance() || QgsProject::instance()->mapLayers().isEmpty();
  if (m_layerEmpty) m_layerEmpty->setVisible(empty);
  if (m_layerTree) m_layerTree->setVisible(!empty);
#else
  if (m_layerEmpty) m_layerEmpty->setVisible(true);
#endif
}

void MainWindow::ensureDefaultBasemaps() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  bool hasSat = false;
  bool hasCad = false;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n.contains(QStringLiteral("위성")))
      hasSat = true;
    if (n.contains(QStringLiteral("VWorld")) && n.contains(QStringLiteral("지적")))
      hasCad = true;
    else if (n == QLatin1String("지적") || n.startsWith(QLatin1String("지적 본번")) ||
             n.startsWith(QLatin1String("지적 부번")) || n.startsWith(QLatin1String("지적(")))
      hasCad = true;
  }
  const QString key = VworldSettings::loadApiKey();
  QString satErr;
  QString cadErr;
  // Add without canvas so LayerOps does not rewrite the current extent/scale.
  if (!hasSat)
    hasSat = LayerOps::addVworldSatelliteMap(proj, nullptr, key, &satErr);
  if (!hasCad && !key.isEmpty())
    hasCad = LayerOps::addVworldCadastralMap(proj, nullptr, key, &cadErr);
  LayerOps::ensureSatelliteAtBottom(proj);
  if (hasSat && hasCad)
    statusBar()->showMessage(QStringLiteral("위성과 지적도를 올려 두었습니다."), 5000);
  else if (hasSat && !hasCad)
    statusBar()->showMessage(
        key.isEmpty()
            ? QStringLiteral("위성은 올렸습니다. 지적도는 VWorld API 키가 필요합니다.")
            : (cadErr.isEmpty() ? QStringLiteral("지적도를 올리지 못했습니다.") : cadErr),
        8000);
  else if (!hasSat)
    statusBar()->showMessage(satErr.isEmpty() ? QStringLiteral("위성을 올리지 못했습니다.") : satErr,
                             8000);
#endif
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr, false);
  if (m_canvas) m_canvas->freeze(true);
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  updateNextActionStatus();
  m_startupViewApplied = false;
  // 지적 WMS는 GetCapabilities 왕복을 동기로 기다린다(현장 측정 약 2.9초).
  // 창이 뜨기 전에 그 왕복을 붙들고 있으면 시작이 그만큼 늦다. 이벤트 루프로
  // 미뤄 창을 먼저 띄우고 배경지도는 뒤따라 올린다.
  if (!m_basemapBootPending) {
    m_basemapBootPending = true;
    QTimer::singleShot(0, this, &MainWindow::loadBootBasemaps);
  }
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
#endif
}

void MainWindow::syncThematicButtons() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  // 범례가 진실이다. 레이어를 지우거나 레이어의 체크를 끄면 아이콘도 꺼진다.
  // isLayerVisible은 레이어가 없으면 false라 삭제·체크해제를 한 번에 덮는다.
  const auto sync = [proj](QAction* act, QToolButton* btn, const QString& title) {
    const bool on = LayerOps::isLayerVisible(proj, title);
    if (act && act->isCheckable() && act->isChecked() != on) {
      const QSignalBlocker block(act);
      act->setChecked(on);
    }
    if (btn && btn->isCheckable() && btn->isChecked() != on) {
      const QSignalBlocker block(btn);
      btn->setChecked(on);
    }
  };
  sync(nullptr, m_btnTerrain, QStringLiteral("지형맵"));
  sync(nullptr, m_btnDem, QStringLiteral("DEM"));
  sync(nullptr, m_btnSoil, QStringLiteral("토양도(흙토람)"));
  sync(m_actGeology, nullptr, QStringLiteral("지질도(KIGAM 1:5만)"));
  sync(m_actRiver, nullptr, QStringLiteral("수계도(하천망)"));
#endif
}

void MainWindow::loadBootBasemaps() {
#if KA_HGIS_HAS_QGIS
  if (!m_basemapBootPending) return;
  m_basemapBootPending = false;
  QElapsedTimer bm;
  bm.start();
  ensureDefaultBasemaps();
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
  KaCrashGuard::logLine(
      QStringLiteral("[boot] 배경지도 %1 ms · 미리보기 = %2 · 병렬렌더 = %3")
          .arg(bm.elapsed())
          .arg(m_canvas && m_canvas->previewJobsEnabled() ? QStringLiteral("켬")
                                                          : QStringLiteral("끔"))
          .arg(QStringLiteral("끔")));
#endif
}

void MainWindow::ensureStartupViewReady() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || m_startupViewApplied) return;
  if (m_canvas->width() < 40 || m_canvas->height() < 40) return;
  m_canvas->freeze(true);
  LayerOps::applyCanvasScreenDpi(m_canvas);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  m_canvas->freeze(true);
  LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  m_canvas->zoomScale(25000.0, true);
  LayerOps::clampCanvasToKorea(m_canvas);
  m_canvas->freeze(false);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  m_startupViewApplied = true;
  QTimer::singleShot(200, this, [this]() {
    if (!m_canvas) return;
    if (m_canvas->scale() > 40000.0 || m_canvas->scale() < 8000.0)
      m_canvas->zoomScale(25000.0, true);
  });
#endif
}

void MainWindow::scheduleMapDisplayRefresh() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (!m_displayRefresh) {
    m_displayRefresh = new QTimer(this);
    m_displayRefresh->setSingleShot(true);
    connect(m_displayRefresh, &QTimer::timeout, this, [this]() {
      if (!m_canvas) return;
      LayerOps::refreshXyzBasemapTiles(m_canvas);
    });
  }
  m_displayRefresh->start(150);
#endif
}

void MainWindow::bindMapDisplayScreen() {
#if KA_HGIS_HAS_QGIS
  QWindow* wh = windowHandle();
  if (!wh || m_mapScreenBound) return;
  m_mapScreenBound = true;
  connect(wh, &QWindow::screenChanged, this, [this](QScreen*) {
    if (m_canvas) LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
    QTimer::singleShot(100, this, [this]() {
      if (m_mainSplit) {
        const int tw = m_mainSplit->width();
        if (tw > 300) {
          const QList<int> sz = m_mainSplit->sizes();
          if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
            const int lw = qBound(200, int(tw * 0.22), 360);
            m_mainSplit->setSizes({lw, tw - lw});
          }
        }
      }
      updateGeometry();
      if (centralWidget()) centralWidget()->updateGeometry();
    });
  });
#endif
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
#if KA_HGIS_HAS_QGIS
  bindMapDisplayScreen();
  if (m_canvas) {
    LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
  }
  if (m_mainSplit) {
    const int tw = m_mainSplit->width();
    if (tw > 300) {
      const QList<int> sz = m_mainSplit->sizes();
      if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
        const int lw = qBound(200, int(tw * 0.22), 360);
        m_mainSplit->setSizes({lw, tw - lw});
      }
    }
  }
#endif
}



void MainWindow::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
  if (m_status) m_status->setWorkCrs(authId);
#if KA_HGIS_HAS_QGIS
  QString err;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, authId);
  if (!LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, authId, &err, false)) {
    QMessageBox::warning(this, QStringLiteral("CRS"), err);
    return;
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::clampCanvasToKorea(m_canvas);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  if (authId.contains(QLatin1String("5187")))
    statusBar()->showMessage(QStringLiteral("동부원점으로 맞춰 두었습니다. 이제 구역을 그리면 됩니다."), 8000);
  else
    statusBar()->showMessage(QStringLiteral("중부원점으로 맞춰 두었습니다. 이제 구역을 그리면 됩니다."), 8000);
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
  LayerOps::zoomToFullMax(m_canvas);
  LayerOps::clampCanvasToKorea(m_canvas);
  statusBar()->showMessage(QStringLiteral("한국 전체 범위"), 4000);
#endif
}

void MainWindow::zoomSelectedLayerMax() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  QgsMapLayer* layer = nullptr;
  if (m_layerTree) {
    layer = m_layerTree->currentLayer();
    if (!layer) {
      const QList<QgsMapLayer*> sel = m_layerTree->selectedLayers();
      if (!sel.isEmpty())
        layer = sel.first();
    }
  }
  if (!layer) {
    zoomMapToFullMax();
    return;
  }
  if (!LayerOps::zoomToLayerMax(m_canvas, layer)) {
    statusBar()->showMessage(
        QStringLiteral("이 레이어에 도형이 없습니다. 먼저 그린 뒤 다시 누르세요: %1").arg(layer->name()),
        8000);
    return;
  }
  statusBar()->showMessage(QStringLiteral("이 레이어로 이동: %1").arg(layer->name()), 5000);
#endif
}

void MainWindow::onMapContextMenu(const QPoint& pos) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if ((m_captureTool && m_canvas->mapTool() == m_captureTool) ||
      (m_vertexTool && m_canvas->mapTool() == m_vertexTool)) {
    Q_UNUSED(pos);
    return;
  }
  QMenu menu(this);
  menu.addAction(QStringLiteral("여기 도형 속성"), this, [this, pos]() { editAttributesAtCanvasPos(pos); });
  menu.addAction(QStringLiteral("속성 편집 도구"), this, &MainWindow::startAttributeEditTool);
  menu.addSeparator();
  menu.addAction(QStringLiteral("전체 최대 보기"), this, &MainWindow::zoomMapToFullMax);
  menu.addAction(QStringLiteral("한국 범위"), this, [this]() {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  });
  menu.addAction(QStringLiteral("선택 레이어 최대 보기"), this, &MainWindow::zoomSelectedLayerMax);
  menu.addSeparator();
  menu.addAction(QStringLiteral("새로고침"), this, [this]() {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  });
  if (m_locationMark) {
    menu.addSeparator();
    menu.addAction(QStringLiteral("찾은 위치 표식 지우기 (%1)").arg(m_locationMarkTitle), this,
                   &MainWindow::clearFoundLocationMark);
  }
  menu.exec(m_canvas->mapToGlobal(pos));
#else
  Q_UNUSED(pos);
#endif
}

#if KA_HGIS_HAS_QGIS
static bool isProjectSurveyDomainLayer(const QgsVectorLayer* vl, const QString& surveyGpkgPath) {
  if (!vl || !vl->isValid() || surveyGpkgPath.isEmpty()) return false;
  const QString key = LayerOps::layerKeyOf(vl);
  if (key.isEmpty() || key.startsWith(QLatin1String("user:"))) return false;
  const QString src = vl->source().split(QLatin1Char('|')).first();
  if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) return false;
  QFileInfo fiSrc(src);
  QFileInfo fiGpkg(surveyGpkgPath);
  if (fiSrc.absoluteFilePath().compare(fiGpkg.absoluteFilePath(), Qt::CaseInsensitive) != 0)
    return false;
  const QStringList domainKeys = {
    QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
    QStringLiteral("feature_line"), QStringLiteral("section_line"),
    QStringLiteral("control_points"), QStringLiteral("trial_trench"),
    QStringLiteral("artifact_point")
  };
  return domainKeys.contains(key);
}
#endif

void MainWindow::onLayerTreeContextMenu(const QPoint& pos) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;
  QPoint vp = pos;
  if (QWidget* vpW = m_layerTree->viewport())
    vp = vpW->mapFrom(m_layerTree, pos);
  const QModelIndex idx = m_layerTree->indexAt(vp);
  if (idx.isValid()) {
    m_layerTree->setCurrentIndex(idx);
    if (QgsLayerTreeNode* node = m_layerTree->index2node(idx)) {
      if (auto* ll = qobject_cast<QgsLayerTreeLayer*>(node)) {
        if (ll->layer())
          m_layerTree->setCurrentLayer(ll->layer());
      }
    }
  }
  QMenu menu(this);
  menu.addAction(QStringLiteral("이름 바꾸기 (두 번 클릭)"), this, &MainWindow::renameSelectedLayer);
  menu.addAction(QStringLiteral("면·외곽선 색"), this, &MainWindow::editCurrentLayerStyle);
  menu.addAction(QStringLiteral("도형 속성"), this, &MainWindow::editCurrentLayerAttributes);
  if (QgsMapLayer* cur = m_layerTree->currentLayer()) {
    if (auto* vl = qobject_cast<QgsVectorLayer*>(cur)) {
      if (LayerOps::hasToggleableLabels(vl)) {
        QMenu* labelMenu = menu.addMenu(QStringLiteral("라벨·명칭 속성"));
        const bool on = LayerOps::labelsVisible(vl);
        labelMenu->addAction(on ? QStringLiteral("라벨 끄기") : QStringLiteral("라벨 켜기"), this, [this, vl, on]() {
          LayerOps::setLabelsVisible(vl, !on);
          if (m_canvas)
            m_canvas->refresh();
          statusBar()->showMessage(!on ? QStringLiteral("라벨 켬") : QStringLiteral("라벨 끔"), 4000);
        });
        labelMenu->addSeparator();

        // 1. 명칭 필드 선택
        QMenu* fieldSub = labelMenu->addMenu(QStringLiteral("명칭 필드 선택"));
        const QString curField = LayerOps::currentLabelField(vl);
        const QgsFields fds = vl->fields();
        for (int i = 0; i < fds.count(); ++i) {
          const QString fn = fds.at(i).name();
          QAction* fa = fieldSub->addAction(fn, this, [this, vl, fn]() {
            const double sz = LayerOps::labelFontSize(vl, 5.0);
            const bool showArea = LayerOps::labelShowArea(vl, false);
            LayerOps::applyNameAttributeLabels(vl, fn, sz, showArea);
            if (m_canvas) m_canvas->refresh();
            statusBar()->showMessage(QStringLiteral("라벨 필드: '%1' (글자 크기: %2 pt)").arg(fn).arg(sz), 4000);
          });
          fa->setCheckable(true);
          fa->setChecked(fn == curField);
        }

        // 2. 면적 속성 체크
        if (vl->geometryType() == Qgis::GeometryType::Polygon) {
          const bool areaOn = LayerOps::labelShowArea(vl, false);
          QAction* actArea = labelMenu->addAction(QStringLiteral("면적(㎡) 함께 표시"), this, [this, vl, curField, areaOn]() {
            const double sz = LayerOps::labelFontSize(vl, 5.0);
            LayerOps::applyNameAttributeLabels(vl, curField, sz, !areaOn);
            if (m_canvas) m_canvas->refresh();
            statusBar()->showMessage(!areaOn ? QStringLiteral("면적 라벨 켬") : QStringLiteral("면적 라벨 끔"), 4000);
          });
          actArea->setCheckable(true);
          actArea->setChecked(areaOn);
        }

        // 3. 글자 크기 조정 (기본 5pt)
        const double curSz = LayerOps::labelFontSize(vl, 5.0);
        QMenu* sizeSub = labelMenu->addMenu(QStringLiteral("글자 크기 (현재 %1 pt)").arg(curSz));
        const QVector<double> sizes = {3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 12.0, 14.0};
        for (double s : sizes) {
          QAction* sa = sizeSub->addAction(QStringLiteral("%1 pt").arg(s), this, [this, vl, curField, s]() {
            const bool showArea = LayerOps::labelShowArea(vl, false);
            LayerOps::applyNameAttributeLabels(vl, curField, s, showArea);
            if (m_canvas) m_canvas->refresh();
            statusBar()->showMessage(QStringLiteral("라벨 글자 크기: %1 pt").arg(s), 4000);
          });
          sa->setCheckable(true);
          sa->setChecked(qFuzzyCompare(s, curSz));
        }

        // 4. 문자 인코딩 변경 (한글 깨짐 해결)
        const QString src = vl->source().split(QLatin1Char('|')).first();
        if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) {
          const QString curEnc = vl->dataProvider() ? vl->dataProvider()->encoding() : QStringLiteral("CP949");
          QMenu* encSub = labelMenu->addMenu(QStringLiteral("문자 인코딩 (한글 깨짐 해결)"));
          const QStringList encodings = {QStringLiteral("CP949"), QStringLiteral("EUC-KR"), QStringLiteral("UTF-8"), QStringLiteral("System")};
          for (const QString& e : encodings) {
            const QString itemText = (e == QLatin1String("CP949")) ? QStringLiteral("CP949 (한국 공공데이터·지적도·유적도 표준)") : e;
            QAction* ea = encSub->addAction(itemText, this, [this, vl, e]() {
              LayerOps::setShapefileEncoding(vl, e);
              const QString nameField = LayerOps::detectNameField(vl);
              if (!nameField.isEmpty()) {
                const double sz = LayerOps::labelFontSize(vl, 5.0);
                const bool showArea = LayerOps::labelShowArea(vl, false);
                LayerOps::applyNameAttributeLabels(vl, nameField, sz, showArea);
              }
              if (m_canvas) m_canvas->refresh();
              statusBar()->showMessage(QStringLiteral("인코딩 적용: %1").arg(e), 4000);
            });
            ea->setCheckable(true);
            ea->setChecked(curEnc.compare(e, Qt::CaseInsensitive) == 0);
          }
        }
      }
      if (LayerOps::layerKeyOf(vl) == QLatin1String("survey_area") && vl->featureCount() > 0) {
        QMenu* trench = menu.addMenu(QStringLiteral("시굴격자"));
        trench->addAction(QStringLiteral("시굴 (구역 10%)"), this, [this]() { applyTrenchByRatio(10.0); });
        trench->addAction(QStringLiteral("표본 (구역 2%)"), this, [this]() { applyTrenchByRatio(2.0); });
      }
    } else if (cur->name().contains(QStringLiteral("지적"))) {
      QAction* cad = menu.addAction(QStringLiteral("글자 (지적 그림에 포함 — 레이어로만 숨김)"));
      cad->setEnabled(false);
    }
  }
  menu.addSeparator();
  menu.addAction(QStringLiteral("이 레이어로 이동"), this, &MainWindow::zoomSelectedLayerMax);
  menu.addAction(QStringLiteral("전체 보기"), this, &MainWindow::zoomMapToFullMax);
  menu.addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  menu.addSeparator();
  // 레이어만 빼면 GPKG의 도형은 남아, 다시 그리기를 누르면 되살아난다.
  // 처음부터 다시 그리려면 도형 자체를 지워야 해서 곧바로 갈 길을 둔다.
  // 주의: 외부에서 추가한 SHP/CAD/참조 파일은 대상이 아니며, 현재 조사 GPKG의 도메인 레이어만 대상이다.
  if (auto* vl = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())) {
    if (isProjectSurveyDomainLayer(vl, m_surveyPath) && vl->featureCount() > 0) {
      menu.addAction(QStringLiteral("그린 도형 모두 지우기 (%1개)").arg(vl->featureCount()), this,
                     &MainWindow::clearDrawnFeaturesOfCurrentLayer);
    }
  }
  // 인터넷에서 받아 오는 배경은 지금 범위를 파일로 받아 둘 수 있다.
  if (auto* rl = qobject_cast<QgsRasterLayer*>(m_layerTree->currentLayer())) {
    if (rl->source().contains(QLatin1String("type=xyz"))) {
      menu.addAction(QStringLiteral("오프라인 저장 — 지금 화면 범위"), this,
                     &MainWindow::saveOfflineTilePack);
    }
  }
  menu.addAction(QStringLiteral("레이어 삭제"), this, &MainWindow::removeSelectedLayers);
  menu.exec(m_layerTree->viewport()->mapToGlobal(pos));
#else
  Q_UNUSED(pos);
#endif
}

void MainWindow::renameSelectedLayer() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;

  QString currentName;
  QgsMapLayer* mapLayer = m_layerTree->currentLayer();
  QgsLayerTreeNode* node = m_layerTree->currentNode();

  if (mapLayer) {
    currentName = mapLayer->name();
  } else if (node && QgsLayerTree::isGroup(node)) {
    currentName = node->name();
  } else {
    QMessageBox::information(this, QStringLiteral("이름 바꾸기"),
                             QStringLiteral("이름을 바꿀 레이어 또는 그룹을 선택하세요."));
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("이름 바꾸기"), QStringLiteral("새 이름:"),
      QLineEdit::Normal, currentName, &ok);
  if (!ok) return;
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("이름 바꾸기"),
                         QStringLiteral("이름은 비울 수 없습니다."));
    return;
  }
  if (trimmed == currentName) return;

  if (mapLayer)
    mapLayer->setName(trimmed);
  else if (node)
    node->setName(trimmed);

  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("이름 변경: %1 → %2").arg(currentName, trimmed), 5000);
#endif
}

void MainWindow::onLayerTreeDoubleClicked(const QModelIndex& index) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !index.isValid()) return;
  m_layerTree->setCurrentIndex(index);
  m_layerTree->edit(index);
#else
  Q_UNUSED(index);
#endif
}

void MainWindow::convertSelectedTo5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl || !vl->isValid()) {
    notify(Notice::Info, QStringLiteral("5179 변환"),
           QStringLiteral("지도 목록에서 변환할 레이어를 선택한 뒤 다시 누르세요."));
    return;
  }
  const QString startDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  const QString suggest = QDir(startDir.isEmpty() ? QDir::homePath() : startDir)
                              .filePath(vl->name() + QStringLiteral("_5179.shp"));
  const QString out = QFileDialog::getSaveFileName(
      this, QStringLiteral("EPSG:5179 SHP 저장 경로"),
      suggest, QStringLiteral("SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::convertToShp5179(vl, out, QgsProject::instance(), &err).isEmpty())
    notify(Notice::Warning, QStringLiteral("5179 변환"), QStringLiteral("저장하지 못했습니다."), err);
  else {
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("5179 변환 저장: %1").arg(QDir::toNativeSeparators(out)), 8000);
    notify(Notice::Success, QStringLiteral("5179 변환"), QStringLiteral("변환해서 저장했습니다."),
           QDir::toNativeSeparators(out));
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::convertSelected5186To5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    notify(Notice::Info, QStringLiteral("중부 → 업로드용"),
           QStringLiteral("보낼 면을 선택한 뒤 누르세요."));
    return;
  }
  if (!vl->crs().isValid() || vl->crs().authid() != QLatin1String("EPSG:5186"))
    vl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  convertSelectedTo5179();
#endif
}

void MainWindow::convertSelected5187To5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    notify(Notice::Info, QStringLiteral("동부 → 업로드용"),
           QStringLiteral("보낼 면을 선택한 뒤 누르세요."));
    return;
  }
  if (!vl->crs().isValid() || vl->crs().authid() != QLatin1String("EPSG:5187"))
    vl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  convertSelectedTo5179();
#endif
}

void MainWindow::startSelectTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    stopCaptureTool();
  if (m_actMeasure)
    m_actMeasure->setChecked(false);
  applySnapConfig();
  // 도형선택은 고르기만이 아니라 꼭짓점을 끌어 고치는 데까지 쓴다.
  // QGIS의 QgsVertexTool은 qgis_app 안에 있어 링크할 수 없어 직접 만든 도구다.
  if (!m_vertexTool) {
    m_vertexTool = new KaVertexEditTool(m_canvas);
    m_vertexTool->setParent(this);
    connect(m_vertexTool, &KaVertexEditTool::statusMessage, this,
            [this](const QString& t) { statusBar()->showMessage(t, 6000); });
  }
  if (m_canvas->mapTool() == m_vertexTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("선택 종료"), 3000);
    return;
  }
  QgsVectorLayer* target = m_layerTree
                               ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())
                               : nullptr;
  if (!target)
    target = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
  if (!target) {
    statusBar()->showMessage(
        QStringLiteral("고칠 레이어를 레이어 목록에서 먼저 고르세요."), 6000);
    return;
  }
  m_canvas->setCurrentLayer(target);
  m_vertexTool->setLayer(target);
  m_canvas->setMapTool(m_vertexTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("%1 — 도형을 클릭한 뒤 꼭짓점을 끌어 고치세요").arg(target->name()), 0);
#endif
}

void MainWindow::startMeasureTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    stopCaptureTool();
  showMapWorkspace();
  applySnapConfig();
  if (!m_measureTool) {
    m_measureTool = new KaMeasureMapTool(m_canvas);
    m_measureTool->setParent(this);
    connect(m_measureTool, &KaMeasureMapTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 0);
    });
  }
  m_measureTool->setSnapEnabled(m_snapEnabled);
  if (m_canvas->mapTool() == m_measureTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    if (m_actMeasure) m_actMeasure->setChecked(false);
    statusBar()->showMessage(QStringLiteral("줄자 종료"), 3000);
    return;
  }
  m_canvas->setMapTool(m_measureTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  if (m_actMeasure)
    m_actMeasure->setChecked(true);
  statusBar()->showMessage(QStringLiteral("줄자: 점을 찍고 우클릭에서 마침을 고르세요. 면적은 면적만 나옵니다."), 0);
#endif
}

void MainWindow::importDemElevationRaster() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("국토지리원 DEM"), QString(),
      QStringLiteral("국토지리원 DEM (*.img *.IMG *.tif *.tiff *.TIF *.TIFF)"));
  if (path.isEmpty()) return;
  QString err;
  if (!LayerOps::addDemElevationRaster(QgsProject::instance(), m_canvas, path, &err)) {
    notify(Notice::Warning, QStringLiteral("DEM"),
           err.isEmpty() ? QStringLiteral("DEM 파일을 열지 못했습니다.") : err);
    if (m_btnDem) m_btnDem->setChecked(false);
    return;
  }
  if (m_btnDem) m_btnDem->setChecked(true);
  statusBar()->showMessage(QStringLiteral("국토지리원 DEM을 올렸습니다. 범례에 높이(m)가 표시됩니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 파일 불러오기"));
#endif
}

void MainWindow::runDemHillshade() {
#if KA_HGIS_HAS_QGIS
  const QString dem = QFileDialog::getOpenFileName(
      this, QStringLiteral("DEM GeoTIFF"), QString(),
      QStringLiteral("GeoTIFF (*.tif *.tiff *.TIF *.TIFF)"));
  if (dem.isEmpty())
    return;
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("지형분석"));
  auto* form = new QFormLayout(&dlg);
  auto* mode = new QComboBox(&dlg);
  mode->addItem(QStringLiteral("다중광원 음영 (유구·분묘)"), static_cast<int>(DemAnalyzer::HillshadeMode::Multi));
  mode->addItem(QStringLiteral("단방향 음영"), static_cast<int>(DemAnalyzer::HillshadeMode::Single));
  auto* az = new QDoubleSpinBox(&dlg);
  az->setRange(0, 360);
  az->setValue(315);
  az->setSuffix(QStringLiteral(" °"));
  auto* alt = new QDoubleSpinBox(&dlg);
  alt->setRange(1, 90);
  alt->setValue(45);
  alt->setSuffix(QStringLiteral(" °"));
  auto* zf = new QDoubleSpinBox(&dlg);
  zf->setRange(0.01, 50);
  zf->setValue(1.0);
  form->addRow(QStringLiteral("방식"), mode);
  form->addRow(QStringLiteral("방위각"), az);
  form->addRow(QStringLiteral("고도각"), alt);
  form->addRow(QStringLiteral("Z계수"), zf);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  DemAnalyzer::Options opt;
  opt.hillshade = static_cast<DemAnalyzer::HillshadeMode>(mode->currentData().toInt());
  opt.azimuthDeg = az->value();
  opt.altitudeDeg = alt->value();
  opt.zFactor = zf->value();
  const QString out = QFileInfo(dem).completeBaseName().isEmpty()
                          ? (dem + QStringLiteral("_hillshade.tif"))
                          : (QFileInfo(dem).absolutePath() + QLatin1Char('/') +
                             QFileInfo(dem).completeBaseName() + QStringLiteral("_hillshade.tif"));
  QString err;
  if (!DemAnalyzer::runHillshadeFile(dem, out, opt, &err)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영기복을 만들지 못했습니다."), err);
    return;
  }
  if (!addRasterFromPath(out)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영 래스터를 맵에 올리지 못했습니다."), out);
    return;
  }
  if (auto* rl = qobject_cast<QgsRasterLayer*>(QgsProject::instance()->mapLayersByName(
          QFileInfo(out).completeBaseName()).value(0, nullptr))) {
    LayerOps::markReferenceLayer(rl);
    rl->setName(QStringLiteral("음영기복"));
    LayerOps::placeInLegendGroup(QgsProject::instance(), rl, QStringLiteral("참조 지도"));
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  statusBar()->showMessage(QStringLiteral("음영기복을 참조 지도에 올렸습니다."), 6000);
#endif
}

namespace {
TrenchGridGenerator::Spec g_pendingTrench;

#if KA_HGIS_HAS_QGIS
TrenchGridGenerator::PickedArea trenchFillFromSurveyLayer(QgsVectorLayer* areaVl) {
  TrenchGridGenerator::PickedArea empty;
  if (!areaVl)
    return empty;
  std::vector<TrenchGridGenerator::SurveyPoly> feats;
  QgsFeature f;
  QgsFeatureIterator it = areaVl->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty())
      continue;
    feats.push_back({f.geometry().asWkb(), f.id()});
  }
  std::vector<qint64> selected;
  const QgsFeatureIds ids = areaVl->selectedFeatureIds();
  selected.reserve(static_cast<size_t>(ids.size()));
  for (QgsFeatureId id : ids)
    selected.push_back(id);
  return TrenchGridGenerator::pickAutoFillArea(feats, selected);
}

QString leftoverSurveyAreaHint(const TrenchGridGenerator::PickedArea& pick) {
  if (pick.totalCount <= pick.usedCount || pick.usedCount <= 0)
    return {};
  if (pick.usedSelection) {
    return QStringLiteral("조사구역 %1곳 중 선택한 %2곳에만 시굴격자를 깝니다.")
        .arg(pick.totalCount)
        .arg(pick.usedCount);
  }
  return QStringLiteral(
             "조사구역 %1곳이 남아 있어 마지막에 그린 구역에만 깝니다. "
             "예전 구역을 쓰려면 그 구역을 선택한 뒤 다시 깔으세요.")
      .arg(pick.totalCount);
}
#endif
}

void MainWindow::startTrenchGrid() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」로 GPKG를 만드세요."));
    return;
  }
  // 시굴조사 도메인: 선택한(없으면 마지막) 조사구역만 규칙 배치로 덮고,
  // 총 굴착 면적이 그 구역 면적의 규정 비율(시굴 10%, 표본 2%)인지 확인한다.
  // 남은 옛 조사구역을 union 하면 격자가 그 큰 구역에 깔린다.
  QByteArray areaWkb;
  double areaM2 = 0.0;
  if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
    const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
    areaWkb = pick.wkb;
    areaM2 = pick.areaM2;
    const QString leftover = leftoverSurveyAreaHint(pick);
    if (!leftover.isEmpty())
      statusBar()->showMessage(leftover, 8000);
  }

  // 세밀 설정(회전 포함)은 하나의 속성 창에서: 모덜리스라 맵을 보면서 조정한다.
  if (!m_trenchDlg) {
    m_trenchDlg = new KaTrenchDialog(this);
    connect(m_trenchDlg, &KaTrenchDialog::applyRequested, this,
            &MainWindow::applyTrenchFromDialog);
    connect(m_trenchDlg, &KaTrenchDialog::manualPlaceRequested, this,
            &MainWindow::beginTrenchOriginPick);
    connect(m_trenchDlg, &KaTrenchDialog::editSingleRequested, this,
            &MainWindow::startTrenchGridEdit);
    connect(m_trenchDlg, &KaTrenchDialog::moveRequested, this,
            &MainWindow::startTrenchGridMove);
  }
  m_trenchDlg->setArea(areaWkb, areaM2);
  m_trenchDlg->setTerrainAspect(terrainAspectForArea(areaWkb));
  m_trenchDlg->show();
  m_trenchDlg->raise();
  if (m_trenchDlg->autoFill()) {
    if (!applyTrenchFromDialog())
      beginTrenchOriginPick();
  } else {
    beginTrenchOriginPick();
  }
#endif
}

// 조사구역 안에서 DEM 표고를 격자로 뽑아 오르막 방위를 낸다.
// DEM이 없거나 평지면 valid=false — 그때는 방위 칸 값을 그대로 쓴다.
TrenchGridGenerator::SlopeAspect MainWindow::terrainAspectForArea(const QByteArray& areaWkb) {
#if KA_HGIS_HAS_QGIS
  TrenchGridGenerator::SlopeAspect none;
  if (areaWkb.isEmpty() || !m_canvas) return none;
  QgsProject* proj = QgsProject::instance();
  if (!proj) return none;

  QgsRasterLayer* dem = nullptr;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      if (rl->name() == QLatin1String("DEM")) { dem = rl; break; }
    }
  }
  if (!dem || !dem->dataProvider()) return none;

  QgsGeometry area;
  area.fromWkb(areaWkb);
  if (area.isNull() || area.isEmpty()) return none;
  const QgsRectangle env = area.boundingBox();
  if (env.isEmpty()) return none;

  const QgsCoordinateReferenceSystem workCrs(m_workCrs);
  QgsCoordinateTransform toDem(workCrs, dem->crs(), proj);
  std::vector<TrenchGridGenerator::ElevSample> samples;
  const int kSteps = 14;  // 14×14 표본이면 사면 방향은 충분히 안정적이다.
  for (int i = 0; i <= kSteps; ++i) {
    for (int j = 0; j <= kSteps; ++j) {
      const double x = env.xMinimum() + env.width() * i / kSteps;
      const double y = env.yMinimum() + env.height() * j / kSteps;
      if (!area.contains(x, y)) continue;  // 구역 밖 표고는 안 쓴다
      QgsPointXY inDem(x, y);
      try {
        inDem = toDem.transform(QgsPointXY(x, y));
      } catch (const QgsException&) {
        return none;
      }
      bool ok = false;
      const double z = dem->dataProvider()->sample(inDem, 1, &ok);
      if (!ok || std::isnan(z)) continue;
      samples.push_back({x, y, z});
    }
  }
  return TrenchGridGenerator::upslopeAspect(samples);
#else
  Q_UNUSED(areaWkb);
  return {};
#endif
}

bool MainWindow::applyTrenchFromDialog() {
#if KA_HGIS_HAS_QGIS
  if (!m_trenchDlg)
    return false;
  const TrenchGridGenerator::Spec sp = m_trenchDlg->spec();
  if (m_trenchDlg->autoFill()) {
    if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
      const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
      m_trenchDlg->setArea(pick.wkb, pick.areaM2);
      const QString leftover = leftoverSurveyAreaHint(pick);
      if (!leftover.isEmpty())
        statusBar()->showMessage(leftover, 8000);
    }
    // 시굴 10% · 표본 2%는 길이·둑을 프로그램이 맞춘다. 「직접 지정」만 사용자 규격.
    const double target = m_trenchDlg->targetPct();
    std::vector<TrenchGridGenerator::Cell> cells;
    if (target > 0.0) {
      const auto plan = TrenchGridGenerator::buildForTargetRatio(
          m_trenchDlg->areaWkb(), target, 2.0, sp.azimuthDeg);
      cells = plan.cells;
    } else {
      cells = TrenchGridGenerator::buildInArea(sp, m_trenchDlg->areaWkb());
    }
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             QStringLiteral("구역에 맞는 트렌치가 없습니다. 맵을 찍어 놓거나 규격을 2×10으로 바꿔 보세요."));
      return false;
    }
    if (!applyTrenchCells(cells, m_trenchDlg->areaM2(), target))
      return false;
    // 깔자마자 마우스로 하나씩 옮길 수 있어야 한다(회전·재배치 뒤도 같다).
    activateTrenchTool(true);
    return true;
  }
  // 수동 모드: 격자가 이미 있으면 중심을 고정한 채 회전·간격만 바꿔 재배치한다.
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (vl && vl->featureCount() > 0) {
    TrenchGridGenerator::Spec centered = sp;
    centered.originX = 0.0;
    centered.originY = 0.0;
    auto cells = TrenchGridGenerator::build(centered);
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             QStringLiteral("격자를 계산하지 못했습니다. 간격과 행·열을 확인하세요."));
      return false;
    }
    double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
    for (const auto& c : cells) {
      for (const auto& pt : c.ring) {
        minx = std::min(minx, pt.first);
        maxx = std::max(maxx, pt.first);
        miny = std::min(miny, pt.second);
        maxy = std::max(maxy, pt.second);
      }
    }
    const QgsPointXY keep = vl->extent().center();
    const double dx = keep.x() - (minx + maxx) * 0.5;
    const double dy = keep.y() - (miny + maxy) * 0.5;
    for (auto& c : cells) {
      for (auto& pt : c.ring) {
        pt.first += dx;
        pt.second += dy;
      }
    }
    return applyTrenchCells(cells, m_trenchDlg->areaM2());
  }
  beginTrenchOriginPick();
  return false;
#else
  return false;
#endif
}

void MainWindow::beginTrenchOriginPick() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_trenchDlg)
    return;
  g_pendingTrench = m_trenchDlg->spec();
  if (!m_trenchOriginTool) {
    m_trenchOriginTool = new QgsMapToolEmitPoint(m_canvas);
    m_trenchOriginTool->setParent(this);
    connect(m_trenchOriginTool, &QgsMapToolEmitPoint::canvasClicked, this,
            [this](const QgsPointXY& pt, Qt::MouseButton btn) {
              if (btn != Qt::LeftButton)
                return;
              placeTrenchGridAt(pt);
            });
  }
  m_canvas->setMapTool(m_trenchOriginTool);
  statusBar()->showMessage(QStringLiteral("시굴격자: 원점을 맵에서 클릭하세요."), 0);
#endif
}

void MainWindow::placeTrenchGridAt(const QgsPointXY& origin) {
#if KA_HGIS_HAS_QGIS
  g_pendingTrench.originX = origin.x();
  g_pendingTrench.originY = origin.y();
  const auto cells = TrenchGridGenerator::build(g_pendingTrench);
  if (cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자를 계산하지 못했습니다. 간격과 범위를 확인하세요."));
    return;
  }
  applyTrenchCells(cells, m_trenchDlg ? m_trenchDlg->areaM2() : 0.0);
#endif
}

void MainWindow::applyTrenchByRatio(double targetPct) {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」로 GPKG를 만드세요."));
    return;
  }
  QgsVectorLayer* areaVl = LayerOps::findByLayerKey(QgsProject::instance(),
                                                    QStringLiteral("survey_area"));
  if (m_layerTree) {
    if (auto* cur = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())) {
      if (LayerOps::layerKeyOf(cur) == QLatin1String("survey_area"))
        areaVl = cur;
    }
  }
  if (!areaVl || areaVl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 조사구역을 그린 뒤 그 레이어에서 우클릭하세요."));
    return;
  }
  const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
  const QString leftover = leftoverSurveyAreaHint(pick);
  if (!leftover.isEmpty())
    statusBar()->showMessage(leftover, 8000);
  if (pick.wkb.isEmpty() || pick.areaM2 <= 0.0) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("조사구역 면을 찾지 못했습니다."));
    return;
  }
  const TrenchGridGenerator::RatioFill plan =
      TrenchGridGenerator::buildForTargetRatio(pick.wkb, targetPct, 2.0);
  if (plan.cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("구역에 맞는 트렌치가 없습니다. 구역을 더 크게 그리거나 툴바 시굴격자로 규격을 바꾸세요."));
    return;
  }
  applyTrenchCells(plan.cells, pick.areaM2, targetPct);
#else
  Q_UNUSED(targetPct);
#endif
}

bool MainWindow::applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells,
                                  double areaM2, double targetPct) {
#if KA_HGIS_HAS_QGIS
  QString err;
  // 한 조사에 격자는 하나: 이전 격자를 지우고 대체한다(겹침 방지).
  if (!TrenchGridGenerator::clearLayer(m_surveyPath, QStringLiteral("trial_trench"), &err)) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("기존 격자를 지우지 못했습니다."), err);
    return false;
  }
  const QString auth = QgsProject::instance() && QgsProject::instance()->crs().isValid()
                           ? QgsProject::instance()->crs().authid()
                           : QStringLiteral("EPSG:5186");
  if (!TrenchGridGenerator::writeGpkg(m_surveyPath, QStringLiteral("trial_trench"), cells, auth, &err)) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자를 조사 파일에 저장하지 못했습니다."), err);
    return false;
  }
  auto* vl = ensureDomainLayerForEdit(QStringLiteral("trial_trench"), QStringLiteral("시굴격자"));
  if (vl) {
    vl->dataProvider()->reloadData();
    vl->updateExtents();
    vl->triggerRepaint();
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  const double t = TrenchGridGenerator::totalArea(cells);
  QString msg = QStringLiteral("시굴격자 %1개 · 총 %2㎡")
                    .arg(cells.size())
                    .arg(QLocale().toString(t, 'f', 0));
  if (areaM2 > 0.0) {
    const double pct = t / areaM2 * 100.0;
    const QString kind = (targetPct > 0.0 && targetPct < 5.0)
                             ? QStringLiteral("표본 기준 2%")
                             : QStringLiteral("시굴 기준 10%");
    msg += QStringLiteral(" · 조사구역의 %1% (%2)")
               .arg(QLocale().toString(pct, 'f', 1), kind);
  }
  msg += QStringLiteral(" — 격자를 끌어 옮기세요. 우클릭 = 개별 삭제");
  statusBar()->showMessage(msg, 0);
  notify(Notice::Success, QStringLiteral("시굴격자"), msg);
  startTrenchGridMove();
  return true;
#else
  Q_UNUSED(cells);
  Q_UNUSED(areaM2);
  Q_UNUSED(targetPct);
  return false;
#endif
}

void MainWindow::startTrenchGridMove() {
  activateTrenchTool(false);
}

void MainWindow::startTrenchGridEdit() {
  activateTrenchTool(true);
}

void MainWindow::activateTrenchTool(bool single) {
#if KA_HGIS_HAS_QGIS
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (!vl || vl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"), QStringLiteral("먼저 시굴격자를 만드세요."));
    return;
  }
  if (!m_trenchMoveTool) {
    m_trenchMoveTool = new KaTrenchMoveTool(m_canvas);
    m_trenchMoveTool->setParent(this);
    connect(m_trenchMoveTool, &KaTrenchMoveTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 6000);
    });
  }
  m_trenchMoveTool->setLayer(vl);
  double snapM = 0.0;
  if (m_mapGrid && m_mapGrid->isEnabled())
    snapM = m_mapGrid->stepMeters();
  else if (m_mapGridStep)
    snapM = m_mapGridStep->value();
  m_trenchMoveTool->setSnapMeters(snapM);
  m_trenchMoveTool->setGridOverlay(m_mapGrid);
  m_trenchMoveTool->setMode(single ? KaTrenchMoveTool::Mode::Single
                                   : KaTrenchMoveTool::Mode::Whole);
  m_canvas->setMapTool(m_trenchMoveTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
#else
  Q_UNUSED(single);
#endif
}

void MainWindow::toggleMapGrid() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (!m_mapGrid)
    m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  KaCanvasGridOverlay::Config cfg = m_mapGrid->config();
  cfg.enabled = m_mapGridCheck && m_mapGridCheck->isChecked();
  cfg.stepMeters = m_mapGridStep ? m_mapGridStep->value() : 20.0;
  cfg.rotationDeg = m_mapGridRot ? m_mapGridRot->value() : 0.0;
  cfg.lineWidth = m_mapGridWidth ? m_mapGridWidth->value() : 1.2;
  cfg.penStyle = m_mapGridDash
                     ? static_cast<Qt::PenStyle>(m_mapGridDash->currentData().toInt())
                     : Qt::DashLine;
  cfg.type = (QApplication::keyboardModifiers() & Qt::ShiftModifier)
                 ? KaCanvasGridOverlay::Type::GeographicDms
                 : KaCanvasGridOverlay::Type::ProjectedMeters;
  m_mapGrid->setConfig(cfg);
  m_mapGrid->setEnabled(cfg.enabled);
  m_canvas->refresh();
  statusBar()->showMessage(
      cfg.enabled
          ? (cfg.type == KaCanvasGridOverlay::Type::GeographicDms
                 ? QStringLiteral("경위도 격자를 켰습니다.")
                 : QStringLiteral("미터 좌표 격자를 켰습니다."))
          : QStringLiteral("좌표 격자를 껐습니다."),
      4000);
#endif
}

void MainWindow::startCoordPointTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  showMapWorkspace();
  applySnapConfig();
  if (!m_coordPointTool) {
    m_coordPointTool = new KaCoordPointMapTool(m_canvas);
    m_coordPointTool->setParent(this);
    connect(m_coordPointTool, &KaCoordPointMapTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 8000);
    });
  }
  m_canvas->setMapTool(m_coordPointTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(QStringLiteral("맵에서 꼭짓점을 찍으세요. Esc로 지웁니다."), 0);
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
    notify(Notice::Warning, QStringLiteral("5179 변환"), QStringLiteral("변환하지 못했습니다."), err);
  else
    notify(Notice::Success, QStringLiteral("5179 변환"),
           QStringLiteral("업로드용 EPSG:5179 SHP을 만들었습니다."), QDir::toNativeSeparators(out));
#endif
}

void MainWindow::undoLastAction() {
  auto* focus = QApplication::focusWidget();
  if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus) ||
      qobject_cast<QTextEdit*>(focus) || qobject_cast<QPlainTextEdit*>(focus))
    return;
#if KA_HGIS_HAS_QGIS
  if (m_viewTabs && m_terrain3dLayoutStudio &&
      m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio->undoLastChange();
    return;
  }
  if (m_viewTabs && m_drawingStudio && m_viewTabs->currentWidget() == m_drawingStudio) {
    m_drawingStudio->undoLastChange();
    return;
  }
  if (m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool &&
      m_captureTool->undoLastVertex()) {
    statusBar()->showMessage(QStringLiteral("꼭짓점 하나를 되돌렸습니다."), 4000);
    return;
  }
  if (m_alignTool && m_canvas && m_canvas->mapTool() == m_alignTool &&
      m_alignTool->removeLastPair()) {
    statusBar()->showMessage(m_alignTool->statusText(), 4000);
    return;
  }
  while (!m_committedUndo.isEmpty()) {
    const auto rec = m_committedUndo.takeLast();
    auto* vl = qobject_cast<QgsVectorLayer*>(QgsProject::instance()->mapLayer(rec.first));
    QString err;
    if (LayerOps::undoCommittedFeature(vl, rec.second, &err)) {
      const QString key = LayerOps::layerKeyOf(vl);
      if (key.startsWith(QLatin1String("user_poly")) && vl->featureCount() <= 0) {
        if (m_editLayer == vl) m_editLayer = nullptr;
        QgsProject::instance()->removeMapLayer(vl->id());
        vl = nullptr;
      }
      if (m_canvas)
        LayerOps::refreshCanvasIfIdle(m_canvas);
      refreshWorkPanel();
      statusBar()->showMessage(QStringLiteral("바로 전에 그린 것을 되돌렸습니다."), 5000);
      return;
    }
  }
#endif
  statusBar()->showMessage(QStringLiteral("되돌릴 것이 없습니다."), 4000);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
#if KA_HGIS_HAS_QGIS
  if (!event) return QMainWindow::eventFilter(watched, event);
  if (m_mapSplitter && watched == m_mapSplitter && event->type() == QEvent::Resize)
    refreshAlignUi();
  if (m_subToolsMode == QLatin1String("align") &&
      (event->type() == QEvent::Wheel || event->type() == QEvent::Resize)) {
    const bool onAlignView =
        (m_alignImage && (watched == m_alignImage || watched == m_alignImage->viewport())) ||
        (m_alignLeftCanvas &&
         (watched == m_alignLeftCanvas || watched == m_alignLeftCanvas->viewport())) ||
        (m_canvas && (watched == m_canvas || watched == m_canvas->viewport()));
    if (onAlignView)
      QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
  }
  if (m_subToolsMode == QLatin1String("align") && event->type() == QEvent::MouseMove) {
    if (auto* me = static_cast<QMouseEvent*>(event)) {
      QWidget* w = qobject_cast<QWidget*>(watched);
      if (w) trackAlignPointer(w->mapToGlobal(me->pos()));
    }
  }

  const bool onCanvas = m_canvas &&
      (watched == m_canvas || watched == m_canvas->viewport());
  if (onCanvas) {
    const QEvent::Type t = event->type();
    if (t == QEvent::Resize || t == QEvent::Show
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || t == QEvent::DevicePixelRatioChange
#endif
    ) {
      LayerOps::applyCanvasScreenDpi(m_canvas);
      if (LayerOps::canvasDisplayEventNeedsTileRefresh(int(t)))
        scheduleMapDisplayRefresh();
      if (m_subToolsMode == QLatin1String("align"))
        QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
    }
    if (t == QEvent::Resize && !m_startupViewApplied)
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
  }
  if (onCanvas && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke && (ke->matches(QKeySequence::Undo) ||
               ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z))) {
      undoLastAction();
      return true;
    }
  }
  if (onCanvas && event->type() == QEvent::MouseButtonDblClick) {
    const bool capturing = m_captureTool && m_canvas->mapTool() == m_captureTool;
    const bool measuring = m_measureTool && m_canvas->mapTool() == m_measureTool;
    if (!capturing && !measuring) {
      auto* me = static_cast<QMouseEvent*>(event);
      if (me && me->button() == Qt::LeftButton) {
        ensureAttributeTool();
        if (m_attributeTool) {
          QgsVectorLayer* layer = nullptr;
          QgsFeature feat;
          if (m_attributeTool->pickAtScreen(me->pos(), &layer, &feat) && layer &&
              !LayerOps::isReferenceLayer(layer)) {
            if (m_layerTree) m_layerTree->setCurrentLayer(layer);
            editCurrentLayerStyle();
            return true;
          }
        }
      }
    }
  }

  const bool fromLayerTree = m_layerTree &&
      (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
       event->type() == QEvent::Drop) &&
      (static_cast<QDropEvent*>(event)->source() == m_layerTree ||
       static_cast<QDropEvent*>(event)->source() == m_layerTree->viewport());
  const bool onLayerDrop = !fromLayerTree &&
      ((m_layerTree &&
        (watched == m_layerTree || watched == m_layerTree->viewport())) ||
       (m_canvas && (watched == m_canvas || watched == m_canvas->viewport())));
  if (onLayerDrop) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
      auto* de = static_cast<QDragEnterEvent*>(event);
      const bool fromBrowser = de->source() == m_fileBrowser ||
                               (m_fileBrowser && de->source() == m_fileBrowser->viewport());
      if ((de->mimeData() && de->mimeData()->hasUrls()) || fromBrowser) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::Drop) {
      auto* de = static_cast<QDropEvent*>(event);
      bool ok = false;
      if (de->mimeData() && de->mimeData()->hasUrls())
        ok = tryAddDroppedUrls(de->mimeData()->urls());
      if (!ok)
        ok = tryAddDroppedPaths(selectedBrowserFiles());
      if (ok) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::KeyPress) {
      auto* ke = static_cast<QKeyEvent*>(event);
      const bool measuring = m_measureTool && m_canvas && m_canvas->mapTool() == m_measureTool;
      if (!measuring && (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace)) {
        removeSelectedLayers();
        return true;
      }
      if (ke->key() == Qt::Key_F2) {
        renameSelectedLayer();
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

// 「그린 도형 모두 지우기」 — 레이어는 그대로 두고 GPKG의 도형만 비운다.
// 조사구역을 새로 그리려는 현장 흐름의 최단 경로다.
// 원격 XYZ 레이어의 주소 틀을 꺼낸다. QGIS 소스는
// "type=xyz&url=<퍼센트인코딩된 주소>&zmax=..." 꼴이다.
static QString kaXyzUrlTemplate(const QgsRasterLayer* rl) {
  if (!rl) return QString();
  const QString src = rl->source();
  if (!src.contains(QLatin1String("type=xyz"))) return QString();
  for (const QString& part : src.split(QLatin1Char('&'))) {
    if (!part.startsWith(QLatin1String("url="))) continue;
    return QUrl::fromPercentEncoding(part.mid(4).toUtf8());
  }
  return QString();
}

void MainWindow::saveOfflineTilePack() {
#if KA_HGIS_HAS_QGIS
  auto* rl = m_layerTree ? qobject_cast<QgsRasterLayer*>(m_layerTree->currentLayer()) : nullptr;
  const QString tmpl = kaXyzUrlTemplate(rl);
  if (tmpl.isEmpty()) {
    notify(Notice::Info, QStringLiteral("오프라인 저장"),
           QStringLiteral("위성처럼 인터넷에서 받아 오는 배경 레이어를 먼저 고르세요."));
    return;
  }
  if (!m_canvas) return;

  // 화면 범위를 웹메르카토르로 옮긴다. 타일은 3857로만 잘려 있다.
  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem web(QStringLiteral("EPSG:3857"));
  const QgsCoordinateReferenceSystem cur = m_canvas->mapSettings().destinationCrs();
  if (cur.isValid() && cur != web) {
    try {
      QgsCoordinateTransform tr(cur, web, QgsProject::instance());
      tr.setBallparkTransformsAreAppropriate(true);
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("오프라인 저장"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }

  TilePackService::Options opt;
  opt.urlTemplate = tmpl;
  opt.jpeg = tmpl.contains(QLatin1String(".jpeg")) || tmpl.contains(QLatin1String(".jpg"));
  opt.referer = QStringLiteral("https://localhost");
  // 지금 화면의 해상도에서 한 단계 더 자세한 데까지 받는다.
  const double mupp = qMax(m_canvas->mapUnitsPerPixel(), 1e-6);
  int z = 0;
  while (z < 19 && TilePackService::resolutionAtZoom(z) > mupp) ++z;
  opt.maxZoom = qBound(10, z + 1, 19);
  opt.minZoom = qMax(8, opt.maxZoom - 6);

  const qint64 tiles = TilePackService::tileCount(ext.xMinimum(), ext.yMinimum(), ext.xMaximum(),
                                                  ext.yMaximum(), opt.minZoom, opt.maxZoom);
  if (tiles <= 0) {
    notify(Notice::Warning, QStringLiteral("오프라인 저장"),
           QStringLiteral("범위가 비었습니다. 조사지역으로 확대한 뒤 다시 하세요."));
    return;
  }
  if (tiles > 20000) {
    notify(Notice::Warning, QStringLiteral("오프라인 저장"),
           QStringLiteral("타일 %1장은 너무 많습니다. 조사지역으로 더 확대한 뒤 하세요.")
               .arg(QLocale().toString(tiles)));
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("오프라인 저장"),
          QStringLiteral("지금 화면 범위를 타일 %1장(줌 %2~%3)으로 받아 둡니다.\n"
                         "받는 동안 잠시 멈춥니다. 계속할까요?")
              .arg(QLocale().toString(tiles))
              .arg(opt.minZoom)
              .arg(opt.maxZoom),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
    return;

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString safe = QString(rl->name()).replace(QRegularExpression(QStringLiteral("[^\\w가-힣]")),
                                                   QStringLiteral("_"));
  const QString out = QDir(dir).filePath(QStringLiteral("%1_오프라인.mbtiles").arg(safe));

  statusBar()->showMessage(QStringLiteral("배경 타일을 받는 중… %1장").arg(tiles), 0);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QElapsedTimer t;
  t.start();
  QString err;
  const bool ok = TilePackService::build(opt, ext.xMinimum(), ext.yMinimum(), ext.xMaximum(),
                                         ext.yMaximum(), out, &err);
  QApplication::restoreOverrideCursor();
  KaCrashGuard::logLine(QStringLiteral("[tilepack] %1 %2 ms tiles=%3 z=%4-%5 ok=%6 %7")
                            .arg(rl->name())
                            .arg(t.elapsed())
                            .arg(tiles)
                            .arg(opt.minZoom)
                            .arg(opt.maxZoom)
                            .arg(ok)
                            .arg(err));
  statusBar()->clearMessage();
  if (!ok) {
    notify(Notice::Critical, QStringLiteral("오프라인 저장"),
           err.isEmpty() ? QStringLiteral("타일을 받지 못했습니다.") : err);
    return;
  }

  const QString packName = QStringLiteral("%1 (오프라인)").arg(rl->name());
  QString addErr;
  if (!LayerOps::addTilePackBasemap(QgsProject::instance(), m_canvas, out, packName, &addErr)) {
    notify(Notice::Warning, QStringLiteral("오프라인 저장"),
           QStringLiteral("파일은 만들었지만 지도에 올리지 못했습니다: %1").arg(addErr));
    return;
  }
  // 같은 그림을 두 번 그릴 이유가 없다. 원격 쪽은 끈다(지우지는 않는다).
  LayerOps::toggleLayerVisibility(QgsProject::instance(), m_canvas, rl->name(), false);
  syncThematicButtons();
  notify(Notice::Success, QStringLiteral("오프라인 저장"),
         QStringLiteral("%1 — 타일 %2장을 저장했습니다. 이제 인터넷 없이 뜹니다.")
             .arg(QDir::toNativeSeparators(out))
             .arg(QLocale().toString(tiles)));
#endif
}

void MainWindow::clearDrawnFeaturesOfCurrentLayer() {
#if KA_HGIS_HAS_QGIS
  auto* vl = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  if (!vl || !isProjectSurveyDomainLayer(vl, m_surveyPath)) {
    statusBar()->showMessage(QStringLiteral("현재 조사에서 직접 그린 레이어만 도형을 비울 수 있습니다"), 4000);
    return;
  }
  const long long n = vl->featureCount();
  if (n <= 0) {
    statusBar()->showMessage(QStringLiteral("%1에 지울 도형이 없습니다").arg(vl->name()), 4000);
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("그린 도형 삭제"),
          QStringLiteral("%1의 도형 %2개를 GPKG에서 지웁니다. 되돌릴 수 없습니다.\n계속할까요?")
              .arg(vl->name())
              .arg(n),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;

  QString err;
  if (!LayerOps::purgeCommittedFeatures(vl, &err)) {
    notify(Notice::Warning, QStringLiteral("그린 도형 삭제"),
           err.isEmpty()
               ? QStringLiteral("지우지 못했습니다. 편집 중인 다른 창을 닫고 다시 해 보세요.")
               : err);
    return;
  }
  vl->triggerRepaint();
  if (m_canvas) m_canvas->refresh();
  m_committedUndo.clear();
  updateNextActionStatus();
  refreshWorkPanel();
  notify(Notice::Success, QStringLiteral("그린 도형 삭제"),
         QStringLiteral("%1의 도형 %2개를 지웠습니다. 이제 새로 그리면 됩니다.")
             .arg(vl->name())
             .arg(n));
#endif
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

  // 그리기는 그릴 때마다 GPKG에 바로 커밋한다(자동 저장). 그래서 레이어만 빼면
  // 도형은 파일에 남고, 「그리기 → 조사구역」이 같은 표를 다시 열어 되살아난다.
  // 현장 규칙: 현재 조사 GPKG에서 직접 그린 레이어만 도형을 지우고, 외부에서 추가한 SHP/CAD/참조 파일은 목록에서만 제거한다(원본 파일 보존).
  QStringList drawn;
  QList<QgsVectorLayer*> domainWithData;
  for (const QString& id : ids) {
    auto* vl = qobject_cast<QgsVectorLayer*>(QgsProject::instance()->mapLayer(id));
    if (!isProjectSurveyDomainLayer(vl, m_surveyPath)) continue;
    if (vl->featureCount() <= 0) continue;
    domainWithData.append(vl);
    drawn << QStringLiteral("%1 %2개").arg(vl->name()).arg(vl->featureCount());
  }

  if (!domainWithData.isEmpty()) {
    const auto ans = QMessageBox::question(
        this, QStringLiteral("조사 자료 삭제"),
        QStringLiteral("%1를 지웁니다. 되돌릴 수 없습니다. 계속할까요?")
            .arg(drawn.join(QStringLiteral(", "))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans != QMessageBox::Yes)
      return;
  }

  int purged = 0;
  QStringList purgeFail;
  for (QgsVectorLayer* vl : domainWithData) {
    if (!vl) continue;
    const int n = static_cast<int>(vl->featureCount());
    QString err;
    if (LayerOps::purgeCommittedFeatures(vl, &err)) {
      purged += n;
    } else {
      purgeFail << vl->name();
      ids.remove(vl->id());
      names.removeAll(vl->name());
    }
  }
  if (!purgeFail.isEmpty()) {
    notify(Notice::Warning, QStringLiteral("조사 자료 삭제"),
           QStringLiteral("%1의 도형을 GPKG에서 지우지 못해 레이어를 남겨 두었습니다.")
               .arg(purgeFail.join(QStringLiteral(", "))));
  }
  if (ids.isEmpty())
    return;

  QgsProject::instance()->removeMapLayers(QList<QString>(ids.begin(), ids.end()));
  if (m_canvas) {
    m_canvas->refresh();
    m_canvas->redrawAllLayers();
  }
  refreshLayerEmptyState();
  updateNextActionStatus();
  statusBar()->showMessage(
      purged > 0 ? QStringLiteral("삭제: %1 — 그린 도형 %2개도 지웠습니다")
                       .arg(names.join(QStringLiteral(", ")))
                       .arg(purged)
                 : QStringLiteral("삭제: %1").arg(names.join(QStringLiteral(", "))),
      6000);
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
  LayerOps::syncMapCanvas(QgsProject::instance(), canvas, false);
  if (canvas->scale() > 80000.0 || canvas->scale() < 100.0)
    canvas->zoomScale(25000.0, true);
  LayerOps::clampCanvasToKorea(canvas);
  LayerOps::refreshXyzBasemapTiles(canvas);
  QString next = QStringLiteral("%1을 올렸습니다. 「그리기」로 구역을 그리세요.").arg(label);
  if (label.contains(QStringLiteral("지적")))
    next = QStringLiteral("지적을 올렸습니다. 가까이 보면 번지가 보입니다.");
  else if (label.contains(QStringLiteral("위성")))
    next = QStringLiteral("위성을 올렸습니다. 「그리기」로 구역을 그리세요.");
  self->statusBar()->showMessage(next, 8000);
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
  LayerOps::refreshCanvasIfIdle(m_canvas);
  m_scaleUiGuard = false;
  statusBar()->showMessage(QStringLiteral("축척 적용 1:%1").arg(s, 0, 'f', 0), 4000);
#endif
}

void MainWindow::refreshMapCanvasNow() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_canvas->isDrawing()) return;
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  updateNextActionStatus();
#endif
}

void MainWindow::addBasemapVworld() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldBaseMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("배경"));
#endif
}

void MainWindow::addBasemapVworldSat() {
#if KA_HGIS_HAS_QGIS
  const QString key = VworldSettings::loadApiKey();
  QString err;
  if (!LayerOps::addVworldSatelliteMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("위성"),
           QStringLiteral("위성영상을 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("위성"));
#endif
}

void MainWindow::addBasemapVworldCadastral() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldCadastralMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("지적도"),
           QStringLiteral("지적도를 올리지 못했습니다."), err);
  else {
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("지적"));
    if (m_canvas && m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(5000.0, true);
  }
#endif
}

void MainWindow::addBasemapOsm() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addOsmBasemap(QgsProject::instance(), m_canvas, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("OSM 배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("OSM"));
#endif
}

void MainWindow::addBasemapGoogle() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::GoogleSatellite, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("위성 배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("위성"));
#endif
}

void MainWindow::loadSurveyLayers(const QString& gpkgOrStub) {
#if KA_HGIS_HAS_QGIS
  if (gpkgOrStub.endsWith(QLatin1String(".stub"))) return;
  QgsProject* proj = QgsProject::instance();
  stopAlignSession();
  m_editLayer = nullptr;
  m_committedUndo.clear();
  LayerOps::removeSurveyDomainLayers(proj);
  proj->setCrs(QgsCoordinateReferenceSystem(m_workCrs));
  if (m_canvas) m_canvas->setDestinationCrs(QgsCoordinateReferenceSystem(m_workCrs));
  m_surveyPath = gpkgOrStub;

  // 이미 피처(도형)가 존재하는 도면 레이어는 한국어 명칭으로 불러온다 (빈 테이블은 레전드를 어지럽히지 않도록 자동 추가하지 않음)
  for (const QString& key : LayerOps::domainLayerKeys()) {
    QgsVectorLayer probe(QStringLiteral("%1|layername=%2").arg(gpkgOrStub, key), key, QStringLiteral("ogr"));
    if (probe.isValid() && probe.featureCount() > 0) {
      QString titleKo = key;
      if (key == QLatin1String("survey_area")) titleKo = QStringLiteral("조사구역");
      else if (key == QLatin1String("feature_poly")) titleKo = QStringLiteral("유구 (면)");
      else if (key == QLatin1String("feature_line")) titleKo = QStringLiteral("유구 (선)");
      else if (key == QLatin1String("section_line")) titleKo = QStringLiteral("단면선");
      else if (key == QLatin1String("control_points")) titleKo = QStringLiteral("기준점");
      else if (key == QLatin1String("artifact_point")) titleKo = QStringLiteral("유물");
      else if (key == QLatin1String("trial_trench")) titleKo = QStringLiteral("시굴격자");
      QString err;
      auto* vl = LayerOps::ensureDomainLayer(proj, gpkgOrStub, key, titleKo, &err);
      if (vl) {
        LayerOps::applyDomainDrawStyle(vl, key);
      }
    }
  }

  // 도면 5장을 미리 만들지 않는다 — 조사를 열 때마다 2.4초를 먹었고(실측),
  // 사용자가 도면을 안 볼 수도 있다. 검수·내보내기·도면 창에서 그때 만든다.
  LayerOps::pruneEmptyLegendGroups(proj);
  if (m_canvas) {
    m_canvas->freeze(true);
    LayerOps::ensureOtfEnabled(proj, m_canvas, m_workCrs);
    LayerOps::syncMapCanvas(proj, m_canvas, false);
    LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  }
  refreshLayerEmptyState();
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
  LayerOps::applyDomainDrawStyle(vl, layerKey);
  if (m_canvas && !QgsProject::instance()->mapLayer(vl->id())) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  }
  if (m_layerTree)
    m_layerTree->setCurrentLayer(vl);
  statusBar()->showMessage(
      QStringLiteral("레이어 준비: %1 — 지도에서 그리세요").arg(vl->name()), 6000);
  return vl;
}

void MainWindow::onLayerTreeRowsMoved() {
  if (!m_canvas) return;
  LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::refreshCanvasIfIdle(m_canvas);
}

void MainWindow::moveSelectedLayer(int dir) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !QgsProject::instance()) return;
  QgsLayerTreeNode* node = m_layerTree->currentNode();
  if (!node || node->nodeType() != QgsLayerTreeNode::NodeLayer) return;
  auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent());
  if (!parent) parent = QgsProject::instance()->layerTreeRoot();
  if (!parent) return;
  const int idx = parent->children().indexOf(node);
  if (idx < 0) return;
  const int dest = idx + dir;
  if (dest < 0 || dest >= parent->children().size()) return;
  QList<QgsMapLayer*> order;
  for (QgsLayerTreeNode* child : parent->children()) {
    if (auto* ln = qobject_cast<QgsLayerTreeLayer*>(child)) {
      if (ln->layer()) order.append(ln->layer());
    }
  }
  if (idx < order.size() && dest < order.size()) {
    order.move(idx, dest);
    parent->reorderGroupLayers(order);
    onLayerTreeRowsMoved();
  }
#else
  Q_UNUSED(dir);
#endif
}

void MainWindow::stopCaptureTool() {
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    m_canvas->unsetMapTool(m_captureTool);
  if (m_attributeTool && m_canvas->mapTool() == m_attributeTool)
    m_canvas->unsetMapTool(m_attributeTool);
  if (m_panTool)
    m_canvas->setMapTool(m_panTool);
  if (m_captureTool)
    m_captureTool->resetSession();
}

QString MainWindow::attributeFieldLabelKo(const QString& fieldName) {
  static const QHash<QString, QString> labels = {
      {QStringLiteral("survey_name"), QStringLiteral("조사명")},
      {QStringLiteral("site_name"), QStringLiteral("유적명")},
      {QStringLiteral("kind"), QStringLiteral("유구종류")},
      {QStringLiteral("period"), QStringLiteral("시대")},
      {QStringLiteral("feature_no"), QStringLiteral("유구번호")},
      {QStringLiteral("artifact_no"), QStringLiteral("유물번호")},
      {QStringLiteral("note"), QStringLiteral("비고")},
      {QStringLiteral("section_id"), QStringLiteral("단면번호")},
      {QStringLiteral("point_id"), QStringLiteral("점ID")},
      {QStringLiteral("x"), QStringLiteral("X")},
      {QStringLiteral("y"), QStringLiteral("Y")},
      {QStringLiteral("z"), QStringLiteral("표고 Z")},
      {QStringLiteral("datum"), QStringLiteral("측지기준계")},
      {QStringLiteral("ellipsoid"), QStringLiteral("타원체")},
      {QStringLiteral("projection"), QStringLiteral("투영")},
      {QStringLiteral("origin"), QStringLiteral("원점")},
      {QStringLiteral("accuracy"), QStringLiteral("정확도 메모")},
      {QStringLiteral("accuracy_m"), QStringLiteral("정확도(m)")},
      {QStringLiteral("pdop"), QStringLiteral("PDOP")},
      {QStringLiteral("fix_type"), QStringLiteral("수신상태")},
      {QStringLiteral("pixel_x"), QStringLiteral("픽셀 X")},
      {QStringLiteral("pixel_y"), QStringLiteral("픽셀 Y")},
  };
  return labels.value(fieldName, fieldName);
}

void MainWindow::ensureAttributeTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_attributeTool) return;
  m_attributeTool = new KaAttributeMapTool(m_canvas);
  m_attributeTool->setParent(this);
  connect(m_attributeTool, &KaAttributeMapTool::featurePicked, this,
          [this](QgsVectorLayer* layer, const QgsFeature& feat) {
            editFeatureAttributes(layer, feat);
          });
  connect(m_attributeTool, &KaAttributeMapTool::pickCanceled, this, [this]() {
    if (m_panTool && m_canvas) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("속성 편집 종료"), 3000);
  });
#endif
}

void MainWindow::editCurrentLayerAttributes() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;
  auto* layer = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer());
  if (!layer || !layer->isValid()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("벡터 레이어(조사구역·유구 등)를 선택한 뒤 다시 실행하세요."));
    return;
  }
  if (layer->featureCount() <= 0) {
    QMessageBox::information(
        this, QStringLiteral("속성"),
        QStringLiteral("「%1」에 도형이 없습니다.\n먼저 그리기로 도형을 만든 뒤 속성을 입력하세요.")
            .arg(layer->name()));
    return;
  }

  QList<QgsFeature> feats;
  QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setFlags(Qgis::FeatureRequestFlag::NoGeometry));
  QgsFeature f;
  while (it.nextFeature(f))
    feats.append(f);

  if (feats.isEmpty()) {
    it = layer->getFeatures();
    while (it.nextFeature(f))
      feats.append(f);
  }
  if (feats.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("피처를 읽을 수 없습니다. 레이어 파일을 확인하세요."));
    return;
  }

  int pick = 0;
  if (feats.size() > 1) {
    QStringList labels;
    labels.reserve(feats.size());
    const QgsFields fields = layer->fields();
    const int kindIdx = fields.indexOf(QStringLiteral("kind"));
    const int nameIdx = fields.indexOf(QStringLiteral("survey_name"));
    const int noIdx = fields.indexOf(QStringLiteral("feature_no"));
    const int idIdx = fields.indexOf(QStringLiteral("point_id"));
    const int secIdx = fields.indexOf(QStringLiteral("section_id"));
    for (const QgsFeature& ft : feats) {
      QString label = QStringLiteral("#%1").arg(ft.id());
      auto take = [&](int idx) {
        if (idx < 0) return;
        const QVariant v = ft.attribute(idx);
        if (v.isValid() && !v.toString().trimmed().isEmpty())
          label += QStringLiteral("  ") + v.toString().trimmed();
      };
      take(noIdx);
      take(kindIdx);
      take(nameIdx);
      take(idIdx);
      take(secIdx);
      labels << label;
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("도형 선택 — %1").arg(layer->name()),
        QStringLiteral("속성을 편집할 도형 (%1개):").arg(feats.size()),
        labels, 0, false, &ok);
    if (!ok) return;
    pick = labels.indexOf(chosen);
    if (pick < 0) pick = 0;
  }

  QgsFeature full;
  if (!layer->getFeatures(QgsFeatureRequest(feats.at(pick).id())).nextFeature(full))
    full = feats.at(pick);
  editFeatureAttributes(layer, full);
#else
  QMessageBox::information(this, QStringLiteral("속성"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::startAttributeEditTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool) {
    stopCaptureTool();
  }
  ensureAttributeTool();
  if (!m_attributeTool) return;
  if (m_canvas->mapTool() == m_attributeTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("속성 편집 종료"), 3000);
    return;
  }
  m_canvas->setMapTool(m_attributeTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("속성 편집: 지도에서 도형을 좌클릭 · ESC=종료"), 0);
#else
  QMessageBox::information(this, QStringLiteral("속성"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

namespace {

class FileListView : public QListWidget {
public:
  explicit FileListView(QWidget* parent = nullptr) : QListWidget(parent) {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(true);
    setIconSize(QSize(0, 0));
  }

protected:
  void startDrag(Qt::DropActions) override {
    QList<QUrl> urls;
    const auto items = selectedItems();
    for (QListWidgetItem* it : items) {
      if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
      const QString p = it->data(Qt::UserRole).toString();
      if (!p.isEmpty()) urls.append(QUrl::fromLocalFile(p));
    }
    if (urls.isEmpty()) return;
    auto* md = new QMimeData;
    md->setUrls(urls);
    QDrag drag(this);
    drag.setMimeData(md);
    drag.exec(Qt::CopyAction);
  }
};

void kaPaintColorButton(QPushButton* b, const QColor& c, const QString& suffix) {
  if (!b) return;
  b->setStyleSheet(KaTheme::colorSwatchStyle(c));
  b->setText(c.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  ") + suffix);
  b->setProperty("kaColor", c);
  b->setCursor(Qt::PointingHandCursor);
}

QPushButton* kaMakeColorButton(QWidget* parent, const QColor& c, const QString& suffix,
                               const QString& pickerTitle, const std::function<void()>& onChanged = {}) {
  auto* b = new QPushButton(parent);
  kaPaintColorButton(b, c.isValid() && c.alpha() > 0 ? c : QColor(22, 163, 74, 160), suffix);
  QObject::connect(b, &QPushButton::clicked, b, [b, suffix, pickerTitle, onChanged]() {
    QColorDialog picker(b->property("kaColor").value<QColor>(), b->window());
    picker.setOption(QColorDialog::DontUseNativeDialog, true);
    picker.setOption(QColorDialog::ShowAlphaChannel, true);
    picker.setWindowTitle(pickerTitle);
    if (picker.exec() != QDialog::Accepted) return;
    const QColor picked = picker.selectedColor();
    if (!picked.isValid()) return;
    kaPaintColorButton(b, picked, suffix);
    if (onChanged) onChanged();
  });
  return b;
}

QWidget* kaWrapLabeled(QWidget* parent, const QString& caption, QWidget* inner) {
  auto* box = new QWidget(parent);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  auto* lab = new QLabel(caption, box);
  v->addWidget(lab);
  v->addWidget(inner);
  return box;
}

QDoubleSpinBox* kaMakeArrowSpin(QWidget* parent, QWidget** rowOut, double minV, double maxV,
                                double step, int decimals, double value) {
  auto* row = new QWidget(parent);
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(0);
  auto* spin = new QDoubleSpinBox(row);
  spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
  spin->setRange(minV, maxV);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setSuffix(QStringLiteral(" mm"));
  spin->setValue(value);
  spin->setMinimumHeight(29);
  spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  h->addWidget(spin, 1);
  if (rowOut) *rowOut = row;
  return spin;
}

}  // namespace

void MainWindow::editCurrentLayerStyle() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree) return;
  auto* layer = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer());
  if (!layer || !layer->isValid()) {
    QMessageBox::information(this, QStringLiteral("모양"),
                             QStringLiteral("벡터 레이어를 선택한 뒤 다시 실행하세요."));
    return;
  }
  if (LayerOps::isReferenceLayer(layer)) {
    QMessageBox::information(this, QStringLiteral("모양"),
                             QStringLiteral("배경(참조) 지도는 여기서 색을 바꾸지 않습니다.\n"
                                            "조사 데이터 레이어(유구·구역 등)를 선택하세요."));
    return;
  }

  QColor fill, stroke;
  double widthMm = 1.2;
  double markerMm = 3.5;
  bool noFill = false;
  bool noStroke = false;
  bool dashed = false;
  LayerOps::readSimpleVectorStyle(layer, &fill, &stroke, &widthMm, &markerMm, &noFill, &noStroke,
                                  &dashed);

  const Qgis::GeometryType gt = layer->geometryType();
  const bool isPoly = gt == Qgis::GeometryType::Polygon;
  const bool isLine = gt == Qgis::GeometryType::Line;
  const bool isPoint = gt == Qgis::GeometryType::Point;

  QDialog dlg(this);
  dlg.setObjectName(QStringLiteral("kaStyleDlg"));
  dlg.setWindowTitle(QStringLiteral("도형 색"));
  dlg.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

  auto* root = new QVBoxLayout(&dlg);
  root->setSpacing(8);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSizeConstraint(QLayout::SetFixedSize);

  auto* title = new QLabel(QStringLiteral("도형 색"), &dlg);
  auto* sub = new QLabel(layer->name(), &dlg);
  sub->setWordWrap(true);
  root->addWidget(title);
  root->addWidget(sub);

  QCheckBox* noFillCheck = nullptr;
  QWidget* fillBox = nullptr;
  QPushButton* fillBtn = nullptr;
  if (isPoly || isPoint) {
    noFillCheck = new QCheckBox(QStringLiteral("채우기 없음 (외곽선만)"), &dlg);
    noFillCheck->setChecked(noFill);
    root->addWidget(noFillCheck);
    fillBtn = kaMakeColorButton(&dlg, fill.alpha() == 0 ? QColor(22, 163, 74, 160) : fill,
                                QStringLiteral("클릭해서 색 고르기"), QStringLiteral("면 색"));
    fillBox = kaWrapLabeled(&dlg, QStringLiteral("면 색"), fillBtn);
    root->addWidget(fillBox);
  }

  auto* noStrokeCheck = new QCheckBox(QStringLiteral("외곽선 없음"), &dlg);
  noStrokeCheck->setChecked(noStroke);
  root->addWidget(noStrokeCheck);

  QCheckBox* dashCheck = nullptr;
  if (isLine || isPoly) {
    dashCheck = new QCheckBox(QStringLiteral("점선"), &dlg);
    dashCheck->setChecked(dashed);
    root->addWidget(dashCheck);
  }

  auto* strokeBtn = kaMakeColorButton(&dlg, stroke.alpha() == 0 ? QColor(21, 128, 61) : stroke,
                                      QStringLiteral("클릭해서 색 고르기"),
                                      isLine ? QStringLiteral("선 색") : QStringLiteral("외곽선 색"));
  auto* strokeBox = kaWrapLabeled(&dlg, isLine ? QStringLiteral("선 색") : QStringLiteral("외곽선 색"),
                                  strokeBtn);
  root->addWidget(strokeBox);

  QWidget* widthRow = nullptr;
  auto* widthSpin = kaMakeArrowSpin(&dlg, &widthRow, 0.2, 12.0, 0.2, 1, widthMm);
  auto* widthBox = kaWrapLabeled(&dlg, isLine ? QStringLiteral("선 굵기") : QStringLiteral("외곽선 굵기"),
                                 widthRow);
  root->addWidget(widthBox);

  QDoubleSpinBox* markerSpin = nullptr;
  if (isPoint) {
    QWidget* markerRow = nullptr;
    markerSpin = kaMakeArrowSpin(&dlg, &markerRow, 1.0, 20.0, 0.5, 1, markerMm);
    root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("점 크기"), markerRow));
  }

  QCheckBox* catCheck = nullptr;
  if (LayerOps::layerKeyOf(layer) == QLatin1String("feature_poly")) {
    catCheck = new QCheckBox(QStringLiteral("종류별 자동 색"), &dlg);
    root->addWidget(catCheck);
  }

  auto applyLive = [this, layer, fillBtn, strokeBtn, noFillCheck, noStrokeCheck, dashCheck, widthSpin,
                    markerSpin, markerMm, catCheck]() {
    if (catCheck && catCheck->isChecked()) {
      LayerOps::applyFeaturePolyStyle(layer);
    } else {
      const QColor outFill = fillBtn ? fillBtn->property("kaColor").value<QColor>() : QColor();
      const QColor outStroke = strokeBtn->property("kaColor").value<QColor>();
      LayerOps::applySimpleVectorStyle(layer, outFill, outStroke, widthSpin->value(),
                                       markerSpin ? markerSpin->value() : markerMm,
                                       noFillCheck && noFillCheck->isChecked(),
                                       noStrokeCheck && noStrokeCheck->isChecked(),
                                       dashCheck && dashCheck->isChecked());
    }
    if (m_canvas) m_canvas->refresh();
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  };

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  auto syncVisible = [&dlg, fillBox, strokeBox, widthBox, noFillCheck, noStrokeCheck, dashCheck,
                      applyLive]() {
    const bool nf = noFillCheck && noFillCheck->isChecked();
    const bool ns = noStrokeCheck && noStrokeCheck->isChecked();
    if (fillBox) fillBox->setVisible(!nf);
    if (strokeBox) strokeBox->setVisible(!ns);
    if (widthBox) widthBox->setVisible(!ns);
    if (dashCheck) dashCheck->setVisible(!ns);
    dlg.adjustSize();
    applyLive();
  };
  if (noFillCheck) connect(noFillCheck, &QCheckBox::toggled, &dlg, syncVisible);
  connect(noStrokeCheck, &QCheckBox::toggled, &dlg, syncVisible);
  if (dashCheck)
    connect(dashCheck, &QCheckBox::toggled, &dlg, [applyLive](bool) { applyLive(); });
  connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
          [applyLive](double) { applyLive(); });
  if (markerSpin)
    connect(markerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
            [applyLive](double) { applyLive(); });
  if (catCheck)
    connect(catCheck, &QCheckBox::toggled, &dlg, [applyLive](bool) { applyLive(); });
  if (fillBtn)
    connect(fillBtn, &QPushButton::clicked, &dlg, [applyLive]() { applyLive(); });
  connect(strokeBtn, &QPushButton::clicked, &dlg, [applyLive]() { applyLive(); });
  syncVisible();

  if (dlg.exec() != QDialog::Accepted) {
    LayerOps::applySimpleVectorStyle(layer, fill, stroke, widthMm, markerMm, noFill, noStroke, dashed);
    if (m_canvas) m_canvas->refresh();
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
    return;
  }
  applyLive();
  statusBar()->showMessage(QStringLiteral("면·선 색 적용: %1").arg(layer->name()), 5000);
#else
  QMessageBox::information(this, QStringLiteral("모양"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::addUserLayer() {
#if KA_HGIS_HAS_QGIS
  QDialog dlg(this);
  dlg.setObjectName(QStringLiteral("kaStyleDlg"));
  dlg.setWindowTitle(QStringLiteral("레이어 추가"));
  dlg.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
  auto* root = new QVBoxLayout(&dlg);
  root->setSpacing(8);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSizeConstraint(QLayout::SetFixedSize);
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("예: 조사구역, 1호 주거지"));
  nameEdit->setMinimumHeight(36);
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("이름"), nameEdit));
  auto* crsHost = new QWidget(&dlg);
  auto* crsRow = new QHBoxLayout(crsHost);
  crsRow->setContentsMargins(0, 0, 0, 0);
  crsRow->setSpacing(8);
  auto* btn5186 = new QPushButton(QStringLiteral("5186  중부"), crsHost);
  auto* btn5187 = new QPushButton(QStringLiteral("5187  동부"), crsHost);
  btn5186->setCheckable(true);
  btn5187->setCheckable(true);
  btn5186->setMinimumHeight(40);
  btn5187->setMinimumHeight(40);
  btn5186->setCursor(Qt::PointingHandCursor);
  btn5187->setCursor(Qt::PointingHandCursor);
  const bool use5187 = m_workCrs.contains(QLatin1String("5187"));
  btn5186->setChecked(!use5187);
  btn5187->setChecked(use5187);
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
  });
  crsRow->addWidget(btn5186, 1);
  crsRow->addWidget(btn5187, 1);
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("좌표계"), crsHost));
  auto* noFill = new QCheckBox(QStringLiteral("채우기 없음 (외곽선만)"), &dlg);
  root->addWidget(noFill);
  auto* fillBtn = kaMakeColorButton(&dlg, QColor(34, 197, 94, 140),
                                    QStringLiteral("클릭해서 색 고르기"), QStringLiteral("면 색"));
  auto* fillBox = kaWrapLabeled(&dlg, QStringLiteral("면 색"), fillBtn);
  root->addWidget(fillBox);
  auto* strokeBtn = kaMakeColorButton(&dlg, QColor(21, 128, 61),
                                      QStringLiteral("클릭해서 색 고르기"), QStringLiteral("외곽선 색"));
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("외곽선 색"), strokeBtn));
  connect(noFill, &QCheckBox::toggled, &dlg, [&dlg, fillBox](bool on) {
    fillBox->setVisible(!on);
    dlg.adjustSize();
  });
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("추가"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString title = nameEdit->text().trimmed();
  if (title.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("레이어 추가"), QStringLiteral("이름을 입력하세요."));
    return;
  }
  if (m_surveyPath.isEmpty()) {
    const auto ans = QMessageBox::question(this, QStringLiteral("레이어"),
                                           QStringLiteral("먼저 새 조사가 필요합니다. 지금 만들까요?"));
    if (ans != QMessageBox::Yes) return;
    newSurvey();
    if (m_surveyPath.isEmpty()) return;
  }
  QString err;
  const QString crsId = btn5187->isChecked() ? QStringLiteral("EPSG:5187")
                                             : QStringLiteral("EPSG:5186");
  auto* vl = LayerOps::createUserPolygonLayer(QgsProject::instance(), m_surveyPath, title,
                                              crsId, &err);
  if (!vl) {
    notify(Notice::Warning, QStringLiteral("레이어 추가"),
           QStringLiteral("레이어를 만들지 못했습니다."), err);
    return;
  }
  LayerOps::applySimpleVectorStyle(vl, fillBtn->property("kaColor").value<QColor>(),
                                   strokeBtn->property("kaColor").value<QColor>(),
                                   1.2, 3.5, noFill->isChecked(), false);
  LayerOps::applyAreaM2Labels(vl);
  if (m_layerTree) m_layerTree->setCurrentLayer(vl);
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("레이어 추가: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("레이어"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::editAttributesAtCanvasPos(const QPoint& canvasPos) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  ensureAttributeTool();
  if (!m_attributeTool) return;
  QgsVectorLayer* layer = nullptr;
  QgsFeature feat;
  if (!m_attributeTool->pickAtScreen(canvasPos, &layer, &feat) || !layer) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("이 위치에 도형이 없습니다.\n"
                                            "유구·조사구역 등을 그린 뒤 다시 클릭하세요."));
    return;
  }
  editFeatureAttributes(layer, feat);
#else
  Q_UNUSED(canvasPos);
#endif
}

void MainWindow::editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature) {
#if KA_HGIS_HAS_QGIS
  if (!layer || !layer->isValid() || !feature.isValid()) return;

  QgsFeature feat = feature;
  if (!layer->getFeatures(QgsFeatureRequest(feat.id())).nextFeature(feat)) {
    QMessageBox::warning(this, QStringLiteral("속성"), QStringLiteral("피처를 다시 읽을 수 없습니다."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("도형 속성 — %1").arg(layer->name()));
  dlg.setMinimumWidth(420);
  auto* form = new QFormLayout(&dlg);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto* tip = new QLabel(
      QStringLiteral("그린 도형의 속성입니다. 종류·시대 등을 입력한 뒤 저장하세요."), &dlg);
  tip->setWordWrap(true);
  form->addRow(tip);

  struct Row {
    int index = -1;
    QString name;
    QWidget* editor = nullptr;
    QMetaType::Type type = QMetaType::QString;
  };
  QVector<Row> rows;
  const QgsFields fields = layer->fields();
  for (int i = 0; i < fields.count(); ++i) {
    const QgsField f = fields.at(i);
    const QString name = f.name();
    if (name.compare(QLatin1String("fid"), Qt::CaseInsensitive) == 0) continue;
    if (name.startsWith(QLatin1String("ogc_"), Qt::CaseInsensitive)) continue;

    Row row;
    row.index = i;
    row.name = name;
    row.type = static_cast<QMetaType::Type>(f.type());

    const QVariant cur = feat.attribute(i);
    if (row.type == QMetaType::Double || row.type == QMetaType::Float ||
        row.type == QMetaType::Int || row.type == QMetaType::LongLong) {
      auto* edit = new QLineEdit(&dlg);
      if (cur.isValid() && !cur.isNull())
        edit->setText(cur.toString());
      row.editor = edit;
    } else {
      auto* edit = new QLineEdit(&dlg);
      edit->setText(cur.toString());
      if (name == QLatin1String("kind"))
        edit->setPlaceholderText(QStringLiteral("예: 주거지, 수혈, 구"));
      else if (name == QLatin1String("period"))
        edit->setPlaceholderText(QStringLiteral("예: 청동기, 원삼국"));
      else if (name == QLatin1String("feature_no"))
        edit->setPlaceholderText(QStringLiteral("예: 1호"));
      row.editor = edit;
    }
    form->addRow(attributeFieldLabelKo(name), row.editor);
    rows.push_back(row);
  }

  if (rows.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("이 레이어에 편집할 속성 필드가 없습니다."));
    return;
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  const bool wasEditable = layer->isEditable();
  if (!wasEditable && !layer->startEditing()) {
    QString detail = layer->dataProvider() ? layer->dataProvider()->error().message() : QString();
    QMessageBox::warning(this, QStringLiteral("속성"),
                         QStringLiteral("편집 모드를 열 수 없습니다: %1\n%2")
                             .arg(layer->name(), detail));
    return;
  }

  layer->beginEditCommand(QStringLiteral("속성 편집"));
  bool ok = true;
  for (const Row& row : rows) {
    auto* edit = qobject_cast<QLineEdit*>(row.editor);
    if (!edit) continue;
    const QString text = edit->text().trimmed();
    QVariant value;
    if (text.isEmpty()) {
      value = QVariant(QString());
    } else if (row.type == QMetaType::Double || row.type == QMetaType::Float) {
      bool conv = false;
      value = text.toDouble(&conv);
      if (!conv) {
        QMessageBox::warning(this, QStringLiteral("속성"),
                             QStringLiteral("숫자 형식이 아닙니다: %1").arg(attributeFieldLabelKo(row.name)));
        ok = false;
        break;
      }
    } else if (row.type == QMetaType::Int || row.type == QMetaType::LongLong) {
      bool conv = false;
      value = text.toLongLong(&conv);
      if (!conv) {
        QMessageBox::warning(this, QStringLiteral("속성"),
                             QStringLiteral("정수 형식이 아닙니다: %1").arg(attributeFieldLabelKo(row.name)));
        ok = false;
        break;
      }
    } else {
      value = text;
    }
    if (!layer->changeAttributeValue(feat.id(), row.index, value)) {
      ok = false;
      break;
    }
  }

  if (!ok) {
    layer->destroyEditCommand();
    if (!wasEditable) layer->rollBack();
    return;
  }
  layer->endEditCommand();

  if (!wasEditable) {
    if (!layer->commitChanges()) {
      QMessageBox::warning(this, QStringLiteral("속성"),
                           QStringLiteral("저장 실패\n%1").arg(layer->commitErrors().join(QLatin1Char('\n'))));
      layer->rollBack();
      return;
    }
  }

  LayerOps::applyDomainDrawStyle(layer, LayerOps::layerKeyOf(layer));
  layer->triggerRepaint();
  if (m_canvas) {
    if (m_canvas->isCachingEnabled())
      layer->triggerRepaint();
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("속성 저장: %1 (#%2)").arg(layer->name()).arg(feat.id()), 5000);
#else
  Q_UNUSED(layer);
  Q_UNUSED(feature);
#endif
}

void MainWindow::onGeometryCaptured(const QgsGeometry& geom) {
  try {
    QgsVectorLayer* layer = m_editLayer;
    if (!layer || !layer->isValid()) {
      statusBar()->showMessage(QStringLiteral("편집 레이어 없음 — 그리기 도구를 다시 선택하세요"), 5000);
      return;
    }
    if (geom.isEmpty() || geom.isNull()) {
      statusBar()->showMessage(QStringLiteral("빈 도형 (면≥3점, 선≥2점) — 이어서 그리세요"), 5000);
      return;
    }
    if (!layer->isEditable() && !layer->startEditing()) {
      QMessageBox::warning(this, QStringLiteral("편집"),
                           QStringLiteral("편집 모드 실패: %1").arg(layer->name()));
      return;
    }

    const QgsFeatureIds beforeIds = layer->allFeatureIds();
    QgsFeature feat(layer->fields());
    feat.setGeometry(geom);
    if (LayerOps::layerKeyOf(layer) == QLatin1String("paleo_landform")) {
      const int kindIdx = layer->fields().indexOf(QStringLiteral("kind"));
      const int statusIdx = layer->fields().indexOf(QStringLiteral("status"));
      if (kindIdx >= 0) feat.setAttribute(kindIdx, QStringLiteral("미분류"));
      if (statusIdx >= 0) feat.setAttribute(statusIdx, QStringLiteral("가설"));
    }
    if (!layer->addFeature(feat)) {
      QMessageBox::warning(this, QStringLiteral("오류"),
                           QStringLiteral("피처 추가 실패\n%1").arg(layer->commitErrors().join(QLatin1Char('\n'))));
      return;
    }

    if (!layer->commitChanges(false)) {
      const QString errs = layer->commitErrors().join(QLatin1Char('\n'));
      layer->rollBack();
      if (!layer->startEditing()) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"),
                             QStringLiteral("도형 저장 실패 후 편집 재개 불가.\n%1").arg(errs));
        return;
      }
      QMessageBox::warning(this, QStringLiteral("저장 실패"),
                           QStringLiteral("도형을 파일에 쓰지 못했습니다.\n%1").arg(errs));
      return;
    }
    qint64 addedId = -1;
    const QgsFeatureIds afterIds = layer->allFeatureIds();
    for (QgsFeatureId id : afterIds) {
      if (!beforeIds.contains(id)) {
        addedId = static_cast<qint64>(id);
        break;
      }
    }
    if (addedId < 0 && feat.id() >= 0)
      addedId = static_cast<qint64>(feat.id());
    if (addedId >= 0)
      m_committedUndo.append(qMakePair(layer->id(), addedId));
    if (!layer->isEditable() && !layer->startEditing()) {
      statusBar()->showMessage(QStringLiteral("저장됨 · 편집 모드 재시작 실패 — 그리기 도구를 다시 선택"), 8000);
    }

    const QString drawnKey = LayerOps::layerKeyOf(layer);
    const bool domain = drawnKey == QLatin1String("survey_area")
                        || drawnKey == QLatin1String("feature_poly")
                        || drawnKey == QLatin1String("feature_line")
                        || drawnKey == QLatin1String("section_line")
                        || drawnKey == QLatin1String("control_points")
                        || drawnKey == QLatin1String("artifact_point");
    if (domain)
      LayerOps::applyDomainDrawStyle(layer, drawnKey);
    if (layer->geometryType() == Qgis::GeometryType::Polygon
        && (domain || drawnKey.startsWith(QLatin1String("user_poly"))))
      LayerOps::applyAreaM2Labels(layer);
    layer->updateExtents();
    layer->triggerRepaint();
    if (m_terrain3dStudio && m_terrain3dStudio->hasScene()) {
      if (m_terrain3dLayoutStudio)
        refreshTerrain3dDrapeAndSheet();
      else
        m_terrain3dStudio->refreshDrape();
    }
    if (m_canvas) {
      m_canvas->freeze(false);
      m_canvas->setRenderFlag(true);
      m_canvas->refresh();
    }
    if (m_captureTool && m_canvas && m_canvas->mapTool() != m_captureTool) {
      m_canvas->setMapTool(m_captureTool);
      m_canvas->setFocus(Qt::OtherFocusReason);
    }

    const long long n = static_cast<long long>(layer->featureCount());
    statusBar()->showMessage(
        QStringLiteral("도형 저장 (%1, %2개) · 점을 끌면 수정 · 빈 곳 좌클릭=이어서 그리기")
            .arg(layer->name())
            .arg(n),
        8000);
    refreshWorkPanel();
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
    if (m_subToolsMode == QLatin1String("align")) stopAlignSession();

    m_canvas->freeze(false);
    m_canvas->setRenderFlag(true);

    if (!layer->isEditable()) {
      if (!layer->startEditing()) {
        QString detail = layer->dataProvider() ? layer->dataProvider()->error().message() : QString();
        QMessageBox::warning(
            this, QStringLiteral("편집"),
            QStringLiteral("편집 모드를 열 수 없습니다: %1\n%2")
                .arg(layer->name(), detail.isEmpty() ? QStringLiteral("GPKG가 다른 프로그램에서 열려 있는지 확인")
                                                     : detail));
        return;
      }
    }

    m_editLayer = layer;
    if (m_layerTree)
      m_layerTree->setCurrentLayer(layer);

    applySnapConfig();

    KaCaptureMapTool::Mode mode = KaCaptureMapTool::Mode::Polygon;
    const Qgis::GeometryType gt = layer->geometryType();
    if (gt == Qgis::GeometryType::Line) mode = KaCaptureMapTool::Mode::Line;
    else if (gt == Qgis::GeometryType::Point) mode = KaCaptureMapTool::Mode::Point;
    else if (gt == Qgis::GeometryType::Null || gt == Qgis::GeometryType::Unknown) {
      QMessageBox::warning(this, QStringLiteral("편집"),
                           QStringLiteral("이 레이어 지오메트리 타입을 알 수 없습니다: %1").arg(layer->name()));
      return;
    }

    if (!m_captureTool) {
      m_captureTool = new KaCaptureMapTool(m_canvas);
      m_captureTool->setParent(this);
      connect(m_captureTool, &KaCaptureMapTool::geometryCaptured, this, &MainWindow::onGeometryCaptured,
              Qt::DirectConnection);
      connect(m_captureTool, &KaCaptureMapTool::vertexMoved, this, [this]() {
        persistSurveyWork();
        if (m_canvas) m_canvas->refresh();
        statusBar()->showMessage(QStringLiteral("꼭짓점을 고쳤습니다. 끌어서 계속 수정하세요."), 5000);
      });
      connect(m_captureTool, &KaCaptureMapTool::captureCanceled, this, [this]() {
        statusBar()->showMessage(
            QStringLiteral("아직 저장 안 됨 — 면은 점 3개 이상, 선은 2개 이상 필요. 우클릭으로 완료."),
            8000);
      });
    }

    m_captureTool->setTargetLayer(layer);
    m_captureTool->setMode(mode);
    m_captureTool->setEasyDraw(false);
    m_canvas->setMapTool(m_captureTool);
    m_canvas->setFocus(Qt::OtherFocusReason);
    m_canvas->setCursor(Qt::CrossCursor);

    const QString how = (mode == KaCaptureMapTool::Mode::Point)
                            ? QStringLiteral("지도 좌클릭 = 점")
                            : QStringLiteral("좌클릭=꼭짓점 / 우클릭=완료 / 그린 뒤 점을 끌어 수정 / ESC=취소");
    statusBar()->showMessage(QStringLiteral("그리기 중: %1 | %2").arg(layer->name(), how), 0);
  } catch (const std::exception& ex) {
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QString::fromUtf8(ex.what()));
  } catch (...) {
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QStringLiteral("내부 오류"));
  }
}
#endif

void MainWindow::startEasyDraw() {
#if KA_HGIS_HAS_QGIS
  m_snapEnabled = true;
  applySnapConfig();
  if (m_surveyPath.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("쉽게그리기"),
                             QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요."));
    return;
  }
  QgsVectorLayer* layer = nullptr;
  if (QgsProject* proj = QgsProject::instance()) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      auto* vl = qobject_cast<QgsVectorLayer*>(l);
      if (!vl || !vl->isValid()) continue;
      if (vl->name() == QLatin1String("쉽게그리기")
          && LayerOps::layerKeyOf(vl).startsWith(QLatin1String("user_poly"))) {
        layer = vl;
        break;
      }
    }
  }
  if (!layer) {
    QString err;
    layer = LayerOps::createUserPolygonLayer(QgsProject::instance(), m_surveyPath,
                                             QStringLiteral("쉽게그리기"), m_workCrs, &err);
    if (!layer) {
      QMessageBox::warning(this, QStringLiteral("쉽게그리기"), err);
      return;
    }
    LayerOps::applySimpleVectorStyle(layer, QColor(30, 103, 198, 70), QColor(30, 103, 198), 1.4, 3.5,
                                     false, false);
    LayerOps::applyAreaM2Labels(layer);
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  beginEdit(layer);
  if (m_captureTool) {
    m_captureTool->setEasyDraw(true);
    m_captureTool->setSnapEnabled(true);
  }
  statusBar()->showMessage(
      QStringLiteral("쉽게그리기 → 「쉽게그리기」레이어에 저장. 지적은 자석만 사용. 우클릭=완료"),
      0);
#else
  statusBar()->showMessage(QStringLiteral("쉽게그리기 (스텁)"));
#endif
}

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
  QgsVectorLayer* cur =
      m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  QgsVectorLayer* target =
      LayerOps::digitizeTargetLayer(QgsProject::instance(), cur, QStringLiteral("feature_poly"));
  if (!target)
    target = ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("유구면"));
  beginEdit(target);
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

void MainWindow::startEditSectionLine() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("section_line"), QStringLiteral("단면선")));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 단면선"), 3000);
#endif
}

void MainWindow::startEditArtifact() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("artifact_point"), QStringLiteral("유물")));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 유물"), 3000);
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
  notify(Notice::Success, QStringLiteral("폴리곤 묶기"),
         QStringLiteral("여러 폴리곤을 하나의 지오메트리로 합쳤습니다."),
         QStringLiteral("문화재 인트라넷 제출 시 「SHP내보내기」하면 "
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
  refreshWorkPanel();
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
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("CSV 기준점"), QString(),
                                                    QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) return;
#if KA_HGIS_HAS_QGIS
  auto* layer = ensureDomainLayerForEdit(QStringLiteral("control_points"), QStringLiteral("GPS기준점"));
  if (!layer) return;
  QString err;
  const int n = LayerOps::importControlPointsCsv(layer, path, &err);
  if (n < 0) {
    QMessageBox::warning(this, QStringLiteral("CSV"), err);
    return;
  }
  m_stubGcp = int(layer->featureCount());
  m_stubHasMeta = true;
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("CSV 기준점 %1개 저장 (합 %2)").arg(n).arg(m_stubGcp), 6000);
  refreshWorkPanel();
#else
  Q_UNUSED(path);
  statusBar()->showMessage(QStringLiteral("스텁: CSV"), 3000);
#endif
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
  if (!m_checklist) return;
#if KA_HGIS_HAS_QGIS
  // 검수 규칙이 도면 존재를 본다. 조사를 열 때가 아니라 여기서 준비한다.
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
#endif
  if (m_checklist->ruleCount() == 0) m_checklist->loadRules(rulesPath());
  const auto results = m_checklist->evaluate(buildProjectState());
  int err = 0, warn = 0;
  for (const auto& r : results) {
    if (r.passed) continue;
    if (r.severity == QLatin1String("error")) err++; else warn++;
  }
  m_lastChecklistErrors = err;
  statusBar()->showMessage(QStringLiteral("검수: error %1 / warn %2").arg(err).arg(warn), 8000);
  refreshWorkPanel();
}

void MainWindow::exportPdf() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
#else
  QMessageBox::warning(this, QStringLiteral("도면"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::exportShpPackage() {
#if KA_HGIS_HAS_QGIS
  LayoutService::ensureDefaultLayouts(QgsProject::instance());
#endif
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
  if (hasErr) {
    QMessageBox::warning(
        this, QStringLiteral("제출 차단"),
        QStringLiteral("도면 검수 error가 있어 제출 패키지를 만들 수 없습니다.\n"
                       "「도면검수」로 항목을 고친 뒤 다시 시도하세요.\n\n%1")
            .arg(summary));
    statusBar()->showMessage(QStringLiteral("제출 차단: 검수 error 잔존"), 8000);
    return;
  }
  QString err;
#if KA_HGIS_HAS_QGIS
  const QString out = ExportService::exportSubmissionPackage(
      QgsProject::instance(), dir, enc, summary, /*blockOnError=*/true, hasErr, &err);
#else
  const QString out;
  err = QStringLiteral("QGIS required");
#endif
  if (out.isEmpty())
    notify(Notice::Warning, QStringLiteral("내보내기"),
           QStringLiteral("제출 패키지를 만들지 못했습니다."), err);
  else {
    m_packageCreated = true;
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
    notify(Notice::Success, QStringLiteral("내보내기"),
           QStringLiteral("제출 패키지를 만들었습니다."), QDir::toNativeSeparators(out));
    refreshWorkPanel();
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
#if KA_HGIS_HAS_QGIS
static bool layerSitsOnWorkMap(QgsMapLayer* layer, const QString& workCrs) {
  if (!layer) return false;
  const QgsRectangle e = layer->extent();
  if (e.isEmpty() || !e.isFinite()) return false;
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(workCrs);
  if (kr.isEmpty()) return true;
  return kr.intersects(e);
}
#endif

void MainWindow::ensureAlignSplit() {
#if KA_HGIS_HAS_QGIS
  if (m_mapSplitter) return;
  auto* mapCard = findChild<QFrame*>(QStringLiteral("mapCard"));
  if (!mapCard || !m_canvas) return;
  auto* mapLay = qobject_cast<QVBoxLayout*>(mapCard->layout());
  if (!mapLay) return;

  m_mapSplitter = new QSplitter(Qt::Horizontal, mapCard);
  m_mapSplitter->setObjectName(QStringLiteral("alignSplitter"));
  m_mapSplitter->setChildrenCollapsible(false);

  m_alignLeftPane = new QWidget(m_mapSplitter);
  auto* ll = new QVBoxLayout(m_alignLeftPane);
  ll->setContentsMargins(0, 0, 0, 0);
  ll->setSpacing(2);
  m_alignLeftLabel = new QLabel(QStringLiteral("왼쪽 · 맞출 도면"), m_alignLeftPane);
  m_alignLeftLabel->setObjectName(QStringLiteral("subToolbarCaption"));
  ll->addWidget(m_alignLeftLabel);

  m_alignImage = new KaImageView(m_alignLeftPane);
  m_alignImage->setCursor(Qt::CrossCursor);
  connect(m_alignImage, &KaImageView::pixelClicked, this, [this](double x, double y) {
    if (!m_alignTool) return;
    m_alignTool->setSourcePoint(x, y);
    if (m_canvas) m_canvas->setMapTool(m_alignTool);
  });
  connect(m_alignImage, &KaImageView::viewChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  ll->addWidget(m_alignImage, 1);

  m_alignLeftCanvas = new QgsMapCanvas(m_alignLeftPane);
  KaTheme::excludeMapSurface(m_alignLeftCanvas);
  m_alignLeftCanvas->setCanvasColor(KaTheme::tokens().canvasNeutral);
  m_alignLeftCanvas->enableAntiAliasing(true);
  m_alignPickTool = new KaAlignPickTool(m_alignLeftCanvas);
  m_alignPickTool->setParent(this);
  connect(m_alignPickTool, &KaAlignPickTool::picked, this, [this](const QgsPointXY& pt) {
    if (!m_alignTool) return;
    m_alignTool->setSourcePoint(pt.x(), pt.y());
    if (m_canvas) m_canvas->setMapTool(m_alignTool);
  });
  m_alignLeftCanvas->setMapTool(m_alignPickTool);
  connect(m_alignLeftCanvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  connect(m_alignLeftCanvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  ll->addWidget(m_alignLeftCanvas, 1);
  m_alignLeftCanvas->hide();

  m_alignPointList = new QListWidget(m_alignLeftPane);
  m_alignPointList->setObjectName(QStringLiteral("alignPointList"));
  m_alignPointList->setMaximumHeight(130);
  m_alignPointList->setToolTip(QStringLiteral("찍은 점. Delete로 지웁니다"));
  auto* delSc = new QShortcut(QKeySequence::Delete, m_alignPointList);
  connect(delSc, &QShortcut::activated, this, &MainWindow::deleteSelectedAlignPoint);
  ll->addWidget(m_alignPointList);

  mapLay->removeWidget(m_canvas);
  m_mapSplitter->addWidget(m_alignLeftPane);
  m_mapSplitter->addWidget(m_canvas);
  m_mapSplitter->setStretchFactor(0, 1);
  m_mapSplitter->setStretchFactor(1, 1);
  mapLay->insertWidget(0, m_mapSplitter, 1);

  m_alignOverlay = new KaAlignLinkOverlay(m_mapSplitter);
  m_alignOverlay->setMouseTracking(true);
  m_alignOverlay->raise();
  m_mapSplitter->setMouseTracking(true);
  m_mapSplitter->installEventFilter(this);
  m_alignOverlay->installEventFilter(this);
  m_alignLeftPane->setMouseTracking(true);
  m_alignLeftPane->installEventFilter(this);
  if (m_alignImage) {
    m_alignImage->setMouseTracking(true);
    m_alignImage->installEventFilter(this);
    if (m_alignImage->viewport()) {
      m_alignImage->viewport()->setMouseTracking(true);
      m_alignImage->viewport()->installEventFilter(this);
    }
  }
  if (m_canvas) {
    m_canvas->setMouseTracking(true);
    m_canvas->installEventFilter(this);
    if (m_canvas->viewport()) {
      m_canvas->viewport()->setMouseTracking(true);
      m_canvas->viewport()->installEventFilter(this);
    }
  }
  if (m_alignLeftCanvas) {
    m_alignLeftCanvas->setMouseTracking(true);
    m_alignLeftCanvas->installEventFilter(this);
    if (m_alignLeftCanvas->viewport()) {
      m_alignLeftCanvas->viewport()->setMouseTracking(true);
      m_alignLeftCanvas->viewport()->installEventFilter(this);
    }
  }
#endif
}

void MainWindow::showAlignSplit() {
#if KA_HGIS_HAS_QGIS
  ensureAlignSplit();
  if (!m_mapSplitter || !m_alignLeftPane || !m_alignTool) return;
  m_alignLeftPane->show();
  if (m_alignTool->isRasterSession()) {
    if (m_alignImage) {
      m_alignImage->show();
      m_alignImage->clearMarks();
      m_alignImage->loadPath(m_alignTool->rasterSourcePath());
    }
    if (m_alignLeftCanvas) m_alignLeftCanvas->hide();
    if (m_alignLeftLabel)
      m_alignLeftLabel->setText(QStringLiteral("왼쪽 · 그림 — 여기를 먼저 찍기"));
  } else {
    if (m_alignImage) m_alignImage->hide();
    if (m_alignLeftCanvas) {
      m_alignLeftCanvas->show();
      LayerOps::applyCanvasScreenDpi(m_alignLeftCanvas);
      if (QgsMapLayer* src = m_alignTool->sourceDisplayLayer()) {
        if (src->crs().isValid())
          m_alignLeftCanvas->setDestinationCrs(src->crs());
        m_alignLeftCanvas->setLayers(QList<QgsMapLayer*>() << src);
        const QgsRectangle ext = src->extent();
        if (!ext.isEmpty() && ext.isFinite())
          m_alignLeftCanvas->setExtent(ext);
        m_alignLeftCanvas->refresh();
      }
    }
    if (m_alignLeftLabel)
      m_alignLeftLabel->setText(QStringLiteral("왼쪽 · CAD — 여기를 먼저 찍기"));
  }
  m_mapSplitter->setSizes({1000, 1000});
#endif
}

void MainWindow::hideAlignSplit() {
#if KA_HGIS_HAS_QGIS
  if (m_alignLeftPane) m_alignLeftPane->hide();
  if (m_mapSplitter) m_mapSplitter->setSizes({0, 1});
  if (m_alignOverlay) m_alignOverlay->hide();
  if (m_alignCursorTimer) m_alignCursorTimer->stop();
  for (auto* m : m_alignLeftMarks) delete m;
  m_alignLeftMarks.clear();
#endif
}

void MainWindow::refreshAlignUi() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool) return;
  QVector<QPointF> pts;
  for (const GeorefService::Pair& p : m_alignTool->pairs())
    pts.append(QPointF(p.srcX, p.srcY));
  QPointF pending;
  const QPointF* pend = nullptr;
  if (m_alignTool->hasPendingSource()) {
    pending = QPointF(m_alignTool->pendingSrcX(), m_alignTool->pendingSrcY());
    pend = &pending;
  }
  if (m_alignImage && m_alignImage->isVisible())
    m_alignImage->setMarks(pts, pend);

  for (auto* m : m_alignLeftMarks) delete m;
  m_alignLeftMarks.clear();
  if (m_alignLeftCanvas && m_alignLeftCanvas->isVisible()) {
    auto addMk = [&](const QgsPointXY& pt, const QColor& col) {
      auto* mk = new QgsVertexMarker(m_alignLeftCanvas);
      mk->setIconType(QgsVertexMarker::ICON_CIRCLE);
      mk->setIconSize(14);
      mk->setPenWidth(2);
      mk->setColor(col);
      mk->setFillColor(QColor(255, 255, 255, 230));
      mk->setCenter(pt);
      mk->show();
      m_alignLeftMarks.append(mk);
    };
    for (const QPointF& p : pts)
      addMk(QgsPointXY(p.x(), p.y()), QColor(220, 38, 38));
    if (pend)
      addMk(QgsPointXY(pend->x(), pend->y()), QColor(234, 179, 8));
  }

  if (m_alignPointList) {
    m_alignPointList->clear();
    const auto& pairs = m_alignTool->pairs();
    for (int i = 0; i < pairs.size(); ++i) {
      m_alignPointList->addItem(
          QStringLiteral("%1번  왼쪽 → 오른쪽").arg(i + 1));
    }
    if (m_alignTool->hasPendingSource()) {
      m_alignPointList->addItem(
          QStringLiteral("%1번  왼쪽만 — 오른쪽 모서리를 찍으세요").arg(pairs.size() + 1));
    }
  }

  updateAlignOverlay();
  if (m_alignTool->hasPendingSource() && !m_alignApplied) {
    if (!m_alignCursorTimer) {
      m_alignCursorTimer = new QTimer(this);
      m_alignCursorTimer->setInterval(16);
      connect(m_alignCursorTimer, &QTimer::timeout, this, [this]() {
        if (m_alignTool && m_alignTool->hasPendingSource() && !m_alignApplied)
          trackAlignPointer(QCursor::pos());
        else if (m_alignCursorTimer)
          m_alignCursorTimer->stop();
      });
    }
    if (!m_alignCursorTimer->isActive()) m_alignCursorTimer->start();
    trackAlignPointer(QCursor::pos());
  } else if (m_alignCursorTimer) {
    m_alignCursorTimer->stop();
  }
#endif
}

void MainWindow::trackAlignPointer(const QPoint& globalPos) {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool || !m_alignTool->hasPendingSource() || m_alignApplied) return;
  if (!m_alignOverlay) return;

  m_alignLiveScreen = m_alignOverlay->mapFromGlobal(globalPos);
  m_alignLiveScreenValid = true;
  m_alignCursorValid = false;

  if (m_canvas) {
    QWidget* vp = m_canvas->viewport() ? static_cast<QWidget*>(m_canvas->viewport())
                                       : static_cast<QWidget*>(m_canvas);
    const QPoint inRight = vp->mapFromGlobal(globalPos);
    if (vp->rect().contains(inRight)) {
      const QgsMapToPixel& m2p = m_canvas->mapSettings().mapToPixel();
      QgsPointXY mapPt = m2p.toMapCoordinates(inRight.x(), inRight.y());
      const QgsPointXY cursorMap = mapPt;
      if (m_canvas->snappingUtils()) {
        const QgsPointLocator::Match hit = m_canvas->snappingUtils()->snapToMap(inRight);
        if (hit.isValid()) {
          mapPt = hit.point();
          const double mupp = m_canvas->mapUnitsPerPixel();
          if (mupp > 1e-12) {
            m_alignLiveScreen += QPoint(
                int(std::lround((mapPt.x() - cursorMap.x()) / mupp)),
                int(std::lround((cursorMap.y() - mapPt.y()) / mupp)));
          }
        }
      }
      m_alignCursorX = mapPt.x();
      m_alignCursorY = mapPt.y();
      m_alignCursorValid = true;
      if (m_alignTool) m_alignTool->setMapHint(mapPt.x(), mapPt.y(), true);
    } else if (m_alignTool) {
      m_alignTool->setMapHint(0, 0, false);
    }
  }
  updateAlignOverlay();
#endif
}

void MainWindow::updateAlignOverlay() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignOverlay || !m_mapSplitter || !m_alignTool || !m_alignLeftPane
      || !m_alignLeftPane->isVisible() || m_alignApplied) {
    if (m_alignOverlay) m_alignOverlay->hide();
    return;
  }
  m_alignOverlay->setGeometry(m_mapSplitter->rect());
  m_alignOverlay->show();
  m_alignOverlay->raise();

  // 오버레이는 스플리터의 자식이라 캔버스·그림뷰의 조상이 아니라 형제다.
  // QWidget::mapTo는 대상이 조상일 때만 유효하고, 형제를 주면 최상위 창 좌표를
  // 돌려줘 화살표가 스플리터 원점만큼 통째로 밀린다. 라이브 점선이 쓰는
  // mapFromGlobal과 같은 기준으로 맞춘다(trackAlignPointer).
  auto toOverlay = [this](QWidget* from, const QPoint& inFrom) -> QPoint {
    if (!from || !m_alignOverlay) return {};
    return m_alignOverlay->mapFromGlobal(from->mapToGlobal(inFrom));
  };
  auto mapToOverlay = [&](QgsMapCanvas* c, double mx, double my) -> QPoint {
    if (!c || !c->viewport()) return {};
    // QgsMapMouseEvent::mapToPixelCoordinates: transform() == 뷰포트 좌표.
    const QgsPointXY xy = c->mapSettings().mapToPixel().transform(QgsPointXY(mx, my));
    return toOverlay(c->viewport(),
                     QPoint(int(std::lround(xy.x())), int(std::lround(xy.y()))));
  };
  auto srcToOverlay = [&](double sx, double sy) -> QPoint {
    // viewPosForPixel이 돌려주는 값은 뷰 위젯이 아니라 뷰포트 좌표다.
    if (m_alignImage && m_alignImage->isVisible() && m_alignImage->viewport())
      return toOverlay(m_alignImage->viewport(), m_alignImage->viewPosForPixel(sx, sy));
    if (m_alignLeftCanvas && m_alignLeftCanvas->isVisible())
      return mapToOverlay(m_alignLeftCanvas, sx, sy);
    return {};
  };

  const auto& pairs = m_alignTool->pairs();
  QVector<QLine> done;
  for (int i = 0; i < pairs.size(); ++i) {
    const QPoint a = srcToOverlay(pairs[i].srcX, pairs[i].srcY);
    const QPoint b = mapToOverlay(m_canvas, pairs[i].mapX, pairs[i].mapY);
    if (!a.isNull() && !b.isNull())
      done.append(QLine(a, b));
  }
  QLine live;
  bool hasLive = false;
  if (m_alignTool->hasPendingSource()) {
    const QPoint a = srcToOverlay(m_alignTool->pendingSrcX(), m_alignTool->pendingSrcY());
    QPoint b;
    if (m_alignCursorValid)
      b = mapToOverlay(m_canvas, m_alignCursorX, m_alignCursorY);
    else if (m_alignLiveScreenValid)
      b = m_alignLiveScreen;
    if (!a.isNull() && !b.isNull()) {
      live = QLine(a, b);
      hasLive = true;
    }
  }
  m_alignOverlay->setLinks(done, live, hasLive);
#endif
}

void MainWindow::deleteSelectedAlignPoint() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool || !m_alignPointList) return;
  const int row = m_alignPointList->currentRow();
  if (row < 0) {
    m_alignTool->removeLastPair();
    return;
  }
  if (row >= m_alignTool->pairCount()) {
    m_alignTool->removeLastPair();
    return;
  }
  m_alignTool->removePairAt(row);
#endif
}

void MainWindow::applyAlignMove() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool) return;
  QString err;
  if (!m_alignTool->applyMove(&err)) {
    QMessageBox::warning(this, QStringLiteral("이동"), err);
    return;
  }
  m_alignApplied = true;
  if (m_alignOverlay) m_alignOverlay->hide();
  hideAlignSplit();
  applySnapConfig();
  ensureDefaultBasemaps();
  QgsMapLayer* aligned = m_alignTool->targetLayer();
  if (aligned) {
    LayerOps::setAlignPending(aligned, false);
    if (QgsProject::instance() && QgsProject::instance()->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(aligned->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (aligned && m_canvas) {
    QList<QgsMapLayer*> stacked = m_canvas->layers();
    if (stacked.isEmpty())
      stacked = LayerOps::visibleLayersPaintOrder(QgsProject::instance());
    stacked.removeAll(aligned);
    int insertAt = stacked.size();
    for (int i = 0; i < stacked.size(); ++i) {
      if (stacked[i] && (LayerOps::isBasemapLayer(stacked[i]) ||
                         stacked[i]->name().contains(QStringLiteral("위성")) ||
                         stacked[i]->name().contains(QStringLiteral("지적")))) {
        insertAt = i;
        break;
      }
    }
    stacked.insert(insertAt, aligned);
    m_canvas->setLayers(stacked);

    QgsRectangle ext = aligned->extent();
    if (!ext.isEmpty() && ext.isFinite() && ext.xMinimum() > 1000.0) {
      m_canvas->setExtent(ext);
      m_canvas->zoomToFeatureExtent(ext);
      m_canvas->zoomScale(m_canvas->scale() * 1.25, true);
    }
  }
  refreshAlignUi();
  const auto kickAlignedPaint = [this]() {
    if (!m_canvas) return;
    if (m_alignTool) {
      if (QgsMapLayer* l = m_alignTool->targetLayer())
        l->triggerRepaint();
    }
    LayerOps::refreshCanvasIfIdle(m_canvas);
  };
  kickAlignedPaint();
  QTimer::singleShot(0, this, kickAlignedPaint);
  QTimer::singleShot(350, this, kickAlignedPaint);
  statusBar()->showMessage(
      QStringLiteral("맞춘 도면을 지금 보는 지적 위에 올렸습니다. 흰 종이만 빼고 먹선은 진하게 보이게 했습니다."),
      10000);
#endif
}

void MainWindow::stopAlignSession() {
#if KA_HGIS_HAS_QGIS
  hideAlignSplit();
  if (!m_alignTool) return;
  if (m_canvas && m_canvas->mapTool() == m_alignTool)
    m_canvas->unsetMapTool(m_alignTool);
  m_alignTool->endSession();
  if (m_panTool && m_canvas) m_canvas->setMapTool(m_panTool);
#endif
}

void MainWindow::showSubToolsAlign() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("align");
  auto* lab = new QLabel(QStringLiteral("  맞추기 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("transform")), QStringLiteral("이동"),
                          this, &MainWindow::applyAlignMove);
  m_subToolbar->addAction(QStringLiteral("점 지우기"), this, [this]() {
    deleteSelectedAlignPoint();
    if (m_alignTool) statusBar()->showMessage(m_alignTool->statusText(), 4000);
  });
  m_subToolbar->addAction(QStringLiteral("되돌리기"), this, [this]() {
    if (m_alignTool) m_alignTool->restoreOriginals();
    m_alignApplied = false;
    refreshAlignUi();
    statusBar()->showMessage(QStringLiteral("맞추기를 처음 상태로 되돌렸습니다"), 4000);
  });
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("save")), QStringLiteral("맞추기 저장"),
                          this, [this]() {
                            if (!m_alignTool) return;
                            if (m_alignTool->pairCount() < 2 && !m_alignApplied) {
                              notify(Notice::Warning, QStringLiteral("맞추기"),
                                     QStringLiteral("점을 2곳 이상 찍은 뒤 저장하세요."));
                              return;
                            }
                            if (!m_alignApplied) {
                              applyAlignMove();
                              if (!m_alignApplied) return;
                            }
                            QString path, err;
                            if (!m_alignTool->saveAligned(&path, &err)) {
                              notify(Notice::Warning, QStringLiteral("맞추기"),
                                     QStringLiteral("맞춘 결과를 저장하지 못했습니다."), err);
                              return;
                            }
                            hideAlignSplit();
                            if (QgsMapLayer* aligned = m_alignTool->targetLayer()) {
                              LayerOps::setAlignPending(aligned, false);
                              if (QgsProject::instance() && QgsProject::instance()->layerTreeRoot()) {
                                if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(aligned->id()))
                                  n->setItemVisibilityChecked(true);
                              }
                              if (m_canvas) {
                                QList<QgsMapLayer*> stacked = m_canvas->layers();
                                stacked.removeAll(aligned);
                                int insertAt = stacked.size();
                                for (int i = 0; i < stacked.size(); ++i) {
                                  if (stacked[i] && (LayerOps::isBasemapLayer(stacked[i]) ||
                                                     stacked[i]->name().contains(QStringLiteral("위성")) ||
                                                     stacked[i]->name().contains(QStringLiteral("지적")))) {
                                    insertAt = i;
                                    break;
                                  }
                                }
                                stacked.insert(insertAt, aligned);
                                m_canvas->setLayers(stacked);
                                aligned->triggerRepaint();
                                LayerOps::refreshCanvasIfIdle(m_canvas);
                              }
                            }
                            notify(Notice::Success, QStringLiteral("맞추기"),
                                   QStringLiteral("맞춰 두었습니다. 이 도면은 참고용이니 제출할 구역은 그리기로 직접 그리세요."),
                                   QDir::toNativeSeparators(path));
                            statusBar()->showMessage(QStringLiteral("맞춤 저장: %1").arg(path), 8000);
                          });
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
#endif
}

void MainWindow::startAlignSession(QgsMapLayer* layer) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !layer) return;
  m_alignApplied = false;
  m_alignCursorValid = false;
  if (!m_alignTool) {
    m_alignTool = new KaAlignMapTool(m_canvas);
    m_alignTool->setParent(this);
    connect(m_alignTool, &KaAlignMapTool::statusChanged, this, [this](const QString& t) {
      statusBar()->showMessage(t, 8000);
    });
    connect(m_alignTool, &KaAlignMapTool::pairsChanged, this, &MainWindow::refreshAlignUi);
    connect(m_alignTool, &KaAlignMapTool::cursorMoved, this, [this](const QgsPointXY& pt) {
      m_alignCursorX = pt.x();
      m_alignCursorY = pt.y();
      m_alignCursorValid = true;
      updateAlignOverlay();
    });
  }
  stopCaptureTool();
  QString err;
  const QgsCoordinateReferenceSystem crs =
      QgsProject::instance() && QgsProject::instance()->crs().isValid()
          ? QgsProject::instance()->crs()
          : QgsCoordinateReferenceSystem(m_workCrs);
  if (!m_alignTool->beginLayer(layer, crs, &err)) {
    QMessageBox::warning(this, QStringLiteral("맞추기"), err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(m_alignTool->targetLayer());
  showAlignSplit();
  showSubToolsAlign();
  applySnapConfig();
  m_canvas->setMapTool(m_alignTool);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  refreshAlignUi();
  statusBar()->showMessage(m_alignTool->statusText(), 10000);
#else
  Q_UNUSED(layer);
#endif
}

void MainWindow::georefAssistant() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  if (!GeorefService::isAlignableLayer(layer)) {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("맞출 도면"), QString(),
        QStringLiteral("도면 (*.png *.jpg *.jpeg *.tif *.tiff *.dxf *.dwg)"));
    if (path.isEmpty()) return;
    const bool ok = GeorefService::isImagePath(path) ? addRasterFromPath(path)
                                                     : addVectorFromPath(path);
    if (!ok) {
      QMessageBox::warning(this, QStringLiteral("맞추기"),
                           QStringLiteral("파일을 열 수 없습니다.\nDWG면 DXF로 저장한 뒤 다시 시도하세요."));
      return;
    }
    layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
    if (!layer) {
      const auto layers = QgsProject::instance()->mapLayers();
      for (auto it = layers.constBegin(); it != layers.constEnd(); ++it) {
        if (GeorefService::isAlignableLayer(it.value())) layer = it.value();
      }
    }
  }
  if (!GeorefService::isAlignableLayer(layer)) {
    QMessageBox::information(this, QStringLiteral("맞추기"),
                             QStringLiteral("JPG·PNG·DXF 도면을 고르거나, 목록에서 도면을 선택한 뒤 다시 누르세요."));
    return;
  }
  startAlignSession(layer);
#else
  QMessageBox::warning(this, QStringLiteral("맞추기"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::openVectorLayer() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("SHP/벡터 추가"), QString(),
      QStringLiteral("Vector (*.shp *.gpkg *.geojson)"));
  if (path.isEmpty()) return;
  LayerOps::prepareShapefileEncoding(path);
  const QString title = QFileInfo(path).completeBaseName();
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    QMessageBox::warning(this, QStringLiteral("오류"),
                         QStringLiteral("열 수 없음: %1").arg(layer->error().message()));
    delete layer;
    return;
  }
  for (const QgsField& f : layer->fields()) {
    if (f.name().contains(QChar(0xFFFD))) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
      break;
    }
  }
  LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
  LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                   false);
  const QString nameField = LayerOps::detectNameField(layer);
  if (!nameField.isEmpty()) {
    LayerOps::applyNameAttributeLabels(layer, nameField, 5.0, false);
  }
  LayerOps::applyLegendCrsLabel(layer);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  QgsProject::instance()->addMapLayer(layer, true);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::zoomToLayerMax(m_canvas, layer);
  }
  statusBar()->showMessage(QStringLiteral("지도를 올렸습니다: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("벡터 로드 시뮬레이션"));
#endif
}

// 흙토람(soil.rda.go.kr) 토양도 신청으로 받은 SHP를 참조 지도로 불러온다.
// VWorld WMS 토양 레이어는 서버 오류가 잦고 분포지형이 없어 파일 방식을 쓴다.
void MainWindow::importSoilShapefile() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("토양도 SHP 불러오기"), QString(),
      QStringLiteral("Shapefile (*.shp)"));
  if (path.isEmpty()) return;

  // 필드 목록과 파일에 기록된 좌표계를 미리 읽어 선택지를 만든다.
  QStringList fieldNames;
  QString fileCrsAuth;
  {
    QgsVectorLayer probe(path, QStringLiteral("probe"), QStringLiteral("ogr"));
    if (!probe.isValid()) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("SHP를 열 수 없습니다: %1").arg(probe.error().message()));
      return;
    }
    const QgsFields flds = probe.fields();
    for (int i = 0; i < flds.count(); ++i) {
      const QgsField f = flds.at(i);
      const auto t = static_cast<QMetaType::Type>(f.type());
      if (t == QMetaType::QString || t == QMetaType::Int || t == QMetaType::LongLong)
        fieldNames.append(f.name());
    }
    if (probe.crs().isValid()) fileCrsAuth = probe.crs().authid();
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("토양도 불러오기 설정"));
  auto* form = new QFormLayout(&dlg);

  auto* crsBox = new QComboBox(&dlg);
  if (!fileCrsAuth.isEmpty())
    crsBox->addItem(QStringLiteral("파일에 기록된 좌표계 사용 — %1").arg(fileCrsAuth), QString());
  crsBox->addItem(QStringLiteral("EPSG:2097 — 중부원점(Bessel) · 흙토람 고시 좌표계"),
                  QStringLiteral("EPSG:2097"));
  crsBox->addItem(QStringLiteral("EPSG:5174 — 중부원점(Bessel, 10.405″ 보정)"),
                  QStringLiteral("EPSG:5174"));
  crsBox->addItem(QStringLiteral("EPSG:5186 — 중부원점(GRS80)"), QStringLiteral("EPSG:5186"));
  crsBox->addItem(QStringLiteral("EPSG:5187 — 동부원점(GRS80)"), QStringLiteral("EPSG:5187"));
  crsBox->addItem(QStringLiteral("EPSG:5179 — UTM-K"), QStringLiteral("EPSG:5179"));
  crsBox->addItem(QStringLiteral("EPSG:4326 — 경위도(WGS84)"), QStringLiteral("EPSG:4326"));
  crsBox->setCurrentIndex(0);
  form->addRow(QStringLiteral("좌표계"), crsBox);

  auto* fieldBox = new QComboBox(&dlg);
  fieldBox->addItem(QStringLiteral("(단색 — 구분 없음)"), QString());
  for (const QString& n : fieldNames)
    fieldBox->addItem(n, n);
  // 분포지형·토양부호 계열 필드가 있으면 미리 고른다.
  static const QRegularExpression kSoilFieldRx(
      QStringLiteral("(분포|지형|토양|tpgrp|topo|dist|soil|sltp|sym)"),
      QRegularExpression::CaseInsensitiveOption);
  for (int i = 1; i < fieldBox->count(); ++i) {
    if (fieldBox->itemText(i).contains(kSoilFieldRx)) {
      fieldBox->setCurrentIndex(i);
      break;
    }
  }
  form->addRow(QStringLiteral("색 구분 필드"), fieldBox);

  auto* note = new QLabel(
      QStringLiteral("흙토람 → 토양도 신청에서 무료로 받은 SHP를 그대로 불러옵니다.\n"
                     "지도가 엉뚱한 위치에 뜨면 좌표계를 EPSG:2097 ↔ 5174로 바꿔 다시 불러오세요.\n"
                     "불러온 뒤에는 작업 좌표계(%1)로 자동 재투영되어 지적·위성과 겹쳐 보입니다.")
          .arg(m_workCrs),
      &dlg);
  note->setWordWrap(true);
  form->addRow(note);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("불러오기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  form->addRow(buttons);
  if (dlg.exec() != QDialog::Accepted) return;

  QString err;
  QgsVectorLayer* layer = LayerOps::addSoilShapefile(
      QgsProject::instance(), m_canvas, path, crsBox->currentData().toString(),
      fieldBox->currentData().toString(), &err);
  if (!layer) {
    notify(Notice::Critical, QStringLiteral("토양도"),
           err.isEmpty() ? QStringLiteral("토양도를 불러오지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  const QString fieldTxt = fieldBox->currentData().toString().isEmpty()
                               ? QStringLiteral("단색")
                               : fieldBox->currentData().toString();
  const QString msg = QStringLiteral("%1 · %2 · 색 구분: %3 — 참조 지도 그룹")
                          .arg(layer->name(), layer->crs().authid(), fieldTxt);
  statusBar()->showMessage(QStringLiteral("토양도를 올렸습니다: %1").arg(msg), 8000);
  notify(Notice::Success, QStringLiteral("토양도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("토양도 로드 시뮬레이션"));
#endif
}

#if KA_HGIS_HAS_QGIS
namespace {
// QGIS 범례 체크 / ArcGIS 레이어 on-off: 이미 있으면 보이기↔숨기기만 한다.
bool toggleExistingOverlay(QgsProject* project, QgsMapCanvas* canvas, const QString& title,
                           QAction* act, QStatusBar* bar) {
  if (!project) return false;
  if (LayerOps::isLayerVisible(project, title)) {
    LayerOps::toggleLayerVisibility(project, canvas, title, false);
    if (act) act->setChecked(false);
    if (bar) bar->showMessage(title + QStringLiteral("를 껐습니다."), 4000);
    return true;
  }
  if (LayerOps::toggleLayerVisibility(project, canvas, title, true)) {
    if (act) act->setChecked(true);
    if (bar) bar->showMessage(title + QStringLiteral("를 다시 켰습니다."), 4000);
    return true;
  }
  return false;
}
}  // namespace
#endif

void MainWindow::toggleTerrainMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("지형맵"), nullptr,
                            statusBar())) {
    if (m_btnTerrain)
      m_btnTerrain->setChecked(
          LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("지형맵")));
    return;
  }
  QString err;
  if (!LayerOps::addElevationHillshadeMap(QgsProject::instance(), m_canvas,
                                          VworldSettings::loadApiKey(), &err)) {
    notify(Notice::Warning, QStringLiteral("지형맵"),
           err.isEmpty() ? QStringLiteral("지형맵을 올리지 못했습니다.") : err);
    if (m_btnTerrain)
      m_btnTerrain->setChecked(false);
    return;
  }
  if (!m_canvas->isDrawing())
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (m_btnTerrain)
    m_btnTerrain->setChecked(true);
  statusBar()->showMessage(QStringLiteral("지형맵을 올렸습니다. 다시 누르면 숨깁니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("지형맵 시뮬레이션"));
#endif
}

void MainWindow::editDemElevationClasses() {
#if KA_HGIS_HAS_QGIS
  QgsRasterLayer* dem = nullptr;
  if (QgsProject* proj = QgsProject::instance()) {
    const QList<QgsMapLayer*> found = proj->mapLayersByName(QStringLiteral("DEM"));
    for (QgsMapLayer* l : found) {
      auto* rl = qobject_cast<QgsRasterLayer*>(l);
      if (rl && rl->isValid() &&
          dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl->renderer())) {
        dem = rl;
        break;
      }
    }
  }
  if (!dem) {
    QMessageBox::information(
        this, QStringLiteral("DEM 높이 구간"),
        QStringLiteral("먼저 DEM을 켜 주세요. 높이(m) 범례가 있는 상세 고도일 때만 "
                       "칸 수·색·이름을 바꿀 수 있습니다."));
    return;
  }
  auto* dlg = new KaDemClassDialog(dem, this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 높이 구간"));
#endif
}

void MainWindow::toggleDemMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  QgsProject* proj = QgsProject::instance();
  QgsRasterLayer* dem = nullptr;
  for (QgsMapLayer* ml : proj->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM")) {
      dem = qobject_cast<QgsRasterLayer*>(ml);
      if (dem) break;
    }
  }

  if (dem) {
    const bool wasVisible = LayerOps::isLayerVisible(proj, QStringLiteral("DEM"));
    const bool makeVisible = !wasVisible;
    LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("DEM"), makeVisible);
    if (m_btnDem)
      m_btnDem->setChecked(makeVisible);

    if (makeVisible) {
      LayerOps::ensureDemRelief(proj, dem);
    }
    if (!LayerOps::isLayerVisible(proj, QStringLiteral("지질도"))) {
      LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("지형 음영"), makeVisible);
    }
    if (!m_canvas->isDrawing()) {
      LayerOps::syncMapCanvas(proj, m_canvas, false);
      m_canvas->refresh();
    }
    statusBar()->showMessage(
        makeVisible ? QStringLiteral("정밀 DEM 입체 지형을 켰습니다. 다시 누르면 숨깁니다.")
                    : QStringLiteral("DEM 지형을 숨겼습니다."),
        4000);
    return;
  }

  QString err;
  if (!LayerOps::addDemColorReliefMap(proj, m_canvas, &err)) {
    notify(Notice::Warning, QStringLiteral("DEM"),
           err.isEmpty() ? QStringLiteral("DEM을 올리지 못했습니다.") : err);
    if (m_btnDem)
      m_btnDem->setChecked(false);
    return;
  }
  if (!m_canvas->isDrawing()) {
    LayerOps::syncMapCanvas(proj, m_canvas, false);
    m_canvas->refresh();
  }
  if (m_btnDem)
    m_btnDem->setChecked(true);
  statusBar()->showMessage(
      QStringLiteral("정밀 DEM 입체 지형을 올렸습니다. 범례에 높이(m)가 표시됩니다. 다시 누르면 숨깁니다."),
      6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 시뮬레이션"));
#endif
}

void MainWindow::startPaleoLandform() {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("고지형"),
                             QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요.\n"
                                            "그다음 조사지역으로 확대한 뒤 다시 「고지형」을 누르면 "
                                            "흙토람 분포지형이 깔립니다."));
    return;
  }
  if (!m_canvas) return;
  QgsProject* proj = QgsProject::instance();
  LayerOps::clampCanvasToThematicScale(m_canvas);

  QgsVectorLayer* soil = PaleoLandformService::findSoilTerrainLayer(proj);
  if (!soil) {
    QgsRectangle ext = m_canvas->extent();
    const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
    const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
    if (canvasCrs.isValid() && canvasCrs != crs5186) {
      try {
        const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
        ext = tr.transformBoundingBox(ext);
      } catch (const QgsException&) {
        QMessageBox::warning(this, QStringLiteral("고지형"),
                             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
        return;
      }
    }
    if (ext.width() > SoilMapService::maxSpanMeters() ||
        ext.height() > SoilMapService::maxSpanMeters()) {
      QMessageBox::information(
          this, QStringLiteral("고지형"),
          QStringLiteral("지금 화면이 너무 넓습니다. 조사지역(한 변 %1km 이하)으로 확대한 뒤 "
                         "다시 「고지형」을 누르세요.\n"
                         "전국·시도 화면에는 분포지형을 깔지 않습니다.")
              .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
      return;
    }
    const QString dir = QFileInfo(m_surveyPath).absolutePath();
    const QString outGpkg = QDir(dir).filePath(QStringLiteral("토양도_흙토람.gpkg"));
    statusBar()->showMessage(QStringLiteral("흙토람에서 분포지형을 받는 중…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString soilErr;
    soil = SoilMapService::downloadAndAdd(proj, m_canvas, ext, outGpkg, &soilErr);
    QApplication::restoreOverrideCursor();
    if (!soil) {
      QMessageBox::warning(this, QStringLiteral("고지형"),
                           soilErr.isEmpty() ? QStringLiteral("분포지형을 받지 못했습니다.")
                                             : soilErr);
      return;
    }
    if (m_btnSoil) m_btnSoil->setChecked(true);
  }
  PaleoLandformService::applyCandidateEmphasis(soil);
  if (QgsLayerTreeLayer* n = proj->layerTreeRoot()->findLayer(soil->id()))
    n->setItemVisibilityChecked(true);
  if (m_layerTree) m_layerTree->setCurrentLayer(soil);

  QString err;
  QgsVectorLayer* layer =
      PaleoLandformService::ensureInterpretationLayer(proj, m_surveyPath, &err);
  if (!layer) {
    notify(Notice::Warning, QStringLiteral("고지형"),
           err.isEmpty() ? QStringLiteral("판독 레이어를 만들지 못했습니다.") : err);
    return;
  }
  const PaleoLandformService::SeedResult seeded =
      PaleoLandformService::seedInterpretationFromSoil(soil, layer, &err);
  beginEdit(layer);
  if (seeded.added > 0) {
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("흙토람 분포지형에서 가설 %1개를 자동으로 깔았습니다. 확정이 아닙니다.")
               .arg(seeded.added));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("흙토람 분포지형에서 가설 면을 자동으로 깔았습니다.\n\n"
                       "선상지 · 해성평탄 · 하안단구는 토양 구분을 옮긴 것입니다.\n"
                       "하성평탄은 안쪽을 구하도, 가장자리를 자연제방 가설로 나눕니다.\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "틀린 면은 지우고 고치세요. 확정이 아닙니다."));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 %1개 — 확정 아님. 좌클릭으로 추가, 우클릭으로 완료")
            .arg(seeded.added),
        0);
  } else {
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("분포지형을 올렸습니다. 진한 색이 입지 후보"
                          "(곡간·선상·해성·하성평탄·홍적대지)입니다."));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("지도에 색 면이 생겼으면 그게 고지형입니다.\n\n"
                       "진한 색 = 유적 입지 후보 (곡간·선상·해성평탄·하성평탄·홍적대지)\n"
                       "연한 색 = 산지·구릉 등\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "이제 그 위에 구하도·자연제방 가설을 그리세요. 확정이 아닙니다."));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 — 진한 색이 입지 후보. 좌클릭으로 그리고 우클릭으로 완료"), 0);
  }
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("고지형 시뮬레이션"));
#endif
}

// 흙토람 공개 지도서버(국립농업과학원 GeoServer)에서 현재 화면 범위의
// 정밀토양도를 내려받아 분포지형 공식 색으로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadSoilTerrain() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  // 본체 클릭만 토글. 화살표 메뉴의 「내려받기」는 다시 받는다.
  if (!qobject_cast<QAction*>(sender())) {
    if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("토양도(흙토람)"),
                              nullptr, statusBar())) {
      if (m_btnSoil)
        m_btnSoil->setChecked(
            LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("토양도(흙토람)")));
      return;
    }
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 토양도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > SoilMapService::maxSpanMeters() ||
      ext.height() > SoilMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("토양도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  // 조사 GPKG 옆에 저장해 다음에도(오프라인 포함) 다시 쓸 수 있게 한다.
  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("토양도_흙토람.gpkg"));

  statusBar()->showMessage(QStringLiteral("흙토람 서버에서 토양도를 내려받는 중…"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QElapsedTimer dlTimer;
  dlTimer.start();
  QString err;
  QgsVectorLayer* layer = SoilMapService::downloadAndAdd(QgsProject::instance(), m_canvas,
                                                         ext, outGpkg, &err);
  QApplication::restoreOverrideCursor();
  KaCrashGuard::logLine(QStringLiteral("[download] 토양도 %1 ms ok=%2 err=%3")
                            .arg(dlTimer.elapsed())
                            .arg(layer != nullptr)
                            .arg(err));
  if (!layer) {
    statusBar()->clearMessage();
    notify(Notice::Critical, QStringLiteral("토양도"),
           err.isEmpty() ? QStringLiteral("토양도를 내려받지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  const QString msg =
      QStringLiteral("분포지형 폴리곤 %1개 — 흙토람 공식 범례색 · %2에 저장")
          .arg(layer->featureCount())
          .arg(QDir::toNativeSeparators(outGpkg));
  if (m_btnSoil) m_btnSoil->setChecked(true);
  statusBar()->showMessage(msg, 10000);
  notify(Notice::Success, QStringLiteral("토양도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("토양도 내려받기 시뮬레이션"));
#endif
}

// KIGAM 공개 지도서버에서 현재 화면 범위의 1:5만 지질도(암상)를 내려받아
// 지질시대별 ICS 표준색 + 암상 기호 라벨로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadGeologyMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas,
                            QStringLiteral("지질도(KIGAM 1:5만)"), m_actGeology, statusBar())) {
    QgsProject* proj = QgsProject::instance();
    const bool on = LayerOps::isLayerVisible(proj, QStringLiteral("지질도(KIGAM 1:5만)"));
    if (on) {
      if (QgsMapLayer* geo = GeologyMapService::existingGeologyLayer(proj))
        GeologyMapService::ensureReliefUnderlay(proj, m_canvas, geo, nullptr);
      if (m_canvas && !m_canvas->isDrawing())
        LayerOps::syncMapCanvas(proj, m_canvas, false);
    }
    LayerOps::toggleLayerVisibility(proj, m_canvas,
                                    GeologyMapService::reliefLayerTitle(), on);
    return;
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 지질도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("지질도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > GeologyMapService::maxSpanMeters() ||
      ext.height() > GeologyMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("지질도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(GeologyMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("지질도_KIGAM.gpkg"));

  statusBar()->showMessage(QStringLiteral("KIGAM 서버에서 지질도를 내려받는 중…"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QElapsedTimer dlTimer;
  dlTimer.start();
  QString err;
  QgsMapLayer* layer = GeologyMapService::downloadAndAdd(QgsProject::instance(), m_canvas,
                                                         ext, outGpkg, &err);
  QApplication::restoreOverrideCursor();
  KaCrashGuard::logLine(QStringLiteral("[download] 지질도 %1 ms ok=%2 err=%3")
                            .arg(dlTimer.elapsed())
                            .arg(layer != nullptr)
                            .arg(err));
  if (!layer) {
    statusBar()->clearMessage();
    notify(Notice::Critical, QStringLiteral("지질도"),
           err.isEmpty() ? QStringLiteral("지질도를 내려받지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  QString msg;
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
    msg = QStringLiteral("암상 폴리곤 %1개 — 지질 색 위에 지형 음영 · %2에 저장")
              .arg(vl->featureCount())
              .arg(QDir::toNativeSeparators(outGpkg));
  } else {
    msg = QStringLiteral("공식 5만 지질도 도폭 위에 지형 음영을 겹쳤습니다.");
  }
  if (m_actGeology) m_actGeology->setChecked(true);
  statusBar()->showMessage(msg, 10000);
  notify(Notice::Success, QStringLiteral("지질도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("지질도 내려받기 시뮬레이션"));
#endif
}

// VWorld 공개 WFS에서 현재 화면 범위의 하천망(국가·지방하천)을 내려받아
// 등급별 물색 + 하천명 라벨로 겹친다. 배경지도와 같은 VWorld 키를 쓴다.
void MainWindow::downloadRiverMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("수계도(하천망)"),
                            m_actRiver, statusBar()))
    return;
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 수계도를 받습니다."), 4000);

  const QString key = VworldSettings::loadApiKey();
  if (key.trimmed().isEmpty()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("VWorld 인증키가 없습니다. 배경지도 설정에서 키를 먼저 "
                          "등록하세요."));
    return;
  }

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("수계도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > RiverMapService::maxSpanMeters() ||
      ext.height() > RiverMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(RiverMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("수계도_VWorld.gpkg"));

  statusBar()->showMessage(QStringLiteral("VWorld 서버에서 하천망을 내려받는 중…"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QElapsedTimer dlTimer;
  dlTimer.start();
  QString err;
  QgsVectorLayer* layer = RiverMapService::downloadAndAdd(QgsProject::instance(), m_canvas,
                                                          ext, key, outGpkg, &err);
  QApplication::restoreOverrideCursor();
  KaCrashGuard::logLine(QStringLiteral("[download] 수계도 %1 ms ok=%2 err=%3")
                            .arg(dlTimer.elapsed())
                            .arg(layer != nullptr)
                            .arg(err));
  if (!layer) {
    statusBar()->clearMessage();
    notify(Notice::Critical, QStringLiteral("수계도"),
           err.isEmpty() ? QStringLiteral("수계도를 내려받지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  const QString msg =
      QStringLiteral("하천 구역 %1개 — 등급별 물색·하천명 라벨 · %2에 저장")
          .arg(layer->featureCount())
          .arg(QDir::toNativeSeparators(outGpkg));
  if (m_actRiver) m_actRiver->setChecked(true);
  statusBar()->showMessage(msg, 10000);
  notify(Notice::Success, QStringLiteral("수계도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("수계도 내려받기 시뮬레이션"));
#endif
}

void MainWindow::persistSurveyWork() {
#if KA_HGIS_HAS_QGIS
  int n = 0;
  if (QgsProject* proj = QgsProject::instance()) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      auto* v = qobject_cast<QgsVectorLayer*>(l);
      if (!v || !v->isValid() || !v->isEditable() || !v->isModified())
        continue;
      if (!v->commitChanges(false))
        continue;
      ++n;
      v->startEditing();
    }
  }
  if (!m_surveyPath.isEmpty())
    rememberSurvey(m_surveyPath, QFileInfo(m_surveyPath).completeBaseName());
  if (n > 0 && statusBar())
    statusBar()->showMessage(QStringLiteral("자동 저장했습니다."), 2500);
#endif
}

void MainWindow::restoreLastSurvey() {
  if (!m_surveyPath.isEmpty())
    return;
  QSettings st = RecentSurveys::userSettings();
  const QString last = RecentSurveys::lastPath(st);
  if (last.isEmpty() || !QFile::exists(last))
    return;
  openRecentSurvey(last);
}

void MainWindow::saveProject() {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    saveProjectAs();
    return;
  }
  persistSurveyWork();
  // 동반되는 QGIS 프로젝트(.qgz) 파일도 함께 저장하여 외부 SHP 레이어, 스타일, 라벨(5pt) 등 전체 작업환경을 보존한다.
  QFileInfo sfi(m_surveyPath);
  const QString qgzPath = sfi.dir().filePath(sfi.completeBaseName() + QStringLiteral(".qgz"));
  QgsProject::instance()->setFileName(qgzPath);
  QgsProject::instance()->write();
  statusBar()->showMessage(QStringLiteral("저장했습니다: %1").arg(sfi.fileName()), 4000);
  notify(Notice::Success, QStringLiteral("저장"),
         QStringLiteral("현재 조사와 작업 중인 모든 레이어를 저장했습니다:\n%1").arg(QDir::toNativeSeparators(m_surveyPath)));
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 저장 시뮬레이션"));
#endif
}

void MainWindow::saveProjectAs() {
#if KA_HGIS_HAS_QGIS
  persistSurveyWork();

  QString defaultPath;
  if (!m_surveyPath.isEmpty()) {
    QFileInfo fi(m_surveyPath);
    defaultPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral("_복사본.gpkg"));
  } else {
    defaultPath = QDir(resolvedDesktopPath()).filePath(QStringLiteral("새조사.gpkg"));
  }

  const QString selected = QFileDialog::getSaveFileName(
      this, QStringLiteral("다른 이름으로 저장"), defaultPath,
      QStringLiteral("고고학 조사 파일 (*.gpkg *.qgz);;GeoPackage (*.gpkg);;QGIS 프로젝트 (*.qgz)"));
  if (selected.isEmpty()) return;

  QFileInfo newFi(selected);
  QString targetGpkg = newFi.suffix().toLower() == QLatin1String("qgz")
      ? newFi.dir().filePath(newFi.completeBaseName() + QStringLiteral(".gpkg"))
      : selected;
  if (!targetGpkg.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
    targetGpkg += QStringLiteral(".gpkg");

  const QString targetQgz = newFi.dir().filePath(newFi.completeBaseName() + QStringLiteral(".qgz"));

  // 1. 기존 조사가 있으면 새 GPKG로 복사하여 조사 데이터(유구, 구역 등) 보존
  if (!m_surveyPath.isEmpty() && QFile::exists(m_surveyPath) && m_surveyPath != targetGpkg) {
    if (QFile::exists(targetGpkg))
      QFile::remove(targetGpkg);
    if (!QFile::copy(m_surveyPath, targetGpkg)) {
      QMessageBox::warning(this, QStringLiteral("저장 실패"),
                           QStringLiteral("파일을 생성할 수 없습니다:\n%1").arg(targetGpkg));
      return;
    }
    // 도면 레이어들의 데이터소스를 새 GPKG 경로로 갱신 (외부 SHP 레이어는 그대로 유지)
    const QStringList domainKeys = LayerOps::domainLayerKeys();
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      auto* vl = qobject_cast<QgsVectorLayer*>(l);
      if (!vl) continue;
      const QString key = LayerOps::layerKeyOf(vl);
      if (domainKeys.contains(key)) {
        vl->setDataSource(QStringLiteral("%1|layername=%2").arg(targetGpkg, key), vl->name(), QStringLiteral("ogr"));
      }
    }
  } else if (m_surveyPath.isEmpty() || !QFile::exists(m_surveyPath)) {
    QString err;
    const QString created = SurveyProjectFactory::createNewSurvey(newFi.dir().absolutePath(),
                                                                  newFi.completeBaseName(),
                                                                  &err, m_workCrs);
    if (created.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("저장 실패"), err);
      return;
    }
    targetGpkg = created;
  }

  m_surveyPath = targetGpkg;

  // 2. QGIS 프로젝트(.qgz) 저장 - 외부에서 넣은 SHP 레이어, 지적/위성 배경, 5pt 라벨 등 모든 레이어 상태가 완벽히 보존됨
  QgsProject::instance()->setFileName(targetQgz);
  if (!QgsProject::instance()->write()) {
    QMessageBox::warning(this, QStringLiteral("경고"), QStringLiteral("프로젝트 파일(.qgz) 저장에 실패했습니다."));
  }

  // 3. 윈도우 타이틀 및 최근 조사 갱신
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(newFi.completeBaseName()));
  rememberSurvey(targetGpkg, newFi.completeBaseName());

  const QString msg = QStringLiteral("작업 중이던 모든 레이어를 유지한 채 새 파일로 저장했습니다:\n%1").arg(QDir::toNativeSeparators(targetGpkg));
  statusBar()->showMessage(QStringLiteral("다른 이름으로 저장했습니다: %1").arg(newFi.fileName()), 5000);
  notify(Notice::Success, QStringLiteral("다른 이름으로 저장"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("다른 이름으로 저장 시뮬레이션"));
#endif
}

void MainWindow::openProject() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("열기"), QString(),
      QStringLiteral("조사 (*.gpkg *.qgz *.qgs);;GeoPackage (*.gpkg);;QGIS (*.qgz *.qgs)"));
  if (path.isEmpty()) return;
  if (QFileInfo(path).suffix().compare(QLatin1String("gpkg"), Qt::CaseInsensitive) == 0) {
    if (openSurveyGpkg(path))
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
    return;
  }
  if (!QgsProject::instance()->read(path)) {
    QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("프로젝트를 열 수 없습니다."));
    return;
  }
  for (QgsMapLayer* ml : QgsProject::instance()->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;
    const QString src = vl->source();
    if (src.contains(QLatin1String(".gpkg"), Qt::CaseInsensitive)) {
      const QString gpkg = src.split(QLatin1Char('|')).first();
      if (QFile::exists(gpkg)) {
        m_surveyPath = gpkg;
        break;
      }
    }
  }
  if (auto* cp = layerByKey(QStringLiteral("control_points")))
    LayerOps::ensureControlPointQualityFields(cp);
  if (auto* fp = layerByKey(QStringLiteral("feature_poly")))
    LayerOps::applyFeaturePolyStyle(fp);
  for (QgsMapLayer* ml : QgsProject::instance()->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM") && ml->isValid()) {
      if (auto* rl = qobject_cast<QgsRasterLayer*>(ml)) {
        if (LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("DEM"))) {
          LayerOps::ensureDemRelief(QgsProject::instance(), rl);
        }
      }
      break;
    }
  }
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, true);
  if (m_canvas) m_canvas->refresh();
  ensureDefaultBasemaps();
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
  rememberSurvey(path, QFileInfo(path).completeBaseName());
  showMapWorkspace();
  updateNextActionStatus();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 열기 시뮬레이션"));
#endif
}
void MainWindow::searchLocation(const QString& query) {
  if (!m_locator) return;
  const QString q = query.trimmed();
  if (q.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("주소·지번·지역·상호를 입력하세요"), 4000);
    return;
  }
  // 검색 칸이 없어졌으므로 중복 실행은 플래그로 막는다.
  if (m_locationSearchBusy) return;
  m_locationSearchBusy = true;
  statusBar()->showMessage(QStringLiteral("위치 검색 중… %1").arg(q), 0);
  m_locator->search(q);
}

void MainWindow::onLocationFailed(const QString& message) {
  m_locationSearchBusy = false;
  statusBar()->showMessage(message, 8000);
  QMessageBox::information(this, QStringLiteral("위치 검색"), message);
}

void MainWindow::onLocationResults(const QVector<LocationHit>& hits) {
  m_locationSearchBusy = false;
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

// 찾은 자리를 십자 표식으로 남긴다. 다음 검색이 오면 그 자리로 옮긴다.
// 조사 도메인이 아니라 화면 표시일 뿐이라 레이어로 만들지 않는다.
void MainWindow::markFoundLocation(const QgsPointXY& mapPt, const QString& title) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (!m_locationMark)
    m_locationMark = new KaFoundLocationMark(m_canvas);
  m_locationMark->setLocation(mapPt, title);
  m_locationMark->show();
  m_locationMarkTitle = title;
#else
  Q_UNUSED(mapPt);
  Q_UNUSED(title);
#endif
}

void MainWindow::clearFoundLocationMark() {
#if KA_HGIS_HAS_QGIS
  if (m_locationMark) {
    delete m_locationMark;
    m_locationMark = nullptr;
  }
  m_locationMarkTitle.clear();
#endif
}

void MainWindow::zoomToLocation(const LocationHit& hit) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  double lon = hit.lon;
  double lat = hit.lat;
  if (qAbs(lon) <= 90.0 && qAbs(lat) > 90.0)
    std::swap(lon, lat);
  if (lon < 120.0 || lon > 135.0 || lat < 30.0 || lat > 45.0) {
    statusBar()->showMessage(
        QStringLiteral("위치 좌표가 한국 범위 밖입니다 (lon=%1 lat=%2)").arg(lon).arg(lat), 8000);
  }

  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest =
      m_workCrs.isEmpty() ? QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"))
                          : QgsCoordinateReferenceSystem(m_workCrs);
  try {
    QgsCoordinateTransform xf(wgs, dest, QgsProject::instance()
                                             ? QgsProject::instance()->transformContext()
                                             : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, dest.authid());

    const QgsPointXY p = xf.transform(QgsPointXY(lon, lat));
    if (hit.hasBbox && hit.west != 0.0 && hit.east != 0.0) {
      QgsRectangle r(hit.west, hit.south, hit.east, hit.north);
      r = xf.transformBoundingBox(r);
      r.scale(1.2);
      m_canvas->setExtent(r);
    } else {
      const double pad = dest.authid().contains(QLatin1String("4326")) ? 0.004 : 400.0;
      m_canvas->setExtent(QgsRectangle(p.x() - pad, p.y() - pad, p.x() + pad, p.y() + pad));
    }
    if (m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(3000.0, true);
    LayerOps::clampCanvasToKorea(m_canvas);
    markFoundLocation(p, hit.title);
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::refreshCanvasIfIdle(m_canvas);
    statusBar()->showMessage(
        QStringLiteral("이동: %1  (lon %2, lat %3 → %4)")
            .arg(hit.title)
            .arg(lon, 0, 'f', 5)
            .arg(lat, 0, 'f', 5)
            .arg(dest.authid()),
        10000);
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
  if (!key.trimmed().isEmpty()) {
    ensureDefaultBasemaps();
    if (m_canvas)
      LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  }
  statusBar()->showMessage(key.isEmpty()
      ? QStringLiteral("VWorld 키 삭제됨")
      : QStringLiteral("VWorld API 키 저장됨 — 위성과 지적을 올립니다"), 6000);
}

void MainWindow::setupFileBrowser() {
  auto* view = new FileListView(this);
  m_fileBrowser = view;
  m_fileBrowser->setObjectName(QStringLiteral("fileBrowser"));
  connect(m_fileBrowser, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileBrowserActivated);
  goFileBrowserRoot(QString());
}

QString MainWindow::resolvedDesktopPath() {
  static QString cached;
  if (!cached.isEmpty() && QFileInfo(cached).isDir())
    return cached;
  const QStringList candidates = {
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
      QDir::homePath() + QStringLiteral("/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/바탕 화면"),
  };
  for (const QString& c : candidates) {
    if (c.isEmpty()) continue;
    const QFileInfo fi(c);
    if (fi.exists() && fi.isDir()) {
      cached = QDir::cleanPath(fi.absoluteFilePath());
      return cached;
    }
  }
  cached = QDir::homePath();
  return cached;
}

void MainWindow::goFileBrowserRoot(const QString& path) {
  if (!m_fileBrowser) return;
  m_fileBrowser->clear();

  QString p = QDir::fromNativeSeparators(path.trimmed());
  if (p.length() == 2 && p[1] == QLatin1Char(':'))
    p += QLatin1Char('/');

  auto addRow = [this](const QString& label, const QString& full, bool isDir) {
    auto* it = new QListWidgetItem(label);
    it->setData(Qt::UserRole, full);
    it->setData(Qt::UserRole + 1, isDir);
    it->setToolTip(QDir::toNativeSeparators(full));
    m_fileBrowser->addItem(it);
  };

  if (p.isEmpty()) {
    m_browserPath.clear();
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo& d : drives)
      addRow(QDir::toNativeSeparators(d.absoluteFilePath()),
             QDir::fromNativeSeparators(d.absoluteFilePath()), true);
    statusBar()->showMessage(QStringLiteral("드라이브 목록 — 폴더를 더블클릭하세요"), 5000);
    return;
  }

  p = QDir::cleanPath(p);
  const QFileInfo fi(p);
  if (!fi.exists() || !fi.isDir()) {
    statusBar()->showMessage(QStringLiteral("폴더 없음 → 드라이브 목록"), 5000);
    goFileBrowserRoot(QString());
    return;
  }
  m_browserPath = QDir::cleanPath(fi.absoluteFilePath());

  QDir dir(m_browserPath);
  dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  dir.setSorting(QDir::Name | QDir::IgnoreCase);
  const QStringList folders = dir.entryList();
  int n = 0;
  for (const QString& name : folders) {
    if (n >= 250) break;
    if (name.compare(QLatin1String("$Recycle.Bin"), Qt::CaseInsensitive) == 0 ||
        name.compare(QLatin1String("System Volume Information"), Qt::CaseInsensitive) == 0)
      continue;
    addRow(QStringLiteral("[폴더] ") + name, dir.absoluteFilePath(name), true);
    ++n;
  }
  dir.setFilter(QDir::Files | QDir::NoSymLinks);
  dir.setNameFilters({QStringLiteral("*.shp"), QStringLiteral("*.dxf"), QStringLiteral("*.dwg"),
                      QStringLiteral("*.gpkg"), QStringLiteral("*.geojson"), QStringLiteral("*.json"),
                      QStringLiteral("*.tif"), QStringLiteral("*.tiff"), QStringLiteral("*.gtiff"),
                      QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")});
  const QStringList files = dir.entryList();
  for (const QString& name : files) {
    if (n >= 400) break;
    addRow(name, dir.absoluteFilePath(name), false);
    ++n;
  }
  statusBar()->showMessage(
      QStringLiteral("경로: %1").arg(QDir::toNativeSeparators(m_browserPath)), 6000);
}

void MainWindow::browseDataFolder() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("조사 데이터 폴더 선택"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  if (!dir.isEmpty())
    goFileBrowserRoot(dir);
}

void MainWindow::onFileBrowserActivated(QListWidgetItem* item) {
  if (!item) return;
  const QString path = item->data(Qt::UserRole).toString();
  const bool isDir = item->data(Qt::UserRole + 1).toBool();
  if (path.isEmpty()) return;
  if (isDir) {
    goFileBrowserRoot(path);
    return;
  }
  const QString low = path.toLower();
  const bool raster = GeorefService::isImagePath(path);
  if (raster ? !addRasterFromPath(path) : !addVectorFromPath(path)) {
    QMessageBox::warning(this, QStringLiteral("파일"),
                         QStringLiteral("지도 레이어로 열 수 없습니다 (SHP/DXF/DWG/GPKG/GeoTIFF/JPG):\n%1")
                             .arg(QDir::toNativeSeparators(path)));
  } else {
    if (m_layersCard && !m_layersCard->isVisible())
      m_layersCard->setVisible(true);
  }
}

QStringList MainWindow::selectedBrowserFiles() const {
  QStringList out;
  if (!m_fileBrowser) return out;
  const auto items = m_fileBrowser->selectedItems();
  for (QListWidgetItem* it : items) {
    if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
    const QString p = it->data(Qt::UserRole).toString();
    if (!p.isEmpty()) out.append(p);
  }
  return out;
}

bool MainWindow::tryAddDroppedUrls(const QList<QUrl>& urls) {
  QStringList paths;
  for (const QUrl& u : urls) {
    if (u.isLocalFile()) paths.append(u.toLocalFile());
  }
  return tryAddDroppedPaths(paths);
}

bool MainWindow::tryAddDroppedPaths(const QStringList& paths) {
  int n = 0;
  for (const QString& path : paths) {
    const QString low = path.toLower();
    if (!(low.endsWith(QLatin1String(".shp")) || low.endsWith(QLatin1String(".dxf")) ||
          low.endsWith(QLatin1String(".dwg")) || low.endsWith(QLatin1String(".gpkg")) ||
          low.endsWith(QLatin1String(".geojson")) || low.endsWith(QLatin1String(".json")) ||
          GeorefService::isImagePath(path)))
      continue;
    const bool raster = GeorefService::isImagePath(path);
    if (raster ? addRasterFromPath(path) : addVectorFromPath(path))
      ++n;
  }
  if (n > 0) {
    if (m_layersCard && !m_layersCard->isVisible())
      m_layersCard->setVisible(true);
    statusBar()->showMessage(QStringLiteral("레이어 %1개 추가됨 (파일→지도)").arg(n), 5000);
  }
  return n > 0;
}

bool MainWindow::addSectionGeoTiffFromPath(const QString& path, const QString& crsAuthId) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl || !rl->isValid()) {
    delete rl;
    return false;
  }
  const QString crsLabel = crsAuthId.isEmpty()
      ? QStringLiteral("EPSG:5187") : crsAuthId;
  rl->setCustomProperty(QStringLiteral("ka_hgis/section_raster"), true);
  rl->setCustomProperty(QStringLiteral("ka_hgis/section_crs_label"), crsLabel);
  // 범례·캔버스에 넣지 않음. 단면도 탭 전용 (위성·지적과 섞지 않음).
  QgsProject::instance()->addMapLayer(rl, false);
  statusBar()->showMessage(
      QStringLiteral("단면 GeoTIFF 추가: %1 · %2").arg(title, crsLabel), 8000);
  return true;
#else
  Q_UNUSED(path);
  Q_UNUSED(crsAuthId);
  return false;
#endif
}

bool MainWindow::addRasterFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl || !rl->isValid()) {
    delete rl;
    return false;
  }
  const bool unrefImage = GeorefService::isImagePath(path)
                          && GeorefService::looksUnreferencedRaster(rl);
  if (unrefImage)
    LayerOps::setAlignPending(rl, true);
  else if (!rl->crs().isValid() && QgsProject::instance() && QgsProject::instance()->crs().isValid()) {
    rl->setCrs(QgsProject::instance()->crs());
    statusBar()->showMessage(
        QStringLiteral("GeoTIFF에 좌표계가 없어 작업 좌표계(%1)로 올렸습니다.").arg(m_workCrs), 8000);
  }
  const bool geotiff = path.toLower().endsWith(QLatin1String(".tif"))
                       || path.toLower().endsWith(QLatin1String(".tiff"))
                       || path.toLower().endsWith(QLatin1String(".gtiff"));
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  QgsProject::instance()->addMapLayer(rl, true);
  if (geotiff)
    LayerOps::knockOutRasterPaper(rl);
  if (m_layerTree) m_layerTree->setCurrentLayer(rl);
  if (unrefImage) {
    if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(rl->id()))
      n->setItemVisibilityChecked(false);
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  refreshLayerEmptyState();
  if (m_canvas)
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (unrefImage) {
    statusBar()->showMessage(
        QStringLiteral("좌표 없는 그림입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    return true;
  }
  if (m_canvas && geotiff && layerSitsOnWorkMap(rl, m_workCrs)) {
    if (!LayerOps::zoomToLayerMax(m_canvas, rl))
      statusBar()->showMessage(QStringLiteral("그림은 올렸지만 범위를 잡지 못했습니다."), 8000);
  }
  const QString crs = rl->crs().isValid() ? rl->crs().authid() : m_workCrs;
  statusBar()->showMessage(
      QStringLiteral("그림 추가: %1 · %2 · 화면 %3").arg(title, crs, m_workCrs), 10000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

bool MainWindow::addVectorFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  LayerOps::prepareShapefileEncoding(path);
  const QString baseTitle = QFileInfo(path).completeBaseName();
  QList<QgsVectorLayer*> added;
  const QList<QgsProviderSublayerDetails> subs =
      QgsProviderRegistry::instance()->querySublayers(path);
  auto takeLayer = [&](QgsVectorLayer* layer, const QString& title) {
    if (!layer || !layer->isValid()) {
      delete layer;
      return;
    }
    // 한글 필드명 깨짐(\uFFFD) 자동 감지 및 CP949 복구
    bool hasGarbled = false;
    for (const QgsField& f : layer->fields()) {
      if (f.name().contains(QChar(0xFFFD))) {
        hasGarbled = true;
        break;
      }
    }
    if (hasGarbled) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
    }
    layer->setName(title);
    if (GeorefService::isCadPath(path))
      LayerOps::markReferenceLayer(layer);
    else
      LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
    LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                     false);
    // SHP 등 벡터 레이어 추가 시 명칭 속성 5PT 자동 라벨링 적용
    const QString nameField = LayerOps::detectNameField(layer);
    if (!nameField.isEmpty()) {
      LayerOps::applyNameAttributeLabels(layer, nameField, 5.0, false);
    }
    LayerOps::applyLegendCrsLabel(layer);
    QgsProject::instance()->addMapLayer(layer, true);
    added.append(layer);
  };
  if (path.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)) {
    bool isSurveyGpkg = false;
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.name() == QLatin1String("survey_area") || d.name() == QLatin1String("feature_poly") ||
          d.name() == QLatin1String("trial_trench") || d.name() == QLatin1String("control_points")) {
        isSurveyGpkg = true;
        break;
      }
    }
    if (isSurveyGpkg) {
      return openSurveyGpkg(path);
    }
  }
  if (!subs.isEmpty()) {
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.type() != Qgis::LayerType::Vector) continue;
      QgsProviderSublayerDetails::LayerOptions opt(QgsProject::instance()->transformContext());
      auto* ml = d.toLayer(opt);
      auto* vl = qobject_cast<QgsVectorLayer*>(ml);
      // GPKG 등 멀티 테이블 파일에서 피처가 0개인 빈 테이블은 레전드를 어지럽히지 않도록 추가하지 않는다.
      if (subs.size() > 1 && vl && vl->isValid() && vl->featureCount() == 0) {
        delete vl;
        continue;
      }
      takeLayer(vl, d.name().isEmpty() ? baseTitle : d.name());
    }
  } else {
    takeLayer(new QgsVectorLayer(path, baseTitle, QStringLiteral("ogr")), baseTitle);
  }
  if (added.isEmpty()) {
    const QString low = path.toLower();
    if (low.endsWith(QLatin1String(".dwg")) || low.endsWith(QLatin1String(".dxf"))) {
      QMessageBox::warning(this, QStringLiteral("파일"),
                           QStringLiteral("이 CAD 파일을 열 수 없습니다.\n"
                                          "DXF는 보통 열리고, DWG는 버전/드라이버에 따라 안 열릴 수 있습니다.\n"
                                          "AutoCAD에서 DXF로 저장한 뒤 다시 끌어 넣으세요.\n%1")
                               .arg(QDir::toNativeSeparators(path)));
    }
    return false;
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_layerTree && !added.isEmpty()) m_layerTree->setCurrentLayer(added.first());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    QgsVectorLayer* zoomLayer = added.first();
    for (QgsVectorLayer* vl : added) {
      if (vl && vl->featureCount() > 0) {
        zoomLayer = vl;
        break;
      }
    }
    if (!layerSitsOnWorkMap(zoomLayer, m_workCrs)) {
      statusBar()->showMessage(
          QStringLiteral("좌표가 없는 도면입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    } else if (!LayerOps::zoomToLayerMax(m_canvas, zoomLayer)) {
      statusBar()->showMessage(
          QStringLiteral("레이어는 추가됐지만 도형이 없습니다: %1").arg(zoomLayer->name()), 8000);
    }
  }
  qint64 feats = 0;
  for (QgsVectorLayer* vl : added)
    feats += vl ? vl->featureCount() : 0;
  statusBar()->showMessage(
      QStringLiteral("레이어 %1개 추가 · 도형 %2개 · 화면 %3")
          .arg(added.size())
          .arg(feats)
          .arg(m_workCrs),
      10000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

void MainWindow::exportReportLayout() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
#else
  QMessageBox::warning(this, QStringLiteral("도면"), QStringLiteral("QGIS 빌드 필요"));
#endif
}


void MainWindow::showAbout() {
  QMessageBox::about(
      this, QStringLiteral("정보"),
      QStringLiteral(
          "필드고고학GIS  버전 1\n"
          "동국문화재연구원\n"
          "향후 업데이트 진행\n"
          "\n"
          "QGIS를 포크하지 않고 qgis_core / qgis_gui를 링크합니다.\n"
          "작업 CRS: EPSG:5186/5187  ·  업로드: EPSG:5179\n"
          "\n"
          "저작권·라이선스\n"
          "QGIS  © QGIS Development Team  ·  GNU GPL v2 이상\n"
          "Qt    © The Qt Company Ltd.  ·  LGPLv3 / GPLv2+\n"
          "GDAL/OGR  © OSGeo  ·  MIT/X11\n"
          "PROJ  © PROJ contributors  ·  MIT\n"
          "GEOS  © GEOS contributors  ·  LGPLv2.1\n"
          "\n"
          "본 소프트웨어는 GNU GPL v2 이상으로 배포됩니다."));
}



