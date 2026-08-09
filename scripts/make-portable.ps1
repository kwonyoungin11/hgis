$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$out = Join-Path $root "dist\ka-hgis-portable"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $root "build\Release\ka-hgis.exe"
if (-not (Test-Path $exe)) { throw "Build ka-hgis.exe first" }
Copy-Item $exe $out -Force
Copy-Item (Join-Path $root "scripts\dev-env.ps1") $out -Force
Copy-Item (Join-Path $root "scripts\run-ka-hgis.ps1") $out -Force
Copy-Item (Join-Path $root "data") $out -Recurse -Force
$docsUser = Join-Path $out "docs\user"
New-Item -ItemType Directory -Force -Path $docsUser | Out-Null
if (Test-Path (Join-Path $root "docs\user")) {
  Copy-Item (Join-Path $root "docs\user\*") $docsUser -Recurse -Force
}
Set-Content (Join-Path $out "run.bat") -Value "@echo off`r`ncd /d %~dp0`r`npowershell -NoProfile -ExecutionPolicy Bypass -File `"%~dp0run-ka-hgis.ps1`" %*`r`n" -Encoding ASCII
Set-Content (Join-Path $out "README.txt") -Value @"
ka-hgis portable (DLLs not vendored)

REQUIREMENTS on this PC:
  - OSGeo4W with qgis-dev (recommended root: C:\OSGeo4W)
  - Also accepts OSGEO4W_ROOT env, then D:\OSGeo4W

RUN:
  run.bat
  or: powershell -File .\run-ka-hgis.ps1

Launcher uses ka-hgis.exe in THIS folder first, then sets PATH via dev-env.ps1.

Full second-PC guide (clone + build): see repo docs/other-pc-setup.md
  https://github.com/kwonyoungin11/hgis
"@ -Encoding ASCII
Write-Host "Portable folder: $out"
Get-ChildItem $out | Select-Object Name
