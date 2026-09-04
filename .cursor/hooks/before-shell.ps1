$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\state.ps1')

$evt = Get-KaCursorStdinJson
if (-not $evt) {
  Write-KaCursorJson @{ permission = 'allow' }
  exit 0
}

$cmd = [string]$evt.command
if ([string]::IsNullOrWhiteSpace($cmd)) {
  Write-KaCursorJson @{ permission = 'allow' }
  exit 0
}

if ($cmd -match '(?i)git\s+push\b.*(--force|--force-with-lease|\s-f\b)') {
  Write-KaCursorJson @{
    permission = 'deny'
    user_message = 'Blocked force-push. ka-hgis hooks deny git push --force.'
    agent_message = 'Do not force-push. Report the need to the user instead.'
  }
  exit 0
}

if ($cmd -match '(?i)git\s+reset\s+--hard') {
  Write-KaCursorJson @{
    permission = 'deny'
    user_message = 'Blocked git reset --hard. Uncommitted work must not be discarded.'
    agent_message = 'Do not reset uncommitted files. Leave the dirty tree as-is.'
  }
  exit 0
}

Write-KaCursorJson @{ permission = 'allow' }
exit 0
