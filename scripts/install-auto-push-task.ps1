# Registers ka-hgis-auto-git-push in Windows Task Scheduler to run every 30 minutes.

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ScriptPath = Join-Path $RepoRoot "scripts\auto-git-push.ps1"
$TaskName = "ka-hgis-auto-git-push"

if (-not (Test-Path $ScriptPath)) {
  Write-Error "Script not found: $ScriptPath"
  exit 1
}

# Use wscript/powershell hidden window execution
$actionCmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$ScriptPath`""

Write-Host "Registering Windows Scheduled Task: $TaskName (every 30 minutes)..." -ForegroundColor Cyan

# schtasks /create /sc minute /mo 30 /tn $TaskName /tr $actionCmd /f
& schtasks.exe /create /tn $TaskName /tr $actionCmd /sc minute /mo 30 /f
if ($LASTEXITCODE -ne 0) {
  Write-Error "Failed to register scheduled task: $LASTEXITCODE"
  exit $LASTEXITCODE
}

Write-Host "`nTask successfully registered!" -ForegroundColor Green
Write-Host "Task details:" -ForegroundColor Yellow
& schtasks.exe /query /tn $TaskName /fo LIST
