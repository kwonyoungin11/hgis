# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-17 14:04:34 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `e896fce` |
| HEAD full | `e896fce31712859a971b9a81dcf960e46733f2d1` |
| HEAD date | 2026-08-17 10:48:58 +0900 |
| HEAD author | kwonyoungin11 <kyi2516@gmail.com> |
| HEAD subject | Ship georef split-align, easy-draw layer, and feature kind/period presets. |
| Total commits | 24 |
| Upstream | `origin/main` |
| Sync | ahead 1 of origin/main (push pending) |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (19 paths) |

## How far we are

- **Recorded tip when this file was written:** Ship georef split-align, easy-draw layer, and feature kind/period presets. (`e896fce` @ 2026-08-17 10:48:58 +0900)
- **Remote sync at write time:** ahead 1 of origin/main (push pending)
- **Uncommitted local changes at write time:** yes (19 paths)

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

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
| `e896fce` | 2026-08-17T10:48:58+09:00 | Ship georef split-align, easy-draw layer, and feature kind/period presets. |
| `3bd6ff8` | 2026-08-16T15:35:45+09:00 | Refresh COMMIT_STATUS after teal chrome and digitize ship. |
| `74abc32` | 2026-08-16T15:35:36+09:00 | Ship teal chrome, artifact digitize, snap, and studio layer-center. |
| `ef06bf1` | 2026-08-16T12:37:19+09:00 | Refresh COMMIT_STATUS after chrome and folder-import ship. |
| `e5c16ea` | 2026-08-16T12:37:07+09:00 | Ship flat chrome, layout grid/paper, and a fast PC folder import. |
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
M	CMakeLists.txt
M	HANDOFF.md
M	data/theme/ka-hgis.qss
M	docs/COMMIT_STATUS.md
M	docs/HANDOFF.md
M	"ka-hgis-\354\213\244\355\226\211.bat"
M	src/app/KaApplication.cpp
M	src/app/KaIcons.cpp
M	src/app/KaIcons.h
M	src/app/KaTheme.cpp
M	src/app/MainWindow.cpp
M	src/app/MainWindow.h
A	src/core/BufferAnalysis.cpp
A	src/core/BufferAnalysis.h
M	src/core/LayerOps.cpp
M	src/core/LayerOps.h
A	tests/test_buffer.cpp
M	tests/test_workflow.cpp
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