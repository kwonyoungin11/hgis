# OpenCode Handoff Guide — ka-hgis (v0.3.0)

> OpenCode & AI agent handoff  
> Updated: 2026-08-08 (GOAL-ORIG-1 + agent mode router)

**Agent operating rules:** see repo-root [`AGENTS.md`](../AGENTS.md)  
(QUICK / FEATURE / ARCHITECTURE mode router; product invariants; build verify).  
This handoff remains **product SSOT**; `AGENTS.md` remains **agent behavior SSOT**.

---

## 1. Product (SSOT)

Korean field archaeology HGIS (standalone C++/Qt6 + QGIS libs):

1. Survey **DATA** in one GPKG — domain keys: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points` (Korean UI titles; logic uses `ka_hgis/layer_key`)
2. **참조 지도** basemap = user-chosen underlay (OSM / VWorld / file) — NOT survey data
3. User **draws** + fills attributes; **편집저장** commits; checklist; submit SHP/PDF in EPSG:5179 + MANIFEST
4. Work CRS 5186/5187 OK; upload via 5179 convert

---

## 2. Recent implementation notes

- VWorld key via `VworldSettings` only (no hardcoded production key)
- Legend groups: **조사 데이터** / **참조 지도**
- `loadSurveyLayers` never `removeAllMapLayers()` — drops domain layers only; keeps basemaps
- Toolbar: 파일 | 편집커밋(편집저장/그리기종료) | 디지타이즈 | 검수·제출
- Basemap from menu; startup does not force auto-basemap spam

---

## 3. Layout

```
src/app/MainWindow.*     UI, digitize, export
src/core/LayerOps.*      basemap, groups, layer_key
src/core/ChecklistEngine.*
src/core/ExportService.* SHP/PDF package (no DXF)
src/core/WorkflowGuide.*
tests/test_workflow.cpp
scripts/dev-env.ps1, build-all.ps1, po-smoke-field.ps1
```

## 4. Build

```
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

OSGeo SSOT: `C:\OSGeo4W`. CMake: `C:\CMake\bin`.