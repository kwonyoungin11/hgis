# Run hook self-checks in separate processes (no shared env).
# Unlazy first: graph-loop's console encoding breaks a later unlazy Stop probe.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File .grok/hooks/bin/self-check-hooks.ps1
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$code = 0
foreach ($name in @('self-check-unlazy.ps1', 'self-check-graph-loop.ps1')) {
  $script = Join-Path $here $name
  Write-Host "===== $name ====="
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script
  if ($LASTEXITCODE -ne 0) { $code = $LASTEXITCODE }
}
if ($code -ne 0) { exit $code }
Write-Host 'self-check-hooks: all PASS'
exit 0
