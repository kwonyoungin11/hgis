$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# Portable: sibling ka-hgis.exe FIRST
$candidates = @(
  (Join-Path $here "ka-hgis.exe"),
  (Join-Path $here "..\build\Release\ka-hgis.exe"),
  (Join-Path $here "..\build\Debug\ka-hgis.exe"),
  (Join-Path $here "..\build\ka-hgis.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "ka-hgis.exe not found beside this script. Build/copy first." }
$devEnv = Join-Path $here "dev-env.ps1"
if (Test-Path $devEnv) { . $devEnv }
& $exe @args
exit $LASTEXITCODE
