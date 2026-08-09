# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-09 18:57:27 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `31e51be` |
| HEAD full | `31e51be755801e241e7c2e699edc0ee3ee10ac54` |
| HEAD date | 2026-08-09 18:54:02 +0900 |
| HEAD author | kwonyoungin11 <kyi2516@gmail.com> |
| HEAD subject | Sync COMMIT_STATUS to HEAD after push ledger update. |
| Total commits | 11 |
| Upstream | `origin/main` |
| Sync | up to date with origin/main |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (6 paths) |

## How far we are

- **Recorded tip when this file was written:** Sync COMMIT_STATUS to HEAD after push ledger update. (`31e51be` @ 2026-08-09 18:54:02 +0900)
- **Remote sync at write time:** up to date with origin/main
- **Uncommitted local changes at write time:** yes (6 paths)

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

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
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
M	OPENCODE_HANDOFF.md
M	README.md
M	docs/other-pc-setup.md
M	docs/vendor/qgis-manual-3.44/README.md
A	scripts/bootstrap-dev-pc.ps1
A	scripts/download-qgis-manuals.ps1
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