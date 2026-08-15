#include "KaDrawingStudio.h"
#include "KaTheme.h"
#include "KaIcons.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"

#include <algorithm>
#include <cmath>

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QAction>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QIODevice>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QUndoStack>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QWheelEvent>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsgui.h>
#include <qgslayout.h>
#include <qgslayoutexporter.h>
#include <qgslayoutguiutils.h>
#include <qgslayoutitem.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutview.h>
#include <qgslayoutviewmouseevent.h>
#include <qgslayoutviewtool.h>
#include <qgslayoutviewtoolpan.h>
#include <qgslayoutviewtoolselect.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmasterlayoutinterface.h>
#include <qgsprintlayout.h>
#include <qgslayoutundostack.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

namespace {
constexpr const char* kSheetName = "user_sheet";
constexpr const char* kIdMap = "ka_map";
constexpr const char* kIdLegend = "ka_legend";
constexpr const char* kIdNorth = "ka_north";
constexpr const char* kIdScaleBar = "ka_scalebar";
constexpr const char* kIdScale = "ka_scale";
constexpr const char* kIdCrs = "ka_crs";

bool isLiveBasemapLayer(QgsMapLayer* layer) {
  if (!layer) return false;
  const QString p = layer->providerType().toLower();
  return p == QLatin1String("wms") || p == QLatin1String("xyz");
}

QgsCoordinateReferenceSystem studioMapCrs(QgsMapCanvas* canvas, QgsProject* project) {
  if (canvas && canvas->mapSettings().destinationCrs().isValid())
    return canvas->mapSettings().destinationCrs();
  if (project && project->crs().isValid())
    return project->crs();
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
}

QgsRectangle studioMapExtent(QgsMapCanvas* canvas, const QgsCoordinateReferenceSystem& crs) {
  QgsRectangle ext;
  if (canvas)
    ext = canvas->extent();
  if (ext.isNull() || !ext.isFinite() || ext.width() <= 0.0 || ext.height() <= 0.0)
    ext = LayerOps::koreaExtentForCrs(crs.isValid() ? crs.authid() : QStringLiteral("EPSG:5186"));
  return ext;
}

int northKindFromRel(const QString& rel) {
  if (rel.contains(QLatin1String("WindRose"), Qt::CaseInsensitive)
      || rel.contains(QLatin1String("rose"), Qt::CaseInsensitive))
    return 3;
  if (rel.contains(QLatin1String("compass"), Qt::CaseInsensitive)
      || rel.contains(QLatin1String("NorthArrow_04"))
      || rel.contains(QLatin1String("NorthArrow_03")))
    return 2;
  if (rel.contains(QLatin1String("NorthArrow"))
      || rel.contains(QLatin1String("arrow"), Qt::CaseInsensitive))
    return 1;
  return 0;
}

void paintNorthMark(QPainter& p, int kind) {
  const QColor ink(17, 24, 39);
  if (kind == 0) {
    QPolygonF tri;
    tri << QPointF(36, 10) << QPointF(48, 38) << QPointF(36, 32) << QPointF(24, 38);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(tri);
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 16, QFont::Bold));
    p.drawText(QRectF(8, 40, 56, 26), Qt::AlignCenter, QStringLiteral("N"));
  } else if (kind == 1) {
    p.setPen(QPen(ink, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(36, 56), QPointF(36, 16));
    QPolygonF head;
    head << QPointF(36, 10) << QPointF(46, 28) << QPointF(36, 22) << QPointF(26, 28);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(head);
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    p.drawText(QRectF(44, 8, 22, 16), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("N"));
  } else if (kind == 2) {
    p.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(36, 40), 20, 20);
    p.drawEllipse(QPointF(36, 40), 12, 12);
    for (int i = 0; i < 8; ++i) {
      const double a = static_cast<double>(i) * 3.141592653589793 / 4.0;
      p.drawLine(QPointF(36.0 + 12.0 * std::cos(a), 40.0 + 12.0 * std::sin(a)),
                 QPointF(36.0 + 20.0 * std::cos(a), 40.0 + 20.0 * std::sin(a)));
    }
    QPolygonF n;
    n << QPointF(36, 8) << QPointF(42, 22) << QPointF(36, 18) << QPointF(30, 22);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPolygon(n);
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 11, QFont::Bold));
    p.drawText(QRectF(24, 26, 24, 16), Qt::AlignCenter, QStringLiteral("N"));
  } else {
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    QPolygonF major;
    major << QPointF(36, 8) << QPointF(40, 36) << QPointF(36, 64) << QPointF(32, 36);
    p.drawPolygon(major);
    QPolygonF minor;
    minor << QPointF(10, 38) << QPointF(36, 42) << QPointF(62, 38) << QPointF(36, 34);
    p.setBrush(QColor(55, 65, 81));
    p.drawPolygon(minor);
    p.setBrush(QColor(17, 24, 39));
    QPolygonF diag;
    diag << QPointF(18, 18) << QPointF(36, 40) << QPointF(54, 18) << QPointF(36, 34);
    p.drawPolygon(diag);
    p.setPen(QPen(Qt::white, 1));
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    p.drawText(QRectF(28, 10, 16, 12), Qt::AlignCenter, QStringLiteral("N"));
  }
}

QString writeNorthPng(int kind) {
  const int s = 512;
  QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(static_cast<double>(s) / 72.0, static_cast<double>(s) / 72.0);
  paintNorthMark(p, kind);
  p.end();
  const QString path = QDir::temp().filePath(QStringLiteral("ka-hgis-north-%1.png").arg(kind));
  if (!img.save(path, "PNG"))
    return {};
  return QFile::exists(path) ? path : QString();
}

QString koreanCrsLabel(const QgsCoordinateReferenceSystem& crs) {
  const QString auth = crs.isValid() ? crs.authid() : QString();
  if (auth == QLatin1String("EPSG:5186"))
    return QStringLiteral("좌표계 중부원점 EPSG:5186");
  if (auth == QLatin1String("EPSG:5187"))
    return QStringLiteral("좌표계 동부원점 EPSG:5187");
  if (auth == QLatin1String("EPSG:5179"))
    return QStringLiteral("좌표계 통일원점 EPSG:5179");
  if (auth == QLatin1String("EPSG:5185"))
    return QStringLiteral("좌표계 서부원점 EPSG:5185");
  if (!auth.isEmpty())
    return QStringLiteral("좌표계 %1").arg(auth);
  return QStringLiteral("좌표계 미지정");
}

QgsLayoutItem* findItemById(QgsLayout* ly, const char* id) {
  if (!ly) return nullptr;
  for (QGraphicsItem* gi : ly->items()) {
    if (auto* it = dynamic_cast<QgsLayoutItem*>(gi)) {
      if (it->id() == QLatin1String(id))
        return it;
    }
  }
  return nullptr;
}

QFrame* makeDrawerCard(QWidget* parent) {
  auto* f = new QFrame(parent);
  f->setObjectName(QStringLiteral("drawerCard"));
  return f;
}

QToolButton* makeSampleButton(QWidget* parent, const QIcon& icon, const QString& text,
                              const QString& tip, Qt::ToolButtonStyle style) {
  auto* b = new QToolButton(parent);
  b->setIcon(icon);
  b->setIconSize(QSize(40, 40));
  b->setText(text);
  b->setToolButtonStyle(style);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return b;
}

QIcon scaleBarPreviewIcon(const char* style) {
  QPixmap pm(72, 40);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(214, 211, 209), 1));
  p.setBrush(QColor(250, 250, 249));
  p.drawRoundedRect(QRectF(1, 4, 70, 32), 5, 5);
  const QString s = QString::fromUtf8(style);
  if (s == QLatin1String("Line Ticks Up")) {
    p.setPen(QPen(QColor(68, 64, 60), 1.6));
    p.drawLine(QPointF(10, 26), QPointF(62, 26));
    for (int i = 0; i < 5; ++i) {
      const double x = 10.0 + i * 13.0;
      p.drawLine(QPointF(x, 26), QPointF(x, 14));
    }
  } else if (s == QLatin1String("Single Box")) {
    p.setPen(QPen(QColor(68, 64, 60), 1));
    for (int i = 0; i < 4; ++i) {
      p.setBrush(i % 2 == 0 ? QColor(68, 64, 60) : QColor(250, 250, 249));
      p.drawRect(QRectF(10 + i * 13, 16, 13, 10));
    }
  } else {
    p.setPen(QPen(QColor(68, 64, 60), 1));
    for (int row = 0; row < 2; ++row) {
      for (int i = 0; i < 4; ++i) {
        const bool dark = ((i + row) % 2) == 0;
        p.setBrush(dark ? QColor(68, 64, 60) : QColor(250, 250, 249));
        p.drawRect(QRectF(10 + i * 13, 12 + row * 8, 13, 8));
      }
    }
  }
  return QIcon(pm);
}

QIcon northPreviewIcon(int kind) {
  QPixmap pm(72, 72);
  pm.fill(QColor(255, 255, 255));
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(QColor(17, 24, 39), 1));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(1, 1, 70, 70), 6, 6);
  paintNorthMark(p, kind);
  return QIcon(pm);
}

QToolButton* makeRailTile(QWidget* parent, const QIcon& icon, const QString& text, const QSize& iconSize) {
  auto* b = new QToolButton(parent);
  b->setIcon(icon);
  b->setIconSize(iconSize);
  b->setText(text);
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setToolTip(text);
  b->setProperty("class", QStringLiteral("sampleTile"));
  return b;
}
}

class KaLayoutMapDrawTool : public QgsLayoutViewTool {
  Q_OBJECT
public:
  explicit KaLayoutMapDrawTool(QgsLayoutView* view)
      : QgsLayoutViewTool(view, QStringLiteral("범위 그리기")) {
    setCursor(Qt::CrossCursor);
  }

  ~KaLayoutMapDrawTool() override { clearBand(); }

  void layoutPressEvent(QgsLayoutViewMouseEvent* event) override {
    if (!event || event->button() != Qt::LeftButton) return;
    QgsLayout* ly = view() ? view()->currentLayout() : nullptr;
    if (!ly) return;
    mStart = event->layoutPoint();
    mDrawing = true;
    clearBand();
    mBand = new QGraphicsRectItem();
    mBand->setPen(QPen(QColor(227, 22, 22, 200), 0));
    mBand->setBrush(QColor(227, 22, 22, 30));
    mBand->setZValue(QgsLayout::ZViewTool);
    mBand->setRect(QRectF(mStart, QSizeF(0.1, 0.1)));
    ly->addItem(mBand);
    event->accept();
  }

  void layoutMoveEvent(QgsLayoutViewMouseEvent* event) override {
    if (!mDrawing || !mBand || !event) return;
    mBand->setRect(QRectF(mStart, event->layoutPoint()).normalized());
    event->accept();
  }

  void layoutReleaseEvent(QgsLayoutViewMouseEvent* event) override {
    if (!mDrawing || !event) return;
    mDrawing = false;
    QRectF r = QRectF(mStart, event->layoutPoint()).normalized();
    if (mBand)
      r = mBand->rect().normalized();
    clearBand();
    if (r.width() >= 8.0 && r.height() >= 8.0)
      emit rectDrawn(r);
    event->accept();
  }

  void deactivate() override {
    mDrawing = false;
    clearBand();
    QgsLayoutViewTool::deactivate();
  }

signals:
  void rectDrawn(const QRectF& layoutRect);

private:
  void clearBand() {
    if (!mBand) return;
    if (QGraphicsScene* sc = mBand->scene())
      sc->removeItem(mBand);
    delete mBand;
    mBand = nullptr;
  }

  QPointF mStart;
  bool mDrawing = false;
  QGraphicsRectItem* mBand = nullptr;
};

class KaLayoutMapAdjustTool : public QgsLayoutViewTool {
  Q_OBJECT
signals:
  void mapViewChanged();
public:
  explicit KaLayoutMapAdjustTool(QgsLayoutView* view)
      : QgsLayoutViewTool(view, QStringLiteral("지도 조정")) {
    setCursor(Qt::OpenHandCursor);
    mZoomTimer.setSingleShot(true);
    mZoomTimer.setInterval(90);
    connect(&mZoomTimer, &QTimer::timeout, this, &KaLayoutMapAdjustTool::flushZoom);
  }

  void layoutPressEvent(QgsLayoutViewMouseEvent* event) override {
    if (!event || event->button() != Qt::LeftButton) {
      if (event) event->ignore();
      return;
    }
    auto* map = mapAt(event->pos());
    if (!map) {
      event->ignore();
      return;
    }
    flushZoom();
    mMoveItem = map;
    mMoveStart = event->layoutPoint();
    mMoving = true;
    setCursor(Qt::ClosedHandCursor);
    event->accept();
  }

  void layoutMoveEvent(QgsLayoutViewMouseEvent* event) override {
    if (!mMoving || !mMoveItem || !event) {
      if (event) event->ignore();
      return;
    }
    mMoveItem->setMoveContentPreviewOffset(event->layoutPoint().x() - mMoveStart.x(),
                                           event->layoutPoint().y() - mMoveStart.y());
    mMoveItem->update();
    event->accept();
  }

  void layoutReleaseEvent(QgsLayoutViewMouseEvent* event) override {
    if (!event || event->button() != Qt::LeftButton || !mMoving || !mMoveItem) {
      if (event) event->ignore();
      return;
    }
    mMoveItem->setMoveContentPreviewOffset(0, 0);
    const double dx = event->layoutPoint().x() - mMoveStart.x();
    const double dy = event->layoutPoint().y() - mMoveStart.y();
    if (std::abs(dx) > 0.2 || std::abs(dy) > 0.2)
      mMoveItem->moveContent(-dx, -dy);
    mMoveItem = nullptr;
    mMoving = false;
    setCursor(Qt::OpenHandCursor);
    emit mapViewChanged();
    event->accept();
  }

  void wheelEvent(QWheelEvent* event) override {
    if (!event) return;
    event->accept();
    auto* map = mapAt(event->position().toPoint());
    if (!map) return;

    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    const bool zoomIn = delta > 0;
    double step = 1.0 + (0.35 * std::min(1.0, std::fabs(static_cast<double>(delta)) / 120.0));
    if (event->modifiers() & Qt::ControlModifier)
      step = 1.0 + (step - 1.0) * 0.35;
    const double factor = zoomIn ? step : (1.0 / step);

    const QPointF scenePt = view() ? view()->mapToScene(event->position().toPoint()) : QPointF();
    mPendingMap = map;
    mPendingPoint = map->mapFromScene(scenePt);
    mPendingFactor *= factor;
    if (!mZoomTimer.isActive()) {
      flushZoom();
      mZoomTimer.start();
    }
  }

  void deactivate() override {
    flushZoom();
    if (mMoveItem)
      mMoveItem->setMoveContentPreviewOffset(0, 0);
    mMoving = false;
    mMoveItem = nullptr;
    QgsLayoutViewTool::deactivate();
  }

private slots:
  void flushZoom() {
    if (!mPendingMap || std::fabs(mPendingFactor - 1.0) < 1e-4) {
      mPendingFactor = 1.0;
      return;
    }
    mPendingMap->zoomContent(mPendingFactor, mPendingPoint);
    mPendingFactor = 1.0;
    emit mapViewChanged();
  }

private:
  QgsLayoutItemMap* mapAt(const QPoint& viewPos) const {
    if (!view() || !layout()) return nullptr;
    const QPointF scenePt = view()->mapToScene(viewPos);
    if (auto* hit = dynamic_cast<QgsLayoutItemMap*>(layout()->layoutItemAt(scenePt, true)))
      return hit;
    QList<QgsLayoutItemMap*> maps;
    layout()->layoutItems(maps);
    return maps.isEmpty() ? nullptr : maps.first();
  }

  QTimer mZoomTimer;
  QPointer<QgsLayoutItemMap> mPendingMap;
  QPointF mPendingPoint;
  double mPendingFactor = 1.0;
  QPointer<QgsLayoutItem> mMoveItem;
  QPointF mMoveStart;
  bool mMoving = false;
};

class KaLayoutMenuProvider : public QgsLayoutViewMenuProvider {
public:
  explicit KaLayoutMenuProvider(KaDrawingStudio* studio) : m_studio(studio) {}

  QMenu* createContextMenu(QWidget* parent, QgsLayout* layout, QPointF layoutPoint) const override {
    if (!m_studio || !layout) return nullptr;
    QgsLayoutItem* hit = layout->layoutItemAt(layoutPoint, true);
    auto* map = dynamic_cast<QgsLayoutItemMap*>(hit);
    if (!map) {
      for (QGraphicsItem* gi : layout->items(layoutPoint)) {
        map = dynamic_cast<QgsLayoutItemMap*>(gi);
        if (map) break;
      }
    }
    if (!map) return nullptr;
    auto* menu = new QMenu(parent);
    menu->addAction(KaIcons::icon(QStringLiteral("layout_activate")),
                    QStringLiteral("지도 조정"),
                    m_studio, &KaDrawingStudio::beginActivateMap);
    menu->addAction(KaIcons::icon(QStringLiteral("layout_center")),
                    QStringLiteral("그린 것 가운데로"),
                    m_studio, &KaDrawingStudio::centerSurveyInMap);
    if (m_studio->isMapAdjusting()) {
      menu->addSeparator();
      menu->addAction(KaIcons::icon(QStringLiteral("layout_activate_done")),
                      QStringLiteral("지도조정끝"),
                      m_studio, &KaDrawingStudio::endActivateMap);
    }
    return menu;
  }

private:
  KaDrawingStudio* m_studio = nullptr;
};

void KaDrawingStudio::ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas) {
  static bool once = false;
  if (once) return;
  once = true;
  if (mapCanvas)
    QgsLayoutGuiUtils::registerGuiForKnownItemTypes(mapCanvas);
}

bool KaDrawingStudio::promptPaper(QWidget* parent, double* widthMm, double* heightMm) {
  if (!widthMm || !heightMm) return false;
  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("용지 설정"));
  dlg.setMinimumWidth(360);
  auto* form = new QFormLayout(&dlg);
  auto* paper = new QComboBox(&dlg);
  paper->addItem(QStringLiteral("A4"), 0);
  paper->addItem(QStringLiteral("A3"), 1);
  paper->addItem(QStringLiteral("사용자 크기"), 2);
  auto* orient = new QComboBox(&dlg);
  orient->addItem(QStringLiteral("가로"), 0);
  orient->addItem(QStringLiteral("세로"), 1);
  auto* w = new QDoubleSpinBox(&dlg);
  w->setRange(50.0, 2000.0);
  w->setDecimals(1);
  w->setSuffix(QStringLiteral(" mm"));
  w->setValue(297.0);
  auto* h = new QDoubleSpinBox(&dlg);
  h->setRange(50.0, 2000.0);
  h->setDecimals(1);
  h->setSuffix(QStringLiteral(" mm"));
  h->setValue(210.0);
  auto applyPreset = [paper, orient, w, h]() {
    const int p = paper->currentData().toInt();
    const bool land = orient->currentData().toInt() == 0;
    const bool custom = p == 2;
    w->setEnabled(custom);
    h->setEnabled(custom);
    if (custom) return;
    double pw = (p == 1) ? 297.0 : 210.0;
    double ph = (p == 1) ? 420.0 : 297.0;
    if (land) std::swap(pw, ph);
    w->setValue(pw);
    h->setValue(ph);
  };
  connect(paper, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, applyPreset);
  connect(orient, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, applyPreset);
  applyPreset();
  form->addRow(QStringLiteral("용지"), paper);
  form->addRow(QStringLiteral("방향"), orient);
  form->addRow(QStringLiteral("가로"), w);
  form->addRow(QStringLiteral("세로"), h);
  auto* tip = new QLabel(QStringLiteral("확인하면 빈 용지가 열립니다. 드래그해서 지도 칸을 만드세요."), &dlg);
  tip->setWordWrap(true);
  form->addRow(tip);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("용지 만들기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return false;
  *widthMm = w->value();
  *heightMm = h->value();
  return true;
}

KaDrawingStudio::KaDrawingStudio(QgsProject* project, QgsMapCanvas* mapCanvas,
                                 double paperWidthMm, double paperHeightMm, QWidget* parent)
    : QMainWindow(parent), m_project(project), m_mapCanvas(mapCanvas),
      m_paperW(paperWidthMm), m_paperH(paperHeightMm) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowTitle(QStringLiteral("조판"));
  resize(1280, 860);
  ensureLayoutGuiRegistered(mapCanvas);
  ensureBlankLayout();
  buildUi();
  beginDrawMapFrame();
  auto* undoAct = new QAction(QStringLiteral("되돌리기"), this);
  undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setShortcutContext(Qt::WindowShortcut);
  connect(undoAct, &QAction::triggered, this, &KaDrawingStudio::undoLastChange);
  addAction(undoAct);
}

void KaDrawingStudio::resetPaper(double widthMm, double heightMm) {
  m_paperW = widthMm;
  m_paperH = heightMm;
  endActivateMap();
  ensureBlankLayout();
  if (m_view) m_view->zoomFull();
  beginDrawMapFrame();
  if (m_status)
    m_status->setText(QStringLiteral("빈 용지입니다. 드래그해서 지도가 들어갈 칸을 그리세요."));
}

void KaDrawingStudio::ensureBlankLayout() {
  if (!m_project) return;
  if (m_view) {
    if (QgsLayoutViewTool* t = m_view->tool())
      m_view->unsetTool(t);
    static_cast<QGraphicsView*>(m_view)->setScene(nullptr);
  }
  LayoutService::createBlankSheet(m_project, m_paperW, m_paperH, QString::fromUtf8(kSheetName));
  attachLayoutToView();
}

void KaDrawingStudio::attachLayoutToView() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  if (m_view->currentLayout() != ly)
    m_view->setCurrentLayout(ly);
  if (m_toolSelect)
    m_toolSelect->setLayout(ly);
  const QColor desk(229, 231, 235);
  ly->setBackgroundBrush(QBrush(desk));
  m_view->setBackgroundBrush(QBrush(desk));
  connect(ly, &QgsLayout::selectedItemChanged, this, &KaDrawingStudio::onLayoutSelectionChanged,
          Qt::UniqueConnection);
  updateInspector(nullptr);
}

QgsPrintLayout* KaDrawingStudio::layout() const {
  if (!m_project) return nullptr;
  return dynamic_cast<QgsPrintLayout*>(
      m_project->layoutManager()->layoutByName(QString::fromUtf8(kSheetName)));
}

QgsLayoutItemMap* KaDrawingStudio::mapItem() const {
  return dynamic_cast<QgsLayoutItemMap*>(findItemById(layout(), kIdMap));
}

void KaDrawingStudio::buildUi() {
  auto* tb = addToolBar(QStringLiteral("조판"));
  tb->setObjectName(QStringLiteral("studioToolbar"));
  tb->setMovable(false);
  tb->setIconSize(QSize(26, 26));
  tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

  auto addTb = [this, tb](const QString& iconId, const QString& text, const QString& tip, auto slot) {
    QAction* a = tb->addAction(KaIcons::icon(iconId), text, this, slot);
    a->setToolTip(tip);
    return a;
  };
  addTb(QStringLiteral("layout_map_frame"), QStringLiteral("지도칸"),
        QStringLiteral("용지에서 드래그해 지도가 들어갈 칸을 만듭니다"),
        &KaDrawingStudio::beginDrawMapFrame);
  addTb(QStringLiteral("layout_select"), QStringLiteral("선택"),
        QStringLiteral("항목을 옮기거나 모서리를 끌어 크기를 바꿉니다"),
        &KaDrawingStudio::useSelectTool);
  addTb(QStringLiteral("layout_pan"), QStringLiteral("옮기기"),
        QStringLiteral("용지를 잡아 옮깁니다"),
        &KaDrawingStudio::usePanTool);
  addTb(QStringLiteral("layout_zoom_full"), QStringLiteral("전체"),
        QStringLiteral("용지 전체가 보이게 합니다"),
        &KaDrawingStudio::zoomFull);
  tb->addSeparator();
  addTb(QStringLiteral("layout_activate"), QStringLiteral("지도조정"),
        QStringLiteral("칸 안의 지도를 옮기고 확대합니다"),
        &KaDrawingStudio::beginActivateMap);
  addTb(QStringLiteral("layout_center"), QStringLiteral("가운데"),
        QStringLiteral("그린 조사 도형이 칸 가운데에 오게 합니다"),
        &KaDrawingStudio::centerSurveyInMap);
  m_actEndAdjust = addTb(QStringLiteral("layout_activate_done"), QStringLiteral("조정끝"),
                         QStringLiteral("지도 조정을 끝냅니다"),
                         &KaDrawingStudio::endActivateMap);
  m_actEndAdjust->setVisible(false);
  tb->addSeparator();
  tb->addAction(KaIcons::icon(QStringLiteral("trash")), QStringLiteral("삭제"), this,
                &KaDrawingStudio::deleteSelectedItems)
      ->setToolTip(QStringLiteral("선택한 항목을 지웁니다 (Delete)"));
  tb->addSeparator();
  addTb(QStringLiteral("pdf"), QStringLiteral("PDF"),
        QStringLiteral("지금 용지를 PDF로 저장합니다"),
        &KaDrawingStudio::savePdf);

  auto* root = new QWidget(this);
  auto* rootLay = new QVBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  auto* top = new QWidget(root);
  top->setObjectName(QStringLiteral("sampleStrip"));
  top->setFixedHeight(108);
  auto* topLay = new QHBoxLayout(top);
  topLay->setContentsMargins(8, 6, 8, 6);
  topLay->setSpacing(8);

  auto* layerBox = new QWidget(top);
  layerBox->setMinimumWidth(180);
  layerBox->setMaximumWidth(240);
  auto* layerLay = new QVBoxLayout(layerBox);
  layerLay->setContentsMargins(0, 0, 0, 0);
  layerLay->setSpacing(4);
  auto* leftCap = new QLabel(QStringLiteral("레이어"), layerBox);
  m_layerModel = new QgsLayerTreeModel(QgsProject::instance()->layerTreeRoot(), this);
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  m_layerTree = new QgsLayerTreeView(layerBox);
  m_layerTree->setObjectName(QStringLiteral("layoutLayerTree"));
  m_layerTree->setModel(m_layerModel);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_layerTree->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &KaDrawingStudio::syncMapFromLayers);
  connect(m_layerModel, &QAbstractItemModel::dataChanged, this, &KaDrawingStudio::syncMapFromLayers);
  layerLay->addWidget(leftCap);
  layerLay->addWidget(m_layerTree, 1);
  topLay->addWidget(layerBox, 0);

  auto* sampleBox = new QWidget(top);
  auto* sampleLay = new QVBoxLayout(sampleBox);
  sampleLay->setContentsMargins(0, 0, 0, 0);
  sampleLay->setSpacing(4);
  auto* sampleCap = new QLabel(QStringLiteral("넣을 것"), sampleBox);
  sampleLay->addWidget(sampleCap);
  auto* sampleRow = new QHBoxLayout;
  sampleRow->setSpacing(8);

  auto* legendBtn = makeRailTile(sampleBox, KaIcons::icon(QStringLiteral("layout_legend")),
                                 QStringLiteral("범례"), QSize(32, 32));
  connect(legendBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceLegend);
  m_cardLegend = nullptr;
  sampleRow->addWidget(legendBtn);

  struct NorthSample { const char* rel; const char* tip; int art; };
  const NorthSample norths[] = {
      {"", "글자 N", 0},
      {"arrows/NorthArrow_02.svg", "화살", 1},
      {"arrows/NorthArrow_04.svg", "나침반", 2},
      {"wind_roses/WindRose_01.svg", "바람장미", 3},
  };
  for (const auto& ns : norths) {
    auto* b = makeRailTile(sampleBox, northPreviewIcon(ns.art), QString::fromUtf8(ns.tip), QSize(40, 40));
    const QString rel = QString::fromUtf8(ns.rel);
    connect(b, &QToolButton::clicked, this, [this, rel]() { beginPlaceNorth(rel); });
    sampleRow->addWidget(b);
  }

  struct BarSample { const char* style; const char* tip; };
  const BarSample bars[] = {
      {"Double Box", "쌍칸"},
      {"Single Box", "외칸"},
      {"Line Ticks Up", "눈금"},
  };
  for (const auto& bs : bars) {
    auto* b = makeRailTile(sampleBox, scaleBarPreviewIcon(bs.style), QString::fromUtf8(bs.tip), QSize(48, 28));
    const QString style = QString::fromUtf8(bs.style);
    connect(b, &QToolButton::clicked, this, [this, style]() { beginPlaceScaleBar(style); });
    sampleRow->addWidget(b);
  }

  auto* scaleLblBtn = makeRailTile(sampleBox, KaIcons::icon(QStringLiteral("layout_scale")),
                                   QStringLiteral("축척글자"), QSize(32, 32));
  connect(scaleLblBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceScaleLabel);
  sampleRow->addWidget(scaleLblBtn);

  auto* crsBtn = makeRailTile(sampleBox, KaIcons::icon(QStringLiteral("crs")),
                              QStringLiteral("좌표계"), QSize(32, 32));
  connect(crsBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceCrsLabel);
  sampleRow->addWidget(crsBtn);
  sampleRow->addStretch(1);
  sampleLay->addLayout(sampleRow, 1);
  topLay->addWidget(sampleBox, 1);

  m_inspector = new QFrame(top);
  m_inspector->setObjectName(QStringLiteral("itemInspector"));
  m_inspector->setMinimumWidth(220);
  m_inspector->setMaximumWidth(280);
  auto* inspLay = new QVBoxLayout(m_inspector);
  inspLay->setContentsMargins(8, 8, 8, 8);
  inspLay->setSpacing(4);
  m_inspectorCap = new QLabel(m_inspector);
  inspLay->addWidget(m_inspectorCap);

  m_legendProps = new QWidget(m_inspector);
  auto* lp = new QFormLayout(m_legendProps);
  lp->setContentsMargins(0, 0, 0, 0);
  lp->setSpacing(4);
  m_legendTitle = new QLineEdit(QStringLiteral("범례"), m_legendProps);
  m_legendFont = new QSpinBox(m_legendProps);
  m_legendFont->setRange(7, 24);
  m_legendFont->setValue(10);
  m_legendFont->setSuffix(QStringLiteral(" pt"));
  connect(m_legendTitle, &QLineEdit::textChanged, this, &KaDrawingStudio::applyLegendSettings);
  connect(m_legendFont, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &KaDrawingStudio::applyLegendSettings);
  lp->addRow(QStringLiteral("제목"), m_legendTitle);
  lp->addRow(QStringLiteral("글자"), m_legendFont);
  inspLay->addWidget(m_legendProps);
  m_inspector->hide();
  topLay->addWidget(m_inspector, 0);

  m_adjustBar = new QFrame(root);
  m_adjustBar->setObjectName(QStringLiteral("mapAdjustBar"));
  auto* barLay = new QHBoxLayout(m_adjustBar);
  barLay->setContentsMargins(12, 8, 12, 8);
  auto* barTxt = new QLabel(
      QStringLiteral("지도 조정 중 — 드래그로 이동, 휠로 확대. 끝나면 「조정끝」."),
      m_adjustBar);
  auto* btnCenter = new QPushButton(QStringLiteral("그린 것 가운데"), m_adjustBar);
  auto* btnDone = new QPushButton(QStringLiteral("조정끝"), m_adjustBar);
  btnDone->setObjectName(QStringLiteral("btnAdjustDone"));
  connect(btnCenter, &QPushButton::clicked, this, &KaDrawingStudio::centerSurveyInMap);
  connect(btnDone, &QPushButton::clicked, this, &KaDrawingStudio::endActivateMap);
  barLay->addWidget(barTxt, 1);
  barLay->addWidget(btnCenter);
  barLay->addWidget(btnDone);
  m_adjustBar->hide();

  m_view = new QgsLayoutView(root);
  m_view->setObjectName(QStringLiteral("layoutView"));
  KaTheme::excludeMapSurface(m_view);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setBackgroundBrush(QBrush(QColor(229, 231, 235)));
  m_view->setFocusPolicy(Qt::StrongFocus);
  m_view->setMenuProvider(new KaLayoutMenuProvider(this));
  m_view->installEventFilter(this);
  if (m_view->viewport())
    m_view->viewport()->installEventFilter(this);

  m_toolSelect = new QgsLayoutViewToolSelect(m_view);
  m_toolPan = new QgsLayoutViewToolPan(m_view);
  m_toolMoveContent = new KaLayoutMapAdjustTool(m_view);
  m_toolDrawMap = new KaLayoutMapDrawTool(m_view);
  connect(m_toolDrawMap, &KaLayoutMapDrawTool::rectDrawn, this, &KaDrawingStudio::onRectDrawn,
          Qt::QueuedConnection);
  connect(m_toolMoveContent, &KaLayoutMapAdjustTool::mapViewChanged, this,
          &KaDrawingStudio::syncScaleDecorations);
  attachLayoutToView();

  m_scaleBar = new QFrame(root);
  m_scaleBar->setObjectName(QStringLiteral("mapAdjustBar"));
  auto* scaleLay = new QHBoxLayout(m_scaleBar);
  scaleLay->setContentsMargins(10, 6, 10, 6);
  scaleLay->setSpacing(6);
  auto* scaleCap = new QLabel(QStringLiteral("축척 조정  1 :"), m_scaleBar);
  m_scaleSpin = new QSpinBox(m_scaleBar);
  m_scaleSpin->setRange(10, 5000000);
  m_scaleSpin->setSingleStep(10);
  m_scaleSpin->setValue(1000);
  m_scaleSpin->setKeyboardTracking(false);
  m_scaleSpin->setGroupSeparatorShown(false);
  m_scaleSpin->setMinimumWidth(88);
  auto* applySc = new QPushButton(QStringLiteral("적용"), m_scaleBar);
  connect(applySc, &QPushButton::clicked, this, &KaDrawingStudio::applyOnScreenScale);
  connect(m_scaleSpin, &QSpinBox::editingFinished, this, &KaDrawingStudio::applyOnScreenScale);
  scaleLay->addWidget(scaleCap);
  scaleLay->addWidget(m_scaleSpin);
  scaleLay->addWidget(applySc);
  m_scaleProps = m_scaleBar;
  const int chipVals[] = {100, 500, 1000, 2000, 5000, 10000, 25000};
  for (int n : chipVals) {
    auto* chip = new QPushButton(QString::number(n), m_scaleBar);
    chip->setObjectName(QStringLiteral("scaleChip"));
    chip->setProperty("denom", n);
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setMinimumWidth(52);
    connect(chip, &QPushButton::clicked, this, [this, n]() {
      if (m_scaleSpin) m_scaleSpin->setValue(n);
      applyOnScreenScale();
    });
    scaleLay->addWidget(chip);
  }
  scaleLay->addStretch(1);

  rootLay->addWidget(top);
  rootLay->addWidget(m_adjustBar);
  rootLay->addWidget(m_view, 1);
  rootLay->addWidget(m_scaleBar);
  setCentralWidget(root);

  m_status = new QLabel(this);
  statusBar()->addWidget(m_status, 1);
  m_status->setText(QStringLiteral("용지에서 드래그해 지도 칸을 만드세요. 항목을 고르고 Delete로 지울 수 있습니다."));
  zoomFull();
}

void KaDrawingStudio::startPlace(PlaceKind kind) {
  endActivateMap();
  m_placeKind = kind;
  if (m_view && m_toolDrawMap)
    m_view->setTool(m_toolDrawMap);
}

void KaDrawingStudio::beginDrawMapFrame() {
  startPlace(PlaceKind::MapFrame);
  if (m_status)
    m_status->setText(QStringLiteral("용지 위에서 드래그해 지도 칸을 만드세요."));
}

void KaDrawingStudio::beginPlaceLegend() {
  endActivateMap();
  m_placeKind = PlaceKind::Legend;
  applyLegendSettings();
  m_placeUndo.append(QString::fromUtf8(kIdLegend));
  finishPlace();
  if (m_status)
    m_status->setText(QStringLiteral("범례를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceNorth(const QString& svgRel) {
  endActivateMap();
  m_pendingNorthSvg = svgRel;
  m_placeKind = PlaceKind::North;
  applyNorthNow();
  m_placeUndo.append(QString::fromUtf8(kIdNorth));
  if (m_status)
    m_status->setText(QStringLiteral("방위표를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceScaleBar(const QString& style) {
  endActivateMap();
  m_pendingScaleBarStyle = style;
  m_placeKind = PlaceKind::ScaleBar;
  applyScaleBarNow();
  m_placeUndo.append(QString::fromUtf8(kIdScaleBar));
  if (m_status)
    m_status->setText(QStringLiteral("축척자를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceScaleLabel() {
  endActivateMap();
  m_placeKind = PlaceKind::ScaleLabel;
  placeScaleLabel(defaultItemRect(kIdScale));
  m_placeUndo.append(QString::fromUtf8(kIdScale));
  if (m_status)
    m_status->setText(QStringLiteral("축척 글자를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::beginPlaceCrsLabel() {
  endActivateMap();
  m_placeKind = PlaceKind::CrsLabel;
  applyCrsLabelNow();
  m_placeUndo.append(QString::fromUtf8(kIdCrs));
  finishPlace();
  if (m_status)
    m_status->setText(QStringLiteral("좌표계를 넣었습니다. 끌어 옮기세요."));
}

void KaDrawingStudio::useSelectTool() {
  if (m_adjustingMap) return;
  if (m_view && m_toolSelect)
    m_view->setTool(m_toolSelect);
  if (m_status)
    m_status->setText(QStringLiteral("항목을 눌러 옮기거나, 모서리를 끌어 가로·세로를 바꾸세요."));
}

void KaDrawingStudio::usePanTool() {
  if (m_adjustingMap) return;
  if (m_view && m_toolPan)
    m_view->setTool(m_toolPan);
  if (m_status)
    m_status->setText(QStringLiteral("용지를 잡아 옮기세요."));
}

void KaDrawingStudio::zoomFull() {
  if (m_view) m_view->zoomFull();
}

void KaDrawingStudio::finishPlace() {
  QTimer::singleShot(0, this, [this]() {
    if (!m_adjustingMap)
      useSelectTool();
    selectPlacedItem();
  });
}

void KaDrawingStudio::selectPlacedItem() {
  auto* ly = layout();
  if (!ly) return;
  const char* id = kIdMap;
  switch (m_placeKind) {
    case PlaceKind::MapFrame:
      id = kIdMap;
      break;
    case PlaceKind::Legend:
      id = kIdLegend;
      break;
    case PlaceKind::North:
      id = kIdNorth;
      break;
    case PlaceKind::ScaleBar:
      id = kIdScaleBar;
      break;
    case PlaceKind::ScaleLabel:
      id = kIdScale;
      break;
    case PlaceKind::CrsLabel:
      id = kIdCrs;
      break;
  }
  if (auto* it = findItemById(ly, id))
    ly->setSelectedItem(it);
}

QgsVectorLayer* KaDrawingStudio::blankMapLayer() {
  if (!m_blankMapLayer) {
    m_blankMapLayer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186"),
        QStringLiteral("layout_blank"),
        QStringLiteral("memory"));
    if (m_blankMapLayer)
      m_blankMapLayer->setParent(this);
  }
  return m_blankMapLayer;
}

void KaDrawingStudio::applyLayersToMap(QgsLayoutItemMap* map, bool includeLiveBasemap, bool refitExtent) {
  if (!map || !m_project) return;
  QList<QgsMapLayer*> layers = LayerOps::visibleLayersPaintOrder(m_project);
  if (!includeLiveBasemap) {
    QList<QgsMapLayer*> safe;
    for (QgsMapLayer* ml : layers) {
      if (ml && !isLiveBasemapLayer(ml))
        safe.append(ml);
    }
    layers = safe;
  }
  if (layers.isEmpty()) {
    if (auto* blank = blankMapLayer())
      layers.append(blank);
  }
  map->setKeepLayerSet(true);
  if (!layers.isEmpty())
    map->setLayers(layers);
  const QgsCoordinateReferenceSystem crs = studioMapCrs(m_mapCanvas, m_project);
  if (crs.isValid())
    map->setCrs(crs);
  if (!refitExtent) return;
  const QgsRectangle ext = studioMapExtent(m_mapCanvas, crs);
  if (!ext.isNull() && ext.isFinite() && ext.width() > 0.0)
    map->zoomToExtent(ext);
}

void KaDrawingStudio::onRectDrawn(const QRectF& layoutRect) {
  if (!layoutRect.isValid() || layoutRect.width() < 8.0 || layoutRect.height() < 8.0)
    return;
  switch (m_placeKind) {
    case PlaceKind::MapFrame:
      createOrResizeMap(layoutRect);
      break;
    case PlaceKind::Legend:
      placeLegend(layoutRect);
      break;
    case PlaceKind::North:
      placeNorth(layoutRect);
      break;
    case PlaceKind::ScaleBar:
      placeScaleBar(layoutRect);
      break;
    case PlaceKind::ScaleLabel:
      placeScaleLabel(layoutRect);
      break;
    case PlaceKind::CrsLabel:
      placeCrsLabel(layoutRect);
      finishPlace();
      break;
  }
}

void KaDrawingStudio::createOrResizeMap(const QRectF& layoutRect) {
  auto* ly = layout();
  if (!ly || !m_view || m_view->currentLayout() != ly) return;

  const QgsCoordinateReferenceSystem crs = studioMapCrs(m_mapCanvas, m_project);
  const QgsRectangle ext = studioMapExtent(m_mapCanvas, crs);
  if (ext.isNull() || !ext.isFinite() || ext.width() <= 0.0)
    return;

  QgsLayoutItemMap* map = mapItem();
  const bool created = !map;
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const auto chrome = LayoutService::standardSheetChrome(page, layoutRect);
  if (created)
    m_placeUndo.append(QString::fromUtf8(kIdMap));
  if (!map) {
    map = new QgsLayoutItemMap(ly);
    map->setId(QString::fromUtf8(kIdMap));
    map->setFrameEnabled(true);
    map->attemptSetSceneRect(chrome.map);
    map->setCrs(crs);
    map->setKeepLayerSet(true);
    applyLayersToMap(map, false, true);
    map->zoomToExtent(ext);
    if (map->scene() != ly)
      ly->addLayoutItem(map);
  } else {
    map->attemptSetSceneRect(chrome.map);
    map->setCrs(crs);
    map->zoomToExtent(ext);
  }

  QPointer<QgsLayoutItemMap> held(map);
  QTimer::singleShot(0, this, [this, held, created]() {
    if (!held) return;
    applyLayersToMap(held, true, false);
    snapMapScaleToNice();
    connect(held, &QgsLayoutItemMap::extentChanged, this, &KaDrawingStudio::syncScaleDecorations,
            Qt::UniqueConnection);
    if (created)
      ensureStandardDecorations();
    else {
      applyStandardChromePositions();
      relinkDecorations();
      applyCrsLabelNow();
    }
    refreshScaleWidgets(true);
    useSelectTool();
    if (auto* ly = layout())
      ly->setSelectedItem(held);
    if (m_status)
      m_status->setText(QStringLiteral(
          "지도 칸을 만들었습니다. 나침반·좌표계·축척자·축척이 들어갔습니다."));
  });
}

QRectF KaDrawingStudio::defaultItemRect(const char* id) const {
  QRectF mapR(12.0, 12.0, m_paperW - 24.0, m_paperH - 24.0);
  if (auto* map = mapItem()) {
    const QRectF b = map->sceneBoundingRect();
    if (b.width() > 8.0 && b.height() > 8.0)
      mapR = b;
  }
  if (qstrcmp(id, kIdLegend) == 0)
    return QRectF(mapR.right() - 52.0, mapR.top() + 6.0, 48.0, 74.0);
  const auto chrome =
      LayoutService::standardSheetChrome(QRectF(0.0, 0.0, m_paperW, m_paperH), mapR);
  if (qstrcmp(id, kIdNorth) == 0)
    return chrome.north;
  if (qstrcmp(id, kIdScaleBar) == 0)
    return chrome.scaleBar;
  if (qstrcmp(id, kIdScale) == 0)
    return chrome.scaleLabel;
  if (qstrcmp(id, kIdCrs) == 0)
    return chrome.crs;
  return QRectF(20.0, 20.0, 40.0, 30.0);
}

void KaDrawingStudio::applyLegendSettings() {
  auto* ly = layout();
  if (!ly) return;
  auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend));
  if (!legend) {
    legend = new QgsLayoutItemLegend(ly);
    legend->setId(QString::fromUtf8(kIdLegend));
    legend->setFrameEnabled(true);
    legend->setBackgroundEnabled(true);
    legend->setResizeToContents(false);
    if (auto* map = mapItem())
      legend->setLinkedMap(map);
    legend->attemptSetSceneRect(defaultItemRect(kIdLegend));
    ly->addLayoutItem(legend);
  }
  legend->setResizeToContents(false);
  legend->setTitle(m_legendTitle ? m_legendTitle->text() : QStringLiteral("범례"));
  const int pt = m_legendFont ? m_legendFont->value() : 10;
  legend->setStyleFont(Qgis::LegendComponent::Title, QFont(QStringLiteral("Malgun Gothic"), pt, QFont::Bold));
  legend->setStyleFont(Qgis::LegendComponent::Group, QFont(QStringLiteral("Malgun Gothic"), pt));
  legend->setStyleFont(Qgis::LegendComponent::Subgroup, QFont(QStringLiteral("Malgun Gothic"), pt));
  legend->setStyleFont(Qgis::LegendComponent::SymbolLabel, QFont(QStringLiteral("Malgun Gothic"), qMax(7, pt - 1)));
  if (auto* map = mapItem())
    legend->setLinkedMap(map);
  legend->updateLegend();
  legend->update();
}

void KaDrawingStudio::placeLegend(const QRectF& layoutRect) {
  applyLegendSettings();
  auto* ly = layout();
  if (!ly) return;
  if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(layoutRect);
    legend->update();
  }
  finishPlace();
}

void KaDrawingStudio::applyNorthNow() {
  placeNorth(findItemById(layout(), kIdNorth)
                 ? findItemById(layout(), kIdNorth)->sceneBoundingRect()
                 : defaultItemRect(kIdNorth));
}

void KaDrawingStudio::placeNorth(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  if (auto* old = findItemById(ly, kIdNorth))
    ly->removeLayoutItem(old);

  const int kind = northKindFromRel(m_pendingNorthSvg);
  const QString png = writeNorthPng(kind);
  QRectF r = layoutRect;
  if (!r.isValid() || r.width() < 28.0 || r.height() < 28.0)
    r = defaultItemRect(kIdNorth);

  if (png.isEmpty()) {
    auto* north = new QgsLayoutItemLabel(ly);
    north->setId(QString::fromUtf8(kIdNorth));
    north->setText(QStringLiteral("N\n↑\n진북"));
    north->setHAlign(Qt::AlignHCenter);
    north->setFont(QFont(QStringLiteral("Malgun Gothic"), 12, QFont::Bold));
    north->attemptSetSceneRect(r);
    north->setFrameEnabled(false);
    ly->addLayoutItem(north);
  } else {
    auto* pic = new QgsLayoutItemPicture(ly);
    pic->setId(QString::fromUtf8(kIdNorth));
    pic->setPicturePath(png, Qgis::PictureFormat::Raster);
    pic->setMode(Qgis::PictureFormat::Raster);
    pic->setResizeMode(QgsLayoutItemPicture::Zoom);
    pic->setNorthMode(QgsLayoutItemPicture::GridNorth);
    if (auto* map = mapItem())
      pic->setLinkedMap(map);
    pic->attemptSetSceneRect(r);
    pic->refreshPicture();
    ly->addLayoutItem(pic);
  }
  if (selectAfter)
    finishPlace();
}

void KaDrawingStudio::applyScaleBarNow() {
  auto* ly = layout();
  if (!ly) return;
  auto* existing = findItemById(ly, kIdScaleBar);
  QRectF r = existing ? existing->sceneBoundingRect() : defaultItemRect(kIdScaleBar);
  if (r.width() < 140.0) {
    r.setWidth(160.0);
    if (r.right() > m_paperW - 6.0)
      r.moveLeft(std::max(6.0, m_paperW - 166.0));
  }
  placeScaleBar(r);
}

void KaDrawingStudio::placeScaleBar(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar));
  if (!sb) {
    sb = new QgsLayoutItemScaleBar(ly);
    sb->setId(QString::fromUtf8(kIdScaleBar));
    ly->addLayoutItem(sb);
  }
  sb->applyDefaultSettings();
  if (auto* map = mapItem())
    sb->setLinkedMap(map);
  sb->setStyle(m_pendingScaleBarStyle.isEmpty() ? QStringLiteral("Line Ticks Up")
                                                 : m_pendingScaleBarStyle);
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  const QRectF keep = (layoutRect.width() >= 32.0 && layoutRect.height() >= 8.0)
                          ? layoutRect
                          : defaultItemRect(kIdScaleBar);
  sb->attemptSetSceneRect(keep);
  applyNiceScaleBar(sb);
  if (selectAfter)
    finishPlace();
}

void KaDrawingStudio::applyCrsLabelNow() {
  auto* ly = layout();
  if (!ly) return;
  auto* existing = findItemById(ly, kIdCrs);
  placeCrsLabel(existing ? existing->sceneBoundingRect() : defaultItemRect(kIdCrs));
}

void KaDrawingStudio::placeCrsLabel(const QRectF& layoutRect) {
  auto* ly = layout();
  if (!ly) return;
  auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdCrs));
  if (!lbl) {
    lbl = new QgsLayoutItemLabel(ly);
    lbl->setId(QString::fromUtf8(kIdCrs));
    lbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    lbl->setBackgroundEnabled(false);
    lbl->setFrameEnabled(false);
    ly->addLayoutItem(lbl);
  }
  lbl->setBackgroundEnabled(false);
  const QgsCoordinateReferenceSystem crs =
      (mapItem() && mapItem()->crs().isValid()) ? mapItem()->crs()
                                                : studioMapCrs(m_mapCanvas, m_project);
  lbl->setText(koreanCrsLabel(crs));
  const QRectF keep = (layoutRect.width() >= 40.0 && layoutRect.height() >= 6.0)
                          ? layoutRect
                          : defaultItemRect(kIdCrs);
  lbl->attemptSetSceneRect(keep);
}

void KaDrawingStudio::placeScaleLabel(const QRectF& layoutRect, bool selectAfter) {
  auto* ly = layout();
  if (!ly) return;
  auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdScale));
  if (!lbl) {
    lbl = new QgsLayoutItemLabel(ly);
    lbl->setId(QString::fromUtf8(kIdScale));
    lbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 9, QFont::Bold));
    ly->addLayoutItem(lbl);
  }
  const int sc = m_scaleSpin ? m_scaleSpin->value()
                             : (mapItem() ? displayScale(mapItem()->scale()) : 1000);
  lbl->setText(QStringLiteral("축척 1 : %1").arg(sc > 0 ? sc : 1000));
  const QRectF keep = (layoutRect.width() >= 36.0 && layoutRect.height() >= 6.0)
                          ? layoutRect
                          : defaultItemRect(kIdScale);
  lbl->attemptSetSceneRect(keep);
  if (selectAfter) {
    finishPlace();
    if (m_status)
      m_status->setText(QStringLiteral("축척 글자를 넣었습니다."));
  }
}

void KaDrawingStudio::syncMapFromLayers() {
  if (auto* map = mapItem())
    applyLayersToMap(map, true, false);
}

void KaDrawingStudio::relinkDecorations() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->updateLegend();
  }
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (auto* pic = dynamic_cast<QgsLayoutItemPicture*>(findItemById(ly, kIdNorth)))
    pic->setLinkedMap(map);
  syncScaleDecorations();
}

void KaDrawingStudio::ensureStandardDecorations() {
  auto* ly = layout();
  if (!ly || !mapItem()) return;
  if (m_pendingNorthSvg.isEmpty())
    m_pendingNorthSvg = QStringLiteral("arrows/NorthArrow_04.svg");
  if (m_pendingScaleBarStyle.isEmpty())
    m_pendingScaleBarStyle = QStringLiteral("Line Ticks Up");
  if (!findItemById(ly, kIdNorth))
    placeNorth(defaultItemRect(kIdNorth), false);
  if (!findItemById(ly, kIdScaleBar))
    placeScaleBar(defaultItemRect(kIdScaleBar), false);
  else if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (!findItemById(ly, kIdScale))
    placeScaleLabel(defaultItemRect(kIdScale), false);
  applyCrsLabelNow();
  applyStandardChromePositions();
  relinkDecorations();
}

void KaDrawingStudio::applyStandardChromePositions() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const auto chrome = LayoutService::standardSheetChrome(
      QRectF(0.0, 0.0, m_paperW, m_paperH), map->sceneBoundingRect());
  if (auto* sb = findItemById(ly, kIdScaleBar))
    sb->attemptSetSceneRect(chrome.scaleBar);
  if (auto* sc = findItemById(ly, kIdScale))
    sc->attemptSetSceneRect(chrome.scaleLabel);
  if (auto* crs = findItemById(ly, kIdCrs))
    crs->attemptSetSceneRect(chrome.crs);
  if (auto* north = findItemById(ly, kIdNorth))
    north->attemptSetSceneRect(chrome.north);
}

void KaDrawingStudio::snapMapScaleToNice() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const double widthMm =
      ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  const double heightMm =
      ly->convertFromLayoutUnits(map->rect().height(), Qgis::LayoutUnit::Millimeters).length();
  QgsRectangle ext = map->extent();
  if (!(widthMm > 0.0) || ext.isNull() || !ext.isFinite() || ext.width() <= 0.0)
    return;
  const double rawW = ext.width() / (widthMm / 1000.0);
  const double rawH = (heightMm > 0.0 && ext.height() > 0.0)
                          ? ext.height() / (heightMm / 1000.0)
                          : rawW;
  const double raw = std::max(rawW, rawH);
  const int nice = LayoutService::niceScaleDenominator(raw > 0.0 ? raw : map->scale());
  if (nice <= 0) return;
  map->setScale(static_cast<double>(nice), true);
  if (m_scaleSpin) {
    const bool blocked = m_scaleSpin->blockSignals(true);
    m_scaleSpin->setValue(nice);
    m_scaleSpin->blockSignals(blocked);
  }
}

void KaDrawingStudio::applyNiceScaleBar(QgsLayoutItemScaleBar* sb) {
  if (!sb) return;
  auto* map = mapItem();
  auto* ly = layout();
  if (map)
    sb->setLinkedMap(map);
  const int segs = 4;
  sb->setNumberOfSegments(segs);
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setUnitLabel(QStringLiteral("m"));
  sb->setSegmentSizeMode(Qgis::ScaleBarSegmentSizeMode::Fixed);
  const double denom = map ? map->scale()
                           : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value()) : 1000.0);
  double mapWmm = 160.0;
  if (map && ly)
    mapWmm = ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  const double segM = LayoutService::niceScaleBarSegmentMeters(mapWmm, denom, segs);
  if (segM > 0.0)
    sb->setUnitsPerSegment(segM);
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const QRectF mapR = map ? map->sceneBoundingRect()
                          : QRectF(12.0, 12.0, m_paperW - 24.0, m_paperH - 24.0);
  const auto chrome = LayoutService::standardSheetChrome(page, mapR);
  QRectF keep = sb->sceneBoundingRect();
  if (keep.width() < 32.0)
    keep = chrome.scaleBar;
  keep.moveTop(chrome.scaleBar.top());
  keep.setHeight(chrome.scaleBar.height());
  if (keep.left() < chrome.map.left() - 0.5)
    keep.moveLeft(chrome.scaleBar.left());
  const double wantW = LayoutService::scaleBarWidthMm(segM, segs, denom);
  const double maxW = std::max(32.0, chrome.north.left() - 4.0 - keep.left());
  if (wantW >= 32.0)
    keep.setWidth(std::clamp(wantW, 32.0, maxW));
  else
    keep.setWidth(std::min(std::max(keep.width(), 32.0), maxW));
  if (keep.bottom() > page.bottom())
    keep.setHeight(std::max(6.0, page.bottom() - keep.top()));
  sb->attemptSetSceneRect(keep);
  sb->refresh();
  sb->attemptSetSceneRect(keep);
  if (auto* crs = findItemById(ly, kIdCrs)) {
    QRectF cr = chrome.crs;
    cr.setLeft(keep.right() + 4.0);
    cr.setRight(chrome.north.left() - 4.0);
    if (cr.width() >= 8.0)
      crs->attemptSetSceneRect(cr);
  }
}

int KaDrawingStudio::displayScale(double raw) {
  return LayoutService::niceScaleDenominator(raw);
}

void KaDrawingStudio::refreshScaleWidgets(bool readFromMap) {
  if (!m_scaleSpin) return;
  if (readFromMap) {
    if (auto* map = mapItem()) {
      const int sc = displayScale(map->scale());
      if (sc > 0) {
        const bool blocked = m_scaleSpin->blockSignals(true);
        m_scaleSpin->setValue(sc);
        m_scaleSpin->blockSignals(blocked);
      }
    }
  }
  if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(layout(), kIdScale)))
    lbl->setText(QStringLiteral("축척 1 : %1").arg(m_scaleSpin->value()));
  syncScaleChips();
}

void KaDrawingStudio::syncScaleDecorations() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const int sc = displayScale(map->scale());
  if (m_scaleSpin && sc > 0) {
    const bool blocked = m_scaleSpin->blockSignals(true);
    m_scaleSpin->setValue(sc);
    m_scaleSpin->blockSignals(blocked);
  }
  if (auto* lbl = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdScale))) {
    if (sc > 0)
      lbl->setText(QStringLiteral("축척 1 : %1").arg(sc));
  }
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (auto* crs = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdCrs))) {
    const QgsCoordinateReferenceSystem dest =
        map->crs().isValid() ? map->crs() : studioMapCrs(m_mapCanvas, m_project);
    crs->setText(koreanCrsLabel(dest));
  }
  syncScaleChips();
}

void KaDrawingStudio::syncScaleChips() {
  if (!m_scaleProps || !m_scaleSpin) return;
  const int v = m_scaleSpin->value();
  const auto chips = m_scaleProps->findChildren<QPushButton*>(QStringLiteral("scaleChip"));
  for (QPushButton* b : chips) {
    const bool on = b->property("denom").toInt() == v;
    const bool blocked = b->blockSignals(true);
    b->setChecked(on);
    b->blockSignals(blocked);
  }
}

void KaDrawingStudio::setDrawerCardActive(QFrame* card) {
  Q_UNUSED(card);
}

void KaDrawingStudio::onLayoutSelectionChanged(QgsLayoutItem* item) {
  updateInspector(item);
}

void KaDrawingStudio::updateInspector(QgsLayoutItem* item) {
  if (!m_inspector) return;
  if (!item) {
    if (auto* ly = layout()) {
      const QList<QgsLayoutItem*> sel = ly->selectedLayoutItems(false);
      item = sel.isEmpty() ? nullptr : sel.first();
    }
  }
  const QString id = item ? item->id() : QString();
  const bool legend = id == QLatin1String(kIdLegend);
  const bool scaleItem = id == QLatin1String(kIdScale) || id == QLatin1String(kIdScaleBar)
                         || id == QLatin1String(kIdMap);
  const bool north = id == QLatin1String(kIdNorth);

  if (m_legendProps) m_legendProps->setVisible(legend);
  if (m_scaleBar) m_scaleBar->setVisible(true);
  m_inspector->setVisible(legend);

  if (m_inspectorCap) {
    if (legend)
      m_inspectorCap->setText(QStringLiteral("범례"));
    else
      m_inspectorCap->clear();
  }

  if (legend) {
    if (auto* lg = dynamic_cast<QgsLayoutItemLegend*>(item)) {
      const bool b1 = m_legendTitle ? m_legendTitle->blockSignals(true) : false;
      const bool b2 = m_legendFont ? m_legendFont->blockSignals(true) : false;
      if (m_legendTitle) m_legendTitle->setText(lg->title());
      if (m_legendFont) {
        const QFont f = lg->styleFont(Qgis::LegendComponent::Title);
        if (f.pointSize() > 0) m_legendFont->setValue(f.pointSize());
      }
      if (m_legendTitle) m_legendTitle->blockSignals(b1);
      if (m_legendFont) m_legendFont->blockSignals(b2);
    }
  }
  if (scaleItem)
    syncScaleChips();

  QFrame* card = nullptr;
  if (legend) card = m_cardLegend;
  else if (north) card = m_cardNorth;
  else if (id == QLatin1String(kIdScaleBar)) card = m_cardScaleBar;
  else if (id == QLatin1String(kIdScale)) card = m_cardScale;
  setDrawerCardActive(card);
}

void KaDrawingStudio::applyOnScreenScale() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly || !m_scaleSpin) {
    if (m_status)
      m_status->setText(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  const int wanted = LayoutService::niceScaleDenominator(static_cast<double>(m_scaleSpin->value()));
  if (wanted <= 0) return;
  const double widthMm = ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
  QgsRectangle ext = map->extent();
  if (ext.isNull() || !ext.isFinite() || ext.width() <= 0.0)
    ext = studioMapExtent(m_mapCanvas, studioMapCrs(m_mapCanvas, m_project));
  const QgsRectangle next = LayoutService::extentForPaperScale(ext, widthMm, static_cast<double>(wanted));
  if (next.isFinite() && next.width() > 0.0)
    map->zoomToExtent(next);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  const bool blocked = m_scaleSpin->blockSignals(true);
  m_scaleSpin->setValue(wanted);
  m_scaleSpin->blockSignals(blocked);
  refreshScaleWidgets();
  syncScaleDecorations();
  if (m_status)
    m_status->setText(QStringLiteral("축척을 1 : %1 로 맞췄습니다.").arg(wanted));
}

QgsRectangle KaDrawingStudio::surveyExtentOnMap(QgsLayoutItemMap* map) const {
  QgsRectangle acc;
  bool have = false;
  if (!map || !m_project) return acc;
  const QgsCoordinateReferenceSystem dest = map->crs().isValid()
                                                ? map->crs()
                                                : studioMapCrs(m_mapCanvas, m_project);
  const QgsCoordinateTransformContext ctx = m_project->transformContext();
  for (QgsMapLayer* ml : m_project->mapLayers()) {
    if (!ml || !ml->isValid()) continue;
    if (LayerOps::isReferenceLayer(ml) || isLiveBasemapLayer(ml)) continue;
    QgsRectangle e = ml->extent();
    if (e.isNull() || !e.isFinite()) continue;
    if (ml->crs().isValid() && dest.isValid() && ml->crs() != dest) {
      try {
        QgsCoordinateTransform xf(ml->crs(), dest, ctx);
        xf.setBallparkTransformsAreAppropriate(true);
        e = xf.transformBoundingBox(e);
      } catch (...) {
        continue;
      }
    }
    if (!e.isFinite()) continue;
    if (!have) {
      acc = e;
      have = true;
    } else {
      acc.combineExtentWith(e);
    }
  }
  if (!have) return {};
  if (acc.width() <= 0.0 || acc.height() <= 0.0) {
    const double pad = 40.0;
    acc = QgsRectangle(acc.center().x() - pad, acc.center().y() - pad,
                       acc.center().x() + pad, acc.center().y() + pad);
  } else {
    acc.scale(1.2);
  }
  return acc;
}

void KaDrawingStudio::beginActivateMap() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly || !m_view || !m_toolMoveContent) {
    if (m_status)
      m_status->setText(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  m_adjustingMap = true;
  if (m_adjustBar) m_adjustBar->show();
  if (m_actEndAdjust) m_actEndAdjust->setVisible(true);
  ly->setSelectedItem(map);
  m_view->setTool(m_toolMoveContent);
  if (m_status)
    m_status->setText(QStringLiteral("지도 조정: 드래그로 이동, 휠로 확대. 끝나면 「지도조정끝」."));
}

void KaDrawingStudio::deleteSelectedItems() {
  if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) ||
      qobject_cast<QAbstractSpinBox*>(QApplication::focusWidget()))
    return;
  auto* ly = layout();
  if (!ly) return;
  const QList<QgsLayoutItem*> sel = ly->selectedLayoutItems(false);
  int n = 0;
  for (QgsLayoutItem* it : sel) {
    if (!it || dynamic_cast<QgsLayoutItemPage*>(it)) continue;
    ly->removeLayoutItem(it);
    ++n;
  }
  updateInspector(nullptr);
  if (m_status) {
    if (n > 0)
      m_status->setText(QStringLiteral("선택한 항목 %1개를 지웠습니다.").arg(n));
    else
      m_status->setText(QStringLiteral("지울 항목을 먼저 선택하세요."));
  }
}

bool KaDrawingStudio::eventFilter(QObject* watched, QEvent* event) {
  if (event && event->type() == QEvent::KeyPress &&
      (watched == m_view || (m_view && watched == m_view->viewport()))) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Undo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z)) {
      undoLastChange();
      return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
      deleteSelectedItems();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void KaDrawingStudio::undoLastChange() {
  if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) ||
      qobject_cast<QAbstractSpinBox*>(QApplication::focusWidget()))
    return;
  auto* ly = layout();
  if (ly && ly->undoStack() && ly->undoStack()->stack() && ly->undoStack()->stack()->canUndo()) {
    ly->undoStack()->stack()->undo();
    if (m_status)
      m_status->setText(QStringLiteral("한 단계 되돌렸습니다."));
    return;
  }
  while (!m_placeUndo.isEmpty()) {
    const QString id = m_placeUndo.takeLast();
    QgsLayoutItem* it = ly ? findItemById(ly, id.toUtf8().constData()) : nullptr;
    if (it && !dynamic_cast<QgsLayoutItemPage*>(it)) {
      ly->removeLayoutItem(it);
      updateInspector(nullptr);
      if (m_status)
        m_status->setText(QStringLiteral("방금 넣은 항목을 되돌렸습니다."));
      return;
    }
  }
  if (m_status)
    m_status->setText(QStringLiteral("되돌릴 것이 없습니다."));
}

void KaDrawingStudio::keyPressEvent(QKeyEvent* event) {
  if (event && (event->matches(QKeySequence::Undo) ||
                ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Z))) {
    undoLastChange();
    event->accept();
    return;
  }
  if (event && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
    deleteSelectedItems();
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

void KaDrawingStudio::endActivateMap() {
  const bool was = m_adjustingMap;
  m_adjustingMap = false;
  if (m_adjustBar) m_adjustBar->hide();
  if (m_actEndAdjust) m_actEndAdjust->setVisible(false);
  if (was)
    snapMapScaleToNice();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (was)
    useSelectTool();
}

void KaDrawingStudio::centerSurveyInMap() {
  auto* map = mapItem();
  if (!map) {
    if (m_status)
      m_status->setText(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  const QgsRectangle ext = surveyExtentOnMap(map);
  if (ext.isNull() || !ext.isFinite()) {
    if (m_status)
      m_status->setText(QStringLiteral("가운데로 둘 조사 도형이 없습니다. 먼저 구역·유구를 그리세요."));
    return;
  }
  map->zoomToExtent(ext);
  snapMapScaleToNice();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (m_status)
    m_status->setText(QStringLiteral("그린 것이 칸 가운데에 오도록 맞췄습니다."));
}

void KaDrawingStudio::savePdf() {
  auto* ly = layout();
  if (!ly) {
    QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("용지가 없습니다."));
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("도면 PDF 저장"), QStringLiteral("도면.pdf"), QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QgsLayoutExporter exporter(ly);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  if (exporter.exportToPdf(path, settings) != QgsLayoutExporter::Success) {
    QMessageBox::warning(this, QStringLiteral("PDF"), QStringLiteral("저장에 실패했습니다."));
    return;
  }
  if (m_status) m_status->setText(QStringLiteral("저장: %1").arg(path));
  QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("저장했습니다.\n%1").arg(path));
}

#include "KaDrawingStudio.moc"
