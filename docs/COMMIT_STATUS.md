# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-09-04 22:14:02 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `b5a903e` |
| HEAD full | `b5a903e51cb50533090ec2dd1183b690130f5e2a` |
| HEAD date | 2026-09-04 22:02:40 +0900 |
| HEAD author | kwonypungin <kyi25@nate.com> |
| HEAD subject | feat(select): 다중 레이어 Shift 피처 선택 도구 및 2개 도형 선택 시 겹침 자동 분할 연동 |
| Total commits | 46 |
| Upstream | `origin/main` |
| Sync | ahead 8 of origin/main (push pending) |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (5 paths) |

## How far we are

- **Recorded tip when this file was written:** feat(select): 다중 레이어 Shift 피처 선택 도구 및 2개 도형 선택 시 겹침 자동 분할 연동 (`b5a903e` @ 2026-09-04 22:02:40 +0900)
- **Remote sync at write time:** ahead 8 of origin/main (push pending)
- **Uncommitted local changes at write time:** yes (5 paths)

### Read this correctly

1. After `git pull`, run `git log -1 --oneline` for the absolute tip (this file may lag by one commit if it was refreshed in `post-commit` and not yet re-committed).
2. The **Recent commits** table is the durable history of what landed.
3. If dirty = yes on a dev machine, another computer will **not** see those paths until commit + push.
4. Always-on update: `.\scripts\install-git-hooks.ps1` once per clone.

## Milestone markers

- `99bd664` commit progress ledger ??Refresh COMMIT_STATUS for baseline HEAD.
- `536977c` commit progress ledger ??Sync COMMIT_STATUS to HEAD after push.
- `c1a3c12` commit progress ledger ??Refresh COMMIT_STATUS ledger after feature commits.
- `31e51be` commit progress ledger ??Sync COMMIT_STATUS to HEAD after push ledger update.
- `6b9f373` commit progress ledger ??Refresh COMMIT_STATUS after bootstrap docs commit.
- `3a5a87a` commit progress ledger ??Sync COMMIT_STATUS to tip after bootstrap push.
- `f4c1901` commit progress ledger ??Sync COMMIT_STATUS to tip after field HGIS push.
- `cf8c011` commit progress ledger ??Refresh COMMIT_STATUS after theme ship; drop old no-MCP rule.
- `ef06bf1` commit progress ledger ??Refresh COMMIT_STATUS after chrome and folder-import ship.
- `3bd6ff8` commit progress ledger ??Refresh COMMIT_STATUS after teal chrome and digitize ship.
- `c4cc35c` commit progress ledger ??Refresh COMMIT_STATUS after buffer and new-survey reset.
- `cf52276` commit progress ledger ??Refresh COMMIT_STATUS after start page and layout coord ship.
- `033fffa` commit progress ledger ??Refresh COMMIT_STATUS after section studio ship.
- `d6db386` commit progress ledger ??Refresh COMMIT_STATUS after Malgun Gothic and unlazy ship.

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
| `b5a903e` | 2026-09-04T22:02:40+09:00 | feat(select): 다중 레이어 Shift 피처 선택 도구 및 2개 도형 선택 시 겹침 자동 분할 연동 |
| `77fccd7` | 2026-09-04T21:57:11+09:00 | feat(core): 선택된 두 도형 간 겹치는 구간 자동 분할(splitTwoOverlappingFeatures) 구현 |
| `d41d825` | 2026-09-04T21:46:34+09:00 | feat(ui): 그리기 툴바에 겹치는 구간 분리(클립) 및 폴리곤 나누기 기능 연동 |
| `72cb771` | 2026-09-04T21:39:40+09:00 | feat(core): 레이어 클립(구간 분리) 및 폴리곤 분할(Split) 코어 연산 추가 |
| `30db1c4` | 2026-09-04T18:05:19+09:00 | fix(layers): ensure satellite layer stays at bottom and prevent deletion via reorderGroupLayers |
| `d910e9b` | 2026-09-04T17:11:58+09:00 | fix(georef): instant canvas update on align move/save and fix raster layer order above basemaps |
| `4a5341d` | 2026-09-02T17:57:24+09:00 | Ship align overlay viewport mapping, scan-ink overlay, paleo/soil maps, and trench field GIS. |
| `83fe782` | 2026-08-31T10:20:41+09:00 | Ship user_sheet submit PDF, 4K/5186 satellite fill, and WMS zoom crash guard. |
| `9592951` | 2026-08-30T18:33:14+09:00 | Widen geology, soil, and river downloads to 4x span so 1:10000 drawings stay covered. |
| `d6db386` | 2026-08-30T12:41:15+09:00 | Refresh COMMIT_STATUS after Malgun Gothic and unlazy ship. |
| `1eb3c3e` | 2026-08-30T12:41:00+09:00 | Ship Malgun Gothic field chrome, section/layout GIS work, and Grok unlazy expert harness. |
| `ff8c099` | 2026-08-23T15:08:48+09:00 | Ship 4K-safe VWorld satellite tiles and pending field GIS work for another machine. |
| `033fffa` | 2026-08-23T08:44:29+09:00 | Refresh COMMIT_STATUS after section studio ship. |
| `bbec31e` | 2026-08-23T08:44:15+09:00 | Ship section drawing studio so Descartes GeoTIFF sheets can continue on another machine. |
| `4a7f44d` | 2026-08-21T23:37:43+09:00 | Update Grok agent rules and hooks for the excavation GIS loop. |
| `be276e4` | 2026-08-21T23:37:27+09:00 | Draw layout rasters in a single pass so tiled basemaps stop leaving blank chunks. |
| `80223e1` | 2026-08-21T23:09:06+09:00 | Fix review findings: wire single-instance guard, anchor layout wheel zoom, repair portable README encoding. |
| `5735677` | 2026-08-21T22:48:49+09:00 | Ship pro survey sheets, Korean map services, trench tools, and crash guard. |
| `cf52276` | 2026-08-18T07:29:54+09:00 | Refresh COMMIT_STATUS after start page and layout coord ship. |
| `a057319` | 2026-08-18T07:29:17+09:00 | Ship start page, recent surveys, splash, auto VWorld basemaps, and layout coordinate callouts. |
| `c4cc35c` | 2026-08-17T14:04:44+09:00 | Refresh COMMIT_STATUS after buffer and new-survey reset. |
| `2ca17b9` | 2026-08-17T14:04:33+09:00 | Ship site buffer rings, new-survey legend reset, and OSGeo launch PATH. |
| `e896fce` | 2026-08-17T10:48:58+09:00 | Ship georef split-align, easy-draw layer, and feature kind/period presets. |
| `3bd6ff8` | 2026-08-16T15:35:45+09:00 | Refresh COMMIT_STATUS after teal chrome and digitize ship. |
| `74abc32` | 2026-08-16T15:35:36+09:00 | Ship teal chrome, artifact digitize, snap, and studio layer-center. |
| `ef06bf1` | 2026-08-16T12:37:19+09:00 | Refresh COMMIT_STATUS after chrome and folder-import ship. |
| `e5c16ea` | 2026-08-16T12:37:07+09:00 | Ship flat chrome, layout grid/paper, and a fast PC folder import. |
| `cf8c011` | 2026-08-15T23:43:28+09:00 | Refresh COMMIT_STATUS after theme ship; drop old no-MCP rule. |
| `bc50839` | 2026-08-15T23:42:38+09:00 | Ship sky-blue theme, studio chrome, and digitize wiring. |
| `17ac917` | 2026-08-15T15:31:08+09:00 | Save resume handoff so reconnect/clone can continue field HGIS work. |

## Staged in this commit

```
M	src/core/LayerOps.cpp
M	src/core/LayerOps.h
M	tests/test_buffer.cpp
```
## Other PC checklist

```powershell
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
git pull
Get-Content docs\COMMIT_STATUS.md -Head 40
# optional hooks on that PC:
.\scripts\install-git-hooks.ps1
```

## Maintainer notes

- This file is updated by `scripts/update-commit-status.ps1`.
- `pre-commit` hook rewrites it and `git add`s it so **every commit records progress**.
- Field survey files (`*.gpkg`, `*.qgz`) stay local (gitignored).
- Portable binary: `dist/ka-hgis-portable/` (needs OSGeo4W on target PC).