# GIS-first verify (QGIS / ArcGIS practice)

This product is a field HGIS on QGIS libraries (Architecture B). Map / layer / CRS / WMS / digitize / layout bugs are GIS problems, not generic UI problems.

## Before changing those areas

1. Name the GIS object: project CRS, layer CRS, OTF, legend vs canvas `setLayers`, WMS GetCapabilities/GetMap, edit buffer, scale.
2. Check in-repo truth, not a guess:
   - `docs/vendor/qgis-manual-3.44/` (PyQGIS cookbook)
   - existing `src/core/LayerOps.*` / `MainWindow.*` call path
   - official QGIS docs: https://docs.qgis.org/latest/en/docs/pyqgis_developer_cookbook/
   - VWorld only via published WMS/WMTS URLs (GetCapabilities / GetMap), never invented tile paths
3. ArcGIS words map through `docs/user/job-cards/arcgis-용어.md` and `docs/research/02-qgis-arcgis-pain-gain.md`. Do not clone ArcMap chrome.
4. Prove the failure with a GIS check (layer in project? on canvas? CRS? scale? GetMap image has pixels?) then patch.

## Roles (Grok subagents)

When the user reports a map problem, spawn these in parallel — read-only first — plus context7, and STA if complexity ≥ 3:

- **qgis-api**: cookbook + `Qgs*` wiring (addMapLayer, destinationCrs, WMS URI, extent)
- **gis-protocol**: VWorld/WMS/WMTS/XYZ live or documented request
- **field-check**: turn the screenshot into a pass/fail (e.g. 병산동 1:2000, 지적 선이 위성 위)

Use MCP and matching skills. Do not invent a fourth GIS theory.

## User is not a GIS engineer

Ask at most one yes/no the user can see (지도에 선이 보이나). CRS, OTF, WMS bbox, layer order are agent work.
