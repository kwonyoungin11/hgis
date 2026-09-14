$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\dev-env.ps1"
Push-Location $Root
try {
  & cmake --preset vs
  if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
  & cmake --build --preset release --parallel
  if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
  Write-Host "BUILD SUCCESS: $Root\build\Release\ka-hgis.exe"
} finally {
  Pop-Location
}
