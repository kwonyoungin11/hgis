# One-shot: verify toolchain → configure → build → test → smoke
# Usage (PowerShell, repo root):
#   .\scripts\bootstrap-dev-pc.ps1
#   .\scripts\bootstrap-dev-pc.ps1 -SkipInstall
#   .\scripts\bootstrap-dev-pc.ps1 -OsgeoRoot D:\OSGeo4W
param(
  [switch]$SkipInstall,
  [string]$OsgeoRoot = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "OK  $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "WARN $msg" -ForegroundColor Yellow }
function Write-Fail($msg) { Write-Host "FAIL $msg" -ForegroundColor Red }

Write-Host "ka-hgis bootstrap-dev-pc" -ForegroundColor Cyan
Write-Host "repo: $Root"

# --- toolchain checks ---
Write-Step "Check Git"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
  Write-Fail "git not found. Install Git for Windows, then re-run."
  exit 2
}
Write-Ok ("git " + (git --version))

Write-Step "Check CMake"
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
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  if (-not $SkipInstall) {
    Write-Warn "CMake missing — trying winget Kitware.CMake"
    winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
    foreach ($c in $cmakeCandidates) {
      if (Test-Path (Join-Path $c "cmake.exe")) { $env:PATH = "$c;$env:PATH"; break }
    }
  }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Fail "cmake.exe not found. Install CMake (prefer C:\CMake\bin)."
  exit 3
}
Write-Ok ("cmake " + (cmake --version | Select-Object -First 1))

Write-Step "Check VS 2022 C++ tools"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$hasVs = $false
if (Test-Path $vsWhere) {
  $inst = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
  if ($inst) { $hasVs = $true; Write-Ok "VS C++ tools: $inst" }
}
if (-not $hasVs) {
  Write-Warn "VS 2022 C++ Build Tools not detected."
  if (-not $SkipInstall) {
    Write-Warn "Run as Admin for auto-install, or install manually:"
    Write-Host '  winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
  }
}

Write-Step "Check OSGeo4W / qgis-dev"
if ($OsgeoRoot) { $env:OSGEO4W_ROOT = $OsgeoRoot }
. "$PSScriptRoot\dev-env.ps1"
if (-not $env:OSGEO4W_ROOT -or -not (Test-Path $env:OSGEO4W_ROOT)) {
  Write-Fail "OSGeo4W not found. Install to C:\OSGeo4W (or set OSGEO4W_ROOT)."
  Write-Host "  .\scripts\install-deps.ps1   # Admin PowerShell"
  Write-Host "  packages: qgis-dev, qt6-devel, gdal-dev-devel, sqlite3-devel, pdal-dev"
  exit 4
}
$qgisDev = Join-Path $env:OSGEO4W_ROOT "apps\qgis-dev"
if (-not (Test-Path $qgisDev)) {
  Write-Fail "qgis-dev missing under $($env:OSGEO4W_ROOT)\apps\qgis-dev"
  Write-Host "  Install OSGeo4W package: qgis-dev"
  exit 5
}
Write-Ok "OSGEO4W_ROOT=$($env:OSGEO4W_ROOT)"
Write-Ok "QGIS_PREFIX_PATH=$($env:QGIS_PREFIX_PATH)"

Write-Step "Optional: download large QGIS user-guide PDFs"
try {
  & "$PSScriptRoot\download-qgis-manuals.ps1"
} catch {
  Write-Warn "manual download skipped: $_"
}

Write-Step "Configure + build + test (build-all)"
& "$PSScriptRoot\build-all.ps1"
if ($LASTEXITCODE -ne 0) {
  Write-Fail "build-all failed with exit $LASTEXITCODE"
  exit $LASTEXITCODE
}

Write-Step "Done — ready to develop"
Write-Ok "exe: $Root\build\Release\ka-hgis.exe"
Write-Host @"

Next:
  .\scripts\run-ka-hgis.ps1
  # agent rules: AGENTS.md , OPENCODE_HANDOFF.md
  # daily sync:
  git pull origin main
  .\scripts\build-all.ps1

Product reminders:
  - New survey = empty legend until you draw/import
  - Digitize: 그리기 → 면/선/구역 → right-click finish → 편집저장
  - Layout editor: 도구 → 조판 편집 창
  - Upload CRS: EPSG:5179 SHP package
"@
exit 0
