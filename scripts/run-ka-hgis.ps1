$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent $here

# Automated flags must stay in-process (ctest / smoke).
$wait = $false
foreach ($a in $args) {
  if ($a -eq "--smoke-quit" -or $a -eq "--qa-phase1" -or $a.StartsWith("--open-gpkg")) { $wait = $true }
}

if (-not $wait) {
  & (Join-Path $repo "launch.ps1")
  exit $LASTEXITCODE
}

. (Join-Path $here "dev-env.ps1")
$candidates = @(
  (Join-Path $repo "build\Release\ka-hgis.exe"),
  (Join-Path $repo "build\Debug\ka-hgis.exe"),
  (Join-Path $repo "build\ka-hgis.exe"),
  (Join-Path $repo "dist\ka-hgis-portable\ka-hgis.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "ka-hgis.exe not found. Build first." }
& $exe @args
exit $LASTEXITCODE
