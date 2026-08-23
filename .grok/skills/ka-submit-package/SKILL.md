---
name: ka-submit-package
description: >
  ka-hgis submission package (SHP EPSG:5179 + MANIFEST + composed sheet PDF).
  Use when the user mentions 제출, 보내기, 검수, SHP, 패키지, 5179, layout_exists,
  or runs /ka-submit-package.
when-to-use: 제출, 보내기, 검수, SHP 패키지, EPSG:5179, MANIFEST
---

# Submission package

Product next (HANDOFF §3): package PDF is the composed studio sheet, not five
auto templates. Checklist must not pass on empty seeded layouts.

## GIS objects

- Work CRS may be 5186/5187. **Upload SHP + PDF CRS = EPSG:5179**.
- Domain layers only via `LayerOps::ensureDomainLayer`. Basemap is 참조 지도.

## Do this

1. SHP: `ExportService::exportSubmissionPackage` → `writeAsVectorFormatV3` to
   EPSG:5179 + `MANIFEST.sha256`. Encoding UTF-8 (CP949 only if asked).
2. PDF: export `user_sheet` (`KaDrawingStudio`) with `QgsLayoutExporter`
   300 DPI / forceVector / `applySingleRasterPassRendering`.
3. Do **not** call `LayoutService::rebuildDefaultLayouts` or `exportDrawingPdfs`
   on the submit path (today `ExportService.cpp` still does — that is the bug).
4. Checklist error still hard-blocks submit (`blockOnError`).
5. `layout_exists:*` in `ProjectStateBuilder` must mean a **composed** sheet
   (map item, non-empty `setLayers`, scale > 0). Name-only `layoutByName` is
   not enough. Prefer `user_sheet` over `site_location` / `feature_plan` / …

## Files

`src/core/ExportService.*`, `LayoutService.*` (`exportDrawingPdfs`,
`rebuildDefaultLayouts`), `ProjectStateBuilder.*`, `ChecklistEngine.*`,
`data/rules/drawing_checklist.v1.json`, `src/app/KaDrawingStudio.cpp`
(`kSheetName`), `tests/test_workflow.cpp` / `tests/test_checklist.cpp`.

## Do not

- DXF as submit. Change upload CRS away from 5179. Auto-seed empty domain
  layers so a template exists. Restore the 7-step rail.
- Section PDF: `SectionLayoutService::exportSectionPdf` (rule 40), not this path.
