#include "MainWindow.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaDrawingStudio.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"
#include "core/ProjectStateBuilder.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"
#include "core/WorkflowGuide.h"

#include <algorithm>
#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
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
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>
#include <QSize>
#include <QListWidgetItem>
#include <QToolBar>
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
#include <QHash>
#include <QMetaType>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QLayout>

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
#include <qgsmaptoolselect.h>
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
#include <qgsproviderregistry.h>
#include <qgsprovidersublayerdetails.h>
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
  refreshWorkPanel();
  updateNextActionStatus();
  auto* undoAct = new QAction(QStringLiteral("되돌리기"), this);
  undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setShortcutContext(Qt::WindowShortcut);
  connect(undoAct, &QAction::triggered, this, &MainWindow::undoLastAction);
  addAction(undoAct);
}

MainWindow::~MainWindow() = default;

bool MainWindow::openSurveyGpkg(const QString& gpkgPath) {
  if (gpkgPath.isEmpty() || !QFile::exists(gpkgPath)) return false;
  m_surveyPath = gpkgPath;
  loadSurveyLayers(gpkgPath);
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
  mainTb->setIconSize(QSize(28, 28));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->setMovable(false);
  mainTb->setFloatable(false);

  auto addIcon = [this, mainTb](const QString& iconId, const QString& text, const QString& tip, auto slot) {
    QAction* a = mainTb->addAction(KaIcons::icon(iconId), text, this, slot);
    a->setToolTip(tip);
    return a;
  };
  addIcon(QStringLiteral("new"), QStringLiteral("새조사"),
          QStringLiteral("오늘 현장 조사를 새로 만듭니다"), &MainWindow::newSurvey);
  addIcon(QStringLiteral("open"), QStringLiteral("열기"),
          QStringLiteral("저장한 조사를 엽니다"), &MainWindow::openProject);
  addIcon(QStringLiteral("save"), QStringLiteral("저장"),
          QStringLiteral("지금 조사를 저장합니다"), &MainWindow::saveProject);
  mainTb->addSeparator();
  addIcon(QStringLiteral("satellite"), QStringLiteral("위성"),
          QStringLiteral("VWorld 위성 사진을 올립니다"), &MainWindow::addBasemapVworldSat);
  addIcon(QStringLiteral("cadastral"), QStringLiteral("지적"),
          QStringLiteral("VWorld 지적도를 올립니다"), &MainWindow::addBasemapVworldCadastral);

  auto* btnDraw = new QToolButton(mainTb);
  btnDraw->setObjectName(QStringLiteral("btnDraw"));
  btnDraw->setIcon(KaIcons::icon(QStringLiteral("draw_poly")));
  btnDraw->setText(QStringLiteral("그리기"));
  btnDraw->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnDraw->setCheckable(true);
  btnDraw->setToolTip(QStringLiteral("구역·유구를 그리고 속성을 넣습니다"));
  connect(btnDraw, &QToolButton::clicked, this, [this, btnDraw](bool on) {
    if (on) showSubToolsDraw();
    else hideSubTools();
    btnDraw->setChecked(m_subToolbar && m_subToolbar->isVisible() && m_subToolsMode == QLatin1String("draw"));
  });
  mainTb->addWidget(btnDraw);

  addIcon(QStringLiteral("select"), QStringLiteral("선택"),
          QStringLiteral("그린 것을 고릅니다"), &MainWindow::startSelectTool);
  addIcon(QStringLiteral("palette"), QStringLiteral("속성"),
          QStringLiteral("지도에서 눌러 종류·시대를 넣습니다"), &MainWindow::startAttributeEditTool);
  mainTb->addSeparator();
  addIcon(QStringLiteral("pdf"), QStringLiteral("도면"),
          QStringLiteral("종이에 지도를 올려 도면을 만듭니다"), &MainWindow::openLayoutDesigner);
  addIcon(QStringLiteral("check"), QStringLiteral("검수"),
          QStringLiteral("제출 전에 빠진 것을 확인합니다"), &MainWindow::runChecklist);

  auto* btnSend = new QToolButton(mainTb);
  btnSend->setObjectName(QStringLiteral("btnSubmit"));
  btnSend->setIcon(KaIcons::icon(QStringLiteral("export")));
  btnSend->setText(QStringLiteral("보내기"));
  btnSend->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnSend->setToolTip(QStringLiteral("업로드용 좌표·SHP 패키지"));
  auto* sendMenu = new QMenu(btnSend);
  sendMenu->addAction(KaIcons::icon(QStringLiteral("upload")), QStringLiteral("업로드용으로 보내기"),
                      this, &MainWindow::convertSelectedTo5179);
  sendMenu->addAction(KaIcons::icon(QStringLiteral("export")), QStringLiteral("SHP 패키지"),
                      this, &MainWindow::exportShpPackage);
  sendMenu->addSeparator();
  sendMenu->addAction(KaIcons::icon(QStringLiteral("crs")), QStringLiteral("중부원점"),
                      this, &MainWindow::setWorkCrs5186);
  sendMenu->addAction(KaIcons::icon(QStringLiteral("crs")), QStringLiteral("동부원점"),
                      this, &MainWindow::setWorkCrs5187);
  btnSend->setMenu(sendMenu);
  btnSend->setPopupMode(QToolButton::InstantPopup);
  mainTb->addWidget(btnSend);

  mainTb->addSeparator();
  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setObjectName(QStringLiteral("locationSearch"));
  m_searchEdit->setPlaceholderText(QStringLiteral("주소 · 지번 검색"));
  m_searchEdit->setMinimumWidth(200);
  m_searchEdit->setMaximumWidth(280);
  m_searchEdit->setClearButtonEnabled(true);
  connect(m_searchEdit, &QLineEdit::returnPressed, this, &MainWindow::runLocationSearch);
  mainTb->addWidget(m_searchEdit);
  mainTb->addAction(KaIcons::icon(QStringLiteral("search")), QStringLiteral("찾기"),
                    this, &MainWindow::runLocationSearch)
      ->setToolTip(QStringLiteral("위치를 찾아 지도로 갑니다"));

  auto* more = new QToolButton(mainTb);
  more->setIcon(KaIcons::icon(QStringLiteral("help")));
  more->setText(QStringLiteral("더보기"));
  more->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  more->setToolTip(QStringLiteral("가끔 쓰는 기능"));
  auto* moreMenu = new QMenu(more);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("벡터 불러오기"),
                      this, &MainWindow::openVectorLayer);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("import")), QStringLiteral("CSV 기준점"),
                      this, &MainWindow::importControlCsv);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("georef")), QStringLiteral("스캔 맞추기"),
                      this, &MainWindow::georefAssistant);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM 배경"),
                      this, &MainWindow::addBasemapOsm);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
                      this, &MainWindow::addBasemapGoogle);
  moreMenu->addSeparator();
  moreMenu->addAction(QStringLiteral("파일함 보이기/숨기기"), this, [this]() {
    if (auto* f = findChild<QFrame*>(QStringLiteral("filesCard")))
      f->setVisible(!f->isVisible());
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
  mainTb->addWidget(more);

  m_subToolbar = addToolBar(QStringLiteral("세부도구"));
  m_subToolbar->setObjectName(QStringLiteral("subToolbar"));
  m_subToolbar->setIconSize(QSize(22, 22));
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
  lab->setStyleSheet(QStringLiteral("color:#115e59;font-weight:700;padding:0 4px;"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("select")), QStringLiteral("선택"),
                          this, &MainWindow::startSelectTool);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_area")), QStringLiteral("구역그리기"),
                          this, &MainWindow::startEditFeaturePoly);
  m_subToolbar->addAction(QStringLiteral("속성"), this, &MainWindow::startAttributeEditTool);
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(QStringLiteral("구역을 그린 뒤 속성을 넣으세요."), 8000);
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
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("cadastral")), QStringLiteral("지적"),
                          this, &MainWindow::addBasemapVworldCadastral);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  if (!LayerOps::hasVisibleReferenceLayer(QgsProject::instance()))
    addBasemapVworldSat();
  else
    statusBar()->showMessage(QStringLiteral("위성 또는 지적을 올리세요."), 6000);
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

void MainWindow::applyPhase1Theme() {
  QPalette light;
  light.setColor(QPalette::Window, QColor(0xf3, 0xea, 0xd8));
  light.setColor(QPalette::WindowText, QColor(0x29, 0x25, 0x24));
  light.setColor(QPalette::Base, QColor(0xff, 0xfa, 0xf3));
  light.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf1, 0xe6));
  light.setColor(QPalette::Text, QColor(0x29, 0x25, 0x24));
  light.setColor(QPalette::Button, QColor(0xf7, 0xf1, 0xe6));
  light.setColor(QPalette::ButtonText, QColor(0x29, 0x25, 0x24));
  light.setColor(QPalette::ToolTipBase, QColor(0xff, 0xfb, 0xeb));
  light.setColor(QPalette::ToolTipText, QColor(0x29, 0x25, 0x24));
  light.setColor(QPalette::BrightText, QColor(0x29, 0x25, 0x24));
  light.setColor(QPalette::Highlight, QColor(0x11, 0x5e, 0x59));
  light.setColor(QPalette::HighlightedText, QColor(0xff, 0xfa, 0xf3));
  light.setColor(QPalette::PlaceholderText, QColor(0x78, 0x71, 0x6c));
  qApp->setPalette(light);
  setPalette(light);

  const QString sheet = QStringLiteral(
      "QMainWindow, QDialog, QLabel, QPushButton, QToolButton, QMenu, QMenuBar,"
      " QDockWidget, QStatusBar, QTreeView, QLineEdit, QComboBox, QAbstractSpinBox,"
      " QCheckBox, QGroupBox, QListWidget, QTextEdit, QPlainTextEdit, QHeaderView {"
      " color: #292524; }"
      "QgsMapCanvas, QWidget#mapCanvas { background: none; border: none; }"
      "QMainWindow, QWidget#centralRoot {"
      " background: qlineargradient(x1:0, y1:0, x2:0.15, y2:1,"
      "  stop:0 #fffaf3, stop:0.22 #f7f1e6, stop:0.62 #ebe2d2, stop:1 #e0d3be);"
      " color: #292524; }"
      "QDialog {"
      " background: qlineargradient(x1:0, y1:0, x2:0.15, y2:1,"
      "  stop:0 #fffaf3, stop:0.45 #f3ead8, stop:1 #e8dcc8);"
      " color: #292524; }"
      "QDialog QLabel { background: transparent; color: #292524; }"
      "QDialog QCheckBox { background: transparent; color: #292524; spacing: 8px; font-weight: 700; }"
      "QDialog QPushButton { color: #292524;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:1 #eadfcb);"
      " border: 1px solid #d6c4a8; border-top: 1px solid #ffffff; border-bottom: 2px solid #b45309;"
      " border-radius: 8px; padding: 8px 16px; min-height: 32px; font-weight: 600; }"
      "QDialog QPushButton:hover { background: #fde68a; }"
      "QDialog QPushButton:default {"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0f766e, stop:1 #115e59);"
      " color: white; border: 1px solid #134e4a; }"
      "QDialog QComboBox, QDialog QLineEdit, QDialog QAbstractSpinBox {"
      " min-height: 36px; font-size: 13px; background: #fffaf3; }"
      "QAbstractSpinBox { padding-right: 26px; min-height: 36px; }"
      "QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {"
      " subcontrol-origin: border; width: 22px;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:1 #e8dcc8);"
      " border-left: 1px solid #d6c4a8; }"
      "QAbstractSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 8px; }"
      "QAbstractSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 8px; }"
      "QAbstractSpinBox::up-arrow { width: 8px; height: 8px; background: #115e59; }"
      "QAbstractSpinBox::down-arrow { width: 8px; height: 8px; background: #115e59; }"
      "QDialog QComboBox::drop-down { width: 28px; border: none; }"
      "QDialog QComboBox QAbstractItemView { background: #fffaf3; color: #292524;"
      " selection-background-color: #115e59; selection-color: white; min-height: 28px; }"
      "QMenuBar { height: 0px; max-height: 0px; border: none; }"
      "QToolBar#mainToolbar { background: #111827; border: none; spacing: 2px; padding: 4px 8px; }"
      "QToolBar#mainToolbar QToolButton { color: #f9fafb; font-weight: 600; font-size: 11px;"
      " padding: 4px 6px; border-radius: 8px; background: transparent; min-width: 44px; }"
      "QToolBar#mainToolbar QToolButton:hover { background: #1f2937; color: #f9fafb; }"
      "QToolBar#mainToolbar QToolButton:checked { background: #0f766e; color: #fff; }"
      "QToolBar#subToolbar { background: #f3f4f6; border: none; border-bottom: 1px solid #e5e7eb;"
      " spacing: 4px; padding: 3px 8px; }"
      "QToolBar#subToolbar QToolButton, QToolBar#subToolbar QLabel {"
      " color: #111827; font-weight: 600; font-size: 12px; padding: 3px 8px; border-radius: 6px; }"
      "QToolBar#subToolbar QToolButton { background: #ffffff; border: 1px solid #e5e7eb; }"
      "QToolBar#subToolbar QToolButton:hover { background: #ecfdf5; color: #111827; }"
      "QLineEdit#locationSearch { background: #ffffff; color: #111827; border: 1px solid #4b5563;"
      " border-radius: 6px; padding: 4px 8px; min-height: 26px; }"
      "QFrame#filesCard, QFrame#layersCard, QFrame#mapCard {"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:0.2 #f7f1e6, stop:1 #efe6d6);"
      " border: 1px solid #d6c4a8; border-top: 1px solid #fffaf3; border-left: 1px solid #fffaf3;"
      " border-bottom: 2px solid #b08968; border-right: 2px solid #b08968;"
      " border-radius: 14px; color: #292524; }"
      "QFrame#filesCard { border-top: 5px solid #b45309; }"
      "QFrame#layersCard { border-top: 5px solid #0f766e; }"
      "QFrame#mapCard { border-top: 5px solid #1c1917; }"
      "QFrame#filesCard QToolButton, QFrame#layersCard QToolButton, QFrame#mapCard QToolButton {"
      " color: #292524;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:1 #eadfcb);"
      " border: 1px solid #d6c4a8; border-bottom: 2px solid #b08968;"
      " border-radius: 8px; padding: 5px 10px; font-weight: 600; min-height: 26px; }"
      "QFrame#filesCard QToolButton:hover, QFrame#layersCard QToolButton:hover, QFrame#mapCard QToolButton:hover {"
      " background: #fde68a; color: #292524; }"
      "QFrame#layersCard QToolButton:checked { background: #115e59; color: #fffaf3; }"
      "QLabel { color: #292524; background: transparent; }"
      "QLabel#cardCaption {"
      " color: #292524; font-weight: 800; font-size: 14px; padding: 8px 10px 6px 10px;"
      " background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #f3ead8, stop:1 #fffaf3);"
      " border-radius: 8px; margin-bottom: 2px; }"
      "QLabel#cardCaptionFiles { color: #b45309; }"
      "QLabel#cardCaptionLayers { color: #115e59; }"
      "QFrame#filesInner, QFrame#layersInner {"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:1 #f3ead8);"
      " border: 1px solid #e8dcc8; border-radius: 10px; }"
      "QStatusBar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #1c1917, stop:1 #134e4a);"
      " color: #fffaf3; font-weight: 600; }"
      "QStatusBar QLabel { color: #fffaf3; }"
      "QTreeView, QTreeView::item, QgsLayerTreeView, QgsLayerTreeView::item {"
      " background: #fffaf3; color: #292524; border: none; border-radius: 8px;"
      " font-size: 13px; padding: 3px; }"
      "QTreeView::item:selected, QgsLayerTreeView::item:selected {"
      " background: #ccfbf1; color: #292524; }"
      "QTreeView::item:hover, QgsLayerTreeView::item:hover {"
      " background: #fde68a; color: #292524; }"
      "QHeaderView::section { background: #f3ead8; color: #292524; }"
      "QLineEdit, QComboBox, QAbstractSpinBox {"
      " border: 1px solid #d6c4a8; border-radius: 8px; padding: 4px 8px;"
      " color: #292524; background: #fffaf3; selection-background-color: #fde68a;"
      " selection-color: #292524; }"
      "QComboBox QAbstractItemView { color: #292524; background: #fffaf3; selection-background-color: #ccfbf1; }"
      "QMenuBar { color: #292524;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #fffaf3, stop:1 #eadfcb);"
      " font-weight: 700; font-size: 13px; }"
      "QMenuBar::item { color: #292524; padding: 6px 14px; background: transparent; }"
      "QMenuBar::item:selected { background: #fde68a; color: #292524; border-radius: 4px; }"
      "QMenuBar::icon { width: 0px; height: 0px; }"
      "QMenu::icon { width: 0px; height: 0px; }"
      "QMenu { color: #292524; background: #fffaf3; border: 1px solid #d6c4a8; padding: 6px; }"
      "QMenu::item { color: #292524; padding: 10px 18px; border-radius: 6px; min-height: 22px; }"
      "QMenu::item:selected { background: #115e59; color: #fffaf3; }"
      "QMessageBox, QMessageBox QLabel { color: #292524; background: #fffaf3; }"
      "QMessageBox QPushButton { color: #292524; background: #eadfcb; border: 1px solid #d6c4a8;"
      " border-radius: 6px; padding: 6px 16px; min-width: 64px; }"
      "QInputDialog { color: #292524; background: #fffaf3; }"
      "QInputDialog QLabel, QDialog QLabel { color: #292524; }"
      "QToolTip { color: #292524; background: #fffbeb; border: 1px solid #b45309; }");
  qApp->setStyleSheet(sheet);
  setStyleSheet(sheet);
}

void MainWindow::updateNextActionStatus() {
  QString msg;
  if (m_surveyPath.isEmpty()) {
    msg = QStringLiteral("먼저 「새조사」로 오늘 현장을 만드세요.");
  } else {
#if KA_HGIS_HAS_QGIS
    const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
    const bool hasDraw = domainLayerCount() > 0;
#else
    const bool hasBg = false;
    const bool hasDraw = (m_stubSurveyArea + m_stubFeatures) > 0;
#endif
    if (!hasBg)
      msg = QStringLiteral("「배경」에서 위성 또는 지적을 올리세요.");
    else if (!hasDraw)
      msg = QStringLiteral("「그리기」로 구역을 그리세요.");
    else
      msg = QStringLiteral("다 그렸으면 「도면만들기」로 종이에 옮기세요.");
  }
  statusBar()->showMessage(msg);
}

void MainWindow::buildUi() {
  applyPhase1Theme();
  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("centralRoot"));
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  m_canvas->setStyleSheet(QString());
  m_canvas->setAttribute(Qt::WA_StyledBackground, false);
  if (m_canvas->viewport()) {
    m_canvas->viewport()->setStyleSheet(QString());
    m_canvas->viewport()->setAttribute(Qt::WA_StyledBackground, false);
    m_canvas->viewport()->setAutoFillBackground(true);
  }
  m_canvas->setCanvasColor(QColor(239, 232, 220));
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
  m_layerTree->setStyleSheet(QStringLiteral(
      "QgsLayerTreeView, QTreeView { background: #fffaf3; color: #292524; border: none;"
      " border-radius: 8px; font-size: 13px; outline: none; }"
      "QgsLayerTreeView::item, QTreeView::item { color: #292524; padding: 3px 4px; min-height: 22px; }"
      "QgsLayerTreeView::item:selected, QTreeView::item:selected {"
      " background: #ccfbf1; color: #292524; }"
      "QgsLayerTreeView::item:hover, QTreeView::item:hover {"
      " background: #fde68a; color: #292524; }"));
  {
    QPalette pal = m_layerTree->palette();
    pal.setColor(QPalette::Base, QColor(0xff, 0xfa, 0xf3));
    pal.setColor(QPalette::Text, QColor(0x29, 0x25, 0x24));
    pal.setColor(QPalette::WindowText, QColor(0x29, 0x25, 0x24));
    pal.setColor(QPalette::BrightText, QColor(0x29, 0x25, 0x24));
    pal.setColor(QPalette::HighlightedText, QColor(0x29, 0x25, 0x24));
    pal.setColor(QPalette::Highlight, QColor(0xcc, 0xfb, 0xf1));
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
  connect(m_layerTree, &QTreeView::doubleClicked, this, &MainWindow::onLayerTreeDoubleClicked);
  m_bridge = new QgsLayerTreeMapCanvasBridge(treeRoot, m_canvas, this);
  m_bridge->setAutoSetupOnFirstLayer(false);

  m_canvas->setContextMenuPolicy(Qt::CustomContextMenu);
  m_canvas->installEventFilter(this);
  if (m_canvas->viewport())
    m_canvas->viewport()->installEventFilter(this);
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

  auto* addBtn = new QToolButton(central);
  addBtn->setObjectName(QStringLiteral("btnAddLayer"));
  addBtn->setText(QStringLiteral("레이어 추가"));
  addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(addBtn, &QToolButton::clicked, this, &MainWindow::addUserLayer);
  auto* delBtn = new QToolButton(central);
  delBtn->setObjectName(QStringLiteral("btnRemoveLayer"));
  delBtn->setIcon(KaIcons::icon(QStringLiteral("trash")));
  delBtn->setText(QStringLiteral("레이어 삭제"));
  delBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(delBtn, &QToolButton::clicked, this, &MainWindow::removeSelectedLayers);

  auto* btnCrs5186 = new QToolButton(central);
  btnCrs5186->setObjectName(QStringLiteral("btnCrs5186"));
  btnCrs5186->setText(QStringLiteral("중부"));
  btnCrs5186->setToolTip(QStringLiteral("중부원점 (EPSG:5186)"));
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
  btnCrs5187->setText(QStringLiteral("동부"));
  btnCrs5187->setToolTip(QStringLiteral("동부원점 (EPSG:5187)"));
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
  auto* capFiles = new QLabel(QStringLiteral("조사 파일"), filesCard);
  capFiles->setObjectName(QStringLiteral("cardCaption"));
  capFiles->setProperty("class", QStringLiteral("cardCaptionFiles"));
  capFiles->setStyleSheet(QStringLiteral("QLabel { color: #b45309; font-weight: 800; font-size: 14px; }"));
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
  btnDesk->setObjectName(QStringLiteral("btnBrowseDesktop"));
  connect(btnDesk, &QToolButton::clicked, this, [this]() {
    goFileBrowserRoot(resolvedDesktopPath());
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
  filesCard->hide();

  auto* layersCard = new QFrame(central);
  layersCard->setObjectName(QStringLiteral("layersCard"));
  auto* layersLay = new QVBoxLayout(layersCard);
  layersLay->setContentsMargins(10, 10, 10, 10);
  layersLay->setSpacing(8);
  auto* capLayers = new QLabel(QStringLiteral("지도 목록"), layersCard);
  capLayers->setObjectName(QStringLiteral("cardCaption"));
  capLayers->setStyleSheet(QStringLiteral("QLabel { color: #115e59; font-weight: 800; font-size: 14px; }"));
  auto* layersInner = new QFrame(layersCard);
  layersInner->setObjectName(QStringLiteral("layersInner"));
  auto* layersInnerLay = new QVBoxLayout(layersInner);
  layersInnerLay->setContentsMargins(6, 6, 6, 6);
  layersInnerLay->addWidget(m_layerTree, 1);
  auto* bottomBar = new QHBoxLayout();
  bottomBar->setSpacing(6);
  bottomBar->addWidget(addBtn);
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
  leftSplit->setStretchFactor(0, 1);
  leftSplit->setStretchFactor(1, 5);
  leftSplit->setSizes({0, 420});
  leftSplit->setMinimumWidth(200);
  leftSplit->setMaximumWidth(280);
  leftSplit->setStyleSheet(QStringLiteral(
      "QSplitter::handle:vertical {"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #e8dcc8, stop:0.5 #0f766e, stop:1 #b45309);"
      " margin: 4px 16px; border-radius: 3px; height: 8px; }"
      "QSplitter::handle:vertical:hover { background: #115e59; }"));

  auto* mapCard = new QFrame(central);
  mapCard->setObjectName(QStringLiteral("mapCard"));
  auto* mapLay = new QVBoxLayout(mapCard);
  mapLay->setContentsMargins(4, 4, 4, 4);
  mapLay->setSpacing(4);
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

  auto* checkDock = new QDockWidget(QStringLiteral("도면 검수"), this);
  checkDock->setObjectName(QStringLiteral("checkDock"));
  checkDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
  m_checkView = new QLabel(checkDock);
  m_checkView->setObjectName(QStringLiteral("checkView"));
  m_checkView->setTextFormat(Qt::RichText);
  m_checkView->setWordWrap(true);
  m_checkView->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_checkView->setMargin(12);
  m_checkView->setText(QStringLiteral("「도면검수」를 실행하면 결과가 여기에 표시됩니다."));
  auto* checkScroll = new QScrollArea(checkDock);
  checkScroll->setWidgetResizable(true);
  checkScroll->setWidget(m_checkView);
  checkDock->setWidget(checkScroll);
  addDockWidget(Qt::RightDockWidgetArea, checkDock);
  checkDock->hide();
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
  title->setStyleSheet(QStringLiteral("font-weight:800;font-size:13px;color:#0f172a;"));
  m_workHint = new QLabel(box);
  m_workHint->setObjectName(QStringLiteral("workHint"));
  m_workHint->setWordWrap(true);
  m_workHint->setStyleSheet(QStringLiteral("color:#334155;"));
  m_workList = new QListWidget(box);
  m_workList->setObjectName(QStringLiteral("workControlList"));
  m_workList->setSpacing(3);
  m_workList->setStyleSheet(QStringLiteral(
      "QListWidget { border:1px solid #cbd5e1; border-radius:8px; padding:4px; }"
      "QListWidget::item { padding:8px; border-radius:6px; }"
      "QListWidget::item:selected { background:#1d4ed8; color:white; }"));
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
  double w = 297.0, h = 210.0;
  if (!KaDrawingStudio::promptPaper(this, &w, &h))
    return;
  if (!m_drawingStudio)
    m_drawingStudio = new KaDrawingStudio(QgsProject::instance(), m_canvas, w, h, this);
  else
    m_drawingStudio->resetPaper(w, h);
  m_drawingStudio->show();
  m_drawingStudio->raise();
  m_drawingStudio->activateWindow();
  statusBar()->showMessage(QStringLiteral("빈 종이에서 끌어 지도 칸을 만드세요."), 6000);
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
  auto paintCrs = [](QPushButton* b, bool on) {
    b->setStyleSheet(on ? QStringLiteral(
        "QPushButton { background:#1d4ed8; color:white; font-weight:800; border:none; border-radius:10px; }"
        "QPushButton:hover { background:#1e40af; }")
                         : QStringLiteral(
        "QPushButton { background:white; color:#0f172a; font-weight:700; border:2px solid #93c5fd; border-radius:10px; }"
        "QPushButton:hover { background:#dbeafe; }"));
  };
  paintCrs(btn5186, !use5187);
  paintCrs(btn5187, use5187);
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187, paintCrs]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
    paintCrs(btn5186, true);
    paintCrs(btn5187, false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187, paintCrs]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
    paintCrs(btn5187, true);
    paintCrs(btn5186, false);
  });
  crsRow->addWidget(btn5186, 1);
  crsRow->addWidget(btn5187, 1);
  auto* tip = new QLabel(QStringLiteral("나중에 「도면만들기」옆에서 업로드용으로 바꿀 수 있습니다."), &dlg);
  tip->setWordWrap(true);
  tip->setStyleSheet(QStringLiteral("color:#475569;font-size:12px;"));
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
  setWindowTitle(QStringLiteral("고고학 전용 HGIS — %1").arg(name));
  updateNextActionStatus();
  refreshWorkPanel();
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr, false);
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::zoomToKorea(m_canvas, m_workCrs);
    m_canvas->zoomScale(80000.0, true);
    LayerOps::clampCanvasToKorea(m_canvas);
    m_canvas->refreshAllLayers();
    m_canvas->refresh();
  }
  updateNextActionStatus();
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
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
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
  if (!LayerOps::isolateAndZoomToLayer(QgsProject::instance(), m_canvas, layer, true)) {
    statusBar()->showMessage(
        QStringLiteral("이 레이어에 도형이 없습니다. 먼저 그린 뒤 다시 누르세요: %1").arg(layer->name()),
        8000);
    return;
  }
  statusBar()->showMessage(QStringLiteral("이 레이어만 보기: %1").arg(layer->name()), 5000);
#endif
}

void MainWindow::onMapContextMenu(const QPoint& pos) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool) {
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
  menu.exec(m_canvas->mapToGlobal(pos));
#else
  Q_UNUSED(pos);
#endif
}

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
  menu.addSeparator();
  menu.addAction(QStringLiteral("이 레이어만 보기"), this, &MainWindow::zoomSelectedLayerMax);
  menu.addAction(QStringLiteral("전체 보기"), this, &MainWindow::zoomMapToFullMax);
  menu.addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  menu.addSeparator();
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
  if (!vl) {
    QMessageBox::information(this, QStringLiteral("업로드용으로 보내기"),
                             QStringLiteral("지도 목록에서 보낼 면을 선택한 뒤 다시 누르세요."));
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
    statusBar()->showMessage(QStringLiteral("업로드용 파일을 만들었습니다."), 8000);
    QMessageBox::information(this, QStringLiteral("보내기"),
                             QStringLiteral("업로드용 파일을 저장했습니다.\n%1").arg(out));
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
    QMessageBox::information(this, QStringLiteral("중부 → 업로드용"),
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
    QMessageBox::information(this, QStringLiteral("동부 → 업로드용"),
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
  if (!m_selectTool) {
    m_selectTool = new QgsMapToolSelect(m_canvas);
    m_selectTool->setParent(this);
  }
  if (m_layerTree && m_layerTree->currentLayer())
    m_canvas->setCurrentLayer(m_layerTree->currentLayer());
  m_canvas->setMapTool(m_selectTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(QStringLiteral("선택: 화살표로 도형을 클릭하세요"), 0);
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

void MainWindow::undoLastAction() {
  auto* focus = QApplication::focusWidget();
  if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus) ||
      qobject_cast<QTextEdit*>(focus) || qobject_cast<QPlainTextEdit*>(focus))
    return;
#if KA_HGIS_HAS_QGIS
  if (m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool &&
      m_captureTool->undoLastVertex()) {
    statusBar()->showMessage(QStringLiteral("꼭짓점 하나를 되돌렸습니다."), 4000);
    return;
  }
  while (!m_committedUndo.isEmpty()) {
    const auto rec = m_committedUndo.takeLast();
    auto* vl = qobject_cast<QgsVectorLayer*>(QgsProject::instance()->mapLayer(rec.first));
    QString err;
    if (LayerOps::undoCommittedFeature(vl, rec.second, &err)) {
      if (m_canvas) {
        m_canvas->clearCache();
        m_canvas->refreshAllLayers();
        m_canvas->refresh();
      }
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

  const bool onCanvas = m_canvas &&
      (watched == m_canvas || watched == m_canvas->viewport());
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
    if (!capturing) {
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
  LayerOps::syncMapCanvas(QgsProject::instance(), canvas, false);
  if (label.contains(QStringLiteral("지적"))) {
    if (canvas->scale() > 8000.0 || canvas->scale() < 200.0)
      canvas->zoomScale(3000.0, true);
  } else if (canvas->scale() > 80000.0 || canvas->scale() < 100.0)
    canvas->zoomScale(50000.0, true);
  LayerOps::clampCanvasToKorea(canvas);
  canvas->clearCache();
  for (int i = 0; i < 20; ++i)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
  QTimer::singleShot(250, self, [canvas, workCrs]() {
    if (!canvas) return;
    LayerOps::ensureOtfEnabled(QgsProject::instance(), canvas, workCrs);
    LayerOps::syncMapCanvas(QgsProject::instance(), canvas, false);
    canvas->refreshAllLayers();
    canvas->refresh();
  });
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
  updateNextActionStatus();
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
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("배경"));
#endif
}

void MainWindow::addBasemapVworldSat() {
#if KA_HGIS_HAS_QGIS
  const QString key = VworldSettings::loadApiKey();
  QString err;
  if (!LayerOps::addVworldSatelliteMap(QgsProject::instance(), m_canvas, key, &err))
    QMessageBox::warning(this, QStringLiteral("위성"), err);
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
    QMessageBox::warning(this, QStringLiteral("지적도"), err);
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
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("위성"));
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
    LayerOps::ensureOtfEnabled(proj, m_canvas, m_workCrs);
    LayerOps::syncMapCanvas(proj, m_canvas, false);
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
  LayerOps::applyDomainDrawStyle(vl, layerKey);
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    m_canvas->refresh();
  }
  if (m_layerTree)
    m_layerTree->setCurrentLayer(vl);
  statusBar()->showMessage(
      QStringLiteral("레이어 준비: %1 — 지도에서 그리세요").arg(vl->name()), 6000);
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
  m_canvas->setMapTool(m_attributeTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("속성 편집: 지도에서 도형을 좌클릭 · ESC=종료"), 0);
#else
  QMessageBox::information(this, QStringLiteral("속성"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

namespace {

void kaPaintColorButton(QPushButton* b, const QColor& c, const QString& suffix) {
  if (!b) return;
  const QString fg = c.lightness() > 150 ? QStringLiteral("#0f172a") : QStringLiteral("#ffffff");
  b->setStyleSheet(QStringLiteral(
      "QPushButton { background:%1; color:%2; font-weight:800; font-size:13px;"
      " border:2px solid #0369a1; border-radius:10px; min-height:40px; padding:8px 12px; }"
      "QPushButton:hover { border-color:#0c4a6e; }")
                       .arg(c.name(QColor::HexRgb), fg));
  b->setText(c.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  ") + suffix);
  b->setProperty("kaColor", c);
  b->setCursor(Qt::PointingHandCursor);
}

QPushButton* kaMakeColorButton(QWidget* parent, const QColor& c, const QString& suffix,
                               const QString& pickerTitle) {
  auto* b = new QPushButton(parent);
  kaPaintColorButton(b, c.isValid() && c.alpha() > 0 ? c : QColor(22, 163, 74, 160), suffix);
  QObject::connect(b, &QPushButton::clicked, b, [b, suffix, pickerTitle]() {
    QColorDialog picker(b->property("kaColor").value<QColor>(), b->window());
    picker.setOption(QColorDialog::DontUseNativeDialog, true);
    picker.setOption(QColorDialog::ShowAlphaChannel, true);
    picker.setWindowTitle(pickerTitle);
    if (picker.exec() != QDialog::Accepted) return;
    const QColor picked = picker.selectedColor();
    if (!picked.isValid()) return;
    kaPaintColorButton(b, picked, suffix);
  });
  return b;
}

QWidget* kaWrapLabeled(QWidget* parent, const QString& caption, QWidget* inner) {
  auto* box = new QWidget(parent);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  auto* lab = new QLabel(caption, box);
  lab->setStyleSheet(QStringLiteral("font-weight:700;color:#0f172a;background:transparent;"));
  v->addWidget(lab);
  v->addWidget(inner);
  return box;
}

QDoubleSpinBox* kaMakeArrowSpin(QWidget* parent, QWidget** rowOut, double minV, double maxV,
                                double step, int decimals, double value) {
  auto* row = new QWidget(parent);
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(6);
  auto* spin = new QDoubleSpinBox(row);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setRange(minV, maxV);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setSuffix(QStringLiteral(" mm"));
  spin->setValue(value);
  spin->setMinimumHeight(40);
  spin->setAlignment(Qt::AlignCenter);
  auto* arrows = new QWidget(row);
  auto* av = new QVBoxLayout(arrows);
  av->setContentsMargins(0, 0, 0, 0);
  av->setSpacing(2);
  const QString arrowQss = QStringLiteral(
      "QPushButton { font-size:11px; font-weight:900; color:#0c4a6e; min-width:36px; max-width:36px;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 #7dd3fc);"
      " border:1px solid #0284c7; border-top:1px solid #ffffff; border-bottom:2px solid #0284c7; }"
      "QPushButton:pressed { background:#38bdf8; }");
  auto* up = new QPushButton(QStringLiteral("▲"), arrows);
  auto* down = new QPushButton(QStringLiteral("▼"), arrows);
  up->setFixedSize(36, 19);
  down->setFixedSize(36, 19);
  up->setCursor(Qt::PointingHandCursor);
  down->setCursor(Qt::PointingHandCursor);
  up->setStyleSheet(arrowQss + QStringLiteral("QPushButton { border-top-left-radius:6px; border-top-right-radius:6px; }"));
  down->setStyleSheet(arrowQss + QStringLiteral("QPushButton { border-bottom-left-radius:6px; border-bottom-right-radius:6px; }"));
  QObject::connect(up, &QPushButton::clicked, spin, [spin]() { spin->stepUp(); });
  QObject::connect(down, &QPushButton::clicked, spin, [spin]() { spin->stepDown(); });
  av->addWidget(up);
  av->addWidget(down);
  h->addWidget(spin, 1);
  h->addWidget(arrows, 0, Qt::AlignVCenter);
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
  LayerOps::readSimpleVectorStyle(layer, &fill, &stroke, &widthMm, &markerMm, &noFill, &noStroke);

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
  title->setStyleSheet(QStringLiteral("font-weight:800;font-size:16px;color:#0f172a;"));
  auto* sub = new QLabel(layer->name(), &dlg);
  sub->setWordWrap(true);
  sub->setStyleSheet(QStringLiteral("font-weight:600;font-size:13px;color:#334155;"));
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

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  auto syncVisible = [&dlg, fillBox, strokeBox, widthBox, noFillCheck, noStrokeCheck]() {
    const bool nf = noFillCheck && noFillCheck->isChecked();
    const bool ns = noStrokeCheck && noStrokeCheck->isChecked();
    if (fillBox) fillBox->setVisible(!nf);
    if (strokeBox) strokeBox->setVisible(!ns);
    if (widthBox) widthBox->setVisible(!ns);
    dlg.adjustSize();
  };
  if (noFillCheck) connect(noFillCheck, &QCheckBox::toggled, &dlg, syncVisible);
  connect(noStrokeCheck, &QCheckBox::toggled, &dlg, syncVisible);
  syncVisible();

  if (dlg.exec() != QDialog::Accepted) return;

  if (catCheck && catCheck->isChecked()) {
    if (LayerOps::applyFeaturePolyStyle(layer)) {
      if (m_canvas) m_canvas->refresh();
      statusBar()->showMessage(QStringLiteral("종류별 색 구분 적용: %1").arg(layer->name()), 5000);
    }
    return;
  }

  const QColor outFill = fillBtn ? fillBtn->property("kaColor").value<QColor>() : fill;
  const QColor outStroke = strokeBtn->property("kaColor").value<QColor>();
  const bool outNoFill = noFillCheck && noFillCheck->isChecked();
  const bool outNoStroke = noStrokeCheck->isChecked();
  const double markerVal = markerSpin ? markerSpin->value() : markerMm;
  if (!LayerOps::applySimpleVectorStyle(layer, outFill, outStroke, widthSpin->value(),
                                        markerVal, outNoFill, outNoStroke)) {
    QMessageBox::warning(this, QStringLiteral("모양"), QStringLiteral("스타일 적용 실패"));
    return;
  }
  if (m_canvas) m_canvas->refresh();
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
  auto paintCrs = [](QPushButton* b, bool on) {
    b->setStyleSheet(on ? QStringLiteral(
        "QPushButton { background:#1d4ed8; color:white; font-weight:800; border:none; border-radius:10px; }"
        "QPushButton:hover { background:#1e40af; }")
                         : QStringLiteral(
        "QPushButton { background:white; color:#0f172a; font-weight:700; border:2px solid #93c5fd; border-radius:10px; }"
        "QPushButton:hover { background:#dbeafe; }"));
  };
  paintCrs(btn5186, btn5186->isChecked());
  paintCrs(btn5187, btn5187->isChecked());
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187, paintCrs]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
    paintCrs(btn5186, true);
    paintCrs(btn5187, false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187, paintCrs]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
    paintCrs(btn5187, true);
    paintCrs(btn5186, false);
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
    QMessageBox::warning(this, QStringLiteral("레이어 추가"), err);
    return;
  }
  LayerOps::applySimpleVectorStyle(vl, fillBtn->property("kaColor").value<QColor>(),
                                   strokeBtn->property("kaColor").value<QColor>(),
                                   1.2, 3.5, noFill->isChecked(), false);
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
  tip->setStyleSheet(QStringLiteral("color:#334155;padding:4px 0 8px 0;"));
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

    QgsFeature feat(layer->fields());
    feat.setGeometry(geom);
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
    qint64 addedId = static_cast<qint64>(feat.id());
    const QgsFeatureIds afterIds = layer->allFeatureIds();
    if ((addedId < 0 || !afterIds.contains(static_cast<QgsFeatureId>(addedId))) && !afterIds.isEmpty())
      addedId = static_cast<qint64>(*std::max_element(afterIds.constBegin(), afterIds.constEnd()));
    if (addedId >= 0)
      m_committedUndo.append(qMakePair(layer->id(), addedId));
    if (!layer->isEditable() && !layer->startEditing()) {
      statusBar()->showMessage(QStringLiteral("저장됨 · 편집 모드 재시작 실패 — 그리기 도구를 다시 선택"), 8000);
    }

    LayerOps::applyDomainDrawStyle(layer, LayerOps::layerKeyOf(layer));
    layer->updateExtents();
    layer->triggerRepaint();
    if (m_canvas) {
      m_canvas->freeze(false);
      m_canvas->setRenderFlag(true);
      LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
      // Survey vectors above basemap (draw last = on top in QGIS canvas list = first in list).
      QList<QgsMapLayer*> layers = m_canvas->layers();
      QList<QgsMapLayer*> ordered;
      ordered.append(layer);
      for (QgsMapLayer* l : layers) {
        if (l && l != layer) ordered.append(l);
      }
      m_canvas->setLayers(ordered);
      m_canvas->clearCache();
      m_canvas->refreshAllLayers();
      m_canvas->refresh();
    }
    if (m_captureTool && m_canvas && m_canvas->mapTool() != m_captureTool) {
      m_canvas->setMapTool(m_captureTool);
      m_canvas->setFocus(Qt::OtherFocusReason);
    }

    const long long n = static_cast<long long>(layer->featureCount());
    statusBar()->showMessage(
        QStringLiteral("도형 저장 (%1, %2개) · 계속 좌클릭 · 우클릭=다음 완료 · 속성은 오른쪽 「속성 고치기」")
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

    m_canvas->freeze(false);
    m_canvas->setRenderFlag(true);
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);

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

    QgsSnappingConfig snap = QgsProject::instance()->snappingConfig();
    snap.setEnabled(false);
    QgsProject::instance()->setSnappingConfig(snap);

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
      connect(m_captureTool, &KaCaptureMapTool::captureCanceled, this, [this]() {
        statusBar()->showMessage(
            QStringLiteral("점 부족 또는 취소 — 면≥3·선≥2 남기고 이어서 그리세요 (고무줄 유지/ESC=전체취소)"),
            6000);
      });
    }

    m_captureTool->setTargetLayer(layer);
    m_captureTool->setMode(mode);
    m_canvas->setMapTool(m_captureTool);
    m_canvas->setFocus(Qt::OtherFocusReason);
    m_canvas->setCursor(Qt::CrossCursor);

    const QString how = (mode == KaCaptureMapTool::Mode::Point)
                            ? QStringLiteral("지도 좌클릭 = 점")
                            : QStringLiteral("좌클릭=꼭짓점 / 우클릭·더블클릭·Enter=완료(도구 유지) / ESC=취소");
    statusBar()->showMessage(QStringLiteral("그리기 중: %1 | %2").arg(layer->name(), how), 0);
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
  QgsVectorLayer* cur = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  if (cur && cur->isValid() && !LayerOps::isReferenceLayer(cur) &&
      cur->geometryType() == Qgis::GeometryType::Polygon) {
    beginEdit(cur);
    return;
  }
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("구역")));
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
  m_lastChecklistErrors = err;
  if (m_checkView) {
    m_checkView->setText(html);
    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("checkDock"))) {
      dock->show();
      dock->raise();
    }
  } else {
    QMessageBox::information(this, QStringLiteral("도면 검수"),
                             QStringLiteral("error %1 / warn %2\n(상세 패널 없음)").arg(err).arg(warn));
  }
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
    QMessageBox::warning(this, QStringLiteral("내보내기"), err);
  else {
    m_packageCreated = true;
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
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
  statusBar()->showMessage(QStringLiteral("지도를 올렸습니다: %1").arg(title), 6000);
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
  setWindowTitle(QStringLiteral("고고학 전용 HGIS — %1").arg(QFileInfo(path).completeBaseName()));
  updateNextActionStatus();
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
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    m_canvas->refreshAllLayers();
    m_canvas->refresh();
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

QString MainWindow::resolvedDesktopPath() {
  QStringList candidates;
  candidates << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
             << (QDir::homePath() + QStringLiteral("/Desktop"))
             << (QDir::homePath() + QStringLiteral("/OneDrive/Desktop"))
             << (QDir::homePath() + QStringLiteral("/OneDrive/바탕 화면"));

  const QStringList scanRoots = {QDir::homePath(), QDir::homePath() + QStringLiteral("/OneDrive")};
  for (const QString& root : scanRoots) {
    QDir d(root);
    if (!d.exists()) continue;
    const QFileInfoList dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : dirs) {
      const QString n = fi.fileName();
      if (n.compare(QLatin1String("Desktop"), Qt::CaseInsensitive) == 0 ||
          n.contains(QStringLiteral("바탕")))
        candidates.prepend(fi.absoluteFilePath());
    }
  }
  for (const QString& c : candidates) {
    if (c.isEmpty()) continue;
    const QFileInfo fi(c);
    if (fi.exists() && fi.isDir())
      return QDir::cleanPath(fi.absoluteFilePath());
  }
  return QDir::homePath();
}

void MainWindow::goFileBrowserRoot(const QString& path) {
  if (!m_fsModel || !m_fileBrowser) return;

  try {
    m_fileBrowser->clearSelection();
    if (m_fileBrowser->selectionModel())
      m_fileBrowser->selectionModel()->clear();

    QString p = QDir::fromNativeSeparators(path.trimmed());
    if (p.isEmpty()) {
      m_fsModel->setRootPath(QStringLiteral(""));
      const QModelIndex root = m_fsModel->index(QStringLiteral(""));
      m_fileBrowser->setRootIndex(root.isValid() ? root : QModelIndex());
      statusBar()->showMessage(QStringLiteral("내 PC 드라이브 목록 — 폴더를 열어 SHP/GPKG를 찾으세요"), 8000);
      return;
    }

    if (p.length() == 2 && p[1] == QLatin1Char(':'))
      p += QLatin1Char('/');
    p = QDir::cleanPath(p);

    QFileInfo fi(p);
    if (!fi.exists() || !fi.isDir()) {
      statusBar()->showMessage(
          QStringLiteral("폴더 없음: %1 → 내 PC로 이동합니다").arg(QDir::toNativeSeparators(p)), 8000);
      goFileBrowserRoot(QString());
      return;
    }
    p = QDir::cleanPath(fi.absoluteFilePath());

    m_fsModel->setRootPath(p);
    auto applyRoot = [this, p]() -> bool {
      if (!m_fsModel || !m_fileBrowser) return false;
      const QModelIndex idx = m_fsModel->index(p);
      if (!idx.isValid()) return false;
      m_fileBrowser->setRootIndex(idx);
      statusBar()->showMessage(
          QStringLiteral("경로: %1  |  SHP·GPKG 더블클릭 또는 아래「지도 레이어」로 드래그")
              .arg(QDir::toNativeSeparators(p)),
          10000);
      return true;
    };

    if (applyRoot()) return;

    statusBar()->showMessage(
        QStringLiteral("경로 여는 중: %1").arg(QDir::toNativeSeparators(p)), 5000);
    QTimer::singleShot(50, this, [this, applyRoot]() {
      if (applyRoot()) return;
      QTimer::singleShot(200, this, [this, applyRoot]() {
        if (!applyRoot()) {
          statusBar()->showMessage(
              QStringLiteral("바탕화면/폴더를 열 수 없습니다. 「폴더…」로 직접 선택하세요"), 8000);
        }
      });
    });
  } catch (const std::exception& ex) {
    statusBar()->showMessage(QStringLiteral("폴더 이동 실패: %1").arg(QString::fromUtf8(ex.what())), 8000);
  } catch (...) {
    statusBar()->showMessage(QStringLiteral("폴더 이동 실패 — 내 PC 또는 폴더…를 사용하세요"), 8000);
  }
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
  const QString baseTitle = QFileInfo(path).completeBaseName();
  QList<QgsVectorLayer*> added;
  const QList<QgsProviderSublayerDetails> subs =
      QgsProviderRegistry::instance()->querySublayers(path);
  auto takeLayer = [&](QgsVectorLayer* layer, const QString& title) {
    if (!layer || !layer->isValid()) {
      delete layer;
      return;
    }
    layer->setName(title);
    LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
    LayerOps::applySimpleVectorStyle(layer, QColor(234, 88, 12, 90), QColor(154, 52, 18), 1.2, 4.0);
    LayerOps::applyLegendCrsLabel(layer);
    QgsProject::instance()->addMapLayer(layer, true);
    added.append(layer);
  };
  if (!subs.isEmpty()) {
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.type() != Qgis::LayerType::Vector) continue;
      QgsProviderSublayerDetails::LayerOptions opt(QgsProject::instance()->transformContext());
      auto* ml = d.toLayer(opt);
      takeLayer(qobject_cast<QgsVectorLayer*>(ml),
                d.name().isEmpty() ? baseTitle : d.name());
    }
  } else {
    takeLayer(new QgsVectorLayer(path, baseTitle, QStringLiteral("ogr")), baseTitle);
  }
  if (added.isEmpty())
    return false;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    QgsVectorLayer* zoomLayer = added.first();
    for (QgsVectorLayer* vl : added) {
      if (vl && vl->featureCount() > 0) {
        zoomLayer = vl;
        break;
      }
    }
    if (!LayerOps::zoomToLayerMax(m_canvas, zoomLayer)) {
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
  QMessageBox::about(this, QStringLiteral("정보"),
    QStringLiteral("고고학 전용 HGIS (ka-hgis) 0.3.0\n"
                   "C++/Qt6 + QGIS 4.x libraries\n"
                   "작업 CRS: 5186/5187 | 업로드: 5179\n"
                   "위치검색: 주소·지번·지역·상호\n"
                   "License: GNU GPLv2 or later"));
}



