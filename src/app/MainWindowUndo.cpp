#include "MainWindow.h"
#include "KaAlignMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaDrawingStudio.h"
#include "KaFeatureSelectTool.h"
#include "KaTerrain3dLayoutStudio.h"
#include "core/LayerOps.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QScopedValueRollback>
#include <QTextEdit>
#include <QTabWidget>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>
#include <qgsvectordataprovider.h>
#include <algorithm>

struct KaRemovedLayers {
  struct Entry {
    std::unique_ptr<QgsMapLayer> layer;
    std::unique_ptr<QgsLayerTreeNode> node;
    QPointer<QgsLayerTreeGroup> parent;
    int index = 0;
  };
  std::vector<Entry> entries;
};

namespace {
bool editingText() {
  QWidget* focus = QApplication::focusWidget();
  return qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus) ||
         qobject_cast<QTextEdit*>(focus) || qobject_cast<QPlainTextEdit*>(focus);
}

// The edit buffer belongs to the user. Undo must not commit other pending edits
// or discard them when the provider cannot save this operation.
bool editFeatures(QgsVectorLayer* layer, const QString& title,
                  const std::function<bool()>& change, QString* error) {
  if (!layer || !layer->isValid() || LayerOps::isReferenceLayer(layer)) {
    *error = QStringLiteral("도형이 있는 조사 레이어를 먼저 선택하세요.");
    return false;
  }
  if (layer->isEditCommandActive()) {
    *error = QStringLiteral("진행 중인 도형 편집을 마친 뒤 다시 실행하세요.");
    return false;
  }
  const bool hadPendingEdits = layer->isModified();
  if (!layer->isEditable() && !layer->startEditing()) {
    *error = QStringLiteral("편집을 시작하지 못했습니다. 파일의 쓰기 권한을 확인하세요.");
    return false;
  }
  layer->beginEditCommand(title);
  if (!change()) {
    layer->destroyEditCommand();
    *error = QStringLiteral("도형을 변경하지 못했습니다. 기존 편집은 유지됩니다.");
    return false;
  }
  layer->endEditCommand();
  if (!hadPendingEdits && !layer->commitChanges(false)) {
    *error = QStringLiteral("화면에는 반영했지만 파일에 저장하지 못했습니다. 편집은 남아 있으니 조사 저장을 다시 시도하세요.\n%1")
                 .arg(layer->commitErrors().join(QLatin1Char('\n')));
  }
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}
}

void MainWindow::removeSelectedLayers() {
  removeLayersFromTree(m_layerTree);
}

void MainWindow::watchUndoFeatureIds(QgsVectorLayer* layer) {
  if (!layer || m_undoObservedLayers.contains(layer->id())) return;
  m_undoObservedLayers.insert(layer->id());
  const QPointer<QgsVectorLayer> guarded(layer);
  auto pending = std::make_shared<QgsFeatureList>();
  connect(layer, &QgsVectorLayer::beforeCommitChanges, this, [guarded, pending](bool) {
    pending->clear();
    if (guarded && guarded->editBuffer())
      *pending = guarded->editBuffer()->addedFeatures().values();
  });
  connect(layer, &QgsVectorLayer::committedFeaturesAdded, this,
      [this, guarded, pending](const QString& id, const QgsFeatureList& committed) {
    if (!guarded || !guarded->dataProvider()) return;
    const auto primaryKeys = guarded->dataProvider()->pkAttributeIndexes();
    for (const auto& saved : committed) {
      auto previous = std::find_if(pending->begin(), pending->end(), [&](const QgsFeature& temporary) {
        if (temporary.geometry().asWkb() != saved.geometry().asWkb()) return false;
        for (int i = 0; i < temporary.attributeCount(); ++i) {
          if (!primaryKeys.contains(i) && temporary.attribute(i) != saved.attribute(i)) return false;
        }
        return true;
      });
      if (previous == pending->end()) continue;
      const auto oldId = previous->id();
      for (auto& action : m_undoActions) {
        if (action.layerId == id && action.featureId == oldId) action.featureId = saved.id();
        for (auto& deleted : action.deletedFeatures) {
          if (deleted.first == id && deleted.second.id() == oldId) {
            deleted.second.setId(saved.id());
            for (int key : primaryKeys) deleted.second.setAttribute(key, saved.attribute(key));
          }
        }
      }
      pending->erase(previous);
    }
  });
}

void MainWindow::removeLayersFromTree(QgsLayerTreeView* tree) {
  if (!tree || m_isOpeningSurvey || m_closingWindow || editingText()) return;
  QList<QgsMapLayer*> selected = tree->selectedLayers();
  if (selected.isEmpty() && tree->currentLayer()) selected.append(tree->currentLayer());
  if (selected.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("제거할 레이어를 먼저 클릭하세요."), 4000);
    return;
  }
  auto* project = QgsProject::instance();
  auto* root = project->layerTreeRoot();
  auto removed = std::make_shared<KaRemovedLayers>();
  // Snapshot all positions before any node is removed. Keep the actual layer,
  // including unsaved edit buffers and memory data, until restored or discarded.
  for (auto* node : root->findLayers()) {
    QgsMapLayer* layer = node->layer();
    if (!layer || !selected.contains(layer)) continue;
    KaRemovedLayers::Entry entry;
    entry.node.reset(node->clone());
    entry.parent = qobject_cast<QgsLayerTreeGroup*>(node->parent());
    entry.index = entry.parent ? entry.parent->children().indexOf(node) : 0;
    removed->entries.push_back(std::move(entry));
  }
  for (auto& entry : removed->entries) {
    auto* node = qobject_cast<QgsLayerTreeLayer*>(entry.node.get());
    QgsMapLayer* layer = node ? project->mapLayer(node->layerId()) : nullptr;
    if (!layer) continue;
    if (m_editLayer == layer) stopCaptureTool();
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) vector->removeSelection();
    entry.layer.reset(project->takeMapLayer(layer));
  }
  if (removed->entries.empty()) return;
  KaUndoAction action;
  action.type = KaUndoAction::LayersRemoved;
  action.removedLayers = std::move(removed);
  m_undoActions.append(action);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  project->setDirty(true);
  refreshLayerEmptyState();
  updateNextActionStatus();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  statusBar()->showMessage(QStringLiteral("레이어를 목록에서 제거했습니다. Ctrl+Z로 복원할 수 있습니다. 원본 파일은 그대로입니다."), 6000);
}

void MainWindow::undoLastAction() {
  if (editingText()) return;
  if (m_viewTabs && m_terrain3dLayoutStudio && m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio->undoLastChange();
    return;
  }
  if (routeEditKeyToActiveStudio(false)) return;
  undoMapAction();
}

void MainWindow::undoMapAction() {
  if (editingText() || m_isOpeningSurvey || m_closingWindow) return;
  if (m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool && m_captureTool->undoLastVertex()) {
    statusBar()->showMessage(QStringLiteral("꼭짓점 하나를 되돌렸습니다."), 4000);
    return;
  }
  if (m_alignTool && m_canvas && m_canvas->mapTool() == m_alignTool && m_alignTool->removeLastPair()) {
    statusBar()->showMessage(m_alignTool->statusText(), 4000);
    return;
  }
  if (m_undoActions.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("되돌릴 것이 없습니다."), 4000);
    return;
  }
  auto* project = QgsProject::instance();
  const KaUndoAction action = m_undoActions.last();
  QString error;
  bool applied = false;
  if (action.type == KaUndoAction::LayersRemoved && action.removedLayers) {
    QScopedValueRollback<bool> restoring(m_isOpeningSurvey, true);
    for (auto& entry : action.removedLayers->entries) {
      if (!entry.layer) continue;
      const QPointer<QgsMapLayer> layer(entry.layer.release());
      if (!project->addMapLayer(layer, false) || !layer) {
        if (layer && !project->mapLayer(layer->id())) entry.layer.reset(layer);
        error = QStringLiteral("레이어를 복원하지 못했습니다. 다시 Ctrl+Z를 눌러 보세요.");
        break;
      }
      auto* parent = entry.parent ? entry.parent.data() : project->layerTreeRoot();
      parent->insertChildNode(std::clamp(entry.index, 0, int(parent->children().size())), entry.node.release());
      if (m_layerTree) m_layerTree->setCurrentLayer(layer);
    }
    applied = error.isEmpty();
  } else if (action.type == KaUndoAction::LayerAdded) {
    if (project->mapLayer(action.layerId)) {
      project->removeMapLayer(action.layerId);
      applied = true;
    }
  } else if (action.type == KaUndoAction::FeatureAdded || action.type == KaUndoAction::FeatureChanged || action.type == KaUndoAction::AttributesChanged) {
    auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(action.layerId));
    if (layer && layer->getFeature(action.featureId).isValid()) {
      QgsGeometry previousGeometry = action.featureData.geometry();
      applied = editFeatures(layer, QStringLiteral("되돌리기"), [&]() {
        if (action.type == KaUndoAction::AttributesChanged) {
          QgsAttributeMap values;
          const auto primary = layer->dataProvider()->pkAttributeIndexes();
          for (int i = 0; i < action.featureData.attributeCount(); ++i)
            if (!primary.contains(i)) values.insert(i, action.featureData.attribute(i));
          return layer->changeAttributeValues(action.featureId, values);
        }
        return action.type == KaUndoAction::FeatureAdded ? layer->deleteFeature(action.featureId)
            : layer->changeGeometry(action.featureId, previousGeometry);
      }, &error);
    }
  } else if (action.type == KaUndoAction::FeatureDeleted) {
    // Restore one user operation (possibly several selected shapes), not one
    // arbitrary feature per key press. Successfully restored entries are removed
    // from the pending command so a failed layer can be retried without duplicates.
    auto& pending = m_undoActions.last().deletedFeatures;
    while (!pending.isEmpty()) {
      const QString id = pending.first().first;
      auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(id));
      if (!layer) break;
      QgsFeatureList restored;
      for (const auto& record : pending) if (record.first == id) restored.append(record.second);
      QgsFeatureList committed;
      const auto connection = connect(layer, &QgsVectorLayer::committedFeaturesAdded, this,
          [&committed](const QString&, const QgsFeatureList& features) { committed = features; });
      const bool ok = editFeatures(layer, QStringLiteral("삭제 되돌리기"), [&]() {
        return layer->addFeatures(restored);
      }, &error);
      disconnect(connection);
      if (!ok) break;
      const QgsFeatureList& actual = committed.isEmpty() ? restored : committed;
      int index = 0;
      for (const auto& record : pending) {
        if (record.first != id) continue;
        if (index < actual.size()) {
          for (auto& older : m_undoActions) {
            if (older.layerId == id && older.featureId == record.second.id())
              older.featureId = actual.at(index).id();
          }
        }
        ++index;
      }
      pending.removeIf([&](const auto& record) { return record.first == id; });
    }
    applied = pending.isEmpty();
  }
  if (!applied) {
    notify(Notice::Warning, QStringLiteral("되돌리기"), error.isEmpty()
        ? QStringLiteral("이전 작업의 레이어나 도형을 찾지 못했습니다. 레이어를 복원한 뒤 다시 실행하세요.") : error);
    return;
  }
  m_undoActions.removeLast();
  project->setDirty(true);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  refreshWorkPanel();
  if (!error.isEmpty()) notify(Notice::Warning, QStringLiteral("되돌리기 저장 확인"), error);
  else statusBar()->showMessage(QStringLiteral("이전 상태로 되돌렸습니다."), 4000);
}

void MainWindow::deleteSelectedFeatures() {
  if (editingText()) return;
  const auto selected = KaFeatureSelectTool::allSelectedFeatures(m_canvas);
  KaUndoAction action;
  action.type = KaUndoAction::FeatureDeleted;
  QSet<QString> done;
  QStringList errors;
  for (const auto& item : selected) {
    auto* layer = item.layer.data();
    if (!layer || done.contains(layer->id()) || LayerOps::isReferenceLayer(layer)) continue;
    done.insert(layer->id());
    QgsFeatureList features;
    for (const auto& candidate : selected) {
      if (candidate.layer == layer) {
        const QgsFeature feature = layer->getFeature(candidate.fid);
        if (feature.isValid()) features.append(feature);
      }
    }
    if (features.isEmpty()) continue;
    QString error;
    if (editFeatures(layer, QStringLiteral("도형 삭제"), [&]() {
      for (const auto& feature : features) if (!layer->deleteFeature(feature.id())) return false;
      return true;
    }, &error)) {
      for (const auto& feature : features) action.deletedFeatures.append({layer->id(), feature});
      layer->removeSelection();
    }
    if (!error.isEmpty()) errors.append(layer->name() + QStringLiteral(": ") + error);
  }
  if (!action.deletedFeatures.isEmpty()) {
    m_undoActions.append(action);
    QgsProject::instance()->setDirty(true);
    if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
    if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
    refreshWorkPanel();
    statusBar()->showMessage(QStringLiteral("도형 %1개를 지웠습니다. Ctrl+Z로 복원할 수 있습니다.").arg(action.deletedFeatures.size()), 6000);
  }
  if (!errors.isEmpty()) notify(Notice::Warning, QStringLiteral("도형 삭제 확인"), errors.join(QLatin1Char('\n')));
}
