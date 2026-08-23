# PostToolUse: record this-turn product-src writes, graph spawns, verify attempts.
$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'graph-loop-state.ps1')

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try { $evt = $raw | ConvertFrom-Json } catch { exit 0 }

$tool = Get-KaToolName $evt
$inputObj = Get-KaToolInput $evt
$cmd = ''
if ($inputObj -and $inputObj.command) { $cmd = [string]$inputObj.command }

function Read-PathFromInput($obj) {
  if ($null -eq $obj) { return '' }
  foreach ($k in @('file_path', 'filePath', 'path', 'target_file', 'targetFile')) {
    if ($obj.PSObject.Properties.Name -contains $k) {
      $v = [string]$obj.$k
      if ($v) { return $v }
    }
  }
  return ''
}

$t = $tool.ToLowerInvariant()

if ($t -match 'search_replace|write|edit|strreplace') {
  Add-KaTurnSrcWrite (Read-PathFromInput $inputObj)
  exit 0
}

if ($t -match 'spawn_subagent|task') {
  $kind = ''
  if ($inputObj) {
    foreach ($k in @('subagent_type', 'subagentType', 'agent_type', 'agentType')) {
      if ($inputObj.PSObject.Properties.Name -contains $k) {
        $kind = [string]$inputObj.$k
        if ($kind) { break }
      }
    }
  }
  if (-not $kind) { $kind = 'subagent' }
  Add-KaTurnGraph $kind
  exit 0
}

if ($t -eq 'workflow') {
  Add-KaTurnGraph 'workflow'
  if ($inputObj -and $inputObj.name) { Add-KaTurnGraph ([string]$inputObj.name) }
  exit 0
}

$evName = [string]$env:GROK_HOOK_EVENT
if ($evName -match 'failure') { exit 0 }

if ($cmd -match 'cmake|ctest|build-all|build-now|smoke-quit|ka-hgis-verify') {
  $code = Get-KaToolExitCode $evt
  if ($null -eq $code -or $code -ne 0) { exit 0 }
  Set-KaVerifyStamp
}

exit 0
