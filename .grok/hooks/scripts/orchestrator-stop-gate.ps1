# Stop gate: wait for background experts; require verify on implementation claims
$ErrorActionPreference = 'SilentlyContinue'
$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try {
  $e = $raw | ConvertFrom-Json
} catch {
  exit 0
}

if ($e.reason -and $e.reason -ne 'end_turn') { exit 0 }
if ($e.stopHookActive -eq $true) { exit 0 }

$tasks = @()
if ($e.backgroundTasks) { $tasks = @($e.backgroundTasks) }
$busy = $tasks | Where-Object {
  $_.status -match 'running|pending|in_progress|active' -or
  ($_.type -eq 'subagent' -and $_.status -notmatch 'completed|failed|cancelled|done')
}
if ($busy -and $busy.Count -gt 0) {
  $names = ($busy | ForEach-Object { if ($_.description) { $_.description } else { $_.type } }) -join '; '
  $msg = "GRAPH ENGINEERING: background experts still running ($names). Wait, merge findings, then finish."
  $out = @{ decision = 'block'; reason = $msg } | ConvertTo-Json -Compress
  [Console]::Out.Write($out)
  exit 0
}

$msg = [string]$e.lastAssistantMessage
if ([string]::IsNullOrWhiteSpace($msg)) { exit 0 }

$implHints = $msg -match '(?i)(implement|fixed|added|changed|refactored|patch|created file|updated|수정|구현|적용)'
$verifyHints = $msg -match '(?i)(test(s)? (pass|passed|fail)|ctest|cmake --build|smoke-quit|build-all\.ps1|verified|build succeeded|compile success|cannot run tests)'
$qaOnly = $msg -match '(?i)(here is how|설명|요약하면|you can|문서에 따르면)' -and -not $implHints

if ($qaOnly) { exit 0 }

if ($implHints -and -not $verifyHints) {
  $reason = @'
LOOP ENGINEERING: implementation claimed without this-turn verify.
Before finishing: cmake --build (touched target) and relevant ctest/smoke, quote exit codes, OR state why none apply. If red, fix and re-run. Merge graph expert findings.
'@
  $out = @{ decision = 'block'; reason = $reason } | ConvertTo-Json -Compress
  [Console]::Out.Write($out)
  exit 0
}

exit 0
