$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# Portable first: sibling ka-hgis.exe next to this script (dist/ka-hgis-portable)
$candidates = @(
  (Join-Path $here "ka-hgis.exe"),
  (Join-Path $here "..\build\Release\ka-hgis.exe"),
  (Join-Path $here "..\build\Debug\ka-hgis.exe"),
  (Join-Path $here "..\build\ka-hgis.exe"),
  (Join-Path $here "..\dist\ka-hgis-portable\ka-hgis.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "ka-hgis.exe not found. Build first or place ka-hgis.exe beside this script." }
# Prefer portable-local or scripts/dev-env.ps1
$devEnv = Join-Path $here "dev-env.ps1"
if (-not (Test-Path $devEnv)) {
  $devEnv = Join-Path $here "dev-env.ps1"
}
if (Test-Path $devEnv) {
  . $devEnv
} else {
  $repoDev = Join-Path $here "..\scripts\dev-env.ps1"
  if (Test-Path $repoDev) { . $repoDev }
}
& $exe @args
exit $LASTEXITCODE
