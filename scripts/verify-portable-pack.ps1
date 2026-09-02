$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$out = Join-Path $root "dist\ka-hgis-portable"
foreach ($n in @("ka-hgis.exe", "start.bat", "run.ps1", "README.txt")) {
  $p = Join-Path $out $n
  if (-not (Test-Path -LiteralPath $p)) { throw "portable missing $n" }
}
$sec = Join-Path $out "config\secrets.ini"
if (-not (Test-Path -LiteralPath $sec)) { throw "portable missing config/secrets.ini" }
$raw = Get-Content -LiteralPath $sec -Raw
if ($raw -notmatch 'ApiKey\s*=\s*\S+') { throw "portable secrets.ini has no ApiKey" }
if (-not (Test-Path (Join-Path $out "share\proj\proj.db"))) { throw "portable missing proj.db" }
if (-not (Test-Path (Join-Path $out "apps\Qt6\plugins\platforms\qwindows.dll"))) { throw "portable missing qwindows.dll" }
Write-Host "portable pack verification passed"
exit 0
