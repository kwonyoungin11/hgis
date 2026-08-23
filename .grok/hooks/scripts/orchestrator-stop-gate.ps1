# Compatibility wrapper — canonical stop gate is ../bin/stop-gate.ps1
$ErrorActionPreference = 'Continue'
$canonical = Join-Path $PSScriptRoot '..\bin\stop-gate.ps1'
$raw = [Console]::In.ReadToEnd()
if (-not (Test-Path -LiteralPath $canonical)) { exit 0 }
$raw | & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $canonical
exit $LASTEXITCODE
