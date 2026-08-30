# UserPromptSubmit — ArcGIS + QGIS + developer team (+ designers, TDD, predev)
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
if ($isExpert) { exit 0 }
$txt = Join-Path $PSScriptRoot 'experts-team.txt'
$ctx = if (Test-Path -LiteralPath $txt) {
  [System.IO.File]::ReadAllText($txt, [System.Text.Encoding]::UTF8)
} else {
  '[EXPERT TEAM] arcgis-expert || qgis-expert then ka-developer; designers; TDD; predev.'
}
$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'UserPromptSubmit'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5
[Console]::Out.Write($payload)
exit 0
