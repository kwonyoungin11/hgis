#include "KaTrenchMoveTool.h"
#include "KaCanvasGridOverlay.h"
#include "core/LayerOps.h"

#include <cmath>

#include <QKeyEvent>

#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsrubberband.h>
#include <qgsvectorlayer.h>

KaTrenchMoveTool::KaTrenchMoveTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::ArrowCursor);
}

KaTrenchMoveTool::~KaTrenchMoveTool() {
  delete m_rubber;
  delete m_singleRubber;
}

void KaTrenchMoveTool::setLayer(QgsVectorLayer* layer) {
  m_layer = layer;
  clearSingleSelection();
}

void KaTrenchMoveTool::setSnapMeters(double meters) {
  m_snapM = meters > 0.0 ? meters : 0.0;
}

void KaTrenchMoveTool::setGridOverlay(KaCanvasGridOverlay* grid) {
  m_grid = grid;
}

void KaTrenchMoveTool::setMode(Mode mode) {
  m_mode = mode;
  m_awaitDrop = false;
  m_dragSingle = false;
  if (m_rubber)
    m_rubber->reset(Qgis::GeometryType::Polygon);
  clearSingleSelection();
  setCursor(mode == Mode::Single ? Qt::ArrowCursor : Qt::CrossCursor);
}

QgsPointXY KaTrenchMoveTool::snapPt(const QgsPointXY& p) const {
  if (m_grid && m_grid->isEnabled())
    return m_grid->snapToGrid(p);
  if (!(m_snapM > 1e-9))
    return p;
  return QgsPointXY(std::round(p.x() / m_snapM) * m_snapM,
                    std::round(p.y() / m_snapM) * m_snapM);
}

QgsPointXY KaTrenchMoveTool::nearestVertex(const QgsPointXY& p) const {
  if (!m_layer)
    return p;
  QgsPointXY best = p;
  double bestD = 1e99;
  QgsFeature f;
  auto it = m_layer->getFeatures();
  while (it.nextFeature(f)) {
    const QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    for (auto v = g.vertices_begin(); v != g.vertices_end(); ++v) {
      const QgsPointXY xy((*v).x(), (*v).y());
      const double d = xy.sqrDist(p);
      if (d < bestD) {
        bestD = d;
        best = xy;
      }
    }
  }
  return best;
}

QgsFeatureId KaTrenchMoveTool::hitTrench(const QgsPointXY& p, QString* nameOut) const {
  if (!m_layer)
    return -1;
  QgsFeature f;
  QgsFeatureIterator it = m_layer->getFeatures();
  while (it.nextFeature(f)) {
    const QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    if (g.boundingBox().contains(p) && g.contains(&p)) {
      if (nameOut)
        *nameOut = f.attribute(QStringLiteral("name")).toString();
      return f.id();
    }
  }
  return -1;
}

void KaTrenchMoveTool::rebuildRubber(const QgsPointXY& offset) {
  if (!canvas() || !m_layer)
    return;
  if (!m_rubber) {
    m_rubber = new QgsRubberBand(canvas(), Qgis::GeometryType::Polygon);
    m_rubber->setWidth(2);
    m_rubber->setColor(QColor(220, 38, 38));
    m_rubber->setFillColor(QColor(220, 38, 38, 40));
  }
  m_rubber->reset(Qgis::GeometryType::Polygon);
  QgsFeatureIterator it = m_layer->getFeatures();
  QgsFeature f;
  bool first = true;
  while (it.nextFeature(f)) {
    QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    g.translate(offset.x(), offset.y());
    m_rubber->addGeometry(g, nullptr, first);
    first = false;
  }
}

void KaTrenchMoveTool::rebuildSingleRubber(QgsFeatureId fid, const QgsPointXY& offset) {
  if (!canvas() || !m_layer || fid < 0)
    return;
  if (!m_singleRubber) {
    m_singleRubber = new QgsRubberBand(canvas(), Qgis::GeometryType::Polygon);
    m_singleRubber->setWidth(3);
    m_singleRubber->setColor(QColor(30, 103, 198));
    m_singleRubber->setFillColor(QColor(30, 103, 198, 60));
  }
  m_singleRubber->reset(Qgis::GeometryType::Polygon);
  QgsFeature f = m_layer->getFeature(fid);
  if (!f.isValid() || !f.hasGeometry())
    return;
  QgsGeometry g = f.geometry();
  g.translate(offset.x(), offset.y());
  m_singleRubber->addGeometry(g, nullptr, true);
}

void KaTrenchMoveTool::clearSingleSelection() {
  m_selFid = -1;
  m_selName.clear();
  if (m_singleRubber)
    m_singleRubber->reset(Qgis::GeometryType::Polygon);
}

void KaTrenchMoveTool::applyTranslate(double dx, double dy) {
  if (!m_layer || (dx == 0.0 && dy == 0.0))
    return;
  if (!m_layer->isEditable() && !m_layer->startEditing())
    return;
  QgsFeatureIterator it = m_layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    QgsGeometry g = f.geometry();
    if (g.isNull())
      continue;
    g.translate(dx, dy);
    m_layer->changeGeometry(f.id(), g);
  }
  m_layer->commitChanges(false);
  if (!m_layer->isEditable())
    m_layer->startEditing();
  m_layer->updateExtents();
  m_layer->triggerRepaint();
  if (canvas())
    canvas()->refresh();
}

void KaTrenchMoveTool::applyTranslateOne(QgsFeatureId fid, double dx, double dy) {
  if (!m_layer || fid < 0 || (dx == 0.0 && dy == 0.0))
    return;
  if (!m_layer->isEditable() && !m_layer->startEditing())
    return;
  QgsFeature f = m_layer->getFeature(fid);
  if (f.isValid() && f.hasGeometry()) {
    QgsGeometry g = f.geometry();
    g.translate(dx, dy);
    m_layer->changeGeometry(fid, g);
  }
  m_layer->commitChanges(false);
  if (!m_layer->isEditable())
    m_layer->startEditing();
  m_layer->updateExtents();
  m_layer->triggerRepaint();
  if (canvas())
    canvas()->refresh();
}

void KaTrenchMoveTool::deleteTrench(QgsFeatureId fid, const QString& name) {
  if (!m_layer || fid < 0)
    return;
  if (!m_layer->isEditable() && !m_layer->startEditing())
    return;
  m_layer->deleteFeature(fid);
  m_layer->commitChanges(false);
  if (!m_layer->isEditable())
    m_layer->startEditing();
  m_layer->updateExtents();
  m_layer->triggerRepaint();
  if (canvas())
    canvas()->refresh();
  if (m_selFid == fid)
    clearSingleSelection();
  emit statusMessage(QStringLiteral("%1 삭제. 남은 트렌치 %2개.")
                         .arg(name.isEmpty() ? QStringLiteral("트렌치") : name)
                         .arg(m_layer->featureCount()));
}

void KaTrenchMoveTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e)
    return;
  if (e->button() == Qt::RightButton) {
    // 우클릭 = 그 트렌치 하나만 삭제 (양쪽 모드 공통)
    m_awaitDrop = false;
    m_dragSingle = false;
    if (m_rubber)
      m_rubber->reset(Qgis::GeometryType::Polygon);
    QString name;
    const QgsFeatureId fid = hitTrench(e->mapPoint(), &name);
    if (fid < 0) {
      emit statusMessage(QStringLiteral("지울 트렌치 위에서 우클릭하세요."));
      return;
    }
    deleteTrench(fid, name);
    return;
  }
  if (e->button() != Qt::LeftButton)
    return;
  const QgsPointXY click = e->mapPoint();

  if (m_mode == Mode::Single) {
    QString name;
    const QgsFeatureId fid = hitTrench(click, &name);
    if (fid < 0) {
      clearSingleSelection();
      emit statusMessage(QStringLiteral("트렌치를 클릭해 선택하세요. 끌면 이동, Delete·우클릭이면 삭제됩니다."));
      return;
    }
    m_selFid = fid;
    m_selName = name;
    m_from = click;
    m_dragSingle = true;
    rebuildSingleRubber(fid, QgsPointXY(0, 0));
    emit statusMessage(QStringLiteral("%1 선택 — 끌어서 이동, Delete = 삭제")
                           .arg(name.isEmpty() ? QStringLiteral("트렌치") : name));
    return;
  }

  m_from = snapPt(click);
  m_awaitDrop = false;
  m_dragging = true;
  rebuildRubber(QgsPointXY(0, 0));
  emit statusMessage(QStringLiteral("격자를 끌어 옮기세요. 놓으면 전체가 이동합니다."));
}

void KaTrenchMoveTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!e)
    return;
  if (m_mode == Mode::Single) {
    if (!m_dragSingle || m_selFid < 0)
      return;
    const QgsPointXY dest = snapPt(e->mapPoint());
    rebuildSingleRubber(m_selFid, QgsPointXY(dest.x() - m_from.x(), dest.y() - m_from.y()));
    return;
  }
  if (m_dragging) {
    const QgsPointXY dest = snapPt(e->mapPoint());
    rebuildRubber(QgsPointXY(dest.x() - m_from.x(), dest.y() - m_from.y()));
  }
}

void KaTrenchMoveTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || e->button() != Qt::LeftButton)
    return;
  const QgsPointXY dest = snapPt(e->mapPoint());
  const double dx = dest.x() - m_from.x();
  const double dy = dest.y() - m_from.y();
  const double mupp = canvas() ? std::max(canvas()->mapUnitsPerPixel(), 1e-6) : 1.0;
  if (m_mode == Mode::Whole && m_dragging) {
    m_dragging = false;
    if (std::hypot(dx, dy) > 2.0 * mupp)
      applyTranslate(dx, dy);
    if (m_rubber)
      m_rubber->reset(Qgis::GeometryType::Polygon);
    emit statusMessage(QStringLiteral("격자 이동 완료. 다시 끌어 옮기거나 우클릭으로 하나씩 지우세요."));
    return;
  }
  if (m_mode != Mode::Single || !m_dragSingle || m_selFid < 0)
    return;
  m_dragSingle = false;
  if (std::hypot(dx, dy) > 2.0 * mupp) {
    applyTranslateOne(m_selFid, dx, dy);
    emit statusMessage(QStringLiteral("%1 이동 완료. Delete = 삭제, 다른 트렌치 클릭 = 선택 변경")
                           .arg(m_selName.isEmpty() ? QStringLiteral("트렌치") : m_selName));
  }
  rebuildSingleRubber(m_selFid, QgsPointXY(0, 0));
}

void KaTrenchMoveTool::keyPressEvent(QKeyEvent* e) {
  if (e && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && m_selFid >= 0) {
    deleteTrench(m_selFid, m_selName);
    e->accept();
    return;
  }
  if (e && e->key() == Qt::Key_Escape) {
    m_awaitDrop = false;
    m_dragSingle = false;
    if (m_rubber)
      m_rubber->reset(Qgis::GeometryType::Polygon);
    clearSingleSelection();
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaTrenchMoveTool::activate() {
  QgsMapTool::activate();
  m_awaitDrop = false;
  m_dragging = false;
  m_dragSingle = false;
  if (m_mode == Mode::Single) {
    setCursor(Qt::ArrowCursor);
    emit statusMessage(QStringLiteral(
        "개별 편집: 트렌치 클릭 = 선택, 끌기 = 이동, Delete·우클릭 = 삭제"));
  } else {
    setCursor(Qt::CrossCursor);
    emit statusMessage(QStringLiteral(
        "전체 이동: 격자를 끌어 옮기세요. 우클릭 = 개별 삭제"));
  }
}

void KaTrenchMoveTool::deactivate() {
  m_dragging = false;
  m_awaitDrop = false;
  m_dragSingle = false;
  if (m_rubber)
    m_rubber->reset(Qgis::GeometryType::Polygon);
  clearSingleSelection();
  QgsMapTool::deactivate();
}
