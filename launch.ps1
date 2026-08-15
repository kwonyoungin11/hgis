$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$logDir = Join-Path $here "build\Release"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
$log = Join-Path $logDir "ka-hgis-launch.log"
function Write-LaunchLog([string]$msg) {
  $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
  Add-Content -LiteralPath $log -Value $line -Encoding UTF8
}

try {
  . "$here\scripts\dev-env.ps1"
  $candidates = @(
    (Join-Path $here "build\Release\ka-hgis.exe"),
    (Join-Path $here "build\ka-hgis.exe"),
    (Join-Path $here "dist\ka-hgis-portable\ka-hgis.exe")
  )
  $exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $exe) { throw "ka-hgis.exe 없음. Release 빌드가 필요합니다." }

  $work = Split-Path $exe
  Write-LaunchLog "start exe=$exe cwd=$work OSGEO=$($env:OSGEO4W_ROOT)"

  $p = Start-Process -FilePath $exe -WorkingDirectory $work -PassThru -WindowStyle Normal
  if (-not $p) { throw "Start-Process가 프로세스를 만들지 못했습니다." }

  $ok = $false
  for ($i = 0; $i -lt 25; $i++) {
    Start-Sleep -Milliseconds 200
    $alive = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    if (-not $alive) { throw "프로세스가 바로 종료되었습니다 (PID $($p.Id)). DLL/경로 문제일 수 있습니다. 로그: $log" }
    if ($alive.MainWindowHandle -ne 0) {
      $ok = $true
      break
    }
  }
  Write-LaunchLog ("pid={0} hwnd={1} title={2}" -f $p.Id, $p.MainWindowHandle, $p.MainWindowTitle)
  if (-not $ok) {
    Write-LaunchLog "window handle still 0 — process is alive, window may be off-screen"
  }
} catch {
  Write-LaunchLog ("FAIL " + $_.Exception.Message)
  throw
}
