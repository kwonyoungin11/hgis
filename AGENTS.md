# ka-hgis Agent Rules (OpenCode / Sisyphus)

You are the coding agent for **ka-hgis**: standalone C++20/Qt6 desktop HGIS
linked to OSGeo4W **qgis-dev** (not a QGIS fork). Korean archaeology field drawings.

**This file overrides generic “max agent graph” defaults for this repo.**
Use the mode router below. Do not force parallel explore/deep graphs on every turn.

Identity: orchestrator when multi-module; implementer when scope is local and clear.
Reply in the user’s language (usually Korean). Code/identifiers in files: English OK; UI strings: Korean.

---

## SSOT (read before non-trivial work)

1. `OPENCODE_HANDOFF.md` (mirror: `docs/OPENCODE_HANDOFF.md`) — product truth
2. `docs/adr/0001-standalone-cpp-qgis-libs.md` — Architecture B, no fork
3. `docs/domain/data-model.md` — GPKG layers & fields
4. `docs/architecture/data-flow.md` — critical path

Also useful: `docs/PO_GOAL_*.md`, `docs/user/gui-scenario-checklist.md`, `docs/COMMIT_STATUS.md`.

---

## Product invariants (NEVER violate)

- Architecture **B**: link `qgis_core` / `qgis_gui`; **do not fork** QGIS; do not reimplement PROJ/GDAL/renderer
- Domain layers (logic keys): `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`
  - store `ka_hgis/layer_key`; Korean titles are UI only
- Legend groups: **조사 데이터** vs **참조 지도** (basemap is NOT survey data)
- Work CRS: EPSG:5186 / 5187 OK; **upload/export = EPSG:5179** SHP(+PDF) + MANIFEST
- `loadSurveyLayers` must NOT `removeAllMapLayers()` — drop domain layers only; keep basemaps
- No hardcoded VWorld production API key — `VworldSettings` only
- No DXF as submit path (SHP/PDF package via `ExportService`)
- GPLv2+ compliance (About notices)

---

## Intent gate

- Explain / investigate / “how does X work” → research only, **no edits**
- Implement only on explicit verbs: implement / add / fix / change / create / wire / 적용 / 구현 / 수정
- Prefer minimal diff; bugfix = no drive-by refactor
- Do not commit unless the user explicitly asks

---

## Tool preference (this repo)

1. `codegraph_explore` **FIRST** for symbols/flows under `src/`
2. Then targeted Read / Edit
3. explore / librarian agents only when location is unknown **or** external QGIS/Qt/GDAL API docs are required
4. Do **not** spawn parallel agent graphs for single-file known fixes

---

## Mode router (pick ONE per turn)

### A) QUICK — default

**When:** one file / known symbol / typo / local bug / UI string / one function

- Solo OK. No mandatory agent graph.
- Flow: codegraph → edit → build/tests for touched area when feasible
- Skip Metis / Momus / Oracle unless stuck twice on the same issue

### B) FEATURE

**When:** app+core, new export path, checklist rule, digitize flow, CRS convert, multi-file behavior change

```
intent → codegraph (+ optional explore bg if unknown)
      → plan only if ambiguous
      → implement (core vs app boundary)
      → verify (cmake build + relevant ctest + smoke if UI path)
```

- Prefer domain/IO/CRS/checklist/export in `src/core/*`
- Prefer menus/tools/canvas/edit tools in `src/app/*`
- MainWindow is a hotspot: extract to core instead of growing MainWindow further
- Split workers only if changes are independent; otherwise one focused implementation

### C) ARCHITECTURE

**When:** IA change, ADR-level decision, large UX, packaging, QGIS upgrade, multi-goal PO

- Read SSOT + relevant `docs/PO_GOAL_*.md`
- Plan / clarify with user before large edits
- Oracle only after 2 failed fix rounds or a true design fork

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
- QGIS API: check in-repo usage first; librarian / Context7 for unfamiliar `Qgs*` APIs
- Extend existing tests; never delete failing tests to “pass”
- No `as any`-style escapes; no empty catch that swallows GIS errors silently

---

## Delegation (FEATURE / ARCHITECTURE only)

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

---

## Global orchestrator interaction

If a global Grok Build / max-subagent skill is loaded:

1. **This `AGENTS.md` wins** for routing and solo-vs-graph decisions in this repo
2. QUICK mode is the default; max fan-out applies only in FEATURE/ARCHITECTURE when units are independent
3. Still keep: intent gate, root-cause fixes, verify-before-done, no commit without ask
