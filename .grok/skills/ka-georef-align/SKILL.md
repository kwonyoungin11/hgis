---
name: ka-georef-align
description: >
  ka-hgis 맞추기 (JPG/DXF onto cadastral on the same canvas). Use when the user
  mentions 맞추기, 지오레프, GCP, Fit To Display, world file, or runs
  /ka-georef-align.
when-to-use: 맞추기, 지오레프, GCP, 스캔 평면도, world file
---

# 맞추기 (georef / align)

HANDOFF 5b. Result is **참조 지도**, not submit geometry, not GNSS
`control_points`. Cadastral WMS recipe stays frozen
(`docs/ERROR_REGRESSION.md`).

## GIS objects

- Work CRS 5186/5187 + OTF. Cadastral / satellite layer CRS **EPSG:3857**.
- Align target is the visible 지적, not a domain GPKG table.

## Do this

1. Same-canvas JPG/DXF. Fit To Display + 2–3 pairs (`GeorefService::fromPairs`).
2. Write world file + sidecar PRJ; apply to raster
   (`writeWorldFile`, `writeSidecarPrj`, `applyWorldFileToRaster`).
3. Legend group `LayerOps::kGroupReference`. `markReferenceLayer`.
4. Refuse affine on domain survey layers (`GeorefService::isDomainSurveyLayer`).

## Files

`src/core/GeorefService.*`, `src/app/KaAlignMapTool.*`,
`LayerOps::georeferenceImageSimple`, tests under `tests/test_georef.cpp`.

## Do not

- Store GCP in `control_points` or export aligned raster as SHP 5179.
- Change cadastral WMS CRS to 5186/5187/5179.
- Treat 맞추기 as the 좌표점 layout tool (`/ka-drawing-studio`).
