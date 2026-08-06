$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\dev-env.ps1"
$candidates = @(
  (Join-Path $here "..\build\Release\ka-hgis.exe"),
  (Join-Path $here "..\build\Debug\ka-hgis.exe"),
  (Join-Path $here "..\build\ka-hgis.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "ka-hgis.exe not found. Build first." }
& $exe @args
exit $LASTEXITCODE
