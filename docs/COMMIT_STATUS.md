# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-09 16:09:56 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `eac6c9c` |
| HEAD full | `eac6c9cd72ab0af7ec1dd671d22abfb6cd36e301` |
| HEAD date | 2026-08-09 16:09:43 +0900 |
| HEAD author | kwonyoungin11 <kyi2516@gmail.com> |
| HEAD subject | Initial baseline: ka-hgis v0.3 archaeology HGIS (C++/Qt6 + QGIS libs). |
| Total commits | 1 |
| Upstream | `origin/main` |
| Sync | up to date with origin/main |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (1 paths) |

## How far we are

- **Recorded tip when this file was written:** Initial baseline: ka-hgis v0.3 archaeology HGIS (C++/Qt6 + QGIS libs). (`eac6c9c` @ 2026-08-09 16:09:43 +0900)
- **Remote sync at write time:** up to date with origin/main
- **Uncommitted local changes at write time:** yes (1 paths)

### Read this correctly

1. After `git pull`, run `git log -1 --oneline` for the absolute tip (this file may lag by one commit if it was refreshed in `post-commit` and not yet re-committed).
2. The **Recent commits** table is the durable history of what landed.
3. If dirty = yes on a dev machine, another computer will **not** see those paths until commit + push.
4. Always-on update: `.\scripts\install-git-hooks.ps1` once per clone.

## Milestone markers

- (auto milestones not matched yet)

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
| `eac6c9c` | 2026-08-09T16:09:43+09:00 | Initial baseline: ka-hgis v0.3 archaeology HGIS (C++/Qt6 + QGIS libs). |

## Staged in this commit

```
M	docs/COMMIT_STATUS.md
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