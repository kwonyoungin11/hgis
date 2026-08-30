# Section drawing studio (do not regress)

Applies when editing `SectionLayoutService.*`, `KaSectionDrawingStudio.*`,
`tests/test_section_*.cpp`, or MainWindow section-tab wiring.

## Layout attach order

```
setCurrentLayout(ly)
m_toolSelect->setLayout(ly)   // creates QgsLayoutMouseHandles + addItem
m_view->setTool(m_toolSelect) // only AFTER setLayout
```

Never `setTool` before the view has a layout. Never `setLayout(nullptr)`.
Detach: `unsetTool` then `QGraphicsView::setScene(nullptr)`.

## Extent vs ticks

Map frame mm size must match GeoTIFF extent aspect (`frameW/H = ext * 1000 / scale`).
Then `zoomToExtent(combinedExtent)`. Do not put a 10×2 m raster into a 390×261 mm
frame and expect ticks to line up.

Section plane = pixel grid (columns=distance, rows=elevation). The 4×4 world
XY is placement only — do not draw with map easting/northing or the photo
tilts. World-XY files: length=`cols*hypot(gt[1],gt[4])`,
height=`rows*hypot(gt[2],gt[5])`, elevBottom=0 + 표고 보정. True section Y
(해발 100–102): keep `gt[5]`. Display copy is north-up local ENGCRS; 5186/5187
are the title only. Do not `setCrs` or `setDpi` on the source GeoTIFF.
Preview 300 DPI, Nearest (사진실측 1:1), no tiled raster.

## UI contract

- Left list: section GeoTIFF only. Never 위성 / 지적 / survey vectors.
- `geoTiffAddRequested(path)` only; MainWindow `addSectionGeoTiffFromPath`
  (no legend, no canvas zoom).
- Opening the tab shows paper + ticks. Adding a GeoTIFF rebuilds to that extent.
- Default: A3 landscape, CRS 5187, scale auto (0), elev interval 0.10 m, ref line `#D7191C` dash 0.20 mm.
- Scale bar: **one** `QgsLayoutItemScaleBar` id `ka_section_scale_bar` on the sheet, left-aligned under the distance ticks (0 m). Styles (쌍칸/외칸/눈금) live on `#sampleStrip` sample tiles, not as extra layout items. No `_single` / `_ticks` / `_numeric` on paper.
- PDF: `SectionLayoutService::exportSectionPdf` (300 DPI, forceVector, AlwaysText).
- Do not migrate `section_line`. Do not overwrite uncommitted `KaDrawingStudio` / `LayoutService` / `test_workflow`.
