$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\state.ps1')

$evt = Get-KaCursorStdinJson
if (-not $evt) { exit 0 }

$status = [string]$evt.status
if ($status -and $status -ne 'completed') { exit 0 }

$loop = 0
if ($null -ne $evt.loop_count) { $loop = [int]$evt.loop_count }
if ($loop -ge 3) { exit 0 }

if (-not (Test-KaCursorReviewPending)) { exit 0 }

$writes = @(Get-KaCursorSrcWrites)
if ($writes.Count -eq 0) {
  Clear-KaCursorReviewPending
  exit 0
}

Clear-KaCursorReviewPending

$root = (Get-KaCursorRepoRoot) -replace '\\', '/'
$list = ($writes | Select-Object -First 12) -join ', '
$secrets = @(Get-KaCursorSecretFlags)
$secretNote = ''
if ($secrets.Count -gt 0) {
  $secretNote = ' Secret-scan flagged: ' + (($secrets | Select-Object -First 6) -join ', ') + '.'
}

$msg = @"
AUTO security review. Do not ask which review to run. Product source was written this turn ($list). Launch exactly one Task subagent now:
subagent_type: security-review
description: Security Review
run_in_background: false

Full Repository Path: $root
Diff: uncommitted changes
Custom Instructions: Review this-turn product edits only. Do not fix findings.$secretNote

After it finishes, print Severity | Location | Finding (or no issues) and stop. Do not start a second review.
"@

Write-KaCursorJson @{ followup_message = $msg }
exit 0
