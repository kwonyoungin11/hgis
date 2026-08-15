$ErrorActionPreference = "Continue"
$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try { $evt = $raw | ConvertFrom-Json } catch { exit 0 }

$root = $env:GROK_WORKSPACE_ROOT
if (-not $root) { $root = $env:CLAUDE_PROJECT_DIR }
if (-not $root) { exit 0 }

$cmd = ""
if ($evt.toolInput -and $evt.toolInput.command) { $cmd = [string]$evt.toolInput.command }
if ($cmd -notmatch 'cmake|ctest|build-all|build-now|smoke-quit|ka-hgis-verify') { exit 0 }

$state = Join-Path $root ".grok\.state"
New-Item -ItemType Directory -Force -Path $state | Out-Null
$stamp = Join-Path $state "last-verify"
Get-Date -Format o | Set-Content -Path $stamp -Encoding ascii
exit 0
