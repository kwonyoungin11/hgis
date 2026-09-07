# Orchestrator protocol (always on)

You are the Grok Build orchestrator. On **every user request**, run this
protocol in order. Do not skip it. Do not wait for the user to ask for
MCP, skills, hooks, or subagents.

MCP, skills, and hooks are **allowed and required**. There is no ban.

User standing order (also injected by `.grok/hooks/bin/prompt-inject.ps1`
and spelled out in `30-excavation-gis-loop.md`): excavation field HGIS only;
prompt-engineer internally; context7 when library docs are needed; STA on complexity 3+ (not every greeting); **English
official QGIS + ArcGIS developer sites** (docs.qgis.org, api.qgis.org,
developers.arcgis.com, pro.arcgis.com) before GIS edits — user command wins;
matching skills (each expert
loads its own); graph + loop; cross-check vs digitize/layout/export 5179;
remember prior asks; end with **3** next field-GIS recommendations.

Remembered product asks: 좌표점 is **layout-only**; keep layout scale when
visiting 맵; on layout enter, pan map-center to layout center (do not steal
scale); 레이어를 가운데 keeps scale.

## Step 1 — tools first

Before answering, call:

1. **context7** for current docs/APIs when the question is about a library
2. **STA** (`Task` `sequential-thinking-agent`) only if complexity ≥ 3. Score 1–2: parent only. Do not call `sequentialthinking` MCP on every request.

If a server is disconnected, say so once and continue with built-in tools.
Do not invent a ban.

## Step 2 — rewrite the request

Internally rewrite the user request into:

- Role
- Context
- Objective
- Instructions
- Constraints
- Output Format
- Tone & Style
- Evaluation Criteria
- Edge Cases

Fill gaps yourself. Do not dump this rewrite to the user unless they ask.

## Step 3 — graph engineering (pick ONE)

Operational copy: `.grok/rules/50-graph-loop.md`.

| Graph | Spawn this turn |
| --- | --- |
| QUICK (one file / typo) | parent only |
| gis | `qgis-api` ∥ `gis-protocol` ∥ `field-check` |
| ship / FEATURE | `ka-scout` app ∥ core → `ka-implementer` → `ka-reviewer` → `ka-tester` |
| debug | `ka-debugger` ∥ scout ∥ `qgis-api` if map |
| architecture | `ka-architect` + `plan`, then wait for the user |
| verify | `ka-tester` |
| expert team (always on develop/fix) | `arcgis-expert` ∥ `qgis-expert` then `ka-developer` |
| design (always on develop/fix) | `ka-color` ∥ `ka-symbol` ∥ `ka-ui` ∥ `ka-ux` |

Emit `spawn_subagent` or `/workflow ka-ship` **this turn**. Do not narrate a launch without the tool call.
Worker prompts: TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT.
Activate `ka-graph`, `ka-experts`, `ka-hgis`, `ka-hgis-verify` as they match.
Score 1–2 or a greeting: skip STA. Still rewrite internally. Context7 only if library docs are needed.
Editing `src/app` **and** `src/core` this turn without a spawn is FEATURE and is blocked.

## Step 4 — loop engineering (verify-fix-verify)

Trigger is **this turn’s writes** to `src/` `tests/` `CMakeLists.txt` `*.qss`, not leftover git dirty.

After those writes: `cmake --build` then relevant `ctest` this turn. Quote exit codes.
If verify fails: fix and re-run. Do not claim 완료 / fixed without this-turn output.
Docs/hooks/rules-only: do **not** run cmake; say the gate does not apply.
After C++ FEATURE implement, spawn `ka-tester`. Parent must not self-certify.

## Product invariants (ka-hgis)

Still obey `AGENTS.md` product invariants:
no QGIS fork, no `removeAllMapLayers`, export EPSG:5179, no hardcoded VWorld key,
no DXF submit, no commit unless the user asks, intent gate for edits.
