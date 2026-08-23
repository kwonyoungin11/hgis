# Stop gate: wait for experts; LOOP on this-turn src writes; GRAPH on app+core without spawn.
# Pre-existing dirty src/ must NOT block a research-only reply.
# This file is ASCII-only so Windows PowerShell 5.1 parses it regardless of console CP.
$ErrorActionPreference = 'Continue'
try { [Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false) } catch { }
. (Join-Path $PSScriptRoot 'graph-loop-state.ps1')

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try { $evt = $raw | ConvertFrom-Json } catch { exit 0 }

if ($evt.reason -and [string]$evt.reason -ne 'end_turn') { exit 0 }
if ($evt.stopHookActive -eq $true) { exit 0 }
if (Test-KaIsExpertEvent $evt) { exit 0 }

function Emit-Block([string]$reason) {
  $payload = @{ decision = 'block'; reason = $reason } | ConvertTo-Json -Compress
  [Console]::Out.Write($payload)
  exit 0
}

function Test-KaClaimsDone([string]$msg) {
  if ([string]::IsNullOrWhiteSpace($msg)) { return $false }
  if ($msg -match '(?i)\b(not\s+implemented|unfixed|bypassed)\b') { return $false }
  if ($msg -match '(?i)\bdone\.') { return $true }
  if ($msg -match '(?i)\bbuild succeeded\b') { return $true }
  if ($msg -match '(?i)(?<!un)(?<!not\s)\b(fixed|passed|implemented)\b') { return $true }
  $needles = @(
    (-join [char[]](0xC644, 0xB8CC)),                          # wan-ryo
    (-join [char[]](0xACE0, 0xCCD0)),                          # go-chyeot
    (-join [char[]](0xC218, 0xC815, 0xD588)),                  # su-jeong-haet
    (-join [char[]](0xD1B5, 0xACFC)),                          # tong-gwa
    (-join [char[]](0xAD6C, 0xD604, 0xD588)),                  # gu-hyeon-haet
    ((-join [char[]](0xBE4C, 0xB4DC)) + ' ' + (-join [char[]](0xC131, 0xACF5)))  # build seong-gong
  )
  foreach ($n in $needles) {
    if ($msg.Contains($n)) { return $true }
  }
  return $false
}

$tasks = @()
if ($evt.backgroundTasks) { $tasks = @($evt.backgroundTasks) }
if ($evt.background_tasks) { $tasks = @($evt.background_tasks) }
$busy = $tasks | Where-Object {
  $_.status -match 'running|pending|in_progress|active' -or
  ($_.type -eq 'subagent' -and $_.status -notmatch 'completed|failed|cancelled|done|success|succeeded|ok')
}
if ($busy -and @($busy).Count -gt 0) {
  $names = (@($busy) | ForEach-Object {
    if ($_.description) { $_.description } else { $_.type }
  }) -join '; '
  Emit-Block "GRAPH ENGINEERING: background experts still running ($names). Wait, merge findings, then finish."
}

$msg = [string]$evt.lastAssistantMessage
if (-not $msg) { $msg = [string]$evt.last_assistant_message }
$writes = @(Get-KaTurnSrcWrites)
$claimsDone = Test-KaClaimsDone $msg

# GRAPH: this turn edited both app and core with no known expert/workflow spawn.
if ((Test-KaTurnTouchesAppAndCore) -and -not (Test-KaKnownGraphSpawn)) {
  Emit-Block @"
GRAPH ENGINEERING: this turn edited src/app and src/core with no spawned expert (ka-scout / ka-implementer / workflow).
FEATURE must spawn the ship graph this turn, then ka-reviewer and ka-tester. QUICK is one module only.
"@
}

# LOOP: only this-turn product src, not leftover git dirty from another session.
# Stamp only — quoting "ctest" in the reply is not evidence.
if ($writes.Count -eq 0) { exit 0 }
if (-not $claimsDone) { exit 0 }

$stampOk = Test-KaVerifyStampNewerThanWrites
if ($stampOk) { exit 0 }

Emit-Block @"
LOOP ENGINEERING: this turn wrote product C++/tests and the reply claims done/fixed, but there is no newer cmake/ctest/smoke stamp.
Run cmake --build build --config Release and ctest (or /ka-hgis-verify / ka-tester), quote exit codes, then finish.
Pre-existing dirty files from another turn do not count; docs/hooks-only turns do not need cmake.
"@
