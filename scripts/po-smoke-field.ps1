# PO field smoke: ctest + app smoke-quit + basemap unit coverage (via workflow tests)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$env:PATH = "C:\CMake\bin;" + $env:PATH
. "$PSScriptRoot\dev-env.ps1"

$exe = Join-Path $Root "build\Release\ka-hgis.exe"
$tests = Join-Path $Root "build\Release\ka_workflow_tests.exe"
if (-not (Test-Path $exe) -or -not (Test-Path $tests)) {
  Write-Host "Release binaries missing — configuring/building..."
  if (-not (Test-Path "build\CMakeCache.txt")) {
    & cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
  }
  & cmake --build build --config Release
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

Write-Host "== ctest =="
& ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "ctest failed" }

Write-Host "== smoke-quit =="
& "$PSScriptRoot\run-ka-hgis.ps1" --smoke-quit
if ($LASTEXITCODE -ne 0) { throw "smoke-quit failed: $LASTEXITCODE" }

Write-Host "po-smoke-field OK (ctest includes osmBasemapValidWithExtent)"
exit 0
