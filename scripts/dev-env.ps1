$ErrorActionPreference = "Stop"
$OSGEO = if ($env:OSGEO4W_ROOT) { $env:OSGEO4W_ROOT } else { "D:\OSGeo4W" }
if (-not (Test-Path $OSGEO)) { throw "OSGEO4W_ROOT not found: $OSGEO" }
$env:OSGEO4W_ROOT = $OSGEO
$qgis = Join-Path $OSGEO "apps\qgis-dev"
if (-not (Test-Path $qgis)) { throw "qgis-dev missing under $OSGEO" }
$env:QGIS_PREFIX_PATH = $qgis
$bins = @(
  (Join-Path $qgis "bin"),
  (Join-Path $OSGEO "apps\Qt6\bin"),
  (Join-Path $OSGEO "apps\gdal-dev\bin"),
  (Join-Path $OSGEO "apps\pdal-dev\bin"),
  (Join-Path $OSGEO "bin")
)
$env:PATH = ($bins + $env:PATH) -join ";"
$env:QT_PLUGIN_PATH = (Join-Path $OSGEO "apps\Qt6\plugins") + ";" + (Join-Path $qgis "qtplugins")
$gdalData = Join-Path $OSGEO "apps\gdal-dev\share\gdal"
if (Test-Path $gdalData) { $env:GDAL_DATA = $gdalData }
$proj = Join-Path $OSGEO "share\proj"
if (Test-Path $proj) { $env:PROJ_DATA = $proj; $env:PROJ_LIB = $proj }
Write-Host "OSGEO4W_ROOT=$env:OSGEO4W_ROOT"
Write-Host "QGIS_PREFIX_PATH=$env:QGIS_PREFIX_PATH"
# GRASS plugins often fail to load without full GRASS install — app still works.
# Do not treat those stderr lines as fatal in callers.
