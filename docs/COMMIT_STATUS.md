# Git Commit Status

> **Auto-generated.** Do not hand-edit.  
> Refresh: `.\scripts\update-commit-status.ps1`  
> Auto on commit: `.githooks/pre-commit` (after `.\scripts\install-git-hooks.ps1`)

## Snapshot

| Field | Value |
|------|--------|
| Updated (local) | 2026-08-06 17:38:45 +09:00 |
| VERSION file | 0.3.0 |
| Branch | `main` |
| HEAD short | `bfa9f90` |
| HEAD full | `bfa9f90bd9c8a4f148618f4908b7ad72826c9c03` |
| HEAD date | 2026-08-06 17:20:54 +0900 |
| HEAD author | kwonyoungin11 <kyi2516@gmail.com> |
| HEAD subject | Polish MainWindow with icons, toolbar, and layer delete |
| Total commits | 37 |
| Upstream | `origin/main` |
| Sync | up to date with origin/main |
| Origin | https://github.com/kwonyoungin11/hgis.git |
| Working tree dirty | **yes** (17 paths) |

## How far we are

- **Last committed work:** Polish MainWindow with icons, toolbar, and layer delete (`bfa9f90` @ 2026-08-06 17:20:54 +0900)
- **Remote sync:** up to date with origin/main
- **Uncommitted local changes:** yes (17 paths)

If dirty = yes, another computer will **not** see those changes until you commit and push.

## Milestone markers

- `2bc9681` foundation / first import ??Add project foundation and license
- `b98c2f6` CI workflow ??Add GitHub Actions CI with sanity and optional self-hosted build
- `22fedf9` version 0.3.0 ??Add workflow tests and bump version to 0.3.0
- `38df550` portable exe in repo ??Track portable dist package with exe for other PCs
- `6ecd673` UI icons ??Add drawn KaIcons set for menus and toolbars

## Recent commits (newest first, max 30)

| Hash | Date | Subject |
|------|------|---------|
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
| `848ac2a` | 2026-08-06T16:20:10+09:00 | Tighten checklist rules and expand unit coverage |
| `687dd60` | 2026-08-06T16:20:10+09:00 | Export real SHP packages with layout PDF and SHA256 manifest |
| `3de7681` | 2026-08-06T16:20:10+09:00 | Add LayoutService for default print layouts and PDF export |
| `d63e82e` | 2026-08-06T16:20:10+09:00 | Add ProjectStateBuilder for live checklist project state |
| `b98c2f6` | 2026-08-06T15:30:00+09:00 | Add GitHub Actions CI with sanity and optional self-hosted build |
| `720e2dc` | 2026-08-06T15:29:59+09:00 | Add ADR, UX, research, and job-card documentation |
| `c3a734d` | 2026-08-06T15:29:59+09:00 | Add Windows dev-env, run, and e2e smoke scripts |

## Staged in this commit

```
A	.githooks/post-merge
A	.githooks/pre-commit
M	README.md
A	docs/COMMIT_STATUS.md
A	scripts/commit.ps1
A	scripts/install-git-hooks.ps1
A	scripts/update-commit-status.ps1
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