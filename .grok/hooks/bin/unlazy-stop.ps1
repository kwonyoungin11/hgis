# Forward Grok Stop JSON to the unlazy stop-hook (no CHECK execution).
# Arms only when a valid scoped .unlazy/<id>/ pipeline exists under cwd.
# Root GATES.md (legacy) does not trap research turns: always pass --scope
# when exactly one valid id exists, and never call node with zero valid ids.
# Missing node / missing skill / no scoped pipeline => fail-open (exit 0).
# ASCII-only so Windows PowerShell 5.1 parses it regardless of console CP.
$ErrorActionPreference = 'Continue'
try { [Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false) } catch { }
try { [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false) } catch { }

$script = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\skills\unlazy\scripts\stop-hook.mjs'))
if (-not (Test-Path -LiteralPath $script)) { exit 0 }

$node = $null
$cmd = Get-Command node -ErrorAction SilentlyContinue
if ($cmd -and $cmd.Source) { $node = [string]$cmd.Source }
if (-not $node) {
  $guess = Join-Path ${env:ProgramFiles} 'nodejs\node.exe'
  if (Test-Path -LiteralPath $guess) { $node = $guess }
}
if (-not $node) { exit 0 }

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

$evt = $null
try { $evt = $raw | ConvertFrom-Json } catch { exit 0 }
if ($null -eq $evt) { exit 0 }
if ($evt.stopHookActive -eq $true) { exit 0 }
if ($evt.reason -and [string]$evt.reason -ne 'end_turn') { exit 0 }
foreach ($k in @('subagentType', 'subagent_type', 'agentType', 'agent_type')) {
  if ($evt.PSObject.Properties.Name -contains $k) {
    $v = [string]$evt.$k
    if ($v) { exit 0 }
  }
}

$root = $null
if ($evt.cwd) { $root = [string]$evt.cwd }
if (-not $root -and $evt.workspaceRoot) { $root = [string]$evt.workspaceRoot }
if (-not $root) { $root = [string]$env:GROK_WORKSPACE_ROOT }
if (-not $root) { $root = (Get-Location).Path }

function Test-KaUnlazyScopeName([string]$name) {
  if ([string]::IsNullOrWhiteSpace($name)) { return $false }
  if ($name -eq '.' -or $name -eq '..' -or $name -eq 'locks') { return $false }
  return $name -match '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$'
}

$unlazyDir = Join-Path $root '.unlazy'
if (-not (Test-Path -LiteralPath $unlazyDir)) { exit 0 }
$scopes = @()
try {
  $scopes = @(Get-ChildItem -LiteralPath $unlazyDir -Directory -ErrorAction Stop |
    Where-Object { Test-KaUnlazyScopeName $_.Name })
} catch { exit 0 }
if ($scopes.Count -lt 1) { exit 0 }

$nodeArgs = @($script)
if ($scopes.Count -eq 1) {
  $nodeArgs += '--scope'
  $nodeArgs += $scopes[0].Name
}

$raw | & $node @nodeArgs
exit $LASTEXITCODE
