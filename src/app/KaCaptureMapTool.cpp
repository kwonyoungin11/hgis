#include "KaCaptureMapTool.h"
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsrubberband.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsmapsettings.h>
#include <QKeyEvent>
#include <QColor>
#include <QTimer>
#include <cmath>

KaCaptureMapTool::KaCaptureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaCaptureMapTool::~KaCaptureMapTool() {
  destroyRubber();
}

QgsMapTool::Flags KaCaptureMapTool::flags() const {
  return QgsMapTool::EditTool | QgsMapTool::AllowZoomRect;
}

void KaCaptureMapTool::setMode(Mode mode) {
  m_mode = mode;
  resetSession();
}

void KaCaptureMapTool::setTargetLayer(QgsVectorLayer* layer) {
  m_layer = layer;
}

void KaCaptureMapTool::resetSession() {
  m_finishing = false;
  m_points.clear();
  destroyRubber();
}

void KaCaptureMapTool::destroyRubber() {
  if (!m_rubber) return;
  m_rubber->reset(Qgis::GeometryType::Line);
  delete m_rubber;
  m_rubber = nullptr;
}

QgsPointXY KaCaptureMapTool::eventMapPoint(QgsMapMouseEvent* e) {
  if (!e || !canvas()) return QgsPointXY();
  QgsPointXY p = e->mapPoint();
  if (std::isnan(p.x()) || std::isnan(p.y()))
    p = toMapCoordinates(e->pos());
  return p;
}

void KaCaptureMapTool::rebuildRubber(const QgsPointXY* cursorOrNull) {
  QgsMapCanvas* c = canvas();
  if (!c) return;

  Qgis::GeometryType gt = Qgis::GeometryType::Line;
  if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
  else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;

  if (!m_rubber) {
    m_rubber = new QgsRubberBand(c, gt);
    m_rubber->setWidth(3);
    m_rubber->setSecondaryStrokeColor(QColor(255, 255, 255, 200));
    m_rubber->setColor(QColor(220, 38, 38));
    m_rubber->setFillColor(QColor(220, 38, 38, 80));
  }

  m_rubber->reset(gt);
  for (const QgsPointXY& p : m_points)
    m_rubber->addPoint(p, false);
  if (cursorOrNull && m_mode != Mode::Point)
    m_rubber->addPoint(*cursorOrNull, true);
  else if (!m_points.isEmpty())
    m_rubber->addPoint(m_points.last(), true);
  m_rubber->show();
}

void KaCaptureMapTool::activate() {
  m_finishing = false;
  m_points.clear();
  destroyRubber();
  if (canvas()) {
    canvas()->setContextMenuPolicy(Qt::PreventContextMenu);
    canvas()->setMouseTracking(true);
  }
  QgsMapTool::activate();
}

void KaCaptureMapTool::deactivate() {
  if (canvas())
    canvas()->setContextMenuPolicy(Qt::DefaultContextMenu);
  destroyRubber();
  m_points.clear();
  m_finishing = false;
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::addVertex(const QgsPointXY& mapPt) {
  if (std::isnan(mapPt.x()) || std::isnan(mapPt.y())) return;

  if (m_mode == Mode::Point) {
    m_points.clear();
    m_points.append(mapPt);
    emit vertexAdded(1);
    finish();
    return;
  }

  if (!m_points.isEmpty()) {
    const QgsPointXY& last = m_points.last();
    if (std::hypot(last.x() - mapPt.x(), last.y() - mapPt.y()) < 1e-9)
      return;
  }
  m_points.append(mapPt);
  rebuildRubber(&mapPt);
  emit vertexAdded(m_points.size());
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  if (e->button() == Qt::RightButton) {
    e->accept();
    return;
  }
}

void KaCaptureMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  e->accept();

  if (e->button() == Qt::LeftButton) {
    const QgsPointXY mapPt = eventMapPoint(e);
    if (std::isnan(mapPt.x()) || std::isnan(mapPt.y())) return;
    addVertex(mapPt);
    return;
  }

  if (e->button() == Qt::RightButton) {
    finish();
  }
}

void KaCaptureMapTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (!e || m_finishing) return;
  e->accept();
  if (m_mode == Mode::Polygon || m_mode == Mode::Line)
    finish();
}

void KaCaptureMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e || m_points.isEmpty() || m_mode == Mode::Point || m_finishing) return;
  const QgsPointXY mapPt = eventMapPoint(e);
  if (std::isnan(mapPt.x())) return;
  rebuildRubber(&mapPt);
}

void KaCaptureMapTool::keyPressEvent(QKeyEvent* e) {
  if (!e || m_finishing) return;
  if (e->key() == Qt::Key_Escape) {
    cancel();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    finish();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) {
    if (!m_points.isEmpty()) {
      m_points.removeLast();
      if (m_points.isEmpty())
        destroyRubber();
      else if (canvas()) {
        const QgsPointXY cur = toMapCoordinates(canvas()->mouseLastXY());
        rebuildRubber(&cur);
      }
      emit vertexAdded(m_points.size());
    }
    e->accept();
  }
}

void KaCaptureMapTool::finish() {
  if (m_finishing) return;
  m_finishing = true;

  QgsGeometry geom;
  bool ok = false;
  QString why;
  if (m_mode == Mode::Point && m_points.size() >= 1) {
    geom = QgsGeometry::fromPointXY(m_points.first());
    ok = !geom.isEmpty();
  } else if (m_mode == Mode::Line && m_points.size() >= 2) {
    geom = QgsGeometry::fromPolylineXY(m_points);
    ok = !geom.isEmpty() && geom.length() > 0;
    if (!ok) why = QStringLiteral("선은 점 2개 이상");
  } else if (m_mode == Mode::Polygon && m_points.size() >= 3) {
    QgsPolylineXY ring = m_points;
    if (!ring.isEmpty() && ring.first() != ring.last())
      ring.append(ring.first());
    geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
    if (!geom.isEmpty() && !geom.isGeosValid()) {
      const QgsGeometry fixed = geom.makeValid();
      if (!fixed.isEmpty()) geom = fixed;
    }
    ok = !geom.isEmpty() && geom.area() > 0;
    if (!ok) why = QStringLiteral("면은 점 3개 이상, 면적>0");
  } else {
    if (m_mode == Mode::Polygon) why = QStringLiteral("면: 좌클릭 3회 이상 후 우클릭");
    else if (m_mode == Mode::Line) why = QStringLiteral("선: 좌클릭 2회 이상 후 우클릭");
  }

  m_points.clear();
  destroyRubber();

  if (!ok) {
    m_finishing = false;
    emit captureCanceled();
    return;
  }

  if (m_layer && m_layer->isValid() && canvas()) {
    const QgsCoordinateReferenceSystem src = canvas()->mapSettings().destinationCrs();
    const QgsCoordinateReferenceSystem dst = m_layer->crs();
    if (src.isValid() && dst.isValid() && src.authid() != dst.authid()) {
      try {
        QgsCoordinateTransform xf(src, dst, QgsProject::instance()->transformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        geom.transform(xf);
      } catch (...) {
      }
    }
  }

  const QgsGeometry out = geom;
  QTimer::singleShot(0, this, [this, out]() {
    m_finishing = false;
    emit geometryCaptured(out);
  });
  Q_UNUSED(why);
}

void KaCaptureMapTool::cancel() {
  m_points.clear();
  destroyRubber();
  m_finishing = false;
  emit captureCanceled();
}
