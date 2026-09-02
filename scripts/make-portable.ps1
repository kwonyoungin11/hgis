# Build a self-contained Windows folder: USB copy, no OSGeo4W install on the target PC.
param(
  [string]$OutDir = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$out = if ($OutDir) { $OutDir } else { Join-Path $root "dist\ka-hgis-portable" }
$exe = Join-Path $root "build\Release\ka-hgis.exe"
if (-not (Test-Path $exe)) { throw "Build ka-hgis.exe first (Release)." }

$OSGEO = $null
if ($env:OSGEO4W_ROOT -and (Test-Path -LiteralPath $env:OSGEO4W_ROOT)) {
  $OSGEO = $env:OSGEO4W_ROOT
} elseif (Test-Path "A:\OSGeo4W") { $OSGEO = "A:\OSGeo4W" }
elseif (Test-Path "C:\OSGeo4W") { $OSGEO = "C:\OSGeo4W" }
elseif (Test-Path "D:\OSGeo4W") { $OSGEO = "D:\OSGeo4W" }
else { throw "OSGEO4W_ROOT not found on this build PC." }

$qgis = Join-Path $OSGEO "apps\qgis-dev"
if (-not (Test-Path $qgis)) { throw "qgis-dev missing under $OSGEO" }

Write-Host "Portable out: $out"
Write-Host "Runtime from: $OSGEO"
if (Test-Path $out) {
  Get-ChildItem $out -Force | Where-Object { $_.Name -notin @('data') } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Invoke-Robo([string]$src, [string]$dst, [string[]]$xd = @()) {
  if (-not (Test-Path $src)) { Write-Host "skip missing $src"; return }
  New-Item -ItemType Directory -Force -Path $dst | Out-Null
  $args = @($src, $dst, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/nc", "/ns", "/np", "/XO")
  if ($xd.Count -gt 0) { $args += @("/XD") + $xd }
  $args += @("/XF", "*.pdb", "*.lib", "*.exp", "*.a", "*.prl")
  & robocopy @args | Out-Null
  $code = $LASTEXITCODE
  if ($code -ge 8) { throw "robocopy failed $code : $src -> $dst" }
}

function Copy-Dlls([string]$src, [string]$dst) {
  if (-not (Test-Path $src)) { return }
  New-Item -ItemType Directory -Force -Path $dst | Out-Null
  Copy-Item (Join-Path $src "*.dll") $dst -Force -ErrorAction SilentlyContinue
}

Copy-Item $exe $out -Force
# PDB가 있으면 함께 배포 — 크래시 로그(KaCrashGuard)가 함수명·줄번호까지 심볼화한다.
$pdb = Join-Path $root "build\Release\ka-hgis.pdb"
if (Test-Path $pdb) { Copy-Item $pdb $out -Force }
if (Test-Path (Join-Path $root "data")) {
  Copy-Item (Join-Path $root "data") $out -Recurse -Force
}
$docsUser = Join-Path $out "docs\user"
New-Item -ItemType Directory -Force -Path $docsUser | Out-Null
if (Test-Path (Join-Path $root "docs\user")) {
  Copy-Item (Join-Path $root "docs\user\*") $docsUser -Recurse -Force
}

Write-Host "Copying QGIS prefix..."
Invoke-Robo $qgis (Join-Path $out "apps\qgis-dev") @("python", "grass", "include", "lib", "doc", "server")

Write-Host "Copying Qt6..."
Copy-Dlls (Join-Path $OSGEO "apps\Qt6\bin") (Join-Path $out "apps\Qt6\bin")
Invoke-Robo (Join-Path $OSGEO "apps\Qt6\plugins") (Join-Path $out "apps\Qt6\plugins")

Write-Host "Copying GDAL/PDAL/OSGeo bin..."
Copy-Dlls (Join-Path $OSGEO "apps\gdal-dev\bin") (Join-Path $out "apps\gdal-dev\bin")
if (Test-Path (Join-Path $OSGEO "apps\gdal-dev\share")) {
  Invoke-Robo (Join-Path $OSGEO "apps\gdal-dev\share") (Join-Path $out "apps\gdal-dev\share")
}
Copy-Dlls (Join-Path $OSGEO "apps\pdal-dev\bin") (Join-Path $out "apps\pdal-dev\bin")
Copy-Dlls (Join-Path $OSGEO "bin") (Join-Path $out "bin")
if (Test-Path (Join-Path $OSGEO "share\proj")) {
  Invoke-Robo (Join-Path $OSGEO "share\proj") (Join-Path $out "share\proj")
}

$qgisBin = Join-Path $qgis "bin"
# Windows loads DLLs from the exe folder first. Copy every runtime bin here
# so double-click / start.bat works without a pre-set PATH.
Copy-Dlls $qgisBin $out
Copy-Dlls (Join-Path $OSGEO "apps\Qt6\bin") $out
Copy-Dlls (Join-Path $OSGEO "apps\gdal-dev\bin") $out
Copy-Dlls (Join-Path $OSGEO "apps\pdal-dev\bin") $out
Copy-Dlls (Join-Path $OSGEO "bin") $out
Get-ChildItem (Join-Path $OSGEO "apps") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
  $b = Join-Path $_.FullName "bin"
  if (Test-Path $b) { Copy-Dlls $b $out }
}

$sys32 = Join-Path $env:WINDIR "System32"
foreach ($vc in @(
    "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll",
    "msvcp140_2.dll", "concrt140.dll", "vccorlib140.dll"
  )) {
  $src = Join-Path $sys32 $vc
  if (Test-Path $src) { Copy-Item $src $out -Force }
}

# Also keep qgis-dev\bin copies of qgis_*.dll for prefix-relative loads.
Copy-Dlls $qgisBin (Join-Path $out "apps\qgis-dev\bin")

$runPs1 = @'
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
'@
Set-Content -LiteralPath (Join-Path $out "run.ps1") -Value $runPs1 -Encoding UTF8
Set-Content -LiteralPath (Join-Path $out "run.bat") -Value @"
@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
"@ -Encoding ASCII
Set-Content -LiteralPath (Join-Path $out "start.bat") -Value @"
@echo off
cd /d "%~dp0"
set "OSGEO4W_ROOT=%~dp0"
set "QGIS_PREFIX_PATH=%~dp0apps\qgis-dev"
set "PATH=%~dp0;%~dp0bin;%~dp0apps\qgis-dev\bin;%~dp0apps\Qt6\bin;%~dp0apps\gdal-dev\bin;%~dp0apps\pdal-dev\bin;%PATH%"
set "QT_PLUGIN_PATH=%~dp0apps\Qt6\plugins"
set "QGIS_PLUGIN_PATH=%~dp0apps\qgis-dev\plugins"
set "GDAL_DATA=%~dp0apps\gdal-dev\share\gdal"
set "PROJ_DATA=%~dp0share\proj"
set "PROJ_LIB=%~dp0share\proj"
start "" "%~dp0ka-hgis.exe"
"@ -Encoding ASCII

$readmeKo = @"
필드고고학GIS  포터블 (Windows 10/11 64비트)

이 폴더 전체를 USB에 두면, QGIS/OSGeo4W를 설치하지 않은 다른 PC에서도 실행됩니다.
Visual Studio 설치도 필요 없습니다.

실행:
  start.bat     ← 이것을 더블클릭
  또는 ka-hgis.exe

주의:
  - apps, bin, share 폴더를 지우면 실행되지 않습니다.
  - 폴더 이름에 한글이 있어도 되지만, 경로가 너무 길면 start.bat 을 쓰세요.
  - VWorld 키는 만든 PC의 키를 config\secrets.ini 에 넣었습니다. USB를 다른 사람에게 주지 마세요.
  - GNU GPL v2 이상 (QGIS 라이브러리 링크). 자세한 공지는 앱 정보 창.

제작: 동국문화재연구원  ·  버전 1
"@
Set-Content -LiteralPath (Join-Path $out "README.txt") -Value $readmeKo -Encoding UTF8
Set-Content -LiteralPath (Join-Path $out "사용법.txt") -Value $readmeKo -Encoding UTF8

function Copy-VworldKeyToPortable([string]$portableRoot) {
  $dstDir = Join-Path $portableRoot "config"
  New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
  $dst = Join-Path $dstDir "secrets.ini"
  $cands = @(
    (Join-Path $env:APPDATA "ka-hgis\ka-hgis-vworld.ini"),
    (Join-Path $env:LOCALAPPDATA "ka-hgis\ka-hgis-vworld.ini"),
    (Join-Path $env:APPDATA "ka-hgis\ka-hgis\ka-hgis-vworld.ini"),
    (Join-Path $env:LOCALAPPDATA "ka-hgis\ka-hgis\ka-hgis-vworld.ini")
  )
  $key = $null
  foreach ($p in $cands) {
    if (-not (Test-Path -LiteralPath $p)) { continue }
    foreach ($line in Get-Content -LiteralPath $p -ErrorAction SilentlyContinue) {
      if ($line -match '^\s*ApiKey\s*=\s*(.+)\s*$') {
        $cand = $Matches[1].Trim().Trim('"')
        if ($cand) { $key = $cand; break }
      }
    }
    if ($key) { break }
  }
  if (-not $key -and $env:VWORLD_API_KEY) { $key = $env:VWORLD_API_KEY.Trim() }
  if (-not $key) {
    Write-Host "VWorld key: not found on this PC (other PC will need 도움말 → API 키)"
    return
  }
  $ini = "[VWorld]`r`nApiKey=$key`r`n"
  [System.IO.File]::WriteAllText($dst, $ini, [System.Text.UTF8Encoding]::new($false))
  Write-Host "VWorld key: copied into portable config/secrets.ini (value not printed)"
}

Copy-VworldKeyToPortable $out

Write-Host "Portable folder ready: $out"
Get-ChildItem $out | Select-Object Name, Mode, @{n='MB';e={ if ($_.PSIsContainer) { '' } else { [math]::Round($_.Length/1MB,1) } }}
$qgisOut = Join-Path $out "apps\qgis-dev\plugins"
Write-Host ("qgis plugins dir exists=" + (Test-Path $qgisOut))
Write-Host ("proj.db exists=" + (Test-Path (Join-Path $out "share\proj\proj.db")))
Write-Host ("qwindows exists=" + (Test-Path (Join-Path $out "apps\Qt6\plugins\platforms\qwindows.dll")))
