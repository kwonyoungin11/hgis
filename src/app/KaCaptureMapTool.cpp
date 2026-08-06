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
  QgsMapCanvas* c = canvas();
  if (c) {
    m_rubber->reset(Qgis::GeometryType::Line);
  }
  delete m_rubber;
  m_rubber = nullptr;
}

void KaCaptureMapTool::rebuildRubber(const QgsPointXY* cursorOrNull) {
  QgsMapCanvas* c = canvas();
  if (!c) return;

  if (!m_rubber) {
    Qgis::GeometryType gt = Qgis::GeometryType::Line;
    if (m_mode == Mode::Polygon) gt = Qgis::GeometryType::Polygon;
    else if (m_mode == Mode::Point) gt = Qgis::GeometryType::Point;
    m_rubber = new QgsRubberBand(c, gt);
    m_rubber->setWidth(2);
    m_rubber->setSecondaryStrokeColor(QColor(255, 255, 255, 180));
    m_rubber->setColor(QColor(37, 99, 235));
    m_rubber->setFillColor(QColor(37, 99, 235, 70));
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
  QgsMapTool::activate();
}

void KaCaptureMapTool::deactivate() {
  destroyRubber();
  m_points.clear();
  m_finishing = false;
  QgsMapTool::deactivate();
}

void KaCaptureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e || !canvas() || m_finishing) return;
  e->accept();

  QgsPointXY mapPt;
  try {
    mapPt = e->mapPoint();
  } catch (...) {
    return;
  }
  if (std::isnan(mapPt.x()) || std::isnan(mapPt.y()))
    return;

  if (e->button() == Qt::LeftButton) {
    if (m_mode == Mode::Point) {
      m_points.clear();
      m_points.append(mapPt);
      finish();
      return;
    }
    m_points.append(mapPt);
    rebuildRubber(&mapPt);
    return;
  }

  if (e->button() == Qt::RightButton) {
    finish();
  }
}

void KaCaptureMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e || m_points.isEmpty() || m_mode == Mode::Point || m_finishing) return;
  try {
    const QgsPointXY mapPt = e->mapPoint();
    rebuildRubber(&mapPt);
  } catch (...) {
  }
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
  if (e->key() == Qt::Key_Backspace) {
    if (!m_points.isEmpty()) {
      m_points.removeLast();
      if (m_points.isEmpty())
        destroyRubber();
      else if (canvas()) {
        const QgsPointXY cur = toMapCoordinates(canvas()->mouseLastXY());
        rebuildRubber(&cur);
      }
    }
    e->accept();
  }
}

void KaCaptureMapTool::finish() {
  if (m_finishing) return;
  m_finishing = true;

  QgsGeometry geom;
  bool ok = false;
  if (m_mode == Mode::Point && m_points.size() >= 1) {
    geom = QgsGeometry::fromPointXY(m_points.first());
    ok = !geom.isEmpty();
  } else if (m_mode == Mode::Line && m_points.size() >= 2) {
    geom = QgsGeometry::fromPolylineXY(m_points);
    ok = !geom.isEmpty();
  } else if (m_mode == Mode::Polygon && m_points.size() >= 3) {
    QgsPolylineXY ring = m_points;
    if (!ring.isEmpty() && ring.first() != ring.last())
      ring.append(ring.first());
    geom = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
    ok = !geom.isEmpty() && geom.isGeosValid();
    if (!ok && !geom.isEmpty()) {
      geom = geom.makeValid();
      ok = !geom.isEmpty();
    }
  }

  const QVector<QgsPointXY> savedPts = m_points;
  m_points.clear();
  destroyRubber();

  if (!ok) {
    m_finishing = false;
    emit captureCanceled();
    return;
  }

  if (m_layer && canvas()) {
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
  Q_UNUSED(savedPts);
}

void KaCaptureMapTool::cancel() {
  m_points.clear();
  destroyRubber();
  m_finishing = false;
  emit captureCanceled();
}

