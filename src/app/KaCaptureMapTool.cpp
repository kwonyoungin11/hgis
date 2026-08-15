#include "KaCaptureMapTool.h"
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsrubberband.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsmapsettings.h>
#include <QKeyEvent>
#include <QKeySequence>
#include <QColor>
#include <cmath>

KaCaptureMapTool::KaCaptureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaCaptureMapTool::~KaCaptureMapTool() {
  destroyRubber();
}

QgsMapTool::Flags KaCaptureMapTool::flags() const {
  return QgsMapTool::EditTool;
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

bool KaCaptureMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out) {
  if (!e || !out || !canvas()) return false;
  try {
    *out = e->mapPoint();
  } catch (...) {
    try {
      *out = toMapCoordinates(e->pos());
    } catch (...) {
      return false;
    }
  }
  if (std::isnan(out->x()) || std::isnan(out->y())) return false;
  return true;
}

void KaCaptureMapTool::rebuildRubber(const QgsPointXY* cursorOrNull) {
  QgsMapCanvas* c = canvas();
  if (!c) return;

  if (!m_rubber) {
    Qgis::GeometryType gt = Qgis::GeometryType::Line;
    if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
    else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;
    m_rubber = new QgsRubberBand(c, gt);
    m_rubber->setWidth(3);
    m_rubber->setSecondaryStrokeColor(QColor(255, 255, 255, 200));
    m_rubber->setColor(QColor(37, 99, 235));
    m_rubber->setFillColor(QColor(37, 99, 235, 80));
  }

  Qgis::GeometryType gt = Qgis::GeometryType::Line;
  if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
  else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;
  m_rubber->reset(gt);

  for (const QgsPointXY& p : m_points)
    m_rubber->addPoint(p, false);

  if (cursorOrNull && m_mode != Mode::Point)
    m_rubber->addPoint(*cursorOrNull, true);
  else if (!m_points.isEmpty())
    m_rubber->addPoint(m_points.last(), true);
}

void KaCaptureMapTool::activate() {
  m_finishing = false;
  m_points.clear();
  destroyRubber();
  if (canvas()) {
    canvas()->setMouseTracking(true);
    canvas()->freeze(false);
    canvas()->setRenderFlag(true);
  }
  QgsMapTool::activate();
  setCursor(Qt::CrossCursor);
}

void KaCaptureMapTool::deactivate() {
  destroyRubber();
  m_points.clear();
  m_finishing = false;
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;

  QgsPointXY mapPt;
  if (!mapPointFromEvent(e, &mapPt)) return;

  if (e->button() == Qt::LeftButton) {
    e->accept();
    if (m_mode == Mode::Point) {
      m_points.clear();
      m_points.append(mapPt);
      finish();
      return;
    }
    m_points.append(mapPt);
    rebuildRubber(&mapPt);
  }
}

void KaCaptureMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  if (e->button() != Qt::RightButton) return;
  e->accept();
  finish();
}

void KaCaptureMapTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (!e || m_finishing || m_mode == Mode::Point) return;
  e->accept();
  QgsPointXY mapPt;
  if (mapPointFromEvent(e, &mapPt)) {
    if (m_points.isEmpty() || m_points.last() != mapPt)
      m_points.append(mapPt);
  }
  finish();
}

void KaCaptureMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e || m_points.isEmpty() || m_mode == Mode::Point || m_finishing) return;
  QgsPointXY mapPt;
  if (!mapPointFromEvent(e, &mapPt)) return;
  rebuildRubber(&mapPt);
}

bool KaCaptureMapTool::undoLastVertex() {
  if (m_finishing || m_points.isEmpty()) return false;
  m_points.removeLast();
  if (m_points.isEmpty())
    destroyRubber();
  else if (canvas()) {
    const QgsPointXY cur = toMapCoordinates(canvas()->mouseLastXY());
    rebuildRubber(&cur);
  }
  return true;
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
  if (e->matches(QKeySequence::Undo) ||
      ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Z)) {
    if (undoLastVertex())
      e->accept();
    return;
  }
  if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) {
    if (undoLastVertex())
      e->accept();
  }
}

void KaCaptureMapTool::finish() {
  if (m_finishing) return;

  const int need = (m_mode == Mode::Point) ? 1 : (m_mode == Mode::Line ? 2 : 3);
  if (m_points.size() < need) {
    emit captureCanceled();
    return;
  }

  m_finishing = true;

  QgsGeometry geom;
  bool ok = false;
  if (m_mode == Mode::Point) {
    geom = QgsGeometry::fromPointXY(m_points.first());
    ok = !geom.isEmpty();
  } else if (m_mode == Mode::Line) {
    geom = QgsGeometry::fromPolylineXY(m_points);
    ok = !geom.isEmpty();
  } else {
    QgsPolylineXY ring = m_points;
    if (!ring.isEmpty() && ring.first() != ring.last())
      ring.append(ring.first());
    geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
    ok = !geom.isEmpty();
    if (ok && !geom.isGeosValid()) {
      const QgsGeometry fixed = geom.makeValid();
      if (!fixed.isEmpty())
        geom = fixed;
    }
    ok = !geom.isEmpty();
  }

  if (!ok) {
    m_finishing = false;
    emit captureCanceled();
    return;
  }

  if (m_layer && canvas()) {
    const QgsCoordinateReferenceSystem src = canvas()->mapSettings().destinationCrs();
    const QgsCoordinateReferenceSystem dst = m_layer->crs();
    if (src.isValid() && dst.isValid() && src != dst) {
      try {
        QgsCoordinateTransform xf(src, dst, QgsProject::instance()
                                                ? QgsProject::instance()->transformContext()
                                                : QgsCoordinateTransformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        if (geom.transform(xf) != Qgis::GeometryOperationResult::Success) {
          m_finishing = false;
          emit captureCanceled();
          return;
        }
      } catch (...) {
        m_finishing = false;
        emit captureCanceled();
        return;
      }
    }
  }

  m_points.clear();
  destroyRubber();
  m_finishing = false;
  emit geometryCaptured(geom);
}

void KaCaptureMapTool::cancel() {
  m_points.clear();
  destroyRubber();
  m_finishing = false;
  emit captureCanceled();
}
