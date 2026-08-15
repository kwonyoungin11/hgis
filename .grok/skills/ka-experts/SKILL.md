---
name: ka-experts
description: >
  Summon ka-hgis expert agents so development keeps moving. Use when the user
  says 전문가, 소환, 전문가소환, council, 멀티그래프, "전문가 불러", or runs
  /ka-experts. FEATURE/ARCHITECTURE work must summon, not stall on the parent.
when-to-use: 전문가소환, 소환, council, expert panel, FEATURE implement, GIS unknown
argument-hint: "[gis|ship|debug|architecture] <task>"
---

# 전문가소환 (expert summon)

The parent **orchestrates**. Experts **do the work**. Do not sit in one context doing a 6-file FEATURE alone.

## Pick a graph (one)

| Graph | Spawn now (same turn, parallel) | Then |
| --- | --- | --- |
| `gis` | `qgis-api` + `gis-protocol` + `field-check` | parent patches from evidence |
| `ship` | `ka-scout` app + `ka-scout` core (+ `qgis-api` if map/CRS) | `ka-implementer` → `ka-reviewer` → `ka-tester` |
| `debug` | `ka-debugger` + `ka-scout` + `qgis-api` if map | implement only after cause is named |
| `architecture` | `ka-architect` + `plan` | wait for user before large edits |
| `verify` | `ka-tester` | no implement |

If the user only says 전문가소환 / 소환 with a task, choose `ship` unless the text is a map bug (`gis`) or a crash/build failure (`debug`).

Host-owned equivalent: `/workflow ka-council` (scout panel) or `/workflow ka-ship` (full FEATURE).

## Spawn rules

1. Emit `spawn_subagent` **in this turn**. Do not narrate a launch without the tool call.
2. `background: true` for every scout so the parent can keep routing.
3. `subagent_type` must be the expert name above (`ka-implementer`, not a vague generalist) unless that type is missing.
4. Every prompt includes TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT (invariants).
5. Prefix `description` with `[ka-scout]`, `[ka-implementer]`, `[qgis-api]`, …
6. Resume a failed expert (`resume_from`). Do not re-discover from zero.
7. After scouts return: one `ka-implementer`. After implement: `ka-reviewer` then `ka-tester`.
8. Isolation `worktree` only when two writers would collide.

## Keep moving

- Do not ask the user EPSG/WMS internals.
- QUICK (one symbol / typo) still stays solo — do not summon a council.
- Never idle waiting for a human between scout and implement on FEATURE.
- ARCHITECTURE still pauses for the user after `ka-architect`.

## Invariants (copy into every worker)

No QGIS fork, no `removeAllMapLayers` on load, no empty-layer auto-add, no hardcoded VWorld key, export EPSG:5179, no DXF submit path.
---
