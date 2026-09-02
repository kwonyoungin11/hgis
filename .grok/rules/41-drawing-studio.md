# Plan drawing studio (do not regress)

Applies when editing `KaDrawingStudio.*`, `LayoutService.*` (sheet/chrome/map
item), `tests/test_workflow.cpp` layout cases, or MainWindow layout-tab wiring.

Crash table: `docs/ERROR_REGRESSION.md` (조판 용지 안 범위 다운).
Section studio is `.grok/rules/40-section-studio.md` — do not copy it here.

## Layout attach order

```
setCurrentLayout(ly)
m_toolSelect->setLayout(ly)   // creates QgsLayoutMouseHandles + addItem
m_view->setTool(m_toolSelect) // only AFTER setLayout
```

Never `setTool` before the view has a layout. Never `setLayout(nullptr)`.
Detach: `unsetTool` then `QGraphicsView::setScene(nullptr)`.

## Map item (QgsLayoutItemMap)

Cookbook: size → `zoomToExtent` → `addLayoutItem`.
ka-hgis: `attemptSetSceneRect` → `setCrs` → dummy/no live WMS → `zoomToExtent`
→ `addLayoutItem` → then satellite/cadastral.

- Never `setExtent` on a sheet map (it resizes the mm frame). Use `zoomToExtent`
  / `setScale(s, true)` / `moveContent`.
- Never `setLayers({})` (loads every WMS). Empty → dummy memory polygon.
- Do not `layout->refresh()` / `map->refresh()` while a WMS job may run.
- Do not use QGIS `QgsLayoutViewRectangularRubberBand` (layout() null crash).

## Product contract

- Sheet name `user_sheet`. Ids: `ka_map`, `ka_legend`, `ka_north`, `ka_scalebar`.
- 맵↔조판: 조판 진입 시 지도 화면 범위·축척을 맵 칸에 그대로 옮긴다
  (`applyCanvasViewToLayoutMap`). nice scale로 분모를 올리지 않는다.
- 좌표점 is layout-only (no map-canvas coord tool; not `control_points`).
- Scale bar: paper mm length stays; retick `unitsPerSegment` only.
- Preview 96 DPI; PDF 300. Rasters: `applySingleRasterPassRendering` (no tiles).
- North on paper = the sample picture, not a letter `N` fallback.

Do not migrate `section_line`. Do not clone QGIS Layout Designer chrome.
