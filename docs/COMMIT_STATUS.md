# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-16 12:37:07 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `cf8c011` |
| HEAD full | `cf8c0115b9c6bf4c2855a51fa8accbc94cccc26f` |
| HEAD date | 2026-08-15 23:43:28 +0900 |
| HEAD author | kwonypungin <kyi25@nate.com> |
| HEAD subject | Refresh COMMIT_STATUS after theme ship; drop old no-MCP rule. |
| Total commits | 19 |
| Upstream | `origin/main` |
| Sync | up to date with origin/main |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (23 paths) |

## How far we are

- **Recorded tip when this file was written:** Refresh COMMIT_STATUS after theme ship; drop old no-MCP rule. (`cf8c011` @ 2026-08-15 23:43:28 +0900)
- **Remote sync at write time:** up to date with origin/main
- **Uncommitted local changes at write time:** yes (23 paths)

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

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
| `cf8c011` | 2026-08-15T23:43:28+09:00 | Refresh COMMIT_STATUS after theme ship; drop old no-MCP rule. |
| `bc50839` | 2026-08-15T23:42:38+09:00 | Ship sky-blue theme, studio chrome, and digitize wiring. |
| `17ac917` | 2026-08-15T15:31:08+09:00 | Save resume handoff so reconnect/clone can continue field HGIS work. |
| `f4c1901` | 2026-08-15T15:29:21+09:00 | Sync COMMIT_STATUS to tip after field HGIS push. |
| `1f5e990` | 2026-08-15T15:29:07+09:00 | Ship field HGIS: layout studio, keyed VWorld basemap, icon chrome, Ctrl+Z undo. |
| `3a5a87a` | 2026-08-09T18:57:37+09:00 | Sync COMMIT_STATUS to tip after bootstrap push. |
| `6b9f373` | 2026-08-09T18:57:28+09:00 | Refresh COMMIT_STATUS after bootstrap docs commit. |
| `30c1a6d` | 2026-08-09T18:57:26+09:00 | Add bootstrap-dev-pc so a fresh clone can build and develop immediately. |
| `31e51be` | 2026-08-09T18:54:02+09:00 | Sync COMMIT_STATUS to HEAD after push ledger update. |
| `c1a3c12` | 2026-08-09T18:53:52+09:00 | Refresh COMMIT_STATUS ledger after feature commits. |
| `e69ab63` | 2026-08-09T18:53:51+09:00 | Skip live VWorld tests when API key missing or rejected. |
| `fedd10d` | 2026-08-09T18:53:49+09:00 | Add separate print layout editor window using QgsLayoutView. |
| `15fc5a4` | 2026-08-09T18:53:48+09:00 | Align survey layer lifecycle with QGIS: empty legend until draw. |
| `3129ef6` | 2026-08-09T18:53:46+09:00 | Harden digitize capture tool finish and keep drawing session. |
| `ca3a77e` | 2026-08-09T18:53:44+09:00 | Fix domain layer symbology so digitized features render on canvas. |
| `f659bdb` | 2026-08-09T18:53:27+09:00 | Add QGIS 3.44 manuals as design guide and agent SSOT rules. |
| `536977c` | 2026-08-09T16:10:06+09:00 | Sync COMMIT_STATUS to HEAD after push. |
| `99bd664` | 2026-08-09T16:09:56+09:00 | Refresh COMMIT_STATUS for baseline HEAD. |
| `eac6c9c` | 2026-08-09T16:09:43+09:00 | Initial baseline: ka-hgis v0.3 archaeology HGIS (C++/Qt6 + QGIS libs). |

## Staged in this commit

```
A	.grok/hooks/bin/prompt-inject.ps1
M	.grok/hooks/bin/stop-gate.ps1
M	.grok/hooks/ka-hgis.json
A	.grok/hooks/orchestrator-protocol.json
A	.grok/hooks/scripts/orchestrator-prompt-inject.ps1
A	.grok/hooks/scripts/orchestrator-stop-gate.ps1
A	.grok/rules/00-orchestrator-protocol.md
M	.grok/rules/10-gis-verify.md
A	.grok/skills/ka-hgis/SKILL.md
A	.grok/skills/orchestrator-protocol/SKILL.md
M	CMakeLists.txt
M	data/theme/ka-hgis.qss
M	docs/COMMIT_STATUS.md
M	src/app/KaDrawingStudio.cpp
M	src/app/KaDrawingStudio.h
M	src/app/KaIcons.cpp
M	src/app/KaIcons.h
M	src/app/KaTheme.cpp
M	src/app/MainWindow.cpp
M	src/app/MainWindow.h
M	src/core/LayoutService.cpp
M	tests/test_theme.cpp
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