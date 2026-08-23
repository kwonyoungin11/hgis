---
name: ka-hgis
description: >
  ka-hgis Grok Build workflow — QGIS layer lifecycle, digitize/export invariants,
  Windows OSGeo4W verify. Use when editing this C++/Qt6 HGIS, touching layers/CRS/export,
  or when the user mentions 새 조사, 디지타이즈, 편집저장, 제출패키지, 위성, 지적,
  조판, 맞추기, 단면도.
---

# ka-hgis (Grok Build)

This repo is a standalone C++20/Qt6 desktop HGIS linked to OSGeo4W qgis-dev. Not a QGIS fork. Not a web app.

## Read first

1. `.grok/NOW.md` — current session / do-not-touch
2. `AGENTS.md` — routing, invariants, build
3. `HANDOFF.md` — product SSOT
4. `docs/domain/data-model.md` and `docs/adr/0001-standalone-cpp-qgis-libs.md` for non-trivial work

## MCP

| Need | Server / tool |
|------|----------------|
| Qt / QGIS / GDAL API | **context7** |
| Multi-step diagnosis | **sequential-thinking** |
| C++ symbols / diagnostics | **clangd** via `lsp` |
| In-repo manuals | `docs/vendor/qgis-manual-3.44/` |

## Specialist skills (load instead of cloning this file)

| Topic | Skill / rule |
| --- | --- |
| 조판 / 축척자 / 좌표점 | `/ka-drawing-studio` · `.grok/rules/41-drawing-studio.md` |
| 제출 SHP+PDF | `/ka-submit-package` |
| 맞추기 | `/ka-georef-align` |
| 단면도 | `.grok/rules/40-section-studio.md` |
| 지적/위성 안 보임 | `/gis-verify` · `docs/ERROR_REGRESSION.md` |

## Invariants (never violate)

- Legend stays empty until the user draws / imports / opens that domain layer
- `LayerOps::ensureDomainLayer` is the only path that adds domain layers
- `loadSurveyLayers` must not `removeAllMapLayers()` and must not auto-add empty domain layers
- Export / upload CRS is **EPSG:5179**; work CRS may be 5186/5187
- No hardcoded VWorld production key — `VworldSettings` / QSettings only
- No DXF submit path

## Verify

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
```

Do not commit unless the user asks.

Graph: FEATURE (app+core) must spawn experts this turn. Loop: this-turn `src/` writes need cmake/ctest before 완료. Docs/hooks-only skip cmake. See `.grok/rules/50-graph-loop.md`.
