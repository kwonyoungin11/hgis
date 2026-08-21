# UserPromptSubmit — excavation HGIS loop
$ErrorActionPreference = 'SilentlyContinue'
$raw = [Console]::In.ReadToEnd()
$isExpert = $false
if (-not [string]::IsNullOrWhiteSpace($raw)) {
  try {
    $evt = $raw | ConvertFrom-Json
    if ($evt.subagentType) { $isExpert = $true }
  } catch { }
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
