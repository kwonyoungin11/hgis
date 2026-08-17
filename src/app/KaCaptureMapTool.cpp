#include "KaCaptureMapTool.h"
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>
#include <qgsmapsettings.h>
#include <QKeyEvent>
#include <QKeySequence>
#include <QColor>
#include <QWidget>
#include <cmath>

KaCaptureMapTool::KaCaptureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaCaptureMapTool::~KaCaptureMapTool() {
  destroyRubber();
  destroySnapMarker();
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

void KaCaptureMapTool::setSnapEnabled(bool on) {
  m_snapEnabled = on;
  if (!on) destroySnapMarker();
}

void KaCaptureMapTool::setEasyDraw(bool on) {
  m_easyDraw = on;
  if (on) {
    m_snapEnabled = true;
    setCursor(Qt::CrossCursor);
  }
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

void KaCaptureMapTool::destroySnapMarker() {
  delete m_snapMark;
  m_snapMark = nullptr;
}

void KaCaptureMapTool::updateSnapMarker(const QgsPointXY& mapPt, bool snapped) {
  QgsMapCanvas* c = canvas();
  if (!c || !m_snapEnabled || !snapped) {
    destroySnapMarker();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(c);
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(14);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(QColor(15, 118, 110));
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(mapPt);
  m_snapMark->show();
}

bool KaCaptureMapTool::nearPoint(const QgsPointXY& a, const QgsPointXY& b) const {
  double tol = 0.15;
  if (canvas()) {
    const double mupp = canvas()->mapUnitsPerPixel();
    if (mupp > 0) tol = std::max(mupp * 10.0, 0.05);
  }
  return a.sqrDist(b) <= tol * tol;
}

int KaCaptureMapTool::indexOfSketchVertex(const QgsPointXY& pt) const {
  for (int i = 0; i < m_points.size(); ++i) {
    if (nearPoint(m_points.at(i), pt)) return i;
  }
  return -1;
}

bool KaCaptureMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snappedOut) {
  if (!e || !out || !canvas()) return false;
  bool snapped = false;
  try {
    *out = e->mapPoint();
  } catch (...) {
    try {
      *out = toMapCoordinates(e->pos());
    } catch (...) {
      return false;
    }
  }
  if (m_snapEnabled && canvas()->snappingUtils()) {
    const QgsPointLocator::Match hit = canvas()->snappingUtils()->snapToMap(e->pos());
    if (hit.isValid()) {
      *out = hit.point();
      snapped = true;
    }
  }
  if (std::isnan(out->x()) || std::isnan(out->y())) return false;
  updateSnapMarker(*out, snapped);
  if (snappedOut) *snappedOut = snapped;
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
    m_rubber->setColor(QColor(15, 118, 110));
    m_rubber->setFillColor(QColor(15, 118, 110, 80));
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
    m_savedMenuPolicy = canvas()->contextMenuPolicy();
    canvas()->setContextMenuPolicy(Qt::PreventContextMenu);
  }
  QgsMapTool::activate();
  setCursor(Qt::CrossCursor);
}

void KaCaptureMapTool::deactivate() {
  if (canvas())
    canvas()->setContextMenuPolicy(m_savedMenuPolicy);
  destroyRubber();
  destroySnapMarker();
  if (!m_finishing)
    m_points.clear();
  m_finishing = false;
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;

  if (e->button() == Qt::RightButton) {
    e->accept();
    finish();
    return;
  }

  QgsPointXY mapPt;
  bool snapped = false;
  if (!mapPointFromEvent(e, &mapPt, &snapped)) return;

  if (e->button() == Qt::LeftButton) {
    e->accept();
    if (m_mode == Mode::Point) {
      m_points.clear();
      m_points.append(mapPt);
      finish();
      return;
    }
    if (m_easyDraw && !m_points.isEmpty()) {
      const int idx = indexOfSketchVertex(mapPt);
      if (idx >= 0) {
        m_points.resize(idx + 1);
        rebuildRubber(&mapPt);
        return;
      }
    }
    if (m_points.isEmpty() || !nearPoint(m_points.last(), mapPt))
      m_points.append(mapPt);
    rebuildRubber(&mapPt);
  }
}

void KaCaptureMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  if (e->button() != Qt::RightButton) return;
  e->accept();
  if (!m_points.isEmpty())
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
  if (!e || m_finishing) return;
  QgsPointXY mapPt;
  bool snapped = false;
  if (!mapPointFromEvent(e, &mapPt, &snapped)) return;
  if (m_points.isEmpty() || m_mode == Mode::Point) {
    return;
  }
  if (m_easyDraw && snapped && indexOfSketchVertex(mapPt) < 0)
    m_points.append(mapPt);
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
    if (!m_points.isEmpty())
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
    QgsPolylineXY line;
    for (const QgsPointXY& p : m_points) {
      if (line.isEmpty() || !nearPoint(line.last(), p))
        line.append(p);
    }
    geom = QgsGeometry::fromPolylineXY(line);
    ok = !geom.isEmpty();
  } else {
    QgsPolylineXY ring;
    for (const QgsPointXY& p : m_points) {
      if (ring.isEmpty() || !nearPoint(ring.last(), p))
        ring.append(p);
    }
    if (ring.size() >= 3 && ring.first() != ring.last())
      ring.append(ring.first());
    geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
    ok = !geom.isEmpty();
    if (ok && !geom.isGeosValid()) {
      const QgsGeometry fixed = geom.makeValid();
      if (!fixed.isEmpty())
        geom = fixed;
    }
    if (!geom.isEmpty() && (geom.isMultipart() || geom.type() != Qgis::GeometryType::Polygon)) {
      QgsGeometry best;
      double bestA = -1;
      const QVector<QgsGeometry> parts = geom.asGeometryCollection();
      for (const QgsGeometry& part : parts) {
        if (part.type() != Qgis::GeometryType::Polygon) continue;
        if (part.isMultipart()) {
          const QgsMultiPolygonXY mp = part.asMultiPolygon();
          for (const QgsPolygonXY& poly : mp) {
            const QgsGeometry one = QgsGeometry::fromPolygonXY(poly);
            const double a = one.area();
            if (a > bestA) {
              bestA = a;
              best = one;
            }
          }
        } else {
          const double a = part.area();
          if (a > bestA) {
            bestA = a;
            best = part;
          }
        }
      }
      if (!best.isEmpty())
        geom = best;
    }
    ok = !geom.isEmpty() && geom.type() == Qgis::GeometryType::Polygon;
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
