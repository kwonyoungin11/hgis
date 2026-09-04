#include "KaDrawingStudio.h"
#include "KaTheme.h"
#include "KaIcons.h"
#include "KaBeginnerRibbon.h"
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
#include <QMouseEvent>
#include <QScrollBar>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
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
#include <QTransform>
#include <QWheelEvent>
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
#include <qgslayoutpoint.h>
#include <qgslayoutsize.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgsmapsettings.h>
#include <qgslayoutitemmapgrid.h>
#include <qgstextformat.h>
#include <qgslayoutitempage.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitempolyline.h>
#include <qgslayoutitempolygon.h>
#include <qgslinesymbol.h>
#include <qgsfillsymbol.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutrendercontext.h>
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
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgswkbtypes.h>

namespace {
constexpr const char* kSheetName = "user_sheet";
constexpr const char* kIdMap = "ka_map";
constexpr const char* kIdLegend = "ka_legend";
constexpr const char* kIdNorth = "ka_north";
constexpr const char* kIdScaleBar = "ka_scalebar";
constexpr const char* kIdScale = "ka_scale";
constexpr const char* kIdCrs = "ka_crs";
constexpr const char* kIdTerrain3d = "ka_terrain3d";
constexpr const char* kGridAnnPrefix = "ka_grid_ann_";
// 화면 미리보기 해상도. 인쇄용 300 DPI로 미리보기를 그리면 A4 지도 칸이
// 2300×3000픽셀이 되어 QGIS가 래스터를 2000픽셀 조각으로 나누고 위성 타일도
// 수백 장 필요해진다. 조각 하나가 비면 위성이 반만 나온 것처럼 보인다.
constexpr double kPreviewDpi = 96.0;

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
    // 측량도면식: N 글자 위 + 반채움 니들(좌흑·우백).
    p.setPen(ink);
    p.setFont(QFont(QStringLiteral("Malgun Gothic"), 13, QFont::Bold));
    p.drawText(QRectF(8, 2, 56, 16), Qt::AlignCenter, QStringLiteral("N"));
    const QPointF tip(36, 20);
    const QPointF tail(36, 58);
    const double halfW = 6.5;
    QPainterPath leftP;
    leftP.moveTo(tip);
    leftP.lineTo(tail.x() - halfW, 64);
    leftP.lineTo(tail);
    leftP.closeSubpath();
    QPainterPath rightP;
    rightP.moveTo(tip);
    rightP.lineTo(tail.x() + halfW, 64);
    rightP.lineTo(tail);
    rightP.closeSubpath();
    p.setPen(QPen(ink, 1.0));
    p.setBrush(ink);
    p.drawPath(leftP);
    p.setBrush(Qt::white);
    p.drawPath(rightP);
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
  if (auth.isEmpty())
    return QStringLiteral("좌표계 미지정");
  const QString name = LayoutService::koreanCrsName(auth);
  return name.isEmpty() ? QStringLiteral("좌표계 %1").arg(auth)
                        : QStringLiteral("%1 · %2").arg(name, auth);
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

QRectF itemPaperRect(QgsLayoutItem* item) {
  if (!item)
    return {};
  const QRectF box(item->pos(), item->rect().size());
  if (box.width() >= 8.0 && box.height() >= 8.0)
    return box;
  return item->sceneBoundingRect();
}

void lockSheetChromeItem(QgsLayoutItem* item) {
  if (!item)
    return;
  item->setLocked(true);
}

void compactSheetScaleBar(QgsLayoutItemScaleBar* sb) {
  if (!sb)
    return;
  sb->setLabelVerticalPlacement(Qgis::ScaleBarDistanceLabelVerticalPlacement::AboveSegment);
  sb->setHeight(2.4);
  sb->setLabelBarSpace(1.0);
  sb->setBoxContentSpace(0.6);
  QgsTextFormat fmt = sb->textFormat();
  QFont f(QStringLiteral("Malgun Gothic"), 7);
  fmt.setFont(f);
  fmt.setSize(7.0);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  sb->setTextFormat(fmt);
  LayoutService::applySheetScaleBarInk(sb);
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
  b->setText(KaBeginnerRibbon::twoLine(text));
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setToolTip(text);
  b->setMinimumHeight(52);
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

class KaLayoutCoordPointTool : public QgsLayoutViewTool {
  Q_OBJECT
signals:
  void pointClicked(const QPointF& layoutPt);
  void undoRequested();
public:
  explicit KaLayoutCoordPointTool(QgsLayoutView* view)
      : QgsLayoutViewTool(view, QStringLiteral("좌표점")) {
    setCursor(Qt::CrossCursor);
  }
  void layoutPressEvent(QgsLayoutViewMouseEvent* event) override {
    if (!event) return;
    if (event->button() == Qt::RightButton) {
      emit undoRequested();
      event->accept();
      return;
    }
    if (event->button() != Qt::LeftButton) {
      event->ignore();
      return;
    }
    emit pointClicked(event->layoutPoint());
    event->accept();
  }
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
    mZoomTimer.setInterval(220);
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
    mZoomTimer.start();
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
    const double cur = mPendingMap->scale();
    if (cur > 10.0) {
      // 휠 위치(mPendingPoint)를 고정점으로 확대·축소한다. 칸 크기는 그대로 두려고
      // setExtent가 아니라 zoomToExtent를 쓴다.
      const QRectF box = mPendingMap->rect();
      const QgsRectangle ext = mPendingMap->extent();
      const bool canAnchor = box.width() > 0.0 && box.height() > 0.0 && ext.isFinite() &&
                             ext.width() > 0.0 && ext.height() > 0.0;
      if (canAnchor) {
        const double fx = (mPendingPoint.x() - box.left()) / box.width();
        const double fy = (mPendingPoint.y() - box.top()) / box.height();
        mPendingMap->zoomToExtent(
            LayoutService::zoomExtentAtAnchor(ext, fx, fy, mPendingFactor));
      } else {
        mPendingMap->setScale(cur / mPendingFactor, true);
      }
    }
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
    if (m_studio->isPlacingCoordPoint()) {
      auto* menu = new QMenu(parent);
      menu->addAction(QStringLiteral("마지막 점 지우기"), m_studio,
                      &KaDrawingStudio::undoLastCoordCallout);
      menu->addAction(QStringLiteral("점찍기 끝"), m_studio, &KaDrawingStudio::endPlaceCoordPoint);
      return menu;
    }
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
  m_coordMapPts.clear();
  m_coordFrameMap.clear();
  attachLayoutToView();
}

void KaDrawingStudio::attachLayoutToView() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  // 위성 배경이 조각 단위로 빈 채 남는 것을 막는다(다시 열린 조판까지 포함).
  LayoutService::applySingleRasterPassRendering(ly);
  // 화면 미리보기는 화면 해상도로 그린다. 300 DPI로 미리보기를 그리면 A4가
  // 2300×3000픽셀이 되어 위성 타일을 수백 장 받아야 하고, 다 못 받으면 배경이
  // 사각형 단위로 빈다. 인쇄·PDF는 내보낼 때만 300 DPI로 올린다(savePdf).
  ly->renderContext().setDpi(kPreviewDpi);
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
  m_view->setAlignment(Qt::AlignCenter);
  m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setDragMode(QGraphicsView::NoDrag);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setMenuProvider(new KaLayoutMenuProvider(this));
  m_view->installEventFilter(this);
  if (m_view->viewport())
    m_view->viewport()->installEventFilter(this);

  m_toolSelect = new QgsLayoutViewToolSelect(m_view);
  m_toolPan = new QgsLayoutViewToolPan(m_view);
  m_toolMoveContent = new KaLayoutMapAdjustTool(m_view);
  m_toolDrawMap = new KaLayoutMapDrawTool(m_view);
  m_toolCoordPoint = new KaLayoutCoordPointTool(m_view);
  connect(m_toolDrawMap, &KaLayoutMapDrawTool::rectDrawn, this, &KaDrawingStudio::onRectDrawn,
          Qt::QueuedConnection);
  connect(m_toolCoordPoint, &KaLayoutCoordPointTool::pointClicked, this,
          &KaDrawingStudio::placeCoordCallout);
  connect(m_toolCoordPoint, &KaLayoutCoordPointTool::undoRequested, this,
          &KaDrawingStudio::undoLastCoordCallout);
  connect(m_toolMoveContent, &KaLayoutMapAdjustTool::mapViewChanged, this,
          &KaDrawingStudio::syncScaleDecorations);
  attachLayoutToView();

  m_scaleSyncTimer = new QTimer(this);
  m_scaleSyncTimer->setSingleShot(true);
  m_scaleSyncTimer->setInterval(280);
  connect(m_scaleSyncTimer, &QTimer::timeout, this, &KaDrawingStudio::flushHeavyScaleSync);

  auto* side = new QFrame(root);
  side->setObjectName(QStringLiteral("studioLeftCol"));
  side->setMinimumWidth(260);
  side->setMaximumWidth(300);
  auto* sideLay = new QVBoxLayout(side);
  sideLay->setContentsMargins(12, 8, 12, 8);
  sideLay->setSpacing(8);

  m_cardLegend = new QFrame(side);
  m_cardLegend->setObjectName(QStringLiteral("itemInspector"));
  auto* legendLay = new QVBoxLayout(m_cardLegend);
  legendLay->setContentsMargins(10, 10, 10, 10);
  legendLay->setSpacing(6);
  auto* legendCap = new QLabel(QStringLiteral("무엇을 넣을까?"), m_cardLegend);
  legendCap->setObjectName(QStringLiteral("cardCaption"));
  legendLay->addWidget(legendCap);
  auto* legendRow = new QHBoxLayout;
  legendRow->setSpacing(14);
  auto* legendBtn = makeRailTile(m_cardLegend, KaIcons::icon(QStringLiteral("layout_legend")),
                                 QStringLiteral("범례를 넣을까?"), QSize(22, 22));
  connect(legendBtn, &QToolButton::clicked, this, [this]() {
    beginPlaceLegend();
    if (m_cardLegend) m_cardLegend->setFocus();
  });
  auto* pdfBtn = makeRailTile(m_cardLegend, KaIcons::icon(QStringLiteral("pdf")),
                              QStringLiteral("PDF로 내보낼까?"), QSize(22, 22));
  pdfBtn->setObjectName(QStringLiteral("btnPrimary"));
  pdfBtn->setToolTip(QStringLiteral("지금 용지를 PDF 파일로 저장합니다"));
  connect(pdfBtn, &QToolButton::clicked, this, &KaDrawingStudio::savePdf);
  legendRow->addWidget(legendBtn, 1);
  legendRow->addWidget(pdfBtn, 1);
  legendLay->addLayout(legendRow);
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
  northLay->addWidget(new QLabel(QStringLiteral("방위를 넣을까?"), m_cardNorth));
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
  scaleLay->addWidget(new QLabel(QStringLiteral("도면 정보를 고칠까?"), m_scaleBar));
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
      chip->setMinimumSize(48, 26);
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

  auto* desk = new QWidget(right);
  auto* deskGrid = new QGridLayout(desk);
  deskGrid->setContentsMargins(0, 0, 0, 0);
  deskGrid->setSpacing(0);
  deskGrid->addWidget(m_view, 0, 0);
  deskGrid->addWidget(m_adjustBar, 0, 0, Qt::AlignTop);
  m_adjustBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  rightLay->addWidget(desk, 1);

  auto* bottomTools = new QWidget(desk);
  bottomTools->setObjectName(QStringLiteral("layoutBottomTools"));
  bottomTools->setAttribute(Qt::WA_StyledBackground, true);
  bottomTools->setStyleSheet(QStringLiteral(
      "QWidget#layoutBottomTools { background: rgba(246,241,232,210); border-radius: 10px; }"));
  auto* btLay = new QHBoxLayout(bottomTools);
  btLay->setContentsMargins(14, 6, 14, 6);
  btLay->setSpacing(22);
  deskGrid->addWidget(bottomTools, 0, 0, Qt::AlignHCenter | Qt::AlignBottom);
  bottomTools->raise();
  auto addBottom = [this, bottomTools](const QString& iconId, const QString& text,
                                       const QString& tip, auto slot) {
    auto* b = new QToolButton(bottomTools);
    b->setIcon(KaIcons::icon(iconId));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIconSize(QSize(22, 22));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    connect(b, &QToolButton::clicked, this, slot);
    return b;
  };
  btLay->addWidget(addBottom(QStringLiteral("layout_coord_point"), QStringLiteral("좌표점"),
                             QStringLiteral("용지에서 꼭짓점을 찍으면 화살표와 X·Y가 붙습니다"),
                             &KaDrawingStudio::beginPlaceCoordPoint));
  btLay->addWidget(addBottom(QStringLiteral("layout_center"), QStringLiteral("레이어를 가운데"),
                             QStringLiteral("축척은 두고, 고른 레이어를 조판 한가운데로 옮깁니다"),
                             &KaDrawingStudio::centerSurveyInMap));
  btLay->addWidget(addBottom(QStringLiteral("layout_select"),
                             KaBeginnerRibbon::twoLine(QStringLiteral("항목을 옮겨볼까?")),
                             QStringLiteral("좌표 상자를 끌어 옮깁니다"),
                             &KaDrawingStudio::useSelectTool));
  side->setMinimumWidth(300);
  side->setMaximumWidth(360);
  rootLay->addWidget(leftCol, 0);
  rootLay->addWidget(right, 1);
  rootLay->addWidget(side, 0);
  setCentralWidget(root);

  m_status = new QLabel(this);
  statusBar()->addWidget(m_status, 1);
  m_status->setText(QStringLiteral(
      "조판 중 — 항목을 끌어 옮기고, 끝나면 「PDF로 내보낼까?」. 작업 좌표계 → 제출 5179."));
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
  const double bottom = 38.0;
  return QRectF(left, top, std::max(40.0, m_paperW - left - right),
                std::max(40.0, m_paperH - top - bottom));
}

void KaDrawingStudio::autoPlaceDefaultSheet() {
  createOrResizeMap(defaultMapRect());
  zoomPaperVisible();
}

void KaDrawingStudio::refreshMapFromProject() {
  syncMapFromLayers();
  ensureStandardDecorations();
}

void KaDrawingStudio::placeTerrain3dPicture(const QString& pngPath, const QgsRectangle& groundExtent,
                                            const QgsCoordinateReferenceSystem& crs) {
  auto* ly = layout();
  if (!ly || pngPath.isEmpty() || !QFile::exists(pngPath))
    return;
  m_holdTerrainExtent = true;
  m_terrainGroundExtent = groundExtent;
  m_terrainCrs = crs;
  QgsLayoutItemMap* map = mapItem();
  QRectF mapR(12.0, 12.0, m_paperW - 24.0, m_paperH - 36.0);
  if (map) {
    const QRectF b = itemPaperRect(map);
    if (b.width() > 8.0 && b.height() > 8.0)
      mapR = b;
    if (crs.isValid())
      map->setCrs(crs);
    if (!groundExtent.isEmpty() && groundExtent.isFinite())
      map->zoomToExtent(groundExtent);
  }
  if (auto* old = findItemById(ly, kIdTerrain3d))
    ly->removeLayoutItem(old);
  auto* pic = new QgsLayoutItemPicture(ly);
  pic->setId(QString::fromUtf8(kIdTerrain3d));
  pic->setPicturePath(pngPath, Qgis::PictureFormat::Raster);
  pic->setMode(Qgis::PictureFormat::Raster);
  pic->setResizeMode(QgsLayoutItemPicture::Zoom);
  pic->setFrameEnabled(false);
  pic->attemptSetSceneRect(mapR);
  pic->refreshPicture();
  ly->addLayoutItem(pic);
  if (map)
    pic->setZValue(map->zValue() + 1.0);
  if (auto* legend = findItemById(ly, kIdLegend))
    legend->setZValue(pic->zValue() + 1.0);
  ensureStandardDecorations();
  applyCrsLabelNow();
}

void KaDrawingStudio::importMapCoordCallouts(const QVector<QgsPointXY>& pts,
                                             const QVector<QString>& letters,
                                             const QVector<QString>& texts,
                                             const QgsGeometry& frame) {
  Q_UNUSED(frame);
  m_importCoordPts = pts;
  m_importCoordLetters = letters;
  m_importCoordTexts = texts;
  QTimer::singleShot(0, this, [this]() { applyImportedCoordCallouts(); });
  QTimer::singleShot(80, this, [this]() { applyImportedCoordCallouts(); });
}

void KaDrawingStudio::applyImportedCoordCallouts() {
  auto* ly = layout();
  auto* map = mapItem();
  const QVector<QgsPointXY>& pts = m_importCoordPts;
  const QVector<QString>& letters = m_importCoordLetters;
  const QVector<QString>& texts = m_importCoordTexts;
  if (!ly || !map || pts.isEmpty()) return;
  QList<QgsLayoutItem*> all;
  ly->layoutItems(all);
  for (QgsLayoutItem* it : all) {
    if (it && it->id().startsWith(QLatin1String("ka_coord_")))
      ly->removeLayoutItem(it);
  }
  m_coordMapPts.clear();
  m_coordFrameMap.clear();
  const QRectF ir = map->rect();
  const QgsRectangle ext = map->extent();
  if (ir.width() < 1.0 || ext.isEmpty() || !ext.isFinite()) return;
  QVector<QRectF> usedBoxes;
  for (int i = 0; i < pts.size(); ++i) {
    const QgsPointXY mapPt = pts.at(i);
    const QPointF item = LayoutService::layoutMapItemFromXy(ext, ir, mapPt.x(), mapPt.y());
    const QPointF tip = map->mapToScene(item);
    const QString tag = i < letters.size() ? letters.at(i)
                                           : QString(QChar(static_cast<char>('A' + (i % 26))));
    const QString text = i < texts.size()
                             ? texts.at(i)
                             : QStringLiteral("X=%1\nY=%2").arg(mapPt.y(), 0, 'f', 3).arg(mapPt.x(), 0, 'f', 3);
    auto* box = new QgsLayoutItemLabel(ly);
    box->setId(QStringLiteral("ka_coord_box_%1").arg(tag));
    box->setText(text);
    box->setFont(QFont(QStringLiteral("Malgun Gothic"), 6));
    box->setHAlign(Qt::AlignLeft);
    box->setVAlign(Qt::AlignVCenter);
    box->setFrameEnabled(true);
    box->setBackgroundEnabled(true);
    box->setBackgroundColor(QColor(255, 255, 255, 235));
    box->setMarginX(0.4);
    box->setMarginY(0.3);
    const qreal bw = 26.0;
    const qreal bh = 7.2;
    const qreal gap = 8.0;
    const QRectF cands[] = {
        QRectF(tip.x() + gap, tip.y() - bh - 4.0, bw, bh),
        QRectF(tip.x() + gap, tip.y() + 4.0, bw, bh),
        QRectF(tip.x() - gap - bw, tip.y() - bh - 4.0, bw, bh),
        QRectF(tip.x() - gap - bw, tip.y() + 4.0, bw, bh),
        QRectF(tip.x() - bw * 0.5, tip.y() - bh - gap, bw, bh),
        QRectF(tip.x() - bw * 0.5, tip.y() + gap, bw, bh),
    };
    QRectF boxR = cands[0];
    for (const QRectF& c : cands) {
      bool hit = false;
      for (const QRectF& u : usedBoxes) {
        if (c.adjusted(-2, -2, 2, 2).intersects(u)) {
          hit = true;
          break;
        }
      }
      if (!hit) {
        boxR = c;
        break;
      }
    }
    usedBoxes.append(boxR);
    const bool placeRight = boxR.center().x() >= tip.x();
    box->attemptSetSceneRect(boxR);
    ly->addLayoutItem(box);
    auto* letterLbl = new QgsLayoutItemLabel(ly);
    letterLbl->setId(QStringLiteral("ka_coord_let_%1").arg(tag));
    letterLbl->setText(tag);
    letterLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    letterLbl->setHAlign(Qt::AlignCenter);
    letterLbl->setVAlign(Qt::AlignVCenter);
    letterLbl->setFrameEnabled(false);
    letterLbl->setBackgroundEnabled(false);
    letterLbl->attemptSetSceneRect(QRectF(tip.x() - 2.4, tip.y() - 5.4, 5.2, 4.2));
    ly->addLayoutItem(letterLbl);
    const QPointF attach(placeRight ? boxR.left() : boxR.right(), boxR.center().y());
    QPointF dir(tip.x() - attach.x(), tip.y() - attach.y());
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 0.4) {
      dir.rx() /= len;
      dir.ry() /= len;
    } else {
      dir = QPointF(placeRight ? -1.0 : 1.0, 0.0);
    }
    const QPointF nor(-dir.y(), dir.x());
    const double ah = 2.4;
    const QPointF neck(tip.x() - dir.x() * ah, tip.y() - dir.y() * ah);
    QPolygonF shaft;
    shaft << attach << neck;
    auto* arr = new QgsLayoutItemPolyline(shaft, ly);
    arr->setId(QStringLiteral("ka_coord_arr_%1").arg(tag));
    arr->setStartMarker(QgsLayoutItemPolyline::NoMarker);
    arr->setEndMarker(QgsLayoutItemPolyline::NoMarker);
    if (auto sym = QgsLineSymbol::createSimple(
            {{QStringLiteral("line_color"), QStringLiteral("#1C1917")},
             {QStringLiteral("line_width"), QStringLiteral("0.22")},
             {QStringLiteral("line_width_unit"), QStringLiteral("MM")}})) {
      arr->setSymbol(sym.get());
    }
    ly->addLayoutItem(arr);
    QPolygonF head;
    head << tip << QPointF(neck.x() + nor.x() * ah * 0.42, neck.y() + nor.y() * ah * 0.42)
         << QPointF(neck.x() - nor.x() * ah * 0.42, neck.y() - nor.y() * ah * 0.42);
    auto* headItem = new QgsLayoutItemPolygon(head, ly);
    headItem->setId(QStringLiteral("ka_coord_head_%1").arg(tag));
    if (auto fs = QgsFillSymbol::createSimple(
            {{QStringLiteral("color"), QStringLiteral("#1C1917")},
             {QStringLiteral("style"), QStringLiteral("solid")},
             {QStringLiteral("outline_style"), QStringLiteral("no")}})) {
      headItem->setSymbol(fs.get());
    }
    ly->addLayoutItem(headItem);
    m_coordMapPts.append(QPointF(mapPt.x(), mapPt.y()));
  }
  if (m_status)
    m_status->setText(QStringLiteral("맵에서 찍은 좌표점 %1개를 도면에 올렸습니다.").arg(pts.size()));
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
  on->setToolTip(QStringLiteral("끄면 도면 안 +와 바깥 테두리 좌표 자가 사라집니다."));
  auto* nums = new QCheckBox(QStringLiteral("칸 밖에 좌표 숫자"), &dlg);
  nums->setChecked(m_gridShowNums);
  auto* step = new QDoubleSpinBox(&dlg);
  step->setRange(0.0, 100000.0);
  step->setDecimals(0);
  step->setSingleStep(10.0);
  step->setSuffix(QStringLiteral(" m"));
  step->setSpecialValueText(QStringLiteral("자동(축척에 맞춤)"));
  step->setValue(m_gridIntervalM);
  step->setKeyboardTracking(false);
  auto apply = [this, on, nums, step]() {
    m_gridEnabled = on->isChecked();
    m_gridShowNums = nums->isChecked();
    m_gridIntervalM = step->value();
    if (auto* map = mapItem())
      applyCrsGrid(map);
    if (m_status)
      m_status->setText(m_gridIntervalM > 0.0
                            ? QStringLiteral("격자 간격 %1 m").arg(m_gridIntervalM, 0, 'f', 0)
                            : QStringLiteral("격자 간격: 자동(축척에 맞춤)"));
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

void KaDrawingStudio::beginPlaceCoordPoint() {
  endActivateMap();
  if (!m_view || !m_toolCoordPoint || !mapItem()) {
    if (m_status)
      m_status->setText(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  if (QgsProject* proj = QgsProject::instance()) {
    QgsSnappingConfig cfg = proj->snappingConfig();
    cfg.setEnabled(true);
    cfg.setMode(Qgis::SnappingMode::AllLayers);
    cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
    cfg.setIntersectionSnapping(true);
    cfg.setSelfSnapping(true);
    cfg.setTolerance(16.0);
    cfg.setUnits(Qgis::MapToolUnit::Pixels);
    proj->setSnappingConfig(cfg);
    if (m_mapCanvas && m_mapCanvas->snappingUtils())
      m_mapCanvas->snappingUtils()->setConfig(cfg);
  }
  m_view->setTool(m_toolCoordPoint);
  if (m_status)
    m_status->setText(QStringLiteral(
        "지도 칸에서 찍으세요. 선·점에 자석이 붙습니다. 우클릭·Delete·Ctrl+Z는 마지막 점 지우기."));
}

bool KaDrawingStudio::isPlacingCoordPoint() const {
  return m_view && m_toolCoordPoint && m_view->tool() == m_toolCoordPoint;
}

void KaDrawingStudio::endPlaceCoordPoint() {
  useSelectTool();
  if (m_status)
    m_status->setText(QStringLiteral("좌표점 찍기를 끝냈습니다."));
}

void KaDrawingStudio::placeCoordCallout(const QPointF& layoutPt) {
  auto* ly = layout();
  auto* map = mapItem();
  if (!ly || !map) return;
  const QPointF itemPt = map->mapFromScene(layoutPt);
  const QRectF ir = map->rect();
  if (ir.width() < 1.0 || ir.height() < 1.0) return;
  if (!LayoutService::layoutMapItemContains(itemPt, ir, 0.8)) {
    if (m_status)
      m_status->setText(QStringLiteral("지도 칸 안에서 찍으세요. 흰 용지에는 점이 안 생깁니다."));
    return;
  }
  const QgsRectangle ext = map->extent();
  if (ext.isEmpty() || !ext.isFinite()) return;

  const QgsPointXY click = LayoutService::layoutMapXyFromItem(ext, ir, itemPt);
  QgsPointXY mapPt = click;
  const double viewPx = m_view ? std::max(1e-9, m_view->transform().m11()) : 1.0;
  const double snapMm = 18.0 / viewPx;
  const double tol = std::max(ext.width() * snapMm / std::max(1.0, ir.width()),
                              ext.height() * snapMm / std::max(1.0, ir.height()));
  bool snapped = false;
  const QgsCoordinateReferenceSystem mapCrs = map->crs();
  QgsProject* proj = m_project ? m_project.data() : QgsProject::instance();
  double bestD = tol;
  QList<QgsMapLayer*> search = map->layers();
  if (proj) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      if (l && !search.contains(l))
        search.append(l);
    }
  }
  for (QgsMapLayer* raw : search) {
    auto* vl = qobject_cast<QgsVectorLayer*>(raw);
    if (!vl || !vl->isValid() || vl->geometryType() == Qgis::GeometryType::Null)
      continue;
    if (LayerOps::isReferenceLayer(vl))
      continue;
    QgsRectangle req(click.x() - tol, click.y() - tol, click.x() + tol, click.y() + tol);
    if (vl->crs().isValid() && mapCrs.isValid() && vl->crs() != mapCrs && proj) {
      try {
        const QgsCoordinateTransform xf(mapCrs, vl->crs(), proj->transformContext());
        req = xf.transformBoundingBox(req);
      } catch (...) {
        continue;
      }
    }
    QgsFeatureIterator it =
        vl->getFeatures(QgsFeatureRequest().setFilterRect(req).setNoAttributes());
    QgsFeature f;
    while (it.nextFeature(f)) {
      QgsGeometry g = f.geometry();
      if (g.isEmpty()) continue;
      if (vl->crs().isValid() && mapCrs.isValid() && vl->crs() != mapCrs && proj) {
        try {
          const QgsCoordinateTransform xf(vl->crs(), mapCrs, proj->transformContext());
          g.transform(xf);
        } catch (...) {
          continue;
        }
      }
      int atV = 0, before = 0, after = 0;
      double vSqr = -1.0;
      const QgsPointXY v = g.closestVertex(click, atV, before, after, vSqr);
      if (vSqr >= 0.0) {
        const double d = std::sqrt(vSqr);
        if (d < bestD) {
          bestD = d;
          mapPt = v;
          snapped = true;
        }
      }
      QgsPointXY onSeg;
      int afterSeg = -1;
      const double sSqr = g.closestSegmentWithContext(click, onSeg, afterSeg);
      if (sSqr >= 0.0) {
        const double d = std::sqrt(sSqr);
        if (d < bestD) {
          bestD = d;
          mapPt = onSeg;
          snapped = true;
        }
      }
    }
  }

  const QPointF itemSnap = LayoutService::layoutMapItemFromXy(ext, ir, mapPt.x(), mapPt.y());
  const QPointF tip = map->mapToScene(itemSnap);

  int n = 0;
  QList<QgsLayoutItemLabel*> labs;
  ly->layoutItems(labs);
  for (QgsLayoutItemLabel* l : labs) {
    if (l && l->id().startsWith(QLatin1String("ka_coord_box_")))
      ++n;
  }
  const QString tag = QString(QChar(static_cast<char>('A' + (n % 26))));

  auto* box = new QgsLayoutItemLabel(ly);
  box->setId(QStringLiteral("ka_coord_box_%1").arg(tag));
  box->setText(QStringLiteral("X=%1\nY=%2").arg(mapPt.y(), 0, 'f', 3).arg(mapPt.x(), 0, 'f', 3));
  box->setFont(QFont(QStringLiteral("Malgun Gothic"), 6));
  box->setHAlign(Qt::AlignLeft);
  box->setVAlign(Qt::AlignVCenter);
  box->setFrameEnabled(true);
  box->setBackgroundEnabled(true);
  box->setBackgroundColor(QColor(255, 255, 255, 235));
  box->setMarginX(0.4);
  box->setMarginY(0.3);
  const bool placeRight = tip.x() + 34.0 < m_paperW - 3.0;
  const bool placeUp = tip.y() - 10.0 > 3.0;
  const QRectF boxR(placeRight ? tip.x() + 6.0 : tip.x() - 32.0,
                    placeUp ? tip.y() - 9.0 : tip.y() + 3.0, 26.0, 7.2);
  box->attemptSetSceneRect(boxR);
  ly->addLayoutItem(box);

  auto* letterLbl = new QgsLayoutItemLabel(ly);
  letterLbl->setId(QStringLiteral("ka_coord_let_%1").arg(tag));
  letterLbl->setText(tag);
  letterLbl->setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
  letterLbl->setHAlign(Qt::AlignCenter);
  letterLbl->setVAlign(Qt::AlignVCenter);
  letterLbl->setFrameEnabled(false);
  letterLbl->setBackgroundEnabled(false);
  letterLbl->attemptSetSceneRect(QRectF(tip.x() - 2.4, tip.y() - 5.4, 5.2, 4.2));
  ly->addLayoutItem(letterLbl);

  const QPointF attach(placeRight ? boxR.left() : boxR.right(), boxR.center().y());
  QPointF dir(tip.x() - attach.x(), tip.y() - attach.y());
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 0.4) {
    dir.rx() /= len;
    dir.ry() /= len;
  } else {
    dir = QPointF(placeRight ? -1.0 : 1.0, 0.0);
  }
  const QPointF nor(-dir.y(), dir.x());
  const double ah = 2.4;
  const QPointF neck(tip.x() - dir.x() * ah, tip.y() - dir.y() * ah);
  QPolygonF shaft;
  shaft << attach << neck;
  auto* arr = new QgsLayoutItemPolyline(shaft, ly);
  arr->setId(QStringLiteral("ka_coord_arr_%1").arg(tag));
  arr->setStartMarker(QgsLayoutItemPolyline::NoMarker);
  arr->setEndMarker(QgsLayoutItemPolyline::NoMarker);
  if (auto sym = QgsLineSymbol::createSimple(
          {{QStringLiteral("line_color"), QStringLiteral("#1C1917")},
           {QStringLiteral("line_width"), QStringLiteral("0.22")},
           {QStringLiteral("line_width_unit"), QStringLiteral("MM")}})) {
    arr->setSymbol(sym.get());
  }
  ly->addLayoutItem(arr);

  QPolygonF head;
  head << tip << QPointF(neck.x() + nor.x() * ah * 0.42, neck.y() + nor.y() * ah * 0.42)
       << QPointF(neck.x() - nor.x() * ah * 0.42, neck.y() - nor.y() * ah * 0.42);
  auto* headItem = new QgsLayoutItemPolygon(head, ly);
  headItem->setId(QStringLiteral("ka_coord_head_%1").arg(tag));
  if (auto fs = QgsFillSymbol::createSimple(
          {{QStringLiteral("color"), QStringLiteral("#1C1917")},
           {QStringLiteral("style"), QStringLiteral("solid")},
           {QStringLiteral("outline_style"), QStringLiteral("no")}})) {
    headItem->setSymbol(fs.get());
  }
  ly->addLayoutItem(headItem);
  m_placeUndo.append(QStringLiteral("ka_coord_%1").arg(tag));
  m_coordMapPts.append(QPointF(mapPt.x(), mapPt.y()));
  updateCoordFrame();
  if (m_status)
    m_status->setText(QStringLiteral("%1점  X=%2  Y=%3%4")
                          .arg(tag)
                          .arg(mapPt.y(), 0, 'f', 3)
                          .arg(mapPt.x(), 0, 'f', 3)
                          .arg(snapped ? QStringLiteral("  (자석)") : QString()));
}

void KaDrawingStudio::undoLastCoordCallout() {
  if (m_coordMapPts.isEmpty()) {
    if (m_status)
      m_status->setText(QStringLiteral("지울 좌표점이 없습니다."));
    return;
  }
  m_coordMapPts.removeLast();
  while (!m_placeUndo.isEmpty() && m_placeUndo.last().startsWith(QLatin1String("ka_coord_")))
    m_placeUndo.removeLast();
  relayoutCoordCallouts();
  if (m_status)
    m_status->setText(m_coordMapPts.isEmpty()
                          ? QStringLiteral("좌표점을 모두 지웠습니다.")
                          : QStringLiteral("마지막 좌표점을 지웠습니다. 남은 점 %1개.")
                                .arg(m_coordMapPts.size()));
}

void KaDrawingStudio::relayoutCoordCallouts() {
  if (m_coordMapPts.isEmpty()) {
    auto* ly = layout();
    if (!ly) return;
    QList<QgsLayoutItem*> all;
    ly->layoutItems(all);
    for (QgsLayoutItem* it : all) {
      if (it && it->id().startsWith(QLatin1String("ka_coord_")))
        ly->removeLayoutItem(it);
    }
    return;
  }
  m_importCoordPts.clear();
  m_importCoordLetters.clear();
  m_importCoordTexts.clear();
  for (int i = 0; i < m_coordMapPts.size(); ++i) {
    const QPointF p = m_coordMapPts.at(i);
    m_importCoordPts.append(QgsPointXY(p.x(), p.y()));
    const QString tag(QChar(static_cast<char>('A' + (i % 26))));
    m_importCoordLetters.append(tag);
    m_importCoordTexts.append(
        QStringLiteral("X=%1\nY=%2").arg(p.y(), 0, 'f', 3).arg(p.x(), 0, 'f', 3));
  }
  applyImportedCoordCallouts();
  updateCoordFrame();
}

void KaDrawingStudio::updateCoordFrame() {
  auto* ly = layout();
  auto* map = mapItem();
  if (!ly || !map) return;
  const QgsRectangle ext = map->extent();
  const QRectF ir = map->rect();
  if (ext.isEmpty() || ir.width() < 1.0 || ir.height() < 1.0) return;

  QVector<QPointF> src = m_coordFrameMap;
  if (src.size() < 3 && m_coordMapPts.size() >= 2)
    src = m_coordMapPts;
  if (src.size() < 2) return;

  auto toLayout = [&](double mx, double my) {
    return map->mapToScene(LayoutService::layoutMapItemFromXy(ext, ir, mx, my));
  };
  QPolygonF ring;
  for (const QPointF& p : src)
    ring << toLayout(p.x(), p.y());
  if (ring.size() >= 2 && ring.first() == ring.last())
    ring.removeLast();
  if (ring.size() < 2) return;

  QgsLayoutItemPolygon* frame = nullptr;
  QList<QgsLayoutItemPolygon*> polys;
  ly->layoutItems(polys);
  for (QgsLayoutItemPolygon* p : polys) {
    if (p && p->id() == QLatin1String("ka_coord_frame")) {
      frame = p;
      break;
    }
  }
  if (!frame) {
    frame = new QgsLayoutItemPolygon(ring, ly);
    frame->setId(QStringLiteral("ka_coord_frame"));
    if (auto fs = QgsFillSymbol::createSimple(
            {{QStringLiteral("color"), QStringLiteral("255,255,255,0")},
             {QStringLiteral("style"), QStringLiteral("no")},
             {QStringLiteral("outline_color"), QStringLiteral("#C2410C")},
             {QStringLiteral("outline_width"), QStringLiteral("0.45")},
             {QStringLiteral("outline_width_unit"), QStringLiteral("MM")}})) {
      frame->setSymbol(fs.get());
    }
    ly->addLayoutItem(frame);
    m_placeUndo.append(frame->id());
  } else {
    frame->setNodes(ring);
  }
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
  m_keepPaperCentered = false;
  if (m_view && m_toolPan)
    m_view->setTool(m_toolPan);
  if (m_status)
    m_status->setText(QStringLiteral("용지를 잡아 옮기세요. 휠 버튼을 누른 채 끌어도 됩니다."));
}

void KaDrawingStudio::zoomFull() {
  zoomPaperVisible();
}

void KaDrawingStudio::recenterPaper() {
  auto* ly = layout();
  if (!m_view || !ly) return;
  QgsLayoutItemPage* page = ly->pageCollection() ? ly->pageCollection()->page(0) : nullptr;
  QRectF pr = page ? page->rect() : QRectF();
  if (page)
    pr = page->mapRectToScene(page->rect());
  if (pr.width() < 8.0 || pr.height() < 8.0)
    pr = QRectF(0, 0, m_paperW, m_paperH);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_view->setAlignment(Qt::AlignCenter);
  m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  const QRectF desk(pr.left() - pr.width() * 1.5, pr.top() - pr.height() * 1.5,
                    pr.width() * 4.0, pr.height() * 4.0);
  m_view->setSceneRect(desk);
  const qreal pad = std::max(pr.width(), pr.height()) * 0.10;
  m_view->fitInView(pr.adjusted(-pad, -pad, pad, pad), Qt::KeepAspectRatio);
  m_view->centerOn(pr.center());
  if (m_view->viewport())
    m_lastFitViewport = m_view->viewport()->size();
}

void KaDrawingStudio::zoomPaperVisible() {
  m_keepPaperCentered = true;
  recenterPaper();
  updatePageOutline();
  QTimer::singleShot(0, this, [this]() {
    if (m_keepPaperCentered) recenterPaper();
  });
  QTimer::singleShot(80, this, [this]() {
    if (m_keepPaperCentered) recenterPaper();
  });
}

void KaDrawingStudio::panLayoutView(const QPoint& from, const QPoint& to) {
  if (!m_view) return;
  const QPoint d = to - from;
  bool moved = false;
  if (auto* h = m_view->horizontalScrollBar()) {
    const int before = h->value();
    h->setValue(before - d.x());
    moved = moved || (h->value() != before);
  }
  if (auto* v = m_view->verticalScrollBar()) {
    const int before = v->value();
    v->setValue(before - d.y());
    moved = moved || (v->value() != before);
  }
  if (moved) return;
  const QTransform t = m_view->transform();
  const qreal sx = t.m11();
  const qreal sy = t.m22();
  if (qAbs(sx) < 1e-12 || qAbs(sy) < 1e-12) return;
  m_view->setTransformationAnchor(QGraphicsView::NoAnchor);
  m_view->translate(static_cast<qreal>(d.x()) / sx, static_cast<qreal>(d.y()) / sy);
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

// (구버전 수제 좌표 라벨은 폐기 — 도곽 주기는 LayoutService::applySurveyFrameGrid의
//  QGIS 내장 주기가 그린다. clearGridCoordinateLabels는 옛 도면 정리용으로 유지.)

void KaDrawingStudio::applyCrsGrid(QgsLayoutItemMap* map) {
  if (!map || !map->grids()) return;
  // 구버전 수제 좌표 라벨 정리 — 도곽 주기는 QGIS 내장(정수 TM, 세로쓰기)을 쓴다.
  clearGridCoordinateLabels();
  if (!m_gridEnabled) {
    QStringList ids;
    const QList<QgsLayoutItemMapGrid*> olds = map->grids()->asList();
    for (QgsLayoutItemMapGrid* old : olds) {
      if (!old) continue;
      old->setEnabled(false);
      if (!old->id().isEmpty()) ids.append(old->id());
      if (!old->name().isEmpty()) ids.append(old->name());
    }
    ids.removeDuplicates();
    for (const QString& id : ids)
      map->grids()->removeGrid(id);
    map->updateBoundingRect();
    map->invalidateCache();
    map->refresh();
    map->update();
    if (auto* ly = layout()) ly->refresh();
    return;
  }
  const QgsCoordinateReferenceSystem crs = map->crs().isValid()
      ? map->crs()
      : studioMapCrs(m_mapCanvas, m_project);
  if (crs.isValid() && !map->crs().isValid())
    map->setCrs(crs);
  // 간격 0 = 자동: 축척·지도폭에 맞는 1-2-5 계열(종이에서 25~70mm).
  double interval = m_gridIntervalM;
  if (!(interval > 0.0)) {
    double mapWmm = 160.0;
    if (auto* ly = layout())
      mapWmm =
          ly->convertFromLayoutUnits(map->rect().width(), Qgis::LayoutUnit::Millimeters).length();
    const double denom = map->scale() > 0.0
                             ? map->scale()
                             : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value()) : 1000.0);
    interval = LayoutService::niceGridIntervalMeters(denom, mapWmm);
  }
  LayoutService::applySurveyFrameGrid(map, interval, true, m_gridShowNums);
  map->invalidateCache();
  map->refresh();
  map->update();
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
  LayerOps::knockOutProjectRasterPaper(m_project);
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
    map->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    if (map->scene() != ly)
      ly->addLayoutItem(map);
  } else {
    map->attemptSetSceneRect(chrome.map);
    map->setCrs(crs);
    map->zoomToExtent(ext);
    map->setCacheMode(QGraphicsItem::ItemCoordinateCache);
  }

  QPointer<QgsLayoutItemMap> held(map);
  QTimer::singleShot(0, this, [this, held]() {
    if (!held) return;
    applyLayersToMap(held, true, false);
    applyCrsGrid(held);
    if (m_holdTerrainExtent && !m_terrainGroundExtent.isEmpty() &&
        m_terrainGroundExtent.isFinite()) {
      if (m_terrainCrs.isValid())
        held->setCrs(m_terrainCrs);
      held->zoomToExtent(m_terrainGroundExtent);
    } else if (m_mapCanvas)
      centerOnMapCanvas();
    else
      snapMapScaleToNice();
    connect(held, &QgsLayoutItemMap::extentChanged, this, &KaDrawingStudio::syncScaleDecorations,
            Qt::UniqueConnection);
    ensureStandardDecorations();
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
    const QRectF b = itemPaperRect(map);
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
  LayoutService::tuneSheetLegend(legend);
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
  if (!r.isValid() || r.width() < 16.0 || r.height() < 16.0)
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
  placeScaleBar(defaultItemRect(kIdScaleBar));
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
  compactSheetScaleBar(sb);
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
  placeCrsLabel(defaultItemRect(kIdCrs));
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
  lbl->setHAlign(Qt::AlignRight);
  lbl->setVAlign(Qt::AlignVCenter);
  const QgsCoordinateReferenceSystem crs =
      (mapItem() && mapItem()->crs().isValid()) ? mapItem()->crs()
                                                : studioMapCrs(m_mapCanvas, m_project);
  lbl->setText(koreanCrsLabel(crs));
  const QRectF keep = (layoutRect.width() >= 24.0 && layoutRect.height() >= 6.0)
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
  lbl->setHAlign(Qt::AlignHCenter);
  lbl->setVAlign(Qt::AlignTop);
  lbl->setBackgroundEnabled(false);
  lbl->setFrameEnabled(false);
  lbl->setReferencePoint(QgsLayoutItem::UpperLeft);
  const QRectF keep = (layoutRect.width() >= 24.0 && layoutRect.height() >= 6.0)
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
    if (auto* ly = layout()) {
      if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
        legend->setLinkedMap(map);
        LayoutService::tuneSheetLegend(legend);
        legend->update();
      }
      ly->refresh();
    }
  }
}

void KaDrawingStudio::relinkDecorations() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  if (auto* legend = dynamic_cast<QgsLayoutItemLegend*>(findItemById(ly, kIdLegend))) {
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    LayoutService::tuneSheetLegend(legend);
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
  // 전문 도면 기본: 측량도면식 니들 방위표 + 교호식(Double Box) 축척자.
  if (m_pendingNorthSvg.isEmpty())
    m_pendingNorthSvg = QStringLiteral("arrows/NorthArrow_02.svg");
  if (m_pendingScaleBarStyle.isEmpty())
    m_pendingScaleBarStyle = QStringLiteral("Double Box");
  if (!findItemById(ly, kIdNorth))
    placeNorth(defaultItemRect(kIdNorth), false);
  if (!findItemById(ly, kIdScaleBar))
    placeScaleBar(defaultItemRect(kIdScaleBar), false);
  if (!findItemById(ly, kIdScale))
    placeScaleLabel(defaultItemRect(kIdScale), false);
  applyCrsLabelNow();
  if (auto* map = mapItem())
    applyCrsGrid(map);
  relinkDecorations();
  applyStandardChromePositions();
}

void KaDrawingStudio::applyStandardChromePositions() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const auto chrome = LayoutService::standardSheetChrome(page, itemPaperRect(map));
  QRectF bar = chrome.scaleBar;
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar))) {
    const double denom = map->scale() > 0.0 ? map->scale()
                                            : (m_scaleSpin ? static_cast<double>(m_scaleSpin->value())
                                                           : 1000.0);
    const int segs = std::max(1, sb->numberOfSegments());
    const double segM = sb->unitsPerSegment();
    const double wantW = LayoutService::scaleBarWidthMm(segM, segs, denom);
    const double maxW = std::max(32.0, chrome.crs.left() - 4.0 - chrome.scaleBar.left());
    if (wantW >= 32.0)
      bar.setWidth(std::clamp(wantW, 32.0, maxW));
    compactSheetScaleBar(sb);
    sb->setReferencePoint(QgsLayoutItem::UpperLeft);
    sb->attemptSetSceneRect(bar);
    sb->refresh();
    bar = QRectF(sb->pos(), sb->rect().size());
    if (bar.width() < 8.0 || bar.height() < 4.0)
      bar = chrome.scaleBar;
    if (bar.right() > chrome.crs.left() - 2.0)
      bar.setWidth(std::max(32.0, chrome.crs.left() - 4.0 - bar.left()));
    lockSheetChromeItem(sb);
  }
  QRectF label = chrome.scaleLabel;
  label.setLeft(bar.left());
  label.setWidth(std::max(36.0, bar.width()));
  label.setHeight(7.0);
  label.moveTop(bar.bottom() + 2.5);
  if (auto* sc = findItemById(ly, kIdScale)) {
    sc->setReferencePoint(QgsLayoutItem::UpperLeft);
    if (auto* scaleLbl = dynamic_cast<QgsLayoutItemLabel*>(sc)) {
      scaleLbl->setHAlign(Qt::AlignHCenter);
      scaleLbl->setVAlign(Qt::AlignTop);
    }
    sc->attemptSetSceneRect(label);
    lockSheetChromeItem(sc);
  }
  if (auto* crs = findItemById(ly, kIdCrs)) {
    crs->attemptSetSceneRect(chrome.crs);
    lockSheetChromeItem(crs);
  }
  if (auto* north = findItemById(ly, kIdNorth)) {
    north->attemptSetSceneRect(chrome.north);
    lockSheetChromeItem(north);
  }
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
  LayoutService::applySheetScaleBarInk(sb);
  applyStandardChromePositions();
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
  if (auto* crs = dynamic_cast<QgsLayoutItemLabel*>(findItemById(ly, kIdCrs))) {
    const QgsCoordinateReferenceSystem dest =
        map->crs().isValid() ? map->crs() : studioMapCrs(m_mapCanvas, m_project);
    crs->setText(koreanCrsLabel(dest));
  }
  syncScaleChips();
  if (m_scaleSyncTimer)
    m_scaleSyncTimer->start();
}

void KaDrawingStudio::flushHeavyScaleSync() {
  auto* map = mapItem();
  auto* ly = layout();
  if (!map || !ly) return;
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(ly, kIdScaleBar)))
    applyNiceScaleBar(sb);
  // 자동 간격이면 축척이 바뀔 때 도곽 간격도 다시 계산한다.
  if (m_gridEnabled)
    applyCrsGrid(map);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
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
                                   QStringLiteral("artifact_point"),
                                   QStringLiteral("trial_trench")};
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
  setFastLayoutPreview(true);
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
  QSet<QString> coordTags;
  int n = 0;
  for (QgsLayoutItem* it : sel) {
    if (!it || dynamic_cast<QgsLayoutItemPage*>(it)) continue;
    const QString id = it->id();
    if (id.startsWith(QLatin1String("ka_coord_"))) {
      const int u = id.lastIndexOf(QLatin1Char('_'));
      if (u >= 0)
        coordTags.insert(id.mid(u + 1));
      continue;
    }
    ly->removeLayoutItem(it);
    ++n;
  }
  if (!coordTags.isEmpty()) {
    QVector<QPointF> keep;
    for (int i = 0; i < m_coordMapPts.size(); ++i) {
      const QString tag(QChar(static_cast<char>('A' + (i % 26))));
      if (!coordTags.contains(tag))
        keep.append(m_coordMapPts.at(i));
    }
    m_coordMapPts = keep;
    relayoutCoordCallouts();
    n += coordTags.size();
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
  const bool onView = m_view && event && (watched == m_view || watched == m_view->viewport());
  if (onView && event->type() == QEvent::Resize && m_keepPaperCentered && !m_mmbPanning) {
    const QSize now = m_view->viewport() ? m_view->viewport()->size() : QSize();
    const QSize d = now - m_lastFitViewport;
    if (qAbs(d.width()) + qAbs(d.height()) > 8) {
      QTimer::singleShot(0, this, [this]() {
        if (m_keepPaperCentered && !m_mmbPanning)
          zoomPaperVisible();
      });
    }
  }
  if (onView && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::MiddleButton) {
      m_mmbPanning = true;
      m_mmbLast = me->pos();
      if (m_view->viewport()) {
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        m_view->viewport()->grabMouse();
      }
      return true;
    }
  }
  if (m_mmbPanning && event &&
      (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease)) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (event->type() == QEvent::MouseMove && (me->buttons() & Qt::MiddleButton)) {
      const QPoint now = me->pos();
      if (qAbs(now.x() - m_mmbLast.x()) + qAbs(now.y() - m_mmbLast.y()) > 2) {
        m_keepPaperCentered = false;
        panLayoutView(m_mmbLast, now);
        m_mmbLast = now;
      }
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && me->button() == Qt::MiddleButton) {
      m_mmbPanning = false;
      if (m_view->viewport()) {
        m_view->viewport()->releaseMouse();
        m_view->viewport()->unsetCursor();
      }
      return true;
    }
  }
  if (onView && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Undo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z)) {
      undoLastChange();
      return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
      if (isPlacingCoordPoint())
        undoLastCoordCallout();
      else
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
  if (isPlacingCoordPoint() ||
      (!m_placeUndo.isEmpty() && m_placeUndo.last().startsWith(QLatin1String("ka_coord_")))) {
    undoLastCoordCallout();
    return;
  }
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
    if (isPlacingCoordPoint())
      undoLastCoordCallout();
    else
      deleteSelectedItems();
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

void KaDrawingStudio::setFastLayoutPreview(bool on) {
  auto* ly = layout();
  if (!ly) return;
  if (on) {
    if (m_savedLayoutDpi <= 1.0)
      m_savedLayoutDpi = ly->renderContext().dpi();
    if (m_savedLayoutDpi < 48.0)
      m_savedLayoutDpi = kPreviewDpi;
    ly->renderContext().setDpi(48.0);
    return;
  }
  if (m_savedLayoutDpi > 1.0) {
    ly->renderContext().setDpi(m_savedLayoutDpi);
    m_savedLayoutDpi = 0.0;
    if (auto* map = mapItem()) {
      map->invalidateCache();
      map->refresh();
    }
  }
}

void KaDrawingStudio::endActivateMap() {
  const bool was = m_adjustingMap;
  m_adjustingMap = false;
  setFastLayoutPreview(false);
  if (m_adjustBar) m_adjustBar->hide();
  if (m_actEndAdjust) m_actEndAdjust->setVisible(false);
  if (was)
    snapMapScaleToNice();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (was)
    useSelectTool();
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
}

void KaDrawingStudio::panLayoutMapTo(const QgsPointXY& center) {
  auto* map = mapItem();
  if (!map) return;
  const QgsRectangle cur = map->extent();
  if (!cur.isFinite() || cur.width() <= 0.0 || cur.height() <= 0.0) return;
  const double keepScale = map->scale();
  const QgsRectangle next(center.x() - cur.width() * 0.5, center.y() - cur.height() * 0.5,
                          center.x() + cur.width() * 0.5, center.y() + cur.height() * 0.5);
  map->zoomToExtent(next);
  if (keepScale > 1.0)
    map->setScale(keepScale, true);
  map->invalidateCache();
  map->refresh();
  if (auto* ly = layout())
    ly->refresh();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
}

void KaDrawingStudio::centerOnMapCanvas() {
  auto* map = mapItem();
  if (!map || !m_mapCanvas) return;
  QgsRectangle ext = m_mapCanvas->extent();
  const QgsCoordinateReferenceSystem src = m_mapCanvas->mapSettings().destinationCrs();
  const QgsCoordinateReferenceSystem dst = map->crs().isValid()
                                               ? map->crs()
                                               : studioMapCrs(m_mapCanvas, m_project);
  if (m_project && src.isValid() && dst.isValid() && src != dst) {
    try {
      const QgsCoordinateTransform xf(src, dst, m_project->transformContext());
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
    }
  }
  if (!LayoutService::applyCanvasViewToLayoutMap(map, ext, m_mapCanvas->scale()))
    return;
  map->invalidateCache();
  map->refresh();
  refreshScaleWidgets(true);
  if (auto* sb = dynamic_cast<QgsLayoutItemScaleBar*>(findItemById(layout(), kIdScaleBar)))
    applyNiceScaleBar(sb);
  if (!m_coordMapPts.isEmpty())
    relayoutCoordCallouts();
  if (m_status)
    m_status->setText(QStringLiteral("지도 화면과 같은 범위·축척으로 맞췄습니다."));
}

void KaDrawingStudio::centerSurveyInMap() {
  auto* map = mapItem();
  if (!map) {
    if (m_status)
      m_status->setText(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  QgsPointXY c;
  bool have = false;
  QgsMapLayer* layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  if (layer && layer->isValid()) {
    QgsRectangle target;
    if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
      vl->updateExtents();
      target = vl->extent();
    }
    if (target.isNull() || !target.isFinite())
      target = layer->extent();
    if (m_project && layer->crs().isValid() && map->crs().isValid() && layer->crs() != map->crs()) {
      try {
        const QgsCoordinateTransform xf(layer->crs(), map->crs(), m_project->transformContext());
        target = xf.transformBoundingBox(target);
      } catch (...) {
      }
    }
    if (target.isFinite() && target.width() > 0.0 && target.width() < 2000000.0) {
      c = target.center();
      have = true;
    }
  }
  if (!have) {
    const QgsRectangle survey = surveyExtentOnMap(map);
    if (survey.isFinite() && survey.width() > 0.0) {
      c = survey.center();
      have = true;
    }
  }
  if (!have) {
    if (m_status)
      m_status->setText(QStringLiteral("가운데로 둘 레이어를 왼쪽에서 고르세요."));
    return;
  }
  panLayoutMapTo(c);
  if (m_status)
    m_status->setText(QStringLiteral("축척 1 : %1 을 유지한 채 조판 가운데로 옮겼습니다.")
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
  if (auto* map = mapItem())
    applyCrsGrid(map);
  if (m_pageOutline) m_pageOutline->hide();
  QgsLayoutExporter exporter(ly);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  // 화면 미리보기는 낮은 해상도로 두고, 인쇄용 해상도는 내보내는 동안만 올린다.
  const double previewDpi = ly->renderContext().dpi();
  ly->renderContext().setDpi(settings.dpi);
  const auto pdfOk = exporter.exportToPdf(path, settings);
  ly->renderContext().setDpi(previewDpi);
  if (auto* map = mapItem()) {
    map->invalidateCache();
    map->refresh();
  }
  if (m_pageOutline) m_pageOutline->show();
  if (pdfOk != QgsLayoutExporter::Success) {
    QMessageBox::warning(this, QStringLiteral("PDF"), QStringLiteral("저장에 실패했습니다."));
    return;
  }
  if (m_status) m_status->setText(QStringLiteral("저장: %1").arg(path));
  QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("저장했습니다.\n%1").arg(path));
}

#include "KaDrawingStudio.moc"
