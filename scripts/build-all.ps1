# One-shot: env + build + ctest + smoke + e2e + portable
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

. "$PSScriptRoot\dev-env.ps1"

$cmakeCandidates = @(
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
if (-not $cmake) { throw "cmake.exe not found. Install CMake and retry." }

if (-not (Test-Path "build\CMakeCache.txt")) {
  & cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKA_HGIS_BUILD_TESTS=ON
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}

& cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

& ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "ctest failed" }

$smoke = Start-Process -FilePath "powershell.exe" -ArgumentList @(
  "-NoProfile", "-ExecutionPolicy", "Bypass",
  "-File", (Join-Path $PSScriptRoot "run-ka-hgis.ps1"), "--smoke-quit"
) -Wait -PassThru -NoNewWindow
if ($smoke.ExitCode -ne 0) { throw "smoke-quit failed: $($smoke.ExitCode)" }

& "$PSScriptRoot\e2e-smoke.ps1"
if ($LASTEXITCODE -ne 0) { throw "e2e failed" }

& "$PSScriptRoot\make-portable.ps1"
$ver = (Get-Content VERSION -Raw).Trim()
Write-Host "build-all OK  version=$ver"
exit 0
