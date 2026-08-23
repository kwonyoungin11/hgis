---
name: ka-drawing-studio
description: >
  ka-hgis plan drawing studio (KaDrawingStudio user_sheet). Use when the user
  mentions 조판, 도면만들기, 축척자, 방위, 범례, 좌표점, user_sheet, or runs
  /ka-drawing-studio.
when-to-use: 조판, 도면만들기, 축척자, 방위표, 범례, 좌표점, layout studio
---

# Plan drawing studio

Crash/attach SSOT: `.grok/rules/41-drawing-studio.md`. Read it before any edit.
Section drawings: rule `40-section-studio.md` — different tab, do not mix.

## GIS objects

- Project / canvas dest CRS: work EPSG:5186 or 5187 (OTF on).
- Layout map CRS: canvas dest, else project, else 5186. Not 3857 for a TM sheet.
- Extent: `zoomToExtent` keeps item mm. `setExtent` does not.
- Scale: denominator 1:N. Entering layout must not steal scale.

## Do this

1. Sheet `user_sheet`. Map id `ka_map`.
2. Add map only after finite scene rect + CRS + `zoomToExtent`.
3. Linked items (`setLinkedMap`): legend `setResizeToContents(false)`, north
   picture + GridNorth, scale bar Fixed segments (not cookbook `applyDefaultSize`
   as the only size path).
4. 도곽: `LayoutService::applySurveyFrameGrid`. Rasters: single pass, no tiles.
5. Submit PDF of this sheet is `/ka-submit-package`, not `rebuildDefaultLayouts`.

## Files

`src/app/KaDrawingStudio.*`, `src/core/LayoutService.*` (`createBlankSheet`,
`applySingleRasterPassRendering`, `applySurveyFrameGrid`), MainWindow layout
enter (`centerOnMapCanvas`). Tests: `tests/test_workflow.cpp` layout cases.

## Do not

- Clone Layout Designer. Touch section studio / `section_line`.
- Put 좌표점 on the map canvas. Change cadastral WMS CRS away from 3857.
- Call `rebuildDefaultLayouts` to “fill” the studio sheet.
