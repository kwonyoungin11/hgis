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
Set-Content (Join-Path $out "README.txt") -Value "Requires OSGeo4W at D:\OSGeo4W or OSGEO4W_ROOT. Run run.bat. DLLs not vendored." -Encoding ASCII
Write-Host "Portable folder: $out"
Get-ChildItem $out | Select-Object Name
