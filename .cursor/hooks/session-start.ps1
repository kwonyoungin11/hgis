$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\state.ps1')
$null = Get-KaCursorStdinJson
Reset-KaCursorTurnState
exit 0
