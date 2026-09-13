# Removes ka-hgis-auto-git-push from Windows Task Scheduler.

$TaskName = "ka-hgis-auto-git-push"

Write-Host "Unregistering Windows Scheduled Task: $TaskName..." -ForegroundColor Cyan

& schtasks.exe /delete /tn $TaskName /f
if ($LASTEXITCODE -eq 0) {
  Write-Host "Task successfully removed." -ForegroundColor Green
} else {
  Write-Host "Task was not found or failed to delete." -ForegroundColor Yellow
}
