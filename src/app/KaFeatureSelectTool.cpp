#include "KaFeatureSelectTool.h"

#include "KaVertexEditTool.h"

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
    : QgsMapTool(canvas) {
  // 「도형수정」을 따로 켜지 않고, 도형을 고르면 곧바로 수정점이 나오게 한다.
  // 편집기는 지도 도구로 걸지 않고 기능만 빌려 쓴다.
  m_vertex = new KaVertexEditTool(canvas);
  m_vertex->setParent(this);
  connect(m_vertex, &KaVertexEditTool::statusMessage, this,
          [this](const QString& t) { emit statusMessage(t); });
}

void KaFeatureSelectTool::setSnapEnabled(bool on) {
  if (m_vertex) m_vertex->setSnapEnabled(on);
}

void KaFeatureSelectTool::syncVertexTarget() {
  if (!m_vertex) return;
  const auto all = allSelectedFeatures(mCanvas);
  if (all.size() == 1 && all[0].layer)
    m_vertex->setTarget(all[0].layer, all[0].fid);
  else
    m_vertex->clearTarget();
}

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
  emit statusMessage(QStringLiteral(
      "도형선택: 도형을 클릭하면 수정점이 나옵니다 — 점을 끌어 옮기고, 우클릭하면 "
      "점추가·점삭제·면적입니다. Shift+클릭은 추가 선택."));
}

void KaFeatureSelectTool::deactivate() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
  m_dragging = false;
  m_vertexDragging = false;
  m_vertexIndex = -1;
  if (m_vertex) m_vertex->clearTarget();
  QgsMapTool::deactivate();
}

void KaFeatureSelectTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  // 이미 고른 도형의 수정점을 눌렀으면 선택을 바꾸지 말고 그 점을 끈다.
  if (m_vertex && m_vertex->hasTarget()) {
    const int idx = m_vertex->vertexNear(e->mapPoint(), 20);
    if (idx >= 0) {
      m_vertexDragging = true;
      m_vertexIndex = idx;
      return;
    }
  }
  m_pressPos = e->pos();
  m_pressMapPt = e->mapPoint();
  m_dragging = false;
}

void KaFeatureSelectTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (m_vertexDragging && m_vertex) {
    m_vertex->previewVertexMove(m_vertexIndex, m_vertex->toLayer(m_vertex->snapMapPoint(e)));
    return;
  }
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

  // 끌던 수정점을 놓았다 — 여기서 한 번만 저장한다.
  if (m_vertexDragging && m_vertex) {
    const int idx = m_vertexIndex;
    m_vertexDragging = false;
    m_vertexIndex = -1;
    if (idx >= 0) {
      const bool ok = m_vertex->moveVertexTo(idx, m_vertex->toLayer(m_vertex->snapMapPoint(e)));
      m_vertex->showVertexMarkers();
      if (mCanvas) mCanvas->refresh();
      emit statusMessage(ok ? QStringLiteral("수정점을 옮겼습니다.")
                            : QStringLiteral("수정점을 옮기지 못했습니다."));
    }
    return;
  }

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

  // 점추가·점삭제는 선에서도 써야 하므로 면적 계산과 따로 판단한다.
  syncVertexTarget();
  int vtxIdx = -1;
  int segAfter = -1;
  QgsPointXY onLine;
  if (m_vertex && m_vertex->hasTarget()) {
    const QgsPointXY mapPt = m_vertex->snapMapPoint(e);
    vtxIdx = m_vertex->vertexNear(mapPt, 24);
    segAfter = m_vertex->segmentNear(mapPt, &onLine, 16);
  }
  const bool canEditVertex = (vtxIdx >= 0 || segAfter >= 0);

  if (polyCount == 0 && !canEditVertex) {
    emit statusMessage(QStringLiteral("우클릭 위치에 도형이 없습니다."));
    return;
  }

  const double pyeong = totalAreaM2 * 0.3025;
  QString areaStr;
  if (polyCount == 0) {
    areaStr.clear();
  } else if (polyCount == 1) {
    areaStr = QStringLiteral("면적: %L1 ㎡ (약 %L2평)")
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  } else {
    areaStr = QStringLiteral("선택한 폴리곤 %1개 총 면적: %L2 ㎡ (약 %L3평)")
                  .arg(polyCount)
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  }
  if (!areaStr.isEmpty())
    emit statusMessage(areaStr);

  QMenu menu(mCanvas);

  // 요구: 선에 우클릭하면 점추가·점삭제가 나와야 한다.
  if (canEditVertex) {
    auto* actAddVtx = menu.addAction(QStringLiteral("점추가"));
    actAddVtx->setEnabled(segAfter >= 0);
    connect(actAddVtx, &QAction::triggered, this, [this, segAfter, onLine]() {
      if (m_vertex && m_vertex->insertVertexAt(segAfter, onLine)) {
        m_vertex->showVertexMarkers();
        if (mCanvas) mCanvas->refresh();
        emit statusMessage(QStringLiteral("점을 넣었습니다."));
      }
    });
    auto* actDelVtx = menu.addAction(QStringLiteral("점삭제"));
    actDelVtx->setEnabled(vtxIdx >= 0);
    connect(actDelVtx, &QAction::triggered, this, [this, vtxIdx]() {
      if (m_vertex && m_vertex->deleteVertexAt(vtxIdx)) {
        m_vertex->showVertexMarkers();
        if (mCanvas) mCanvas->refresh();
        emit statusMessage(QStringLiteral("점을 지웠습니다."));
      }
    });
    menu.addSeparator();
  }

  if (!areaStr.isEmpty()) {
    auto* actArea = menu.addAction(areaStr);
    QFont boldFont = actArea->font();
    boldFont.setBold(true);
    actArea->setFont(boldFont);

    auto* actCopy = menu.addAction(QStringLiteral("면적 복사"));
    connect(actCopy, &QAction::triggered, [areaStr]() {
      QApplication::clipboard()->setText(areaStr);
    });
    menu.addSeparator();
  }

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
    if (m_vertex) m_vertex->clearTarget();
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
  syncVertexTarget();
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
  syncVertexTarget();
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
