# Grok Build preset (ka-hgis)

Grok-native stack is **on**: MCP, project skills, project hooks, clangd LSP,
workflows, multi-graph experts. OpenCode / Sisyphus / `.agents/` stay off.
Cursor+Grok 브릿지(`.cursor/rules/grok-ka-hgis.mdc` → `.grok/NOW.md`)는 허용. Playwright/web harness는 넣지 않는다.

## When to use what

| Surface | Use | Do not |
| --- | --- | --- |
| Built-in tools | default path | skip them in favor of MCP |
| `lsp` (clangd) | after C++ edits | guess `Qgs*` signatures |
| context7 MCP | unfamiliar `Qgs*` after in-repo miss | invent QGIS API |
| sequential-thinking MCP | FEATURE/ARCHITECTURE / debug | one-line typo |
| Skills | `/ka-experts` `/ka-graph` `/gis-verify` `/ka-hgis-verify` `/ka-drawing-studio` `/ka-submit-package` `/ka-georef-align` | `/using-superpowers` |
| Workflows | `ka-ship` `ka-council` `ka-verify` `feature-ship` | 40-agent graphs for one file |
| Hooks | safety + one verify reminder | treat Stop as full CI |

## Multi-graph (FEATURE always summons)

- **QUICK**: one file / typo — parent only.
- **gis**: `qgis-api` ∥ `gis-protocol` ∥ `field-check`
- **ship**: scouts → `ka-implementer` → `ka-reviewer` → `ka-tester`
- **debug**: `ka-debugger` ∥ scout ∥ `qgis-api`
- **architecture**: `ka-architect` + `plan`, then user
- **verify**: `ka-tester`

Parent orchestrates. Experts work. FEATURE does not stall waiting for a human between scout and implement.

Worker prompts MUST include: TASK, EXPECTED OUTCOME, MUST DO, MUST NOT DO, CONTEXT.

## Loop engineering

Trigger = this-turn writes under `src/` `tests/` `CMakeLists.txt` `*.qss`.
Before 완료 / fixed / tests pass: run the proving command this turn and quote the exit code.
Docs/hooks/rules-only: skip cmake and say so.
Stop hook blocks only this-turn product-src + a 완료 claim with no newer `.grok/.state/last-verify`.
Pre-existing dirty `src/` from another session does not block a research reply.
See `.grok/rules/50-graph-loop.md`.

## Still forbidden

- OpenCode (`.agents/`, `opencode.json`, Sisyphus trailers)
- Hardcoded VWorld keys, `removeAllMapLayers` on survey load, DXF as submit path
- Changing export CRS away from EPSG:5179 without an explicit user request
