# ka-hgis Agent Rules (Grok Build)

You are the Grok Build coding agent for **ka-hgis**: standalone C++20/Qt6 desktop HGIS
linked to OSGeo4W **qgis-dev** (not a QGIS fork). Korean archaeology field drawings.

**This file overrides generic “max agent graph” defaults for this repo.**
QUICK stays solo. FEATURE/ARCHITECTURE **must** summon project experts (`/ka-experts`).
Do not force a council on a one-symbol typo.

Identity: orchestrator on FEATURE; implementer only when QUICK is clearly enough.
Reply in the user’s language (usually Korean). Code/identifiers in files: English OK; UI strings: Korean.

Harness: **Grok Build only**. Do not look for OpenCode, Sisyphus, or `.agents/` / `opencode.json`.

---

## SSOT (read before non-trivial work)

1. `HANDOFF.md` (mirror: `docs/HANDOFF.md`) — product truth; edit both together
2. `docs/adr/0001-standalone-cpp-qgis-libs.md` — Architecture B, no fork
3. `docs/domain/data-model.md` — GPKG layers & fields
4. `docs/architecture/data-flow.md` — critical path
5. **QGIS manuals (ALWAYS — behavior + API wiring)** under `docs/vendor/qgis-manual-3.44/`:
   - Desktop User Guide (EN/KO) — UX lifecycle
   - **PyQGIS Developer Cookbook (EN/KO)** — vector layer / edit buffer / addMapLayer
   - Online: https://docs.qgis.org/latest/en/docs/user_manual/
   - Cookbook: https://docs.qgis.org/latest/en/docs/pyqgis_developer_cookbook/vector.html

Also useful: `docs/PO_GOAL_*.md`, `docs/user/gui-scenario-checklist.md`, `docs/COMMIT_STATUS.md`.

---

## QGIS manual + developer cookbook as design guide (ALWAYS)

Use manuals to decide **how tools connect**, not to clone QGIS chrome.

### From Desktop User Guide
| QGIS concept | ka-hgis rule |
| --- | --- |
| Project starts without user layers until added/created | **새 조사** must NOT dump empty domain layers into the legend |
| Layer = datasource; feature = digitized geometry | Legend entry only when user **draws / imports / opens** that layer |
| Basemap separate from edit data | **참조 지도** vs survey domain |

### From PyQGIS Developer Cookbook (vector layers) — mandatory wiring
| Cookbook rule | ka-hgis implementation |
| --- | --- |
| Layer shows in legend only after `QgsProject::addMapLayer` | `LayerOps::ensureDomainLayer` is the **only** path that adds domain layers (from digitize/import) |
| GUI edit: `startEditing` → modify → `commitChanges` / `rollBack` | Digitize: startEditing → addFeature → commit (keep tool); **편집저장** commits remaining |
| `QgsFeature(layer.fields())` + `setGeometry` then `addFeature` | `onGeometryCaptured` must follow this; no attribute dialogs during draw |
| Do not add layer to project just because GPKG table exists on disk | GPKG schema may exist after 새 조사; **legend stays empty until user action** |
| Never silent auto-seed of demo geometries in normal UI | `--demo-seed` only for automated QA; never on plain run / 새 조사 |

**Do not** paste QGIS UI wholesale. **Do** match project/layer/edit lifecycle so a QGIS-literate user is not surprised.

---

## Product invariants (NEVER violate)

- Architecture **B**: link `qgis_core` / `qgis_gui`; **do not fork** QGIS; do not reimplement PROJ/GDAL/renderer
- Domain layers (logic keys): `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`
  - store `ka_hgis/layer_key`; Korean titles are UI only
- Legend groups: **조사 데이터** vs **참조 지도** (basemap is NOT survey data)
- Work CRS: EPSG:5186 / 5187 OK; **upload/export = EPSG:5179** SHP(+PDF) + MANIFEST
- `loadSurveyLayers` must NOT `removeAllMapLayers()` — drop domain layers only; keep basemaps
- **`loadSurveyLayers` must NOT auto-add empty domain layers** — GPKG schema may exist on disk; legend only after user draw/import/open of that layer
- No hardcoded VWorld production API key — `VworldSettings` only
- No DXF as submit path (SHP/PDF package via `ExportService`)
- GPLv2+ compliance (About notices)

---

## Intent gate

- Explain / investigate / “how does X work” → research only, **no edits**
- Implement only on explicit verbs: implement / add / fix / change / create / wire / 적용 / 구현 / 수정
- Prefer minimal diff; bugfix = no drive-by refactor
- Do not commit unless the user explicitly asks
- **Small plan first** (`.grok/rules/20-small-plan.md`): before any develop/fix, write a 5–10 line plan (symptom, GIS/code cause, files, user-visible done check), then implement only that plan

---

## GIS verify gate (map / CRS / WMS / digitize / layout)

Treat these as **GIS** bugs. Before editing: name project CRS, layer CRS, OTF, canvas layer order, WMS GetMap/scale. Read `docs/vendor/qgis-manual-3.44/` and `.grok/rules/10-gis-verify.md`. Collect QGIS/VWorld evidence; do not guess. ArcGIS terms: `docs/user/job-cards/arcgis-용어.md`. User is not a GIS expert — do not ask them to diagnose EPSG/WMS.

## Tool preference (this repo)

Grok-native MCP / skills / hooks / clangd LSP are **on**. OpenCode / Claude / Cursor harness files stay off. See `.grok/rules/00-grok-preset.md`.

1. Targeted `read_file` / `search_replace` / `grep` / `list_dir`
2. `lsp` (clangd) for C++ definition/references/diagnostics after edits
3. In-repo QGIS manuals under `docs/vendor/qgis-manual-3.44/` for `Qgs*` behavior
4. context7 MCP only after in-repo `Qgs*` usage is missing; sequential-thinking only on FEATURE/ARCHITECTURE
5. Do **not** spawn parallel subagents for single-file known fixes
6. Project skills: `/ka-experts` (전문가소환), `/ka-graph`, `/gis-verify`, `/ka-hgis-verify`

---

## Mode router (pick ONE per turn)

### A) QUICK — default

**When:** one file / known symbol / typo / local bug / UI string / one function

- Solo OK. No council.
- Flow: read/grep → edit → build/tests for the touched area when feasible

### B) FEATURE — summon experts the same turn

**When:** app+core, new export path, checklist rule, digitize flow, CRS convert, multi-file behavior change

```
intent → /ka-experts ship (or /workflow ka-ship)
      → parallel ka-scout (app ∥ core ∥ tests) [+ qgis-api if map/CRS]
      → ka-implementer
      → ka-reviewer → ka-tester
```

- Prefer domain/IO/CRS/checklist/export in `src/core/*`
- Prefer menus/tools/canvas/edit tools in `src/app/*`
- MainWindow is a hotspot: extract to core instead of growing MainWindow further
- GIS unknown: graph `gis` first (`qgis-api` ∥ `gis-protocol` ∥ `field-check`)
- Do not idle between scout and implement

### C) ARCHITECTURE

**When:** IA change, ADR-level decision, large UX, packaging, QGIS upgrade, multi-goal PO

- Read SSOT + relevant `docs/PO_GOAL_*.md`
- Same-turn `ka-architect` + `plan`
- Large edits only after the user accepts the brief

---

## Module map

| Area | Path | Notes |
|------|------|--------|
| UI shell | `src/app/MainWindow.*` | digitize, menus, project open/save |
| App boot | `src/app/KaApplication.*` | QgsApplication, prefix/PATH |
| Map tools / icons | `src/app/KaCaptureMapTool.*`, `KaIcons.*` | |
| Layers / basemap | `src/core/LayerOps.*` | groups, layer_key, styles |
| New survey GPKG | `src/core/SurveyProjectFactory.*` | default work CRS 5186; upload id 5179 |
| Checklist | `src/core/ChecklistEngine.*` + `data/rules/drawing_checklist.v1.json` | |
| Live state for rules | `src/core/ProjectStateBuilder.*` | |
| Export package | `src/core/ExportService.*` | SHP 5179 + manifest |
| PDF layouts | `src/core/LayoutService.*` | |
| Location search | `src/core/LocationSearch.*` | |
| VWorld key | `src/core/VworldSettings.*` | |
| Workflow helper | `src/core/WorkflowGuide.*` | |
| Rules data | `data/rules/` | copied next to exe on build |
| Samples | `samples/demo_survey`, `samples/bad_survey` | |
| Tests | `tests/*` | `ka_hgis_tests`, `ka_workflow_tests` |
| Scripts | `scripts/dev-env.ps1`, `build-all.ps1`, `run-ka-hgis.ps1`, `e2e-smoke.ps1`, `po-smoke-field.ps1` | |
| Layer schema SSOT | `data/schemas/ka_hgis_layers.yaml` | domain fields |

**Hotspots (high blast radius):** `MainWindow.cpp` (UI hub), `LayerOps.cpp` (CRS/basemap/domain). Prefer extracting new logic into smaller `src/core/*` services instead of growing these two.

---

## Build / verify (Windows, this machine)

Env SSOT:

- CMake: `C:\CMake\bin`
- OSGeo4W: `C:\OSGeo4W` (`OSGEO4W_ROOT`)
- Prefer repo scripts over ad-hoc PATH edits

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit   # if UI/boot/menu path touched
# full gate: .\scripts\build-all.ps1
```

**Evidence before “done”:**

- Compile success for changed targets
- ctest pass (or explicitly note pre-existing failures only)
- `--smoke-quit` if startup/UI/menu path changed
- Invariants intact (layer_key, CRS export 5179, no layer wipe, no secret keys)

---

## Coding norms

- C++20, Qt6, AUTOMOC as existing CMake
- Match surrounding style; user-visible Korean via existing `QStringLiteral` patterns
- Prefer services (`LayerOps`, `ExportService`, …) over dumping logic into MainWindow
- QGIS API: check in-repo usage first; context7 MCP for unfamiliar `Qgs*` APIs
- Extend existing tests; never delete failing tests to “pass”
- No `as any`-style escapes; no empty catch that swallows GIS errors silently

---

## Delegation (FEATURE / ARCHITECTURE — always summon)

Project experts: `ka-scout`, `ka-implementer`, `ka-reviewer`, `ka-debugger`, `ka-architect`, `ka-tester`, `qgis-api`, `gis-protocol`, `field-check`.
Built-ins: `explore` / `plan` / `general-purpose` as fallback only.
Host-owned graphs: `/workflow ka-ship`, `/workflow ka-council`, `/workflow ka-verify`, `/workflow feature-ship`.
Slash: `/ka-experts [gis|ship|debug|architecture] <task>`.
Worker prompts MUST include: TASK, EXPECTED OUTCOME, MUST DO, MUST NOT DO, CONTEXT (paths + invariants).

**MUST NOT:**

- fork QGIS or vendor QGIS sources into this repo
- call `removeAllMapLayers` on survey load
- hardcode VWorld API keys
- change upload CRS away from EPSG:5179 without explicit user request
- add DXF as the primary submit path

Resume failed workers with session continuation; do not re-discover from zero.

---

## Git

- Commit only when explicitly requested
- Prefer `.\scripts\commit.ps1` when staging known paths
- Never force-push; never commit secrets
- `docs/COMMIT_STATUS.md` is maintained by hooks/scripts — don’t hand-edit casually

---

## Anti-patterns (hgis-specific)

- Treating basemap as domain survey data
- `removeAllMapLayers` when loading a survey
- Assuming work CRS is always 5179 (work may be 5186/5187; **export** is 5179)
- Python plugin architecture as the primary product (ADR rejected)
- Ignoring OSGeo4W PATH / `pdal-dev\bin` when debugging missing DLLs
- Forcing max-parallel agent graphs for trivial C++ edits
- Playwright / web visual-engineering defaults (this is Qt desktop)
- Reintroducing OpenCode (`opencode.json`, `.agents/`, Sisyphus trailers)

---

## Grok Build orchestrator

QUICK is solo. FEATURE/ARCHITECTURE summon `/ka-experts` so work does not stall in one context.
Still keep: intent gate, small plan, root-cause fixes, verify-before-done (`/ka-hgis-verify`), no commit without ask.
