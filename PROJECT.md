# Project: ka-hgis (Korean Field Archaeology Desktop HGIS)

## Architecture
- Architecture B: Standalone C++20/Qt6 desktop application linked to OSGeo4W `qgis_core` and `qgis_gui`. No QGIS fork; do not reimplement PROJ/GDAL/renderer.
- Domain Layers: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`, `trial_trench`. Logic property `ka_hgis/layer_key`.
- Legend Groups: `조사 데이터` (Domain data) vs `참조 지도` (Basemap & references).
- CRS Policy: Working CRS EPSG:5186 / EPSG:5187; Submission package export CRS strictly EPSG:5179.
- Sub-controllers: `ProjectLifecycleController`, `DigitizingStateController`, `LayerStateController` extracted from monolithic `MainWindow` and `LayerOps`.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Layout Composition Validation | Prevent empty/dummy `layout_blank` memory layers from passing `isComposedStudioSheet` | M1 | Survey R1 |
| 2 | Checklist Rule Tightening | Tighten `drawing_checklist.v1.json` & `ProjectStateBuilder` so uncomposed templates fail | M1 | Survey R1 |
| 3 | Submission Package Layout Bundling | Seamlessly export `user_sheet` -> `조사도면.pdf`, check errors, support `section_sheet` | M1 | Survey R1 |
| 4 | Streamed Package Hashing | Chunked 64KB hashing for `MANIFEST.sha256` avoiding memory spikes | M1 | Survey R1 / R3 |
| 5 | Geometry Auto-Repair Pipeline | Implement `sanitizeAndRepairGeometry` (`removeDuplicateNodes` -> `makeValid` -> `buffer(0)`) | M2 | Survey R2 |
| 6 | Digitizing Flow Intranet Guard | Enforce geometry repair in `onGeometryCaptured` and vertex edits to prevent upload rejection | M2 | Survey R2 |
| 7 | VWorld Key Flexible Regex | Update `withVworldApiKey` regex to handle non-hyphenated, empty, or custom keys | M2 | Survey R2 |
| 8 | Cadastral XML Key Refresh | Update local `vworld-cadastral.xml` and reload raster data provider in `refreshVworldApiKeyInLayers` | M2 | Survey R2 |
| 9 | UI VWorld Key Sync | Wire `MainWindow::configureVworldKey` to refresh active project layer sources and tile caches | M2 | Survey R2 |
| 10 | Hotspot Controller Extraction | Extract `ProjectLifecycleController`, `DigitizingStateController`, `LayerStateController` | M2 | Survey R2 |
| 11 | Baseline Test Regression Fix | Fix aspect margin in `TestWorkflow::zoomToKorea_5186StaysInsideMercatorSatelliteQuad` | M2 | Survey R2 / R3 |
| 12 | Async Submission Export | Offload 5179 reprojection, SHP writing, and hashing from GUI thread with progress updates | M3 | Survey R3 |
| 13 | High-DPI Scale Flutter Fix | Decouple `applyCanvasScreenDpi` from `extentsChanged` (move to resize / DPR change events) | M3 | Survey R3 |
| 14 | Tile Heal Loop Elimination | Debounce `m_tileHealCount` reset so VWorld rate-limiting doesn't trigger infinite reload | M3 | Survey R3 |
| 15 | In-Memory Tile Cache Tuning | Tune `QgsSettings` tile cache size to 256 and standardize tile datasource URIs | M3 | Survey R3 |
| 16 | 100% CTest & Smoke Pass | Verify all 14 test suites pass, 0 compiler warnings under /W4, clean smoke-quit | M4 | Survey Baseline |
| 17 | Forensic Integrity Audit | Pass adversarial forensic audit against cheating/dummy facades | M4 | Survey Baseline |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | R1: Submission Package & Layout Integration | Layout composition validation, checklist tightening, `user_sheet` -> `조사도면.pdf` bundling, streamed SHA256 | none | IN_PROGRESS |
| M2 | R2: Hotspot De-risking, VWorld Lifecycle & Geometry Repair | `sanitizeAndRepairGeometry`, digitizing commit guard, VWorld key propagation, sub-controller extraction, test baseline fix | M1 | PLANNED |
| M3 | R3: Async Operations & High-DPI Tile Stability | Async export task with progress reporting, decoupling canvas DPI from extent changes, tile heal loop debounce, cache tuning | M2 | PLANNED |
| M4 | Final E2E Test Suite Pass & Adversarial Verification | 100% pass of all 14 CTest suites, 0 warnings under /W4, clean smoke-quit, forensic audit | M3 | PLANNED |

## Interface Contracts
### `LayoutService` ↔ `ProjectStateBuilder` ↔ `ExportService`
- `bool LayoutService::isComposedStudioSheet(QgsProject* project, const QString& layoutName = QStringLiteral("user_sheet"))`: Returns true only if map frame exists, has positive scale, valid finite extent, and non-empty vector/raster layers (excluding `layout_blank` and reference layers).
- `bool ExportService::exportSubmissionPackage(QgsProject* project, const QString& outDir, const QString& encoding, QString* errorOut)`: Validates composed sheet, calls `LayoutService::exportLayoutPdf`, writes `조사도면.pdf`, writes 5179 SHPs, and computes streamed `MANIFEST.sha256`.

### `LayerOps` ↔ `DigitizingStateController` / `MainWindow`
- `LayerOps::GeometryRepairResult LayerOps::sanitizeAndRepairGeometry(const QgsGeometry& inputGeom, Qgis::GeometryType expectedType)`: Validates GEOS correctness, removes duplicate nodes, applies `makeValid()` and `buffer(0)` fallback, and extracts dominant polygon part if multipart.
- `int LayerOps::refreshVworldApiKeyInLayers(QgsProject* project, const QString& currentKey, QStringList* changed)`: Updates WMTS/WMS URLs and cadastral GDAL XML files, reloading providers.

### `ExportService` Async Pipeline
- Main Thread: Snapshot feature sources via `QgsVectorFileWriter::prepareWriteAsVectorFormat`.
- Background Worker: Stream shapefile conversion and 64KB chunked SHA256 calculation; report progress [0.0 - 100.0].
- Completion: Notify GUI, show success/failure dialog, refresh submission state.

## Code Layout
- `src/core/ExportService.h / .cpp`: Export submission package, shapefile reprojection, SHA256 manifest.
- `src/core/LayoutService.h / .cpp`: Layout templates, studio sheet composition validation, PDF export.
- `src/core/ChecklistEngine.h / .cpp`: Rule evaluation against project state.
- `src/core/ProjectStateBuilder.h / .cpp`: Gathers project state for checklist evaluation.
- `src/core/LayerOps.h / .cpp`: Layer keys, basemaps, geometry sanitization/repair, key refresh.
- `src/app/controllers/`: Extracted sub-controllers (`ProjectLifecycleController`, `DigitizingStateController`).
- `src/app/MainWindow.h / .cpp`: UI shell, tool actions, progress notifications.
- `src/app/KaCaptureMapTool.h / .cpp`: Map digitizing tool.
- `tests/test_checklist.cpp`, `tests/test_workflow.cpp`: Test cases for validation, lifecycle, and export.
