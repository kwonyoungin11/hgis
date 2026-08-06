# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-06 17:56:03 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `cf24f11` |
| HEAD full | `cf24f11135c845dc778047de19b29629686c146b` |
| HEAD date | 2026-08-06 17:40:59 +0900 |
| HEAD author | kwonyoungin11 <kyi2516@gmail.com> |
| HEAD subject | Make post-commit only refresh ledger without amend |
| Total commits | 44 |
| Upstream | `origin/main` |
| Sync | up to date with origin/main |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (13 paths) |

## How far we are

- **Recorded tip when this file was written:** Make post-commit only refresh ledger without amend (`cf24f11` @ 2026-08-06 17:40:59 +0900)
- **Remote sync at write time:** up to date with origin/main
- **Uncommitted local changes at write time:** yes (13 paths)

### Read this correctly

1. After `git pull`, run `git log -1 --oneline` for the absolute tip (this file may lag by one commit if it was refreshed in `post-commit` and not yet re-committed).
2. The **Recent commits** table is the durable history of what landed.
3. If dirty = yes on a dev machine, another computer will **not** see those paths until commit + push.
4. Always-on update: `.\scripts\install-git-hooks.ps1` once per clone.

## Milestone markers

- `2bc9681` foundation / first import ??Add project foundation and license
- `b98c2f6` CI workflow ??Add GitHub Actions CI with sanity and optional self-hosted build
- `22fedf9` version 0.3.0 ??Add workflow tests and bump version to 0.3.0
- `38df550` portable exe in repo ??Track portable dist package with exe for other PCs
- `6ecd673` UI icons ??Add drawn KaIcons set for menus and toolbars
- `c4156c2` commit progress ledger ??Add always-on git commit progress ledger in docs/COMMIT_STATUS.md
- `7a48524` commit progress ledger ??Amend commit status after commit so HEAD hash is accurate
- `d9b7c2e` commit progress ledger ??Refresh COMMIT_STATUS snapshot to current HEAD
- `ff2adeb` commit progress ledger ??Sync COMMIT_STATUS after push

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
| `cf24f11` | 2026-08-06T17:40:59+09:00 | Make post-commit only refresh ledger without amend |
| `555bb36` | 2026-08-06T17:40:44+09:00 | Stop post-commit amend; keep ledger refresh dirty until next commit |
| `2e1d8c8` | 2026-08-06T17:40:27+09:00 | Simplify commit ledger hooks and clarify how to read progress |
| `ff2adeb` | 2026-08-06T17:39:43+09:00 | Sync COMMIT_STATUS after push |
| `d9b7c2e` | 2026-08-06T17:39:30+09:00 | Refresh COMMIT_STATUS snapshot to current HEAD |
| `7a48524` | 2026-08-06T17:39:15+09:00 | Amend commit status after commit so HEAD hash is accurate |
| `c4156c2` | 2026-08-06T17:38:44+09:00 | Add always-on git commit progress ledger in docs/COMMIT_STATUS.md |
| `bfa9f90` | 2026-08-06T17:20:54+09:00 | Polish MainWindow with icons, toolbar, and layer delete |
| `6ecd673` | 2026-08-06T17:20:54+09:00 | Add drawn KaIcons set for menus and toolbars |
| `f621d5e` | 2026-08-06T17:20:54+09:00 | Ignore local survey GPKG and QGZ project files |
| `38df550` | 2026-08-06T17:10:30+09:00 | Track portable dist package with exe for other PCs |
| `ffd3b70` | 2026-08-06T17:10:30+09:00 | Document v0.3 work CRS workflow and portable run path |
| `ff91bc9` | 2026-08-06T17:10:30+09:00 | Add one-shot build-all script and relative launch bats |
| `3c34b03` | 2026-08-06T17:10:30+09:00 | Cover georef worldfile and 5186-to-5179 SHP conversion |
| `9960315` | 2026-08-06T17:10:29+09:00 | Add work CRS menus, Korea basemaps, and 5179 upload convert |
| `795a568` | 2026-08-06T17:10:29+09:00 | Default new surveys to work CRS 5186 or 5187 |
| `57f1d5f` | 2026-08-06T17:10:29+09:00 | Add Korea basemaps, work CRS helpers, and 5179 SHP convert |
| `65cc1b6` | 2026-08-06T16:30:44+09:00 | Migrate layers and layouts when opening existing projects |
| `4b824a9` | 2026-08-06T16:30:44+09:00 | Add pixel_x and pixel_y fields for georeference GCPs |
| `f5404b7` | 2026-08-06T16:30:25+09:00 | Add manual GUI scenario checklist for field workflows |
| `3046d08` | 2026-08-06T16:30:25+09:00 | Add portable pack script and run workflow tests in e2e |
| `22fedf9` | 2026-08-06T16:30:25+09:00 | Add workflow tests and bump version to 0.3.0 |
| `a170348` | 2026-08-06T16:30:24+09:00 | Wire LayerOps tools and real CRS actions in MainWindow |
| `a96e5c1` | 2026-08-06T16:30:24+09:00 | Enrich default layouts with extent, legend, and titles |
| `6c10f8d` | 2026-08-06T16:30:24+09:00 | Add LayerOps for reproject, basemap, style, and georef |
| `a9e84f4` | 2026-08-06T16:30:24+09:00 | Ignore portable dist output directory |
| `aff389c` | 2026-08-06T16:20:11+09:00 | Refresh e2e smoke script and demo GCP sample |
| `9097a63` | 2026-08-06T16:20:11+09:00 | Update job cards and document architecture data flow |
| `941f6f2` | 2026-08-06T16:20:11+09:00 | Wire layout and state services into CMake and MainWindow |
| `6e2cfed` | 2026-08-06T16:20:11+09:00 | Add GCP accuracy and fix-type fields to survey factory |

## Staged in this commit

```
M	.gitignore
M	CMakeLists.txt
M	docs/COMMIT_STATUS.md
A	src/app/KaCaptureMapTool.cpp
A	src/app/KaCaptureMapTool.h
M	src/app/KaIcons.cpp
M	src/app/MainWindow.cpp
M	src/app/MainWindow.h
M	src/core/LayerOps.cpp
M	src/core/LayoutService.cpp
M	src/core/LayoutService.h
A	src/core/LocationSearch.cpp
A	src/core/LocationSearch.h
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