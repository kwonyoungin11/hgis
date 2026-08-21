$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:OSGEO4W_ROOT = $here
$env:QGIS_PREFIX_PATH = Join-Path $here "apps\qgis-dev"
$bins = @(
  $here,
  (Join-Path $here "bin"),
  (Join-Path $here "apps\qgis-dev\bin"),
  (Join-Path $here "apps\Qt6\bin"),
  (Join-Path $here "apps\gdal-dev\bin"),
  (Join-Path $here "apps\pdal-dev\bin")
) | Where-Object { Test-Path $_ }
$env:PATH = ($bins + $env:PATH) -join ";"
$qtPlug = Join-Path $here "apps\Qt6\plugins"
if (Test-Path $qtPlug) { $env:QT_PLUGIN_PATH = $qtPlug }
$env:QGIS_PLUGIN_PATH = Join-Path $here "apps\qgis-dev\plugins"
$gdal = Join-Path $here "apps\gdal-dev\share\gdal"
if (Test-Path $gdal) { $env:GDAL_DATA = $gdal }
$proj = Join-Path $here "share\proj"
if (Test-Path $proj) { $env:PROJ_DATA = $proj; $env:PROJ_LIB = $proj }
$exe = Join-Path $here "ka-hgis.exe"
if (-not (Test-Path $exe)) { throw "ka-hgis.exe missing in $here" }
& $exe @args
exit $LASTEXITCODE
