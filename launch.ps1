$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
. "$here\scripts\dev-env.ps1"
$exe = "$here\dist\ka-hgis-portable\ka-hgis.exe"
if (-not (Test-Path $exe)) {
    $exe = "$here\build\Release\ka-hgis.exe"
}
if (-not (Test-Path $exe)) {
    $exe = "$here\build\ka-hgis.exe"
}
Write-Host "Launching $exe ..."
Start-Process -FilePath $exe
