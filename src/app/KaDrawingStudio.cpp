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
#include <QButtonGroup>
#include <QCheckBox>
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
#include <QSet>
#include <QShowEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QGraphicsRectItem>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QWheelEvent>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgspointxy.h>
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
#include <qgslayoutitemmapgrid.h>
#include <qgstextformat.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutview.h>
#include <qgslayoutviewmouseevent.h>
#include <qgslayoutviewtool.h>
#include <qgslayoutviewtoolpan.h>
#include <qgslayoutviewtoolselect.h>
#include <qgslayertree.h>
#include <qgslayertreenode.h>
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
constexpr const char* kGridAnnPrefix = "ka_grid_ann_";

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
  auto paperIcon = [](bool landscape) {
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(15, 23, 42), 1.6));
    p.setBrush(QColor(255, 255, 255));
    if (landscape)
      p.drawRoundedRect(QRectF(5, 14, 38, 22), 3, 3);
    else
      p.drawRoundedRect(QRectF(13, 4, 22, 40), 3, 3);
    return QIcon(pm);
  };
  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("용지 설정"));
  dlg.setMinimumWidth(400);
  auto* form = new QFormLayout(&dlg);
  auto makeChoice = [&dlg](const QString& text, const QIcon& icon, bool checked) {
    auto* b = new QToolButton(&dlg);
    b->setText(text);
    if (!icon.isNull()) b->setIcon(icon);
    b->setIconSize(QSize(28, 28));
    b->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextUnderIcon);
    b->setCheckable(true);
    b->setChecked(checked);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumSize(88, 56);
    return b;
  };
  auto* paperRow = new QWidget(&dlg);
  auto* paperLay = new QHBoxLayout(paperRow);
  paperLay->setContentsMargins(0, 0, 0, 0);
  auto* btnA4 = makeChoice(QStringLiteral("A4"), QIcon(), true);
  auto* btnA3 = makeChoice(QStringLiteral("A3"), QIcon(), false);
  auto* btnCustom = makeChoice(QStringLiteral("직접 입력"), QIcon(), false);
  auto* paperGroup = new QButtonGroup(&dlg);
  paperGroup->setExclusive(true);
  paperGroup->addButton(btnA4, 0);
  paperGroup->addButton(btnA3, 1);
  paperGroup->addButton(btnCustom, 2);
  paperLay->addWidget(btnA4);
  paperLay->addWidget(btnA3);
  paperLay->addWidget(btnCustom);
  paperLay->addStretch(1);

  auto* orientRow = new QWidget(&dlg);
  auto* orientLay = new QHBoxLayout(orientRow);
  orientLay->setContentsMargins(0, 0, 0, 0);
  auto* btnPort = makeChoice(QStringLiteral("세로"), paperIcon(false), true);
  auto* btnLand = makeChoice(QStringLiteral("가로"), paperIcon(true), false);
  auto* orientGroup = new QButtonGroup(&dlg);
  orientGroup->setExclusive(true);
  orientGroup->addButton(btnLand, 0);
  orientGroup->addButton(btnPort, 1);
  orientLay->addWidget(btnPort);
  orientLay->addWidget(btnLand);
  orientLay->addStretch(1);

  auto* w = new QDoubleSpinBox(&dlg);
  w->setRange(50.0, 2000.0);
  w->setDecimals(1);
  w->setSuffix(QStringLiteral(" mm"));
  w->setValue(210.0);
  auto* h = new QDoubleSpinBox(&dlg);
  h->setRange(50.0, 2000.0);
  h->setDecimals(1);
  h->setSuffix(QStringLiteral(" mm"));
  h->setValue(297.0);
  auto applyPreset = [paperGroup, orientGroup, w, h]() {
    const int p = paperGroup->checkedId();
    const bool land = orientGroup->checkedId() == 0;
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
  QObject::connect(paperGroup, &QButtonGroup::idClicked, &dlg, [applyPreset](int) { applyPreset(); });
  QObject::connect(orientGroup, &QButtonGroup::idClicked, &dlg, [applyPreset](int) { applyPreset(); });
  applyPreset();
  form->addRow(QStringLiteral("용지"), paperRow);
  form->addRow(QStringLiteral("방향"), orientRow);
  form->addRow(QStringLiteral("가로"), w);
  form->addRow(QStringLiteral("세로"), h);
  auto* tip = new QLabel(
      QStringLiteral("확인하면 A4/A3 용지에 지도가 바로 올라갑니다. 격자 숫자는 칸 밖에 찍힙니다."),
      &dlg);
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
  autoPlaceDefaultSheet();
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
  m_paperFitPending = true;
  zoomPaperVisible();
  autoPlaceDefaultSheet();
  if (m_status)
    m_status->setText(QStringLiteral("용지에 지도를 올려 두었습니다. 축척을 맞추거나 그린곳 가운데를 누르세요."));
}

void KaDrawingStudio::ensureBlankLayout() {
  if (!m_project) return;
  if (m_pageOutline) {
    if (m_pageOutline->scene())
      m_pageOutline->scene()->removeItem(m_pageOutline);
    delete m_pageOutline;
    m_pageOutline = nullptr;
  }
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
  updatePageOutline();
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
  auto* root = new QWidget(this);
  auto* rootLay = new QHBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  auto* tb = addToolBar(QStringLiteral("조판"));
  tb->setObjectName(QStringLiteral("studioToolbar"));
  tb->setMovable(false);
  tb->setIconSize(QSize(20, 20));
  tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  auto addTb = [this, tb](const QString& iconId, const QString& text, const QString& tip, auto slot) {
    QAction* a = tb->addAction(KaIcons::icon(iconId), text, this, slot);
    a->setToolTip(tip);
    return a;
  };
  addTb(QStringLiteral("layout_map_frame"), QStringLiteral("지도칸 그리기"),
        QStringLiteral("용지에서 드래그해 지도가 들어갈 칸을 만듭니다"),
        &KaDrawingStudio::beginDrawMapFrame);
  addTb(QStringLiteral("layout_select"), QStringLiteral("선택"),
        QStringLiteral("항목을 고르고, 끌어 옮기거나 모서리로 크기를 바꿉니다"),
        &KaDrawingStudio::useSelectTool);
  addTb(QStringLiteral("layout_pan"), QStringLiteral("용지 옮기기"),
        QStringLiteral("용지 전체를 잡아 화면에서 옮깁니다"),
        &KaDrawingStudio::usePanTool);
  addTb(QStringLiteral("layout_zoom_full"), QStringLiteral("용지 전체"),
        QStringLiteral("용지가 화면에 크게 보이게 합니다"),
        &KaDrawingStudio::zoomFull);
  addTb(QStringLiteral("layout_activate"), QStringLiteral("지도 밀기"),
        QStringLiteral("칸 안의 지도를 밀고 휠로 확대합니다"),
        &KaDrawingStudio::beginActivateMap);
  addTb(QStringLiteral("layout_center"), QStringLiteral("레이어를 가운데"),
        QStringLiteral("지금 축척을 유지한 채, 고른 레이어를 칸 한가운데로 옮깁니다"),
        &KaDrawingStudio::centerSurveyInMap);
  addTb(QStringLiteral("crs"), QStringLiteral("좌표 격자"),
        QStringLiteral("좌표 격자, 간격, 칸 밖 숫자를 설정합니다"),
        &KaDrawingStudio::focusGridSettings);
  m_actEndAdjust = addTb(QStringLiteral("layout_activate_done"), QStringLiteral("밀기 끝"),
                         QStringLiteral("지도 밀기를 끝냅니다"),
                         &KaDrawingStudio::endActivateMap);
  m_actEndAdjust->setVisible(false);
  tb->addAction(KaIcons::icon(QStringLiteral("trash")), QStringLiteral("지우기"), this,
                &KaDrawingStudio::deleteSelectedItems)
      ->setToolTip(QStringLiteral("고른 항목을 지웁니다 (Delete)"));
  auto* spacer = new QWidget(tb);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  tb->addWidget(spacer);
  QAction* pdfAct = addTb(QStringLiteral("pdf"), QStringLiteral("PDF 저장"),
                          QStringLiteral("지금 용지를 PDF 파일로 저장합니다"),
                          &KaDrawingStudio::savePdf);
  if (auto* b = qobject_cast<QToolButton*>(tb->widgetForAction(pdfAct)))
    b->setObjectName(QStringLiteral("btnPrimary"));

  auto* leftCol = new QFrame(root);
  leftCol->setObjectName(QStringLiteral("studioLeftCol"));
  leftCol->setMinimumWidth(220);
  leftCol->setMaximumWidth(280);
  auto* leftLay = new QVBoxLayout(leftCol);
  leftLay->setContentsMargins(10, 10, 10, 10);
  leftLay->setSpacing(6);

  auto* layerBox = new QFrame(leftCol);
  layerBox->setObjectName(QStringLiteral("layersCard"));
  auto* layerLay = new QVBoxLayout(layerBox);
  layerLay->setContentsMargins(8, 8, 8, 8);
  layerLay->setSpacing(4);
  auto* leftCap = new QLabel(QStringLiteral("레이어"), layerBox);
  leftCap->setObjectName(QStringLiteral("cardCaption"));
  m_layerModel = new QgsLayerTreeModel(QgsProject::instance()->layerTreeRoot(), this);
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  m_layerTree = new QgsLayerTreeView(layerBox);
  m_layerTree->setObjectName(QStringLiteral("layoutLayerTree"));
  m_layerTree->setModel(m_layerModel);
  m_layerTree->setMinimumWidth(160);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(m_layerTree->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &KaDrawingStudio::syncMapFromLayers);
  connect(m_layerModel, &QAbstractItemModel::dataChanged, this, &KaDrawingStudio::syncMapFromLayers);
  if (QgsLayerTree* tree = QgsProject::instance()->layerTreeRoot()) {
    connect(tree, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode*) { syncMapFromLayers(); });
  }
  layerLay->addWidget(leftCap);
  layerLay->addWidget(m_layerTree, 1);
  auto* layerEmpty = new QLabel(
      QStringLiteral("이 도면에는 레이어가 없습니다.\n필요한 레이어를 켜고\n용지 위에 올려 보세요."),
      layerBox);
  layerEmpty->setObjectName(QStringLiteral("emptyState"));
  layerEmpty->setAlignment(Qt::AlignCenter);
  layerEmpty->setWordWrap(true);
  layerEmpty->setVisible(QgsProject::instance()->mapLayers().isEmpty());
  layerLay->addWidget(layerEmpty);
  auto syncEmpty = [this, layerEmpty]() {
    const bool empty = !QgsProject::instance() || QgsProject::instance()->mapLayers().isEmpty();
    if (layerEmpty) layerEmpty->setVisible(empty);
    if (m_layerTree) m_layerTree->setVisible(!empty);
  };
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [syncEmpty](const QList<QgsMapLayer*>&) { syncEmpty(); });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this,
          [syncEmpty](const QStringList&) { syncEmpty(); });
  syncEmpty();

  m_inspector = nullptr;
  m_inspectorCap = nullptr;
  m_legendProps = nullptr;
  leftLay->addWidget(layerBox, 1);

  auto* right = new QWidget(root);
  auto* rightLay = new QVBoxLayout(right);
  rightLay->setContentsMargins(0, 0, 0, 0);
  rightLay->setSpacing(0);

  m_adjustBar = new QFrame(right);
  m_adjustBar->setObjectName(QStringLiteral("mapAdjustBar"));
  auto* barLay = new QHBoxLayout(m_adjustBar);
  barLay->setContentsMargins(12, 8, 12, 8);
  auto* barTxt = new QLabel(
      QStringLiteral("지도 조정 중 — 드래그로 이동, 휠로 확대. 끝나면 「조정끝」."),
      m_adjustBar);
  auto* btnCenter = new QPushButton(QStringLiteral("레이어를 가운데"), m_adjustBar);
  auto* btnDone = new QPushButton(QStringLiteral("조정끝"), m_adjustBar);
  btnDone->setObjectName(QStringLiteral("btnAdjustDone"));
  connect(btnCenter, &QPushButton::clicked, this, &KaDrawingStudio::centerSurveyInMap);
  connect(btnDone, &QPushButton::clicked, this, &KaDrawingStudio::endActivateMap);
  barLay->addWidget(barTxt, 1);
  barLay->addWidget(btnCenter);
  barLay->addWidget(btnDone);
  m_adjustBar->hide();

  m_view = new QgsLayoutView(right);
  m_view->setObjectName(QStringLiteral("layoutView"));
  KaTheme::excludeMapSurface(m_view);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setBackgroundBrush(QBrush(QColor(232, 228, 220)));
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

  auto* side = new QFrame(root);
  side->setObjectName(QStringLiteral("studioLeftCol"));
  side->setMinimumWidth(260);
  side->setMaximumWidth(300);
  auto* sideLay = new QVBoxLayout(side);
  sideLay->setContentsMargins(10, 10, 10, 10);
  sideLay->setSpacing(10);

  m_cardLegend = new QFrame(side);
  m_cardLegend->setObjectName(QStringLiteral("itemInspector"));
  auto* legendLay = new QVBoxLayout(m_cardLegend);
  legendLay->setContentsMargins(10, 10, 10, 10);
  legendLay->setSpacing(6);
  auto* legendCap = new QLabel(QStringLiteral("범례"), m_cardLegend);
  legendCap->setObjectName(QStringLiteral("cardCaption"));
  legendLay->addWidget(legendCap);
  auto* legendBtn = makeRailTile(m_cardLegend, KaIcons::icon(QStringLiteral("layout_legend")),
                                 QStringLiteral("범례 넣기"), QSize(22, 22));
  connect(legendBtn, &QToolButton::clicked, this, [this]() {
    beginPlaceLegend();
    if (m_cardLegend) m_cardLegend->setFocus();
  });
  legendLay->addWidget(legendBtn);
  m_legendTitle = new QLineEdit(m_cardLegend);
  m_legendTitle->setPlaceholderText(QStringLiteral("제목을 입력하세요"));
  m_legendTitle->setText(QStringLiteral("범례"));
  connect(m_legendTitle, &QLineEdit::textChanged, this, &KaDrawingStudio::applyLegendSettings);
  legendLay->addWidget(new QLabel(QStringLiteral("제목"), m_cardLegend));
  legendLay->addWidget(m_legendTitle);
  auto* fontRow = new QHBoxLayout;
  m_legendFont = new QSpinBox(m_cardLegend);
  m_legendFont->setRange(7, 24);
  m_legendFont->setValue(10);
  m_legendFont->setSuffix(QStringLiteral(" pt"));
  connect(m_legendFont, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &KaDrawingStudio::applyLegendSettings);
  m_legendBold = new QCheckBox(QStringLiteral("B"), m_cardLegend);
  m_legendBold->setChecked(true);
  m_legendItalic = new QCheckBox(QStringLiteral("I"), m_cardLegend);
  connect(m_legendBold, &QCheckBox::toggled, this, &KaDrawingStudio::applyLegendSettings);
  connect(m_legendItalic, &QCheckBox::toggled, this, &KaDrawingStudio::applyLegendSettings);
  fontRow->addWidget(new QLabel(QStringLiteral("글자"), m_cardLegend));
  fontRow->addWidget(m_legendFont, 1);
  fontRow->addWidget(m_legendBold);
  fontRow->addWidget(m_legendItalic);
  legendLay->addLayout(fontRow);
  sideLay->addWidget(m_cardLegend);

  m_cardNorth = new QFrame(side);
  m_cardNorth->setObjectName(QStringLiteral("itemInspector"));
  auto* northLay = new QVBoxLayout(m_cardNorth);
  northLay->setContentsMargins(10, 10, 10, 10);
  northLay->setSpacing(6);
  northLay->addWidget(new QLabel(QStringLiteral("방위"), m_cardNorth));
  auto* northRow = new QHBoxLayout;
  northRow->setSpacing(6);
  struct NorthSample { const char* rel; const char* tip; int art; };
  const NorthSample norths[] = {
      {"", "북 글자", 0},
      {"arrows/NorthArrow_02.svg", "북 화살", 1},
      {"arrows/NorthArrow_04.svg", "나침반", 2},
      {"wind_roses/WindRose_01.svg", "바람장미", 3},
  };
  for (const auto& ns : norths) {
    auto* b = makeRailTile(m_cardNorth, northPreviewIcon(ns.art), QString::fromUtf8(ns.tip), QSize(32, 32));
    const QString rel = QString::fromUtf8(ns.rel);
    connect(b, &QToolButton::clicked, this, [this, rel]() { beginPlaceNorth(rel); });
    northRow->addWidget(b);
  }
  northLay->addLayout(northRow);
  auto* northSizeRow = new QHBoxLayout;
  northSizeRow->addWidget(new QLabel(QStringLiteral("크기"), m_cardNorth));
  m_northSize = new QDoubleSpinBox(m_cardNorth);
  m_northSize->setRange(12.0, 80.0);
  m_northSize->setDecimals(0);
  m_northSize->setSuffix(QStringLiteral(" mm"));
  m_northSize->setValue(28.0);
  connect(m_northSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double mm) {
    auto* it = findItemById(layout(), kIdNorth);
    if (!it) return;
    QRectF r = it->sceneBoundingRect();
    r.setWidth(mm);
    r.setHeight(mm);
    it->attemptSetSceneRect(r);
    it->update();
  });
  northSizeRow->addWidget(m_northSize, 1);
  northLay->addLayout(northSizeRow);
  sideLay->addWidget(m_cardNorth);

  m_scaleBar = new QFrame(side);
  m_scaleBar->setObjectName(QStringLiteral("itemInspector"));
  auto* scaleLay = new QVBoxLayout(m_scaleBar);
  scaleLay->setContentsMargins(10, 10, 10, 10);
  scaleLay->setSpacing(8);
  scaleLay->addWidget(new QLabel(QStringLiteral("축척"), m_scaleBar));
  m_scaleSpin = new QSpinBox(m_scaleBar);
  m_scaleSpin->setRange(10, 5000000);
  m_scaleSpin->setSingleStep(10);
  m_scaleSpin->setValue(1000);
  m_scaleSpin->setKeyboardTracking(false);
  m_scaleSpin->setGroupSeparatorShown(false);
  auto* applySc = new QPushButton(QStringLiteral("적용"), m_scaleBar);
  connect(applySc, &QPushButton::clicked, this, &KaDrawingStudio::applyOnScreenScale);
  connect(m_scaleSpin, &QSpinBox::editingFinished, this, &KaDrawingStudio::applyOnScreenScale);
  auto* scaleTop = new QHBoxLayout;
  scaleTop->addWidget(new QLabel(QStringLiteral("1 :"), m_scaleBar));
  scaleTop->addWidget(m_scaleSpin, 1);
  scaleTop->addWidget(applySc);
  scaleLay->addLayout(scaleTop);
  m_scaleProps = m_scaleBar;
  auto addChipRow = [&](const int* vals, int count) {
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    for (int i = 0; i < count; ++i) {
      const int n = vals[i];
      auto* chip = new QToolButton(m_scaleBar);
      chip->setObjectName(QStringLiteral("scaleChip"));
      chip->setText(QString::number(n));
      chip->setToolButtonStyle(Qt::ToolButtonTextOnly);
      chip->setCheckable(true);
      chip->setCursor(Qt::PointingHandCursor);
      chip->setMinimumSize(56, 36);
      chip->setProperty("denom", n);
      connect(chip, &QToolButton::clicked, this, [this, n]() {
        if (m_scaleSpin) m_scaleSpin->setValue(n);
        applyOnScreenScale();
      });
      row->addWidget(chip);
    }
    row->addStretch(1);
    scaleLay->addLayout(row);
  };
  const int rowA[] = {100, 200, 250};
  const int rowB[] = {300, 400, 500};
  const int rowC[] = {1000, 2000, 5000};
  addChipRow(rowA, 3);
  addChipRow(rowB, 3);
  addChipRow(rowC, 3);
  auto* barRow = new QHBoxLayout;
  barRow->setSpacing(6);
  struct BarSample { const char* style; const char* tip; };
  const BarSample bars[] = {
      {"Double Box", "쌍칸"},
      {"Single Box", "외칸"},
      {"Line Ticks Up", "눈금"},
  };
  for (const auto& bs : bars) {
    auto* b = makeRailTile(m_scaleBar, scaleBarPreviewIcon(bs.style), QString::fromUtf8(bs.tip), QSize(40, 22));
    const QString style = QString::fromUtf8(bs.style);
    connect(b, &QToolButton::clicked, this, [this, style]() { beginPlaceScaleBar(style); });
    barRow->addWidget(b);
  }
  scaleLay->addLayout(barRow);
  auto* extraRow = new QHBoxLayout;
  auto* scaleLblBtn = makeRailTile(m_scaleBar, KaIcons::icon(QStringLiteral("layout_scale")),
                                   QStringLiteral("축척 글자"), QSize(24, 24));
  connect(scaleLblBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceScaleLabel);
  extraRow->addWidget(scaleLblBtn);
  auto* crsBtn = makeRailTile(m_scaleBar, KaIcons::icon(QStringLiteral("crs")),
                              QStringLiteral("좌표계"), QSize(24, 24));
  connect(crsBtn, &QToolButton::clicked, this, &KaDrawingStudio::beginPlaceCrsLabel);
  extraRow->addWidget(crsBtn);
  extraRow->addStretch(1);
  scaleLay->addLayout(extraRow);
  sideLay->addWidget(m_scaleBar);
  sideLay->addStretch(1);

  rightLay->addWidget(m_adjustBar);
  rightLay->addWidget(m_view, 1);

  rootLay->addWidget(leftCol, 0);
  rootLay->addWidget(right, 1);
  rootLay->addWidget(side, 0);
  setCentralWidget(root);

  m_status = new QLabel(this);
  statusBar()->addWidget(m_status, 1);
  m_status->setText(QStringLiteral("용지 위에 올릴 걸 골라 두었습니다. 축척을 맞추거나 레이어를 가운데로 옮기세요."));
  m_paperFitPending = true;
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
    m_status->setText(QStringLiteral("칸을 다시 그리려면 용지 위에서 드래그하세요. 아니면 이미 올라간 지도를 쓰세요."));
}

QRectF KaDrawingStudio::defaultMapRect() const {
  const double left = 22.0;
  const double top = 12.0;
  const double right = 16.0;
  const double bottom = 32.0;
  return QRectF(left, top, std::max(40.0, m_paperW - left - right),
                std::max(40.0, m_paperH - top - bottom));
}

void KaDrawingStudio::autoPlaceDefaultSheet() {
  createOrResizeMap(defaultMapRect());
  zoomPaperVisible();
}

void KaDrawingStudio::refreshMapFromProject() {
  syncMapFromLayers();
  if (auto* map = mapItem())
    applyCrsGrid(map);
}

void KaDrawingStudio::focusGridSettings() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("격자 설정"));
  dlg.setMinimumWidth(280);
  auto* root = new QVBoxLayout(&dlg);
  root->setContentsMargins(14, 12, 14, 12);
  root->setSpacing(8);
  auto* on = new QCheckBox(QStringLiteral("좌표 격자"), &dlg);
  on->setChecked(m_gridEnabled);
  auto* nums = new QCheckBox(QStringLiteral("칸 밖에 좌표 숫자"), &dlg);
  nums->setChecked(m_gridShowNums);
  auto* step = new QDoubleSpinBox(&dlg);
  step->setRange(1.0, 100000.0);
  step->setDecimals(0);
  step->setSingleStep(10.0);
  step->setSuffix(QStringLiteral(" m"));
  step->setValue(m_gridIntervalM);
  step->setKeyboardTracking(false);
  auto apply = [this, on, nums, step]() {
    m_gridEnabled = on->isChecked();
    m_gridShowNums = nums->isChecked();
    m_gridIntervalM = step->value() > 0.0 ? step->value() : 20.0;
    if (auto* map = mapItem())
      applyCrsGrid(map);
    if (m_status)
      m_status->setText(QStringLiteral("격자 간격 %1 m").arg(m_gridIntervalM, 0, 'f', 0));
  };
  connect(on, &QCheckBox::toggled, &dlg, [apply](bool) { apply(); });
  connect(nums, &QCheckBox::toggled, &dlg, [apply](bool) { apply(); });
  connect(step, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
          [apply](double) { apply(); });
  connect(step, &QDoubleSpinBox::editingFinished, &dlg, [apply]() { apply(); });
  root->addWidget(on);
  auto* stepRow = new QHBoxLayout;
  stepRow->addWidget(new QLabel(QStringLiteral("간격"), &dlg));
  stepRow->addWidget(step, 1);
  root->addLayout(stepRow);
  root->addWidget(nums);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  box->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  root->addWidget(box);
  apply();
  dlg.exec();
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
  zoomPaperVisible();
}

void KaDrawingStudio::zoomPaperVisible() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  QgsLayoutItemPage* page = ly->pageCollection() ? ly->pageCollection()->page(0) : nullptr;
  QRectF pr = page ? page->sceneBoundingRect() : QRectF(0, 0, m_paperW, m_paperH);
  if (pr.width() < 8.0 || pr.height() < 8.0)
    pr = QRectF(0, 0, m_paperW, m_paperH);
  const qreal mx = pr.width() * 0.125;
  const qreal my = pr.height() * 0.125;
  m_view->fitInView(pr.adjusted(-mx, -my, mx, my), Qt::KeepAspectRatio);
  updatePageOutline();
}

void KaDrawingStudio::updatePageOutline() {
  // Do not add a raw QGraphicsRectItem into the QgsLayout scene.
  // QGIS item resize/export walks the scene as layout items and that crash-exits the app.
}

void KaDrawingStudio::clearGridCoordinateLabels() {
  auto* ly = layout();
  if (!ly) return;
  QList<QgsLayoutItemLabel*> labels;
  ly->layoutItems(labels);
  for (QgsLayoutItemLabel* lbl : labels) {
    if (lbl && lbl->id().startsWith(QLatin1String(kGridAnnPrefix)))
      ly->removeLayoutItem(lbl);
  }
}

void KaDrawingStudio::placeGridCoordinateLabels(QgsLayoutItemMap* map) {
  auto* ly = layout();
  if (!ly || !map) return;
  clearGridCoordinateLabels();
  if (!m_gridShowNums) return;
  const QgsRectangle ext = map->extent();
  if (!ext.isFinite() || ext.width() <= 0.0 || ext.height() <= 0.0) return;
  const double step = m_gridIntervalM > 0.0 ? m_gridIntervalM : 20.0;
  const QRectF r = map->sceneBoundingRect();
  auto toX = [&](double mx) {
    return r.left() + (mx - ext.xMinimum()) / ext.width() * r.width();
  };
  auto toY = [&](double my) {
    return r.bottom() - (my - ext.yMinimum()) / ext.height() * r.height();
  };
  auto first = [&](double lo) { return std::ceil(lo / step) * step; };

  QVector<double> xs, ys;
  for (double x = first(ext.xMinimum()); x <= ext.xMaximum() + 1e-6; x += step) xs.push_back(x);
  for (double y = first(ext.yMinimum()); y <= ext.yMaximum() + 1e-6; y += step) ys.push_back(y);
  const int xStride = std::max(1, static_cast<int>(xs.size()) / 16);
  const int yStride = std::max(1, static_cast<int>(ys.size()) / 16);

  int n = 0;
  auto addLbl = [&](const QRectF& box, const QString& text, Qt::AlignmentFlag align) {
    auto* lbl = new QgsLayoutItemLabel(ly);
    lbl->setId(QStringLiteral("%1%2").arg(QLatin1String(kGridAnnPrefix)).arg(n++));
    lbl->setText(text);
    lbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 6));
    lbl->setHAlign(align);
    lbl->setVAlign(Qt::AlignVCenter);
    lbl->setFrameEnabled(false);
    lbl->setBackgroundEnabled(false);
    lbl->attemptSetSceneRect(box);
    ly->addLayoutItem(lbl);
  };

  for (int i = 0; i < xs.size(); i += xStride) {
    const double x = xs[i];
    const double lx = toX(x);
    if (lx < r.left() - 0.5 || lx > r.right() + 0.5) continue;
    const QString t = QString::number(x, 'f', 2);
    addLbl(QRectF(lx - 11.0, r.top() - 4.6, 22.0, 4.2), t, Qt::AlignHCenter);
    addLbl(QRectF(lx - 11.0, r.bottom() + 0.6, 22.0, 4.2), t, Qt::AlignHCenter);
  }
  for (int i = 0; i < ys.size(); i += yStride) {
    const double y = ys[i];
    const double lyout = toY(y);
    if (lyout < r.top() - 0.5 || lyout > r.bottom() + 0.5) continue;
    const QString t = QString::number(y, 'f', 2);
    addLbl(QRectF(r.left() - 24.0, lyout - 2.1, 23.0, 4.2), t, Qt::AlignRight);
    addLbl(QRectF(r.right() + 0.8, lyout - 2.1, 23.0, 4.2), t, Qt::AlignLeft);
  }
}

void KaDrawingStudio::applyCrsGrid(QgsLayoutItemMap* map) {
  if (!map || !map->grids()) return;
  QStringList ids;
  const QList<QgsLayoutItemMapGrid*> olds = map->grids()->asList();
  for (QgsLayoutItemMapGrid* old : olds) {
    if (!old) continue;
    old->setEnabled(false);
    old->setAnnotationEnabled(false);
    if (!old->id().isEmpty()) ids.append(old->id());
    if (!old->name().isEmpty()) ids.append(old->name());
  }
  ids.removeDuplicates();
  for (const QString& id : ids)
    map->grids()->removeGrid(id);
  if (!m_gridEnabled) {
    clearGridCoordinateLabels();
    map->updateBoundingRect();
    map->invalidateCache();
    map->refresh();
    map->update();
    if (auto* ly = layout()) ly->refresh();
    return;
  }
  auto* g = new QgsLayoutItemMapGrid(QStringLiteral("crs_grid"), map);
  map->grids()->addGrid(g);
  const QgsCoordinateReferenceSystem crs = map->crs().isValid()
      ? map->crs()
      : studioMapCrs(m_mapCanvas, m_project);
  if (crs.isValid())
    g->setCrs(crs);
  g->setEnabled(true);
  g->setUnits(Qgis::MapGridUnit::MapUnits);
  g->setStyle(Qgis::MapGridStyle::Lines);
  const double interval = m_gridIntervalM > 0.0 ? m_gridIntervalM : 20.0;
  g->setIntervalX(interval);
  g->setIntervalY(interval);
  g->setOffsetX(0.0);
  g->setOffsetY(0.0);
  g->setGridLineWidth(0.07);
  g->setGridLineColor(QColor(30, 41, 59, 128));
  g->setFrameStyle(Qgis::MapGridFrameStyle::NoFrame);
  g->setAnnotationEnabled(false);
  map->updateBoundingRect();
  map->invalidateCache();
  map->refresh();
  placeGridCoordinateLabels(map);
  if (auto* ly = layout())
    ly->refresh();
}

void KaDrawingStudio::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  if (m_paperFitPending) {
    m_paperFitPending = false;
    QTimer::singleShot(0, this, [this]() { zoomPaperVisible(); });
  }
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
  map->setFollowVisibilityPreset(false);
  map->setLayers(layers);
  map->invalidateCache();
  map->refresh();
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
    applyCrsGrid(held);
    snapMapScaleToNice();
    connect(held, &QgsLayoutItemMap::extentChanged, this, &KaDrawingStudio::syncScaleDecorations,
            Qt::UniqueConnection);
    if (created)
      ensureStandardDecorations();
    else {
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
  const int weight = (m_legendBold && m_legendBold->isChecked()) ? QFont::Bold : QFont::Normal;
  QFont titleFont(QStringLiteral("Malgun Gothic"), pt, weight);
  if (m_legendItalic) titleFont.setItalic(m_legendItalic->isChecked());
  QFont bodyFont(QStringLiteral("Malgun Gothic"), pt, QFont::Normal);
  if (m_legendItalic) bodyFont.setItalic(m_legendItalic->isChecked());
  legend->setStyleFont(Qgis::LegendComponent::Title, titleFont);
  legend->setStyleFont(Qgis::LegendComponent::Group, bodyFont);
  legend->setStyleFont(Qgis::LegendComponent::Subgroup, bodyFont);
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
  if (auto* map = mapItem()) {
    applyLayersToMap(map, true, false);
    if (auto* ly = layout())
      ly->refresh();
  }
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
}

int KaDrawingStudio::displayScale(double raw) {
  if (!(raw > 0.0) || !std::isfinite(raw))
    return 1000;
  return std::max(10, static_cast<int>(std::lround(raw)));
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
  applyCrsGrid(map);
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

  if (m_scaleBar) m_scaleBar->setVisible(true);
  if (m_cardLegend) m_cardLegend->setVisible(true);
  if (m_cardNorth) m_cardNorth->setVisible(true);

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
  const int wanted = std::max(10, m_scaleSpin->value());
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
  const QSet<QString> drawnKeys = {QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                                   QStringLiteral("feature_line"), QStringLiteral("section_line"),
                                   QStringLiteral("control_points"),
                                   QStringLiteral("artifact_point")};
  for (QgsMapLayer* ml : m_project->mapLayers()) {
    if (!ml || !ml->isValid()) continue;
    if (LayerOps::isReferenceLayer(ml) || isLiveBasemapLayer(ml)) continue;
    const QString key = LayerOps::layerKeyOf(ml);
    if (!drawnKeys.contains(key)) continue;
    if (auto* root = m_project->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = root->findLayer(ml->id())) {
        if (!n->isVisible()) continue;
      }
    }
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
  if (event && event->type() == QEvent::Resize && m_paperFitPending && m_view &&
      (watched == m_view || watched == m_view->viewport())) {
    QTimer::singleShot(0, this, [this]() { zoomPaperVisible(); });
  }
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
  QgsRectangle target;
  QgsMapLayer* layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  if (layer && layer->isValid()) {
    target = layer->extent();
    if (m_project && layer->crs().isValid() && map->crs().isValid() && layer->crs() != map->crs()) {
      try {
        const QgsCoordinateTransform xf(layer->crs(), map->crs(), m_project->transformContext());
        target = xf.transformBoundingBox(target);
      } catch (...) {
      }
    }
  }
  if (target.isNull() || !target.isFinite())
    target = surveyExtentOnMap(map);
  if (target.isNull() || !target.isFinite()) {
    if (m_status)
      m_status->setText(QStringLiteral("가운데로 둘 레이어를 왼쪽에서 고르세요."));
    return;
  }
  const double keepScale = map->scale();
  const QgsRectangle cur = map->extent();
  const QgsPointXY c = target.center();
  const double hw = (cur.isFinite() && cur.width() > 0.0) ? cur.width() * 0.5 : target.width() * 0.5;
  const double hh = (cur.isFinite() && cur.height() > 0.0) ? cur.height() * 0.5 : target.height() * 0.5;
  map->setExtent(QgsRectangle(c.x() - hw, c.y() - hh, c.x() + hw, c.y() + hh));
  if (keepScale > 1.0)
    map->setScale(keepScale);
  map->invalidateCache();
  map->refresh();
  if (auto* ly = layout())
    ly->refresh();
  refreshScaleWidgets(true);
  applyCrsGrid(map);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (m_status)
    m_status->setText(QStringLiteral("축척 1 : %1 을 유지한 채 레이어를 가운데로 옮겼습니다.")
                          .arg(displayScale(map->scale())));
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
  if (m_pageOutline) m_pageOutline->hide();
  QgsLayoutExporter exporter(ly);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  const auto pdfOk = exporter.exportToPdf(path, settings);
  if (m_pageOutline) m_pageOutline->show();
  if (pdfOk != QgsLayoutExporter::Success) {
    QMessageBox::warning(this, QStringLiteral("PDF"), QStringLiteral("저장에 실패했습니다."));
    return;
  }
  if (m_status) m_status->setText(QStringLiteral("저장: %1").arg(path));
  QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("저장했습니다.\n%1").arg(path));
}

#include "KaDrawingStudio.moc"
