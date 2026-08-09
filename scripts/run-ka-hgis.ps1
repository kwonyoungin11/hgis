# Launch ka-hgis with OSGeo4W env.
# NOTE: QGIS may print non-fatal GRASS plugin warnings on stderr.
# PowerShell "Stop" + native stderr looks like a crash — we avoid that.
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\dev-env.ps1"

$candidates = @(
  (Join-Path $here "..\build\Release\ka-hgis.exe"),
  (Join-Path $here "..\build\Debug\ka-hgis.exe"),
  (Join-Path $here "..\build\ka-hgis.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "ka-hgis.exe not found. Build first." }
$exe = (Resolve-Path $exe).Path
$wd = Split-Path -Parent $exe

# Prefer not loading broken GRASS plugins (optional; warnings are still harmless)
if (-not $env:QGIS_PLUGINPATH) {
  $plug = Join-Path $env:QGIS_PREFIX_PATH "plugins"
  if (Test-Path $plug) { $env:QGIS_PLUGINPATH = $plug }
}

$argList = @($args)
$smoke = $argList -contains "--smoke-quit"

# Start-Process does not turn stderr into terminating ErrorRecords
$outLog = Join-Path $env:TEMP "ka-hgis-stdout.log"
$errLog = Join-Path $env:TEMP "ka-hgis-stderr.log"
Remove-Item $outLog, $errLog -ErrorAction SilentlyContinue

if ($smoke) {
  $p = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $wd `
    -Wait -PassThru -NoNewWindow `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  $code = $p.ExitCode
  if (Test-Path $errLog) {
    $errText = Get-Content $errLog -Raw -ErrorAction SilentlyContinue
    if ($errText) {
      # Show only unexpected lines (hide known GRASS noise)
      $filtered = $errText -split "`r?`n" | Where-Object {
        $_ -and ($_ -notmatch "plugin_grass|provider_grass|grassraster")
      }
      if ($filtered) {
        Write-Host "--- ka-hgis messages ---" -ForegroundColor DarkYellow
        $filtered | ForEach-Object { Write-Host $_ }
      }
    }
  }
  if ($code -ne 0) {
    Write-Host "ka-hgis exited with code $code" -ForegroundColor Red
    if (Test-Path $errLog) { Get-Content $errLog | Select-Object -Last 30 | ForEach-Object { Write-Host $_ } }
  } else {
    Write-Host "ka-hgis smoke-quit OK (exit 0). GRASS plugin warnings ignored if any." -ForegroundColor Green
  }
  exit $code
}

# Interactive GUI: detach so the shell stays clean
if ($argList.Count -gt 0) {
  Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $wd
} else {
  Start-Process -FilePath $exe -WorkingDirectory $wd
}
Write-Host "Started: $exe"
Write-Host "Note: GRASS plugin dll warnings (if any) are non-fatal."
exit 0
