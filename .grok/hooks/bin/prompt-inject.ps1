# UserPromptSubmit — excavation HGIS loop + new-turn graph/loop state
$ErrorActionPreference = 'SilentlyContinue'
. (Join-Path $PSScriptRoot 'graph-loop-state.ps1')
$raw = [Console]::In.ReadToEnd()
$isExpert = $false
if (-not [string]::IsNullOrWhiteSpace($raw)) {
  try {
    $evt = $raw | ConvertFrom-Json
    if (Test-KaIsExpertEvent $evt) { $isExpert = $true }
  } catch { }
}
# Parent turn only: experts must not wipe the parent's this-turn write/graph log.
if (-not $isExpert) {
  try { Reset-KaTurnState } catch { }
}
$here = $PSScriptRoot
$name = if ($isExpert) { 'excavation-loop-expert.txt' } else { 'excavation-loop.txt' }
$txt = Join-Path $here $name
$ctx = if (Test-Path -LiteralPath $txt) {
  [System.IO.File]::ReadAllText($txt, [System.Text.Encoding]::UTF8)
} else {
  '[EXCAVATION-GIS LOOP] sequential-thinking + context7; QGIS+ArcGIS docs; graph+loop; export 5179.'
}
$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'UserPromptSubmit'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5
[Console]::Out.Write($payload)
exit 0
