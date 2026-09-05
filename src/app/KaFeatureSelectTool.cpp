#include "KaFeatureSelectTool.h"

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsgeometry.h>
#include <qgsrubberband.h>
#include <qgsproject.h>
#include <qgscoordinatetransform.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsdistancearea.h>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QTimer>
#include <QColor>
#include <cmath>
#include <limits>

KaFeatureSelectTool::KaFeatureSelectTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {}

KaFeatureSelectTool::~KaFeatureSelectTool() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
}

void KaFeatureSelectTool::activate() {
  QgsMapTool::activate();
  if (mCanvas) {
    mCanvas->setCursor(Qt::ArrowCursor);
  }
  emit statusMessage(QStringLiteral("도형선택: 클릭으로 도형 선택 / Shift+클릭으로 추가 선택 / 우클릭 시 면적(㎡·평) 계산"));
}

void KaFeatureSelectTool::deactivate() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
  m_dragging = false;
  QgsMapTool::deactivate();
}

void KaFeatureSelectTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  m_pressPos = e->pos();
  m_pressMapPt = e->mapPoint();
  m_dragging = false;
}

void KaFeatureSelectTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (!(e->buttons() & Qt::LeftButton)) return;

  if (!m_dragging) {
    if ((e->pos() - m_pressPos).manhattanLength() > 4) {
      m_dragging = true;
      if (!m_rubberBand && mCanvas) {
        m_rubberBand = new QgsRubberBand(mCanvas, Qgis::GeometryType::Polygon);
        m_rubberBand->setColor(QColor(30, 103, 198, 180));
        m_rubberBand->setWidth(1);
        m_rubberBand->setFillColor(QColor(30, 103, 198, 40));
      }
    }
  }

  if (m_dragging && m_rubberBand) {
    QgsRectangle rect(m_pressMapPt, e->mapPoint());
    m_rubberBand->setToGeometry(QgsGeometry::fromRect(rect), nullptr);
  }
}

void KaFeatureSelectTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (e->button() == Qt::RightButton) {
    handleContextMenu(e);
    return;
  }
  if (e->button() != Qt::LeftButton) return;

  const bool shift = (e->modifiers() & Qt::ShiftModifier);

  if (m_dragging) {
    m_dragging = false;
    if (m_rubberBand) {
      delete m_rubberBand;
      m_rubberBand = nullptr;
    }
    QgsRectangle mapRect(m_pressMapPt, e->mapPoint());
    selectInRect(mapRect, shift);
  } else {
    selectAtPoint(e->mapPoint(), shift);
  }
}

void KaFeatureSelectTool::handleContextMenu(QgsMapMouseEvent* e) {
  if (!mCanvas || !QgsProject::instance()) return;

  // 선택된 도형이 없으면 우클릭한 위치의 도형을 선택
  auto all = allSelectedFeatures(mCanvas);
  if (all.isEmpty()) {
    selectAtPoint(e->mapPoint(), false);
    all = allSelectedFeatures(mCanvas);
  }

  double totalAreaM2 = 0.0;
  int polyCount = 0;
  bool hasMultipart = false;

  for (const auto& item : all) {
    if (!item.layer || !item.layer->isValid() || item.layer->geometryType() != Qgis::GeometryType::Polygon)
      continue;
    QgsFeature f;
    if (!item.layer->getFeatures(QgsFeatureRequest(item.fid)).nextFeature(f) || !f.hasGeometry())
      continue;
    QgsGeometry geom = f.geometry();
    if (geom.isEmpty()) continue;
    if (geom.isMultipart()) hasMultipart = true;

    QgsDistanceArea da;
    da.setSourceCrs(item.layer->crs(), QgsProject::instance()->transformContext());
    da.setEllipsoid(QgsProject::instance()->ellipsoid());
    double area = da.measureArea(geom);
    if (area <= 0.0) area = std::abs(geom.area());
    totalAreaM2 += area;
    polyCount++;
  }

  if (polyCount == 0) {
    emit statusMessage(QStringLiteral("우클릭 위치에 폴리곤 도형이 없습니다."));
    return;
  }

  const double pyeong = totalAreaM2 * 0.3025;
  QString areaStr;
  if (polyCount == 1) {
    areaStr = QStringLiteral("면적: %L1 ㎡ (약 %L2평)")
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  } else {
    areaStr = QStringLiteral("선택한 폴리곤 %1개 총 면적: %L2 ㎡ (약 %L3평)")
                  .arg(polyCount)
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  }
  emit statusMessage(areaStr);

  QMenu menu(mCanvas);
  auto* actArea = menu.addAction(areaStr);
  QFont boldFont = actArea->font();
  boldFont.setBold(true);
  actArea->setFont(boldFont);

  auto* actCopy = menu.addAction(QStringLiteral("면적 복사"));
  connect(actCopy, &QAction::triggered, [areaStr]() {
    QApplication::clipboard()->setText(areaStr);
  });
  menu.addSeparator();

  if (polyCount >= 2) {
    auto* actMerge = menu.addAction(QStringLiteral("폴리곤 묶기 (선택된 %1개 하나로 합치기)").arg(polyCount));
    connect(actMerge, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestMerge(); });
    });
  }
  if (polyCount == 2) {
    auto* actSplitOverlap = menu.addAction(QStringLiteral("중첩부 잘라서 나누기 (겹친 구간 분할)"));
    connect(actSplitOverlap, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestSplit(); });
    });
  }
  if (hasMultipart) {
    auto* actExplode = menu.addAction(QStringLiteral("폴리곤 나누기 (묶인 그룹 분리)"));
    connect(actExplode, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestSplit(); });
    });
  }
  if (polyCount == 1 && !hasMultipart) {
    auto* actSplit = menu.addAction(QStringLiteral("폴리곤 나누기 (선을 그어 자르기)"));
    connect(actSplit, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestSplit(); });
    });
  }

  menu.addSeparator();
  auto* actDeselect = menu.addAction(QStringLiteral("선택 해제"));
  connect(actDeselect, &QAction::triggered, [this]() {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        vl->removeSelection();
        vl->triggerRepaint();
      }
    }
    if (mCanvas) mCanvas->refresh();
    emit selectionChanged(0);
    emit statusMessage(QStringLiteral("선택 해제됨"));
  });

  menu.exec(QCursor::pos());
}

void KaFeatureSelectTool::selectAtPoint(const QgsPointXY& mapPt, bool addToSelection) {
  if (!mCanvas || !QgsProject::instance()) return;

  if (!addToSelection) {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        if (!vl->selectedFeatureIds().isEmpty()) {
          vl->removeSelection();
          vl->triggerRepaint();
        }
      }
    }
  }

  QList<QgsMapLayer*> layers = mCanvas->layers();
  QgsVectorLayer* hitLayer = nullptr;
  QgsFeatureId hitFid = -1;

  const double mapTol = mCanvas->mapUnitsPerPixel() * 10.0;

  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;

    QgsCoordinateTransform xf;
    const bool needXf = (mCanvas->mapSettings().destinationCrs() != vl->crs());
    if (needXf) {
      xf = QgsCoordinateTransform(mCanvas->mapSettings().destinationCrs(), vl->crs(),
                                  QgsProject::instance()->transformContext());
      xf.setBallparkTransformsAreAppropriate(true);
    }

    QgsPointXY layerPt = mapPt;
    if (needXf) {
      try {
        layerPt = xf.transform(mapPt);
      } catch (...) {
        continue;
      }
    }

    const double layerTol = needXf ? (mapTol * (vl->crs().mapUnits() == Qgis::DistanceUnit::Degrees ? 0.00001 : 1.0)) : mapTol;
    const QgsRectangle searchBox(layerPt.x() - layerTol, layerPt.y() - layerTol,
                                 layerPt.x() + layerTol, layerPt.y() + layerTol);

    QgsFeatureRequest req;
    req.setFilterRect(searchBox);
    QgsFeatureIterator it = vl->getFeatures(req);
    QgsFeature f;
    const QgsGeometry layerProbe = QgsGeometry::fromPointXY(layerPt);

    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
      QgsGeometry g = f.geometry();

      if (vl->geometryType() == Qgis::GeometryType::Polygon) {
        if (g.contains(layerProbe) || g.distance(layerProbe) <= layerTol) {
          hitLayer = vl;
          hitFid = f.id();
          break;
        }
      } else {
        if (g.distance(layerProbe) <= layerTol) {
          hitLayer = vl;
          hitFid = f.id();
          break;
        }
      }
    }

    if (hitLayer && hitFid >= 0) break;
  }

  if (hitLayer && hitFid >= 0) {
    if (addToSelection && hitLayer->selectedFeatureIds().contains(hitFid)) {
      hitLayer->deselect(hitFid);
    } else {
      hitLayer->select(hitFid);
    }
    hitLayer->triggerRepaint();
  }

  mCanvas->refresh();

  auto all = allSelectedFeatures(mCanvas);
  emit selectionChanged(all.size());

  if (all.isEmpty()) {
    emit statusMessage(QStringLiteral("선택된 도형 없음"));
  } else if (all.size() == 1) {
    emit statusMessage(QStringLiteral("도형 1개 선택됨 (%1) — Shift+클릭으로 다른 도형도 선택 가능").arg(all[0].layer->name()));
  } else if (all.size() == 2) {
    emit statusMessage(QStringLiteral("도형 2개 선택됨 (%1, %2) — [폴리곤 나누기] 클릭 시 겹치는 구간이 자동 분할됩니다!").arg(all[0].layer->name(), all[1].layer->name()));
  } else {
    emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
  }
}

void KaFeatureSelectTool::selectInRect(const QgsRectangle& mapRect, bool addToSelection) {
  if (!mCanvas || !QgsProject::instance()) return;

  if (!addToSelection) {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        if (!vl->selectedFeatureIds().isEmpty()) {
          vl->removeSelection();
          vl->triggerRepaint();
        }
      }
    }
  }

  const QgsGeometry mapGeom = QgsGeometry::fromRect(mapRect);
  for (QgsMapLayer* ml : mCanvas->layers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;

    QgsCoordinateTransform xf;
    const bool needXf = (mCanvas->mapSettings().destinationCrs() != vl->crs());
    if (needXf) {
      xf = QgsCoordinateTransform(mCanvas->mapSettings().destinationCrs(), vl->crs(),
                                  QgsProject::instance()->transformContext());
      xf.setBallparkTransformsAreAppropriate(true);
    }

    QgsGeometry layerGeom = mapGeom;
    if (needXf) {
      try {
        layerGeom.transform(xf);
      } catch (...) {
        continue;
      }
    }

    QgsFeatureRequest req;
    req.setFilterRect(layerGeom.boundingBox());
    QgsFeatureIterator it = vl->getFeatures(req);
    QgsFeature f;
    QgsFeatureIds toSelect;
    while (it.nextFeature(f)) {
      if (!f.hasGeometry()) continue;
      if (f.geometry().intersects(layerGeom)) {
        toSelect.insert(f.id());
      }
    }
    if (!toSelect.isEmpty()) {
      vl->selectByIds(toSelect, Qgis::SelectBehavior::AddToSelection);
      vl->triggerRepaint();
    }
  }

  mCanvas->refresh();
  auto all = allSelectedFeatures(mCanvas);
  emit selectionChanged(all.size());
  emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
}

QList<KaFeatureSelectTool::SelectedItem> KaFeatureSelectTool::allSelectedFeatures(QgsMapCanvas* canvas) {
  QList<SelectedItem> list;
  Q_UNUSED(canvas);
  if (!QgsProject::instance()) return list;

  for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(l);
    if (!vl || !vl->isValid()) continue;
    for (QgsFeatureId fid : vl->selectedFeatureIds()) {
      list.append({vl, fid});
    }
  }
  return list;
}
