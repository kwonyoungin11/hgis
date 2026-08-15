---
name: gis-verify
description: >
  GIS-first diagnosis for map, CRS, WMS/VWorld, digitize, layout, and legend
  bugs in ka-hgis. Use when the user reports 지도가 안 보임, 지적, 위성,
  EPSG, WMS, GetMap, 디지타이즈, 범례, or runs /gis-verify.
when-to-use: Map / CRS / WMS / digitize / layout / legend problems
---

# GIS-first verify

This is a GIS bug, not a generic UI bug. Follow `.grok/rules/10-gis-verify.md`.

## Before any edit

Name these objects (agent work — do not ask the user to diagnose EPSG/WMS):

1. Project CRS (work may be 5186/5187; **export** is 5179)
2. Layer CRS
3. OTF / `destinationCrs`
4. Legend vs canvas `setLayers` / layer order
5. WMS GetCapabilities / GetMap / scale
6. Edit buffer (`startEditing` → modify → `commitChanges`)

Read in-repo truth: `docs/vendor/qgis-manual-3.44/`, `src/core/LayerOps.*`, `src/app/MainWindow.*`.

## Graph (read-only, at most these)

- `qgis-api` — cookbook + `Qgs*` wiring (`addMapLayer`, edit buffer, extent)
- `gis-protocol` — VWorld WMS/WMTS documented URL / GetMap evidence
- `field-check` — turn a screenshot into pass/fail (e.g. 병산동 1:2000, 지적 선이 위성 위)

Do not invent a fourth GIS theory.

## User-facing

Ask at most one yes/no the user can see (지도에 선이 보이나). Then patch from evidence.

## Invariants

- Basemap is **참조 지도**, not survey data
- Domain layers enter the legend only via `LayerOps::ensureDomainLayer`
- No `removeAllMapLayers` on survey load
- VWorld key only via `VworldSettings`
---
