---
name: ka-graph
description: >
  Route ka-hgis work on the QUICK / FEATURE / ARCHITECTURE multi-graph and
  summon experts. Use when the user asks for 그래프, 멀티그래프, fan-out,
  FEATURE, ARCHITECTURE, or runs /ka-graph.
when-to-use: Multi-module work, 멀티그래프, workflow, FEATURE, ARCHITECTURE
argument-hint: "[quick|gis|ship|debug|architecture] <task>"
---

# ka-hgis multi-graph

QUICK stays solo. FEATURE/ARCHITECTURE **must** summon experts (`/ka-experts`).

## Graphs

| Name | Nodes | Command |
| --- | --- | --- |
| QUICK | parent only | (no workflow) |
| `gis` | qgis-api ∥ gis-protocol ∥ field-check | `/ka-experts gis …` or `/workflow ka-council {"task":"…","kind":"gis"}` |
| `ship` | scout app ∥ core ∥ tests → implementer → reviewer → tester | `/workflow ka-ship {"objective":"…"}` |
| `debug` | ka-debugger ∥ scout ∥ qgis-api | `/workflow ka-council {"task":"…","kind":"debug"}` |
| `verify` | ka-tester | `/ka-hgis-verify` or `/workflow ka-verify` |
| `architecture` | ka-architect + plan | then wait for the user |

Also available: `/workflow feature-ship`, `/workflow review-changes`.

## FEATURE (default when not a one-liner)

1. Same-turn parallel scouts (`background: true`).
2. One `ka-implementer`.
3. `ka-reviewer` then `ka-tester`. Fail closed.
4. Do not idle between scout and implement.

## Worker contract

TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT (invariants). Resume, do not re-discover.

## Do not

- Council on a one-symbol typo
- Same-model rubber stamps with no cmake/ctest/GetMap evidence
- Two writers on one file without `worktree`
---
