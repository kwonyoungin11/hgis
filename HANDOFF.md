# ka-hgis Handoff — product SSOT (v0.3.0)

> Updated: 2026-08-15. Grok Build only. MCP / skills / hooks **off**.

**Agent rules:** [`AGENTS.md`](./AGENTS.md)  
**This file + `docs/HANDOFF.md`:** edit together.

---

## Resume after reconnect (same PC or clone)

Remote: `https://github.com/kwonyoungin11/hgis.git` · branch **`main`**

```powershell
cd D:\qgis
git pull origin main
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
.\scripts\run-ka-hgis.ps1
```

Launch for the field user (do **not** start `ka-hgis.exe` raw):

- `고고학 전용 HGIS.lnk` → `scripts\start-ka-hgis.vbs` → `launch.ps1`
- or `.\ka-hgis-실행.bat`

Other PC first time: `docs/other-pc-setup.md` · `.\scripts\bootstrap-dev-pc.ps1`  
VWorld key is **local only** (`VworldSettings` / 도움말 → API 키). Never commit `config/secrets.ini`.

---

## 1. Product

Korean field archaeology HGIS (C++20/Qt6 + OSGeo4W `qgis-dev`, Architecture B, no QGIS fork):

1. GPKG survey store; **legend empty until draw/import** (`LayerOps::ensureDomainLayer` only)
2. Domain keys: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`
3. **참조 지도** (위성/지적) vs **조사 데이터**
4. Digitize: startEditing → addFeature → commit (keep tool). **Ctrl+Z** undoes last vertex, then last saved feature
5. **도면 만들기** = `KaDrawingStudio` (not QGIS Layout Designer). Samples for north/scale/legend/CRS. **Ctrl+Z** removes last placed item
6. Work CRS EPSG:5186 / 5187; **export SHP+PDF+MANIFEST = EPSG:5179**. Checklist error hard-blocks 제출
7. Icon toolbar (새조사/열기/저장/위성/지적/그리기/선택/속성/도면/검수/보내기/찾기). Text menu bar **hidden**. File drawer and 작업 제어 dock hidden by default (**더보기**)

---

## 2. What landed (do not rebuild)

| Area | Notes |
| --- | --- |
| VWorld 위성 | Stored API key → `api.vworld.kr` WMTS **first**; xdworld only if no key |
| VWorld 지적 | Frozen tiled WMS `crs=EPSG:3857` + KEY/DOMAIN. Do not put 5186/5187/5179 in WMS CRS list |
| Digitize / attrs | `KaCaptureMapTool`, `KaAttributeMapTool`, `ensureDomainLayer`. 그리기: 조사구역/유구면/유구선/단면선/기준점 |
| Layout studio | `src/app/KaDrawingStudio.*` — 160 mm scale bar, PNG north = sample, CRS label |
| Launch | `scripts/start-ka-hgis.vbs` + `launch.ps1` (Job Object safe) |
| Chrome theme | `KaTheme` + `data/theme/ka-hgis.qss` sky 3D / black 2px regions |
| Tests | `tests/test_workflow.cpp` (satellite key-first, undo feature, scale bar width, …) |

---

## 3. Next (if continuing production)

1. Export package PDF should be the **composed studio sheet**, not `rebuildDefaultLayouts` 5 templates
2. Checklist `layout_exists:*` must not pass on empty auto-seeded layouts
3. Do **not**: QGIS fork, DXF submit, restore 7-step rail, change cadastral WMS recipe, hardcode VWorld key

---

## 4. Layout / build

```
src/app/MainWindow.*        chrome, digitize, export
src/app/KaDrawingStudio.*   조판
src/app/KaCaptureMapTool.*  그리기
src/app/KaAttributeMapTool.* 속성 클릭
src/core/LayerOps.*         basemap, layer_key, undoCommittedFeature
src/core/ExportService.*    SHP 5179 + MANIFEST
src/core/LayoutService.*    default 5 layouts (package PDF still uses these)
src/core/ChecklistEngine.*
tests/test_workflow.cpp
```

```
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
.\scripts\run-ka-hgis.ps1
```

This machine: OSGeo `D:\OSGeo4W` (also `C:\OSGeo4W` in docs). CMake `C:\CMake\bin`.

---

## 5. Grok Build

| Surface | Path |
| --- | --- |
| Agent rules | `AGENTS.md` |
| Product SSOT | `HANDOFF.md` + `docs/HANDOFF.md` |
| Preset (MCP/skills/hooks/LSP/graph) | `.grok/rules/00-grok-preset.md` |
| clangd | `.clangd` + `.grok/lsp.json` |
| Skills | `/ka-experts` `/ka-graph` `/gis-verify` `/ka-hgis-verify` |
| Experts | `ka-scout` `ka-implementer` `ka-reviewer` `ka-debugger` `ka-architect` `ka-tester` `qgis-api` `gis-protocol` `field-check` |
| Workflows | `/workflow ka-ship` `/workflow ka-council` `/workflow ka-verify` |
