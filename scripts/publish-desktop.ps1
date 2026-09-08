# Copy Release ka-hgis.exe to the folder the desktop icon launches.
# Icon: 고고학 전용 HGIS.lnk -> dist\ka-hgis-portable\start.bat -> ka-hgis.exe
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$src = Join-Path $root "build\Release\ka-hgis.exe"
$dstDir = Join-Path $root "dist\ka-hgis-portable"
$dst = Join-Path $dstDir "ka-hgis.exe"
if (-not (Test-Path -LiteralPath $src)) { throw "Release ka-hgis.exe missing. Build first." }
if (-not (Test-Path -LiteralPath $dstDir)) { throw "dist\ka-hgis-portable missing." }

& (Join-Path $PSScriptRoot 'verify-release.ps1') -CheckOnly
if ($LASTEXITCODE -ne 0) { throw 'Release verification did not pass. Portable was not changed.' }

if (Get-Process -Name ka-hgis -ErrorAction SilentlyContinue) {
  throw "HGIS is still running. Close it after saving, then publish again. No process was stopped."
}
Copy-Item -LiteralPath $src -Destination $dst -Force
$qssSrc = Join-Path $root "data\theme\ka-hgis.qss"
$qssDstDir = Join-Path $dstDir "data\theme"
if (Test-Path -LiteralPath $qssSrc) {
  New-Item -ItemType Directory -Force -Path $qssDstDir | Out-Null
  Copy-Item -LiteralPath $qssSrc -Destination (Join-Path $qssDstDir "ka-hgis.qss") -Force
}
$dongSrc = Join-Path $root "data\korea_dongs.json"
$dongDstDir = Join-Path $dstDir "data"
if (Test-Path -LiteralPath $dongSrc) {
  New-Item -ItemType Directory -Force -Path $dongDstDir | Out-Null
  Copy-Item -LiteralPath $dongSrc -Destination (Join-Path $dongDstDir "korea_dongs.json") -Force
}
$pdb = Join-Path $root "build\Release\ka-hgis.pdb"
if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination $dstDir -Force }

$a = Get-Item -LiteralPath $src
$b = Get-Item -LiteralPath $dst
if ($a.Length -ne $b.Length) { throw "portable exe size mismatch after copy" }
Write-Host ("published {0} bytes -> {1}" -f $b.Length, $dst)
exit 0
