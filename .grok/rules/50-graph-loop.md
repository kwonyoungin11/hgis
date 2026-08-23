# Graph + loop engineering (fail closed)

Always-on with `00-orchestrator-protocol.md`. Hooks enforce a subset.
This-turn state: `.grok/.state/turn-src-writes.txt`, `turn-graph.txt`, `last-verify`.

## Graph (pick ONE, spawn this turn)

| Graph | When | Spawn now |
| --- | --- | --- |
| QUICK | one file / one module / typo | parent only |
| gis | map / CRS / WMS / legend / digitize / layout | `qgis-api` ∥ `gis-protocol` ∥ `field-check` |
| ship / FEATURE | app+core, export, checklist, multi-file | `ka-scout` app ∥ core → `ka-implementer` → `ka-reviewer` → `ka-tester` |
| debug | crash / build red / wrong GIS result | `ka-debugger` ∥ scout ∥ `qgis-api` if map |
| architecture | IA / ADR / packaging / QGIS upgrade | `ka-architect` + `plan`, then wait |
| verify | after C++ when parent must not self-certify | `ka-tester` |

Rules:

1. Emit `spawn_subagent` or `/workflow ka-ship` **this turn**. Do not say you launched without the tool call.
2. Scouts `background: true`. Wait with `get_command_or_subagent_output` before implement.
3. FEATURE does not idle for a human between scout and implement.
4. Worker prompts: TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT (invariants).
5. Editing **both** `src/app` and `src/core` this turn without a spawn is FEATURE. The stop hook blocks it.
6. Skip spawn only for a one-word greeting. Status/research still uses scouts when the question spans the product.

Host equivalent: `/workflow ka-ship`, `/workflow ka-council`, `/workflow ka-verify`.

## Loop (verify-fix-verify)

Trigger = **this turn wrote** `src/`, `tests/`, `CMakeLists.txt`, or `*.qss`.
Not trigger = leftover `git diff` from another session, or docs/hooks/rules only.

1. Name what changed.
2. Docs/hooks/rules-only: **do not** run cmake; say the gate does not apply.
3. C++/CMake/tests: `cmake --build build --config Release` then relevant `ctest`. Quote exit codes.
4. Startup/menu/UI path: also `.\scripts\run-ka-hgis.ps1 --smoke-quit`.
5. Fail → fix → re-run. Do not claim 완료 / fixed / tests pass without this-turn output.
6. After C++ implement, spawn `ka-tester` (or `/workflow ka-verify`). Parent must not self-certify a FEATURE build.
7. Successful cmake/ctest (exit code 0) writes `.grok/.state/last-verify` (post-tool + ka-tester). Failed cmake must not stamp.

## Stop hook (what it actually checks)

- Background experts still running → GRAPH wait.
- This turn wrote `src/app` **and** `src/core` with empty `turn-graph` → GRAPH block.
- This turn wrote product src **and** the reply claims 완료/fixed **and** no newer `.grok/.state/last-verify` stamp → LOOP block. Quoting `ctest` in the reply is not evidence.
- Research/status with only pre-existing dirty src → **allow**.

## Must not

- Treat Stop as full CI.
- Council a one-symbol typo.
- Reset unrelated dirty product files to “clean the gate”.
- Claim 완료 from a previous turn’s log.
