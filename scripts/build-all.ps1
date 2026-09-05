# One-shot: env + build + ctest + smoke + e2e + portable
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

. "$PSScriptRoot\dev-env.ps1"

$cmakeCandidates = @(
  "C:\CMake\bin",
  "${env:ProgramFiles}\CMake\bin",
  "${env:ProgramFiles(x86)}\CMake\bin"
)
foreach ($c in $cmakeCandidates) {
  if (Test-Path (Join-Path $c "cmake.exe")) {
    $env:PATH = "$c;$env:PATH"
    break
  }
}
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) { throw "cmake.exe not found. Install CMake at C:\CMake\bin (preferred) and retry." }

$osgeoArg = @()
if ($env:OSGEO4W_ROOT) {
  $osgeoSlash = ($env:OSGEO4W_ROOT -replace '\\', '/')
  $osgeoArg = @("-DOSGEO4W_ROOT=$osgeoSlash")
}

if (-not (Test-Path "build\CMakeCache.txt")) {
  & cmake -S . -B build -G "Visual Studio 17 2022" -A x64 @osgeoArg -DKA_HGIS_BUILD_TESTS=ON
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}

& cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

# Best-effort: refresh clangd compile_commands.json (never fail the main pipeline)
try {
  & "$PSScriptRoot\gen-compile-commands.ps1"
  if ($LASTEXITCODE -ne 0) {
    Write-Host "WARN: clangd compile_commands refresh exited $LASTEXITCODE"
  }
} catch {
  Write-Host "WARN: clangd compile_commands refresh failed: $_"
}

& ctest --test-dir build -C Release --output-on-failure --parallel
if ($LASTEXITCODE -ne 0) { throw "ctest failed" }

& "$PSScriptRoot\run-ka-hgis.ps1" --smoke-quit
if ($LASTEXITCODE -ne 0) { throw "smoke-quit failed: $LASTEXITCODE" }

& "$PSScriptRoot\e2e-smoke.ps1"
if ($LASTEXITCODE -ne 0) { throw "e2e failed" }

& "$PSScriptRoot\make-portable.ps1"
$ver = (Get-Content VERSION -Raw).Trim()
Write-Host "build-all OK  version=$ver"
exit 0
