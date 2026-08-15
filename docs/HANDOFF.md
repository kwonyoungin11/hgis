# ka-hgis Handoff — product SSOT (v0.3.0)

> Grok Build product truth for this repo.  
> Updated: 2026-08-15 (OpenCode removed; Grok Build is the only agent harness)

**Agent operating rules:** repo-root [`AGENTS.md`](../AGENTS.md)  
(QUICK / FEATURE / ARCHITECTURE; product invariants; build verify).  
This file is **product SSOT**. `AGENTS.md` is **Grok Build behavior SSOT**.

---

## 1. Product (SSOT)

Korean field archaeology HGIS (standalone C++/Qt6 + QGIS libs):

1. Survey store = GPKG schema on disk; **legend empty until user draws/imports** (QGIS layer lifecycle)
2. Domain keys (when added): `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points` (`ka_hgis/layer_key`)
3. **참조 지도** basemap = user-chosen underlay — NOT survey data
4. Digitize only (no attr popups while drawing); **편집저장** / per-feature commit; checklist blocks export on error
5. **도면 만들기** (`KaDrawingStudio`): 용지 → 지도 칸 드래그. 방위표·축척자는 샘플을 고른 뒤 드래그로 위치·크기. 지도 칸 우클릭 **지도 조정**(ArcGIS Activate와 같음) → 그린 것 가운데 → **지도조정끝**. QGIS 조판 디자이너 아님. 제출 SHP/PDF **EPSG:5179** + MANIFEST
7. **작업 제어** 독: 각 단계를 직접 눌러 실행·상태 확인 (새 조사/배경/그리기/속성/GPS/검수/도면/제출)
6. Work CRS 5186/5187 OK; upload 5179

**Other PC:** `docs/other-pc-setup.md` · `.\scripts\bootstrap-dev-pc.ps1`

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

## 5. Grok Build surface

| Surface | Path |
|---------|------|
| Agent rules | `AGENTS.md` |
| Product SSOT | `HANDOFF.md` (repo root; this file is the docs mirror) |
| Project MCP | `.grok/config.toml` (repowise) |
| Project skill | `.grok/skills/ka-hgis/` |
| Skill focus | `.grok/rules/10-skill-focus.md` |
| clangd flags | `.clangd` (user LSP: `~/.grok/lsp.json`) |
| Workspace memory | `~/.grok/memory/qgis-a61a2c3a/MEMORY.md` |
| User MCP (global) | context7, exa, firecrawl, sequential-thinking, playwright, github, linear |
