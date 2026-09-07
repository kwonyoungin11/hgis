#include "AndongWindow.h"

#include "core/AndongPack.h"
#include "core/LayerOps.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QtGlobal>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsfeatureid.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaptoolpan.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

AndongWindow::AndongWindow(QWidget* parent) : QMainWindow(parent) {
  setObjectName(QStringLiteral("andongWindow"));
  setWindowTitle(QStringLiteral("안동시 문화유산 지도"));
  resize(1400, 860);
  setupUi();
  applyTheme();
  loadPack();
}

AndongWindow::~AndongWindow() = default;

void AndongWindow::setupUi() {
  auto* root = new QWidget(this);
  root->setObjectName(QStringLiteral("andongRoot"));
  auto* rootLay = new QVBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  auto* top = new QWidget(root);
  top->setObjectName(QStringLiteral("andongTop"));
  auto* topLay = new QHBoxLayout(top);
  topLay->setContentsMargins(12, 8, 12, 8);
  auto* brand = new QWidget(top);
  brand->setObjectName(QStringLiteral("andongBrand"));
  auto* brandLay = new QHBoxLayout(brand);
  brandLay->setContentsMargins(0, 0, 0, 0);
  brandLay->setSpacing(8);
  auto* mark = new QLabel(brand);
  mark->setObjectName(QStringLiteral("andongMark"));
  mark->setFixedSize(48, 48);
  mark->setAlignment(Qt::AlignCenter);
  mark->setScaledContents(true);
  QPixmap pm(AndongPack::cityMarkPath());
  if (pm.isNull())
    pm = QPixmap(QStringLiteral("D:/hgis/data/andong/andong_mark.png"));
  if (!pm.isNull())
    mark->setPixmap(pm);
  auto* city = new QLabel(QStringLiteral("안동시"), brand);
  city->setObjectName(QStringLiteral("andongCity"));
  brandLay->addWidget(mark);
  brandLay->addWidget(city);
  auto* titles = new QWidget(top);
  auto* tLay = new QVBoxLayout(titles);
  tLay->setContentsMargins(0, 0, 0, 0);
  tLay->setSpacing(0);
  auto* title = new QLabel(QStringLiteral("안동시 문화유산 지도"), titles);
  title->setObjectName(QStringLiteral("andongTitle"));
  auto* sub = new QLabel(QStringLiteral("오프라인  ·  안동시만 위성  ·  휠 확대  ·  끌어서 이동"), titles);
  sub->setObjectName(QStringLiteral("andongSub"));
  tLay->addWidget(title);
  tLay->addWidget(sub);
  m_search = new QLineEdit(top);
  m_search->setObjectName(QStringLiteral("andongSearch"));
  m_search->setPlaceholderText(QStringLiteral("유적명·유산명 찾기"));
  auto* findBtn = new QPushButton(QStringLiteral("찾기"), top);
  auto* homeBtn = new QPushButton(QStringLiteral("안동시 전체"), top);
  topLay->addWidget(brand);
  topLay->addWidget(titles, 1);
  topLay->addWidget(m_search, 1);
  topLay->addWidget(findBtn);
  topLay->addWidget(homeBtn);

  auto* split = new QSplitter(Qt::Horizontal, root);
  auto* scroll = new QScrollArea(split);
  scroll->setWidgetResizable(true);
  scroll->setMinimumWidth(280);
  scroll->setMaximumWidth(420);
  m_panel = new QWidget(scroll);
  m_panelLay = new QVBoxLayout(m_panel);
  m_panelLay->setContentsMargins(10, 8, 10, 8);
  m_panelLay->setSpacing(6);
  scroll->setWidget(m_panel);

  m_canvas = new QgsMapCanvas(split);
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  m_canvas->setCanvasColor(Qt::white);
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  m_canvas->setParallelRenderingEnabled(true);
  m_canvas->setPreviewJobsEnabled(true);
  m_pan = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_pan);
  split->addWidget(scroll);
  split->addWidget(m_canvas);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);

  m_status = new QLabel(root);
  m_status->setObjectName(QStringLiteral("andongStatus"));
  m_status->setText(QStringLiteral("지도를 불러오는 중…"));

  rootLay->addWidget(top);
  rootLay->addWidget(split, 1);
  rootLay->addWidget(m_status);
  setCentralWidget(root);

  connect(findBtn, &QPushButton::clicked, this, &AndongWindow::findName);
  connect(m_search, &QLineEdit::returnPressed, this, &AndongWindow::findName);
  connect(homeBtn, &QPushButton::clicked, this, &AndongWindow::zoomCity);
}

void AndongWindow::applyTheme() {
  const QStringList cands = {
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/theme/andong.qss")),
      QStringLiteral("D:/hgis/data/theme/andong.qss"),
      QDir::current().filePath(QStringLiteral("data/theme/andong.qss")),
  };
  for (const QString& p : cands) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;
    setStyleSheet(QString::fromUtf8(f.readAll()));
    return;
  }
}

void AndongWindow::addGroup(const QString& title) {
  auto* box = new QGroupBox(title, m_panel);
  auto* lay = new QVBoxLayout(box);
  lay->setContentsMargins(8, 14, 8, 6);
  lay->setSpacing(4);
  m_panelLay->addWidget(box);
}

void AndongWindow::addLayerRow(const QString& title, QgsMapLayer* layer, bool hasLabels) {
  if (!layer)
    return;
  auto* box = qobject_cast<QGroupBox*>(m_panelLay->itemAt(m_panelLay->count() - 1)->widget());
  if (!box)
    return;
  auto* row = new QWidget(box);
  auto* lay = new QHBoxLayout(row);
  lay->setContentsMargins(0, 0, 0, 0);
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
    const AndongPack::PresentationStyle st = AndongPack::presentationStyleFor(title, vl);
    auto* chip = new QLabel(row);
    chip->setObjectName(QStringLiteral("andongLegendChip"));
    chip->setFixedSize(18, 18);
    chip->setToolTip(QStringLiteral("범례 · %1").arg(title));
    const QString fill = st.noFill ? QStringLiteral("transparent") : st.fill.name(QColor::HexArgb);
    const QString dash = st.dashed ? QStringLiteral("dashed") : QStringLiteral("solid");
    chip->setStyleSheet(QStringLiteral("background:%1; border:2px %2 %3; border-radius:3px;")
                            .arg(fill, dash, st.stroke.name(QColor::HexRgb)));
    lay->addWidget(chip);
  }
  auto* vis = new QCheckBox(title, row);
  vis->setChecked(true);
  vis->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  lay->addWidget(vis, 1);
  QCheckBox* lab = nullptr;
  if (hasLabels) {
    lab = new QCheckBox(QStringLiteral("이름"), row);
    lab->setChecked(true);
    lab->setToolTip(QStringLiteral("명칭만 끄고 켭니다"));
    lay->addWidget(lab);
  }
  box->layout()->addWidget(row);
  Row r{layer, vis, lab};
  m_rows.push_back(r);
  connect(vis, &QCheckBox::toggled, this, &AndongWindow::rebuildCanvas);
  if (lab) {
    connect(lab, &QCheckBox::toggled, this, [layer](bool on) {
      LayerOps::setLabelsVisible(layer, on);
    });
  }
}

void AndongWindow::loadPack() {
  auto* project = QgsProject::instance();
  project->clear();
  project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  LayerOps::ensureOtfEnabled(project, m_canvas, QStringLiteral("EPSG:5179"));

  auto* legendHint = new QLabel(QStringLiteral("범례 · 이름 왼쪽 색칸이 지도 색입니다"), m_panel);
  legendHint->setObjectName(QStringLiteral("andongLegendHint"));
  legendHint->setWordWrap(true);
  m_panelLay->addWidget(legendHint);

  const QString assets = AndongPack::resolveAssetRoot();
  const QString data = AndongPack::resolveDataRoot();
  m_mask = AndongPack::loadCityMask(project, AndongPack::cityBoundaryPath(assets));

  addGroup(QStringLiteral("배경"));
  auto* bg = qobject_cast<QGroupBox*>(m_panelLay->itemAt(m_panelLay->count() - 1)->widget());
  auto* bgLay = qobject_cast<QVBoxLayout*>(bg ? bg->layout() : nullptr);
  m_satBox = new QCheckBox(QStringLiteral("위성 (안동시만)"), bg);
  m_satBox->setObjectName(QStringLiteral("andongSatBox"));
  m_cadBox = new QCheckBox(QStringLiteral("지적"), bg);
  m_jibunBox = new QCheckBox(QStringLiteral("지번"), bg);
  m_satBox->setChecked(true);
  m_cadBox->setChecked(true);
  m_jibunBox->setChecked(true);
  auto* opRow = new QWidget(bg);
  opRow->setObjectName(QStringLiteral("andongSatOpacityRow"));
  auto* opLay = new QHBoxLayout(opRow);
  opLay->setContentsMargins(0, 0, 0, 4);
  opLay->setSpacing(8);
  auto* opLab = new QLabel(QStringLiteral("투명도"), opRow);
  opLab->setObjectName(QStringLiteral("andongSatOpacityLabel"));
  m_satOpacity = new QSlider(Qt::Horizontal, opRow);
  m_satOpacity->setObjectName(QStringLiteral("andongSatOpacity"));
  m_satOpacity->setRange(0, 100);
  m_satOpacity->setValue(100);
  m_satOpacity->setPageStep(10);
  m_satOpacity->setSingleStep(5);
  m_satOpacityValue = new QLabel(QStringLiteral("100%"), opRow);
  m_satOpacityValue->setObjectName(QStringLiteral("andongSatOpacityValue"));
  m_satOpacityValue->setMinimumWidth(36);
  m_satOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  opLay->addWidget(opLab);
  opLay->addWidget(m_satOpacity, 1);
  opLay->addWidget(m_satOpacityValue);
  if (bgLay) {
    bgLay->addWidget(m_satBox);
    bgLay->addWidget(opRow);
    bgLay->addWidget(m_cadBox);
    bgLay->addWidget(m_jibunBox);
  }
  connect(m_satBox, &QCheckBox::toggled, this, &AndongWindow::rebuildCanvas);
  connect(m_satOpacity, &QSlider::valueChanged, this, &AndongWindow::onSatOpacity);
  connect(m_cadBox, &QCheckBox::toggled, this, &AndongWindow::rebuildCanvas);
  connect(m_jibunBox, &QCheckBox::toggled, this, &AndongWindow::rebuildCanvas);

  if (QgsRasterLayer* sat =
          AndongPack::loadLocalRaster(AndongPack::satelliteRasterPath(assets), QStringLiteral("위성"))) {
    project->addMapLayer(sat, false);
    m_satellite = sat;
  }
  if (QgsRasterLayer* cad =
          AndongPack::loadLocalRaster(AndongPack::cadastralRasterPath(assets), QStringLiteral("지적"))) {
    project->addMapLayer(cad, false);
    m_cadastral = cad;
  }
  if (QgsRasterLayer* jb =
          AndongPack::loadLocalRaster(AndongPack::jibunRasterPath(assets), QStringLiteral("지번"))) {
    project->addMapLayer(jb, false);
    m_jibun = jb;
  }

  const QVector<AndongPack::ShpLayer> layers = AndongPack::catalog(data);
  QString lastGroup;
  int loaded = 0;
  QgsVectorLayer* cadVec = nullptr;
  for (const AndongPack::ShpLayer& spec : layers) {
    QgsVectorLayer* vl = AndongPack::loadShapefile(spec.path, spec.title);
    if (!vl)
      continue;
    if (vl->featureCount() <= 0) {
      delete vl;
      continue;
    }
    AndongPack::applyPresentationStyle(vl, spec.title);
    const bool labels = AndongPack::applyLabels(vl);
    if (AndongPack::isCadastralLayer(spec.title, vl)) {
      cadVec = vl;
      if (!m_jibun)
        m_jibun = vl;
      if (!m_cadastral)
        m_cadastral = vl;
    }
    project->addMapLayer(vl, false);
    if (spec.group != lastGroup) {
      addGroup(spec.group);
      lastGroup = spec.group;
    }
    addLayerRow(spec.title, vl, labels);
    ++loaded;
  }

  if (QgsVectorLayer* emd = AndongPack::loadAndongEmd(QString())) {
    project->addMapLayer(emd, false);
    addGroup(QStringLiteral("행정"));
    addLayerRow(QStringLiteral("읍면동"), emd, true);
    ++loaded;
  }

  if (!m_satellite) {
    m_satBox->setEnabled(false);
    m_satBox->setChecked(false);
    m_satBox->setToolTip(QStringLiteral("satellite.mbtiles를 data/andong에 두면 켜집니다"));
    if (m_satOpacity)
      m_satOpacity->setEnabled(false);
    if (m_satOpacityValue)
      m_satOpacityValue->setEnabled(false);
  } else {
    onSatOpacity(m_satOpacity ? m_satOpacity->value() : 100);
  }
  if (!m_cadastral) {
    m_cadBox->setEnabled(false);
    m_cadBox->setChecked(false);
    m_cadBox->setToolTip(QStringLiteral("지적 SHP 또는 cadastral.mbtiles를 넣으면 켜집니다"));
  }
  if (!m_jibun) {
    m_jibunBox->setEnabled(false);
    m_jibunBox->setChecked(false);
    m_jibunBox->setToolTip(QStringLiteral("지번 필드가 있는 지적 SHP가 있으면 글자만 끕니다"));
  } else if (cadVec && m_jibun == cadVec) {
    disconnect(m_jibunBox, nullptr, this, nullptr);
    connect(m_jibunBox, &QCheckBox::toggled, this, [cadVec](bool on) {
      LayerOps::setLabelsVisible(cadVec, on);
    });
  }

  m_panelLay->addStretch(1);
  rebuildCanvas();
  zoomCity();

  QStringList bits;
  bits << QStringLiteral("SHP %1장").arg(loaded);
  bits << (m_satellite ? QStringLiteral("위성 있음") : QStringLiteral("위성 팩 없음(여백+유적만)"));
  bits << (m_cadastral ? QStringLiteral("지적 있음") : QStringLiteral("지적 파일 없음"));
  bits << QStringLiteral("휠=확대  끌기=이동  이름 칸=명칭만");
  m_status->setText(bits.join(QStringLiteral("  ·  ")));
}

void AndongWindow::rebuildCanvas() {
  auto* project = QgsProject::instance();
  QList<QgsMapLayer*> paint;
  for (const Row& r : m_rows) {
    if (!r.layer || !r.vis || !r.vis->isChecked())
      continue;
    paint << r.layer;
    if (QgsLayerTree* root = project->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = root->findLayer(r.layer->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  if (m_mask)
    paint << m_mask;
  if (m_jibun && m_jibunBox && m_jibunBox->isChecked() && m_jibun != m_cadastral &&
      !qobject_cast<QgsVectorLayer*>(m_jibun))
    paint << m_jibun;
  if (m_cadastral && m_cadBox && m_cadBox->isChecked() &&
      !qobject_cast<QgsVectorLayer*>(m_cadastral))
    paint << m_cadastral;
  if (m_satellite && m_satBox && m_satBox->isChecked())
    paint << m_satellite;
  m_canvas->setLayers(paint);
  m_canvas->refresh();
}

void AndongWindow::onSatOpacity(int percent) {
  const int p = qBound(0, percent, 100);
  if (m_satOpacityValue)
    m_satOpacityValue->setText(QStringLiteral("%1%").arg(p));
  if (!m_satellite)
    return;
  AndongPack::applyRasterOpacityPercent(m_satellite, p);
  m_canvas->refresh();
}

void AndongWindow::zoomCity() {
  QgsRectangle ext = AndongPack::cityExtent5179(m_mask);
  if (ext.isEmpty())
    return;
  m_canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  m_canvas->setExtent(ext);
  m_canvas->zoomToFeatureExtent(ext);
  m_canvas->refresh();
}

void AndongWindow::findName() {
  const QString q = m_search->text().trimmed();
  if (q.isEmpty())
    return;
  const QString needle = q.toLower();
  for (const Row& r : m_rows) {
    auto* vl = qobject_cast<QgsVectorLayer*>(r.layer);
    if (!vl || !r.vis || !r.vis->isChecked())
      continue;
    const QString field = AndongPack::preferredLabelField(vl);
    if (field.isEmpty())
      continue;
    QgsFeatureIterator it = vl->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QString v = f.attribute(field).toString();
      if (v.contains(q, Qt::CaseInsensitive) || v.toLower().contains(needle)) {
        const QgsRectangle e = f.geometry().boundingBox();
        if (!e.isEmpty()) {
          QgsRectangle zoom = e.buffered(e.width() * 0.8 + 80.0);
          m_canvas->zoomToFeatureExtent(zoom);
          m_canvas->flashFeatureIds(vl, QgsFeatureIds{f.id()});
          m_status->setText(QStringLiteral("%1  →  %2").arg(vl->name(), v));
          return;
        }
      }
    }
  }
  m_status->setText(QStringLiteral("찾는 이름이 없습니다: %1").arg(q));
}
