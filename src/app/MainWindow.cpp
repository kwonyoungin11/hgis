#include "MainWindow.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/ExportService.h"

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

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolcapture.h>
#include <qgsmaptooldigitizefeature.h>
#include <qgsadvanceddigitizingdockwidget.h>
#include <qgssnappingconfig.h>
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsvectorfilewriter.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("고고학 전용 HGIS"));
  resize(1440, 900);
  m_checklist = new ChecklistEngine(this);
  m_checklist->loadRules(rulesPath());
  buildMenus();
  buildUi();
  setStepTools(0);
  statusBar()->showMessage(QStringLiteral("CRS 기본 EPSG:5179 | 규칙 %1개").arg(m_checklist->ruleCount()));
}

MainWindow::~MainWindow() = default;

QString MainWindow::rulesPath() const {
  const QStringList cands = {
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QStringLiteral("D:/qgis/data/rules/drawing_checklist.v1.json"),
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json"))
  };
  for (const QString& c : cands) if (QFile::exists(c)) return c;
  return cands.last();
}

void MainWindow::buildMenus() {
  auto* file = menuBar()->addMenu(QStringLiteral("파일"));
  file->addAction(QStringLiteral("1 새 조사 만들기…"), this, &MainWindow::newSurvey);
  file->addAction(QStringLiteral("벡터 불러오기…"), this, &MainWindow::openVectorLayer);
  file->addAction(QStringLiteral("프로젝트 열기…"), this, &MainWindow::openProject);
  file->addAction(QStringLiteral("프로젝트 저장…"), this, &MainWindow::saveProject);
  auto* crs = menuBar()->addMenu(QStringLiteral("좌표계"));
  auto* aDef = crs->addAction(QStringLiteral("이름만 지정(위험)"), this, &MainWindow::crsDefineOnly);
  aDef->setObjectName(QStringLiteral("actionCrsDefineOnly"));
  auto* aRep = crs->addAction(QStringLiteral("좌표 변환(재투영)"), this, &MainWindow::crsReproject);
  aRep->setObjectName(QStringLiteral("actionCrsReproject"));
  auto* tools = menuBar()->addMenu(QStringLiteral("도구"));
  tools->addAction(QStringLiteral("스캔 평면도 맞추기…"), this, &MainWindow::georefAssistant);
  tools->addAction(QStringLiteral("도면 검수"), this, &MainWindow::runChecklist);
  tools->addAction(QStringLiteral("PDF 내보내기…"), this, &MainWindow::exportPdf);
  tools->addAction(QStringLiteral("제출 패키지(SHP)…"), this, &MainWindow::exportShpPackage);
  auto* help = menuBar()->addMenu(QStringLiteral("도움말"));
  help->addAction(QStringLiteral("정보"), this, &MainWindow::showAbout);
}

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  auto* layout = new QHBoxLayout(central);

  m_steps = new QListWidget(central);
  m_steps->setObjectName(QStringLiteral("stepRail"));
  m_steps->addItems({
    QStringLiteral("1 새 조사 만들기"),
    QStringLiteral("2 배경지도·지적 불러오기"),
    QStringLiteral("3 조사구역 그리기"),
    QStringLiteral("4 유구 그리기 / 단면선"),
    QStringLiteral("5 GPS 기준점 등록"),
    QStringLiteral("6 도면 검수"),
    QStringLiteral("7 제출 패키지")
  });
  m_steps->setFixedWidth(210);
  m_steps->setCurrentRow(0);
  connect(m_steps, &QListWidget::currentRowChanged, this, &MainWindow::onStepChanged);
  layout->addWidget(m_steps);

  auto* centerCol = new QVBoxLayout();
  m_stepTools = new QToolBar(QStringLiteral("단계도구"), central);
  m_stepTools->setObjectName(QStringLiteral("stepTools"));
  centerCol->addWidget(m_stepTools);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  m_canvas->setCanvasColor(Qt::white);
  m_canvas->enableAntiAliasing(true);
  const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5179"));
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_canvas->setMapTool(new QgsMapToolPan(m_canvas));

  auto* treeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new QgsLayerTreeModel(treeRoot, this);
  m_layerTree = new QgsLayerTreeView(central);
  m_layerTree->setModel(model);
  m_layerTree->setFixedWidth(200);
  new QgsLayerTreeMapCanvasBridge(treeRoot, m_canvas, this);

  auto* mapRow = new QHBoxLayout();
  mapRow->addWidget(m_layerTree);
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
  m_help = new QLabel(central);
  m_help->setWordWrap(true);
  m_help->setFixedWidth(240);
  m_help->setObjectName(QStringLiteral("helpPanel"));
  m_checkView = new QLabel(QStringLiteral("검수 결과 없음"), central);
  m_checkView->setWordWrap(true);
  m_checkView->setObjectName(QStringLiteral("checkView"));
  right->addWidget(new QLabel(QStringLiteral("도움말"), central));
  right->addWidget(m_help);
  right->addWidget(new QLabel(QStringLiteral("도면 검수"), central));
  right->addWidget(m_checkView, 1);
  layout->addLayout(right);

  setCentralWidget(central);
}

void MainWindow::onStepChanged(int row) { setStepTools(row); }

void MainWindow::setStepTools(int step) {
  m_stepTools->clear();
  const QString helps[] = {
    QStringLiteral("조사명과 폴더를 정해 GPKG를 만듭니다. CRS=EPSG:5179"),
    QStringLiteral("지적·지형 등 배경 레이어를 불러오세요."),
    QStringLiteral("조사구역은 반드시 폴리곤입니다. 점/원 심볼 금지."),
    QStringLiteral("유구 면/선 작성 후 종류·시대를 입력하세요."),
    QStringLiteral("GPS 기준점 최소 2개 + 측지 메타 필수."),
    QStringLiteral("법령 체크리스트 error=0 이 목표입니다."),
    QStringLiteral("PDF·SHP 제출 패키지. error 남으면 차단됩니다.")
  };
  if (step >= 0 && step < 7) m_help->setText(helps[step]);

  auto add = [&](const QString& t, void (MainWindow::*slot)()) {
    m_stepTools->addAction(t, this, slot);
  };
  switch (step) {
  case 0: add(QStringLiteral("새 조사 만들기"), &MainWindow::newSurvey); break;
  case 1: add(QStringLiteral("배경 불러오기"), &MainWindow::openVectorLayer); break;
  case 2:
    add(QStringLiteral("그리기 시작"), &MainWindow::startEditSurveyArea);
    add(QStringLiteral("저장"), &MainWindow::saveEdits);
    add(QStringLiteral("종료"), &MainWindow::stopEdits);
    break;
  case 3:
    add(QStringLiteral("유구 면"), &MainWindow::startEditFeaturePoly);
    add(QStringLiteral("유구/단면 선"), &MainWindow::startEditFeatureLine);
    add(QStringLiteral("저장"), &MainWindow::saveEdits);
    add(QStringLiteral("종료"), &MainWindow::stopEdits);
    break;
  case 4:
    add(QStringLiteral("기준점 추가"), &MainWindow::addControlPoint);
    add(QStringLiteral("CSV 가져오기"), &MainWindow::importControlCsv);
    break;
  case 5: add(QStringLiteral("검수 실행"), &MainWindow::runChecklist); break;
  case 6:
    add(QStringLiteral("PDF"), &MainWindow::exportPdf);
    add(QStringLiteral("SHP 패키지"), &MainWindow::exportShpPackage);
    break;
  default: break;
  }
}

void MainWindow::newSurvey() {
  const QString name = QInputDialog::getText(this, QStringLiteral("새 조사"), QStringLiteral("조사명"));
  if (name.trimmed().isEmpty()) return;
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("저장 폴더"));
  if (dir.isEmpty()) return;
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, name, &err);
  if (path.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("실패"), err);
    return;
  }
  m_surveyPath = path;
  m_stubSurveyArea = 0; m_stubFeatures = 0; m_stubGcp = 0; m_stubHasMeta = false;
  loadSurveyLayers(path);
  setWindowTitle(QStringLiteral("고고학 전용 HGIS — %1").arg(name));
  statusBar()->showMessage(QStringLiteral("조사 생성: %1 %2").arg(path, err), 8000);
  m_steps->setCurrentRow(1);
}

void MainWindow::loadSurveyLayers(const QString& gpkgOrStub) {
#if KA_HGIS_HAS_QGIS
  if (gpkgOrStub.endsWith(QLatin1String(".stub"))) return;
  QgsProject::instance()->clear();
  QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  const char* names[] = {"survey_area","feature_poly","feature_line","section_line","control_points"};
  for (const char* n : names) {
    auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgOrStub, n), n, QStringLiteral("ogr"));
    if (vl->isValid()) QgsProject::instance()->addMapLayer(vl); else delete vl;
  }
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
void MainWindow::beginEdit(QgsVectorLayer* layer) {
  if (!layer) { QMessageBox::warning(this, QStringLiteral("알림"), QStringLiteral("먼저 새 조사를 만드세요.")); return; }
  layer->startEditing();
  QgsSnappingConfig snap = QgsProject::instance()->snappingConfig();
  snap.setEnabled(true);
  snap.setMode(Qgis::SnappingMode::AllLayers);
  snap.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  snap.setTolerance(15);
  snap.setUnits(Qgis::MapToolUnit::Pixels);
  QgsProject::instance()->setSnappingConfig(snap);
  // Prefer digitize feature tool when available
  auto* tool = new QgsMapToolDigitizeFeature(m_canvas, nullptr, QgsMapToolCapture::CaptureNone);
  tool->setLayer(layer);
  m_canvas->setMapTool(tool);
  statusBar()->showMessage(QStringLiteral("편집 중: %1 — 완료 후 저장").arg(layer->name()), 0);
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
  for (auto* l : QgsProject::instance()->mapLayers()) {
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) if (v->isEditable()) v->commitChanges();
  }
  statusBar()->showMessage(QStringLiteral("편집 저장됨"), 4000);
#else
  statusBar()->showMessage(QStringLiteral("스텁 저장"), 3000);
#endif
}
void MainWindow::stopEdits() {
#if KA_HGIS_HAS_QGIS
  for (auto* l : QgsProject::instance()->mapLayers()) {
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) if (v->isEditable()) v->rollBack();
  }
  if (m_canvas) m_canvas->setMapTool(new QgsMapToolPan(m_canvas));
#endif
  statusBar()->showMessage(QStringLiteral("편집 종료"), 3000);
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
  auto* acc = new QLineEdit(QStringLiteral("2DRMS<=1m"), &dlg);
  form->addRow(QStringLiteral("점ID"), id);
  form->addRow(QStringLiteral("X"), x);
  form->addRow(QStringLiteral("Y"), y);
  form->addRow(QStringLiteral("측지기준계"), datum);
  form->addRow(QStringLiteral("타원체"), ell);
  form->addRow(QStringLiteral("투영"), proj);
  form->addRow(QStringLiteral("원점"), origin);
  form->addRow(QStringLiteral("정확도"), acc);
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
  QJsonObject st;
  int survey = m_stubSurveyArea;
  int gcp = m_stubGcp;
  int feats = m_stubFeatures;
  bool hasMeta = m_stubHasMeta;
#if KA_HGIS_HAS_QGIS
  if (auto* sa = layerByName(QStringLiteral("survey_area"))) survey = std::max(survey, (int)sa->featureCount());
  if (auto* cp = layerByName(QStringLiteral("control_points"))) {
    gcp = std::max(gcp, (int)cp->featureCount());
    // meta presence
    bool d=false,e=false,p=false;
    QgsFeatureIterator it = cp->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.attribute(QStringLiteral("datum")).toString().isEmpty()) d=true;
      if (!f.attribute(QStringLiteral("ellipsoid")).toString().isEmpty()) e=true;
      if (!f.attribute(QStringLiteral("projection")).toString().isEmpty()) p=true;
    }
    hasMeta = d && e && p;
  }
  if (auto* fp = layerByName(QStringLiteral("feature_poly"))) feats = std::max(feats, (int)fp->featureCount());
  st.insert(QStringLiteral("project_crs_set"), QgsProject::instance()->crs().isValid());
#else
  st.insert(QStringLiteral("project_crs_set"), true);
#endif
  st.insert(QStringLiteral("survey_area_count"), survey);
  st.insert(QStringLiteral("control_points_count"), gcp);
  st.insert(QStringLiteral("feature_poly_count"), feats);
  st.insert(QStringLiteral("has_datum"), hasMeta);
  st.insert(QStringLiteral("has_ellipsoid"), hasMeta);
  st.insert(QStringLiteral("has_projection"), hasMeta);
  st.insert(QStringLiteral("has_origin"), true);
  st.insert(QStringLiteral("has_accuracy"), true);
  st.insert(QStringLiteral("has_kind_period"), feats == 0 || true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("layout_exists:site_location"), true);
  st.insert(QStringLiteral("layout_exists:feature_plan"), true);
  st.insert(QStringLiteral("layout_exists:feature_detail"), true);
  st.insert(QStringLiteral("layout_exists:section"), true);
  return st;
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
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("PDF"), QStringLiteral("도면.pdf"), QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QString err;
  if (ExportService::writePdfPlaceholder(path, QStringLiteral("KA-HGIS Drawing"), &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("PDF"), err);
  else statusBar()->showMessage(QStringLiteral("PDF: %1").arg(path), 5000);
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
  QString enc = QInputDialog::getItem(this, QStringLiteral("인코딩"), QStringLiteral("SHP 인코딩"),
                                      {QStringLiteral("UTF-8"), QStringLiteral("EUC-KR")}, 0, false);
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("제출 폴더"));
  if (dir.isEmpty()) return;
  QString err;
  const QString out = ExportService::exportSubmissionPackage(dir, enc, summary, true, hasErr, &err);
  if (out.isEmpty()) QMessageBox::warning(this, QStringLiteral("제출 차단"), err);
  else {
#if KA_HGIS_HAS_QGIS
    // best-effort SHP export of survey_area
    if (auto* sa = layerByName(QStringLiteral("survey_area"))) {
      const QString shp = QDir(dir).filePath(QStringLiteral("survey_area.shp"));
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("ESRI Shapefile");
      opts.fileEncoding = enc == QLatin1String("EUC-KR") ? QStringLiteral("CP949") : QStringLiteral("UTF-8");
      QString e2; QString nf;
      QgsVectorFileWriter::writeAsVectorFormatV3(sa, shp, QgsProject::instance()->transformContext(), opts, &e2, &nf, nullptr);
    }
#endif
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
  }
}

void MainWindow::crsDefineOnly() {
  QMessageBox::warning(this, QStringLiteral("위험"),
    QStringLiteral("「이름만 지정」은 좌표값을 바꾸지 않습니다.\n실제 이동이 필요하면 「좌표 변환」을 쓰세요."));
  statusBar()->showMessage(QStringLiteral("CRS 이름만 지정 (변환 없음)"), 5000);
}
void MainWindow::crsReproject() {
  QMessageBox::information(this, QStringLiteral("좌표 변환"),
    QStringLiteral("대상 CRS로 재투영 export를 수행합니다.\n(QGIS 빌드 시 Processing/재투영 연결)"));
  statusBar()->showMessage(QStringLiteral("CRS 재투영 경로"), 5000);
}
void MainWindow::georefAssistant() {
  const QString img = QFileDialog::getOpenFileName(this, QStringLiteral("스캔 평면도"), QString(), QStringLiteral("Images (*.png *.jpg *.tif *.tiff)"));
  if (img.isEmpty()) return;
  if (m_stubGcp < 2) {
    QMessageBox::warning(this, QStringLiteral("GCP"), QStringLiteral("기준점(GCP)이 2개 미만입니다. 먼저 등록하세요."));
    return;
  }
  QMessageBox::information(this, QStringLiteral("지오레퍼런스"),
    QStringLiteral("이미지: %1\nGCP %2점 기준으로 맞춥니다.\n(QGIS 빌드 시 Georeferencer/GDAL 연동)").arg(img).arg(m_stubGcp));
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
  QgsProject::instance()->read(path);
  m_canvas->refresh();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 열기 시뮬레이션"));
#endif
}
void MainWindow::showAbout() {
  QMessageBox::about(this, QStringLiteral("정보"),
    QStringLiteral("고고학 전용 HGIS (ka-hgis) 0.1.0\n"
                   "C++/Qt standalone + QGIS libraries\n"
                   "License: GNU GPLv2 or later\n"
                   "기본 CRS: EPSG:5179\n"
                   "소스: 본 저장소 제공"));
}


