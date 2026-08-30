# Self-check for graph/loop stop-gate. Does not touch product src.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File .grok/hooks/bin/self-check-graph-loop.ps1
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
. (Join-Path $here 'graph-loop-state.ps1')
$gate = Join-Path $here 'stop-gate.ps1'
$root = Get-KaRepoRoot
$env:GROK_WORKSPACE_ROOT = $root

$state = Get-KaStateDir
$hadStamp = Test-Path -LiteralPath (Get-KaVerifyStampFile)
$bakSrc = Join-Path $state 'turn-src-writes.bak'
$bakGraph = Join-Path $state 'turn-graph.bak'
$bakStamp = Join-Path $state 'last-verify.bak'
foreach ($pair in @(
    @{ src = (Get-KaTurnSrcFile); bak = $bakSrc },
    @{ src = (Get-KaTurnGraphFile); bak = $bakGraph },
    @{ src = (Get-KaVerifyStampFile); bak = $bakStamp }
  )) {
  if (Test-Path -LiteralPath $pair.src) {
    Copy-Item -LiteralPath $pair.src -Destination $pair.bak -Force
  } elseif (Test-Path -LiteralPath $pair.bak) {
    Remove-Item -LiteralPath $pair.bak -Force
  }
}

function Invoke-StopGate([hashtable]$evt) {
  $json = $evt | ConvertTo-Json -Compress -Depth 6
  $tmp = Join-Path $state ("stop-gate-in-{0}.json" -f [guid]::NewGuid().ToString('n'))
  $utf8 = New-Object System.Text.UTF8Encoding $false
  [System.IO.File]::WriteAllText($tmp, $json, $utf8)
  try {
    $raw = [System.IO.File]::ReadAllText($tmp, $utf8)
    $oldOut = [Console]::OutputEncoding
    [Console]::OutputEncoding = $utf8
    $script:OutputEncoding = $utf8
    try {
      $out = $raw | & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $gate 2>$null
    } finally {
      [Console]::OutputEncoding = $oldOut
    }
    return [string]$out
  } finally {
    Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
  }
}

function Assert-Allow([string]$name, [string]$out) {
  if ($out -match '"decision"\s*:\s*"block"') {
    throw "FAIL $name : expected allow, got block: $out"
  }
  Write-Host "PASS $name (allow)"
}

function Assert-Block([string]$name, [string]$out, [string]$needle) {
  if ($out -notmatch '"decision"\s*:\s*"block"') {
    throw "FAIL $name : expected block, got: $out"
  }
  if ($needle -and $out -notmatch [regex]::Escape($needle)) {
    throw "FAIL $name : block missing '$needle': $out"
  }
  Write-Host "PASS $name (block $needle)"
}

$failed = 0
try {
  Reset-KaTurnState
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    stopHookActive = $false
    lastAssistantMessage = '단면도는 이미 완료. 다시 만들지 말 것.'
  }
  Assert-Allow 'research-done-word-no-this-turn-writes' $out

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  Add-KaTurnSrcWrite 'src/core/LayerOps.cpp'
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = '구현했습니다.'
  }
  Assert-Block 'feature-app-core-no-spawn' $out 'GRAPH ENGINEERING'

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  Add-KaTurnSrcWrite 'tests/test_workflow.cpp'
  Add-KaTurnGraph 'ka-scout'
  # ASCII claim verbs — nested powershell stdin must not depend on console CP949.
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'Change is done. Need to finish.'
  }
  Assert-Block 'this-turn-src-done-no-stamp' $out 'LOOP ENGINEERING'

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  Add-KaTurnSrcWrite 'tests/test_workflow.cpp'
  Add-KaTurnGraph 'ka-tester'
  Set-KaVerifyStamp
  Start-Sleep -Milliseconds 20
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'Change is done. Need to finish.'
  }
  Assert-Allow 'this-turn-src-with-stamp' $out

  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'Change is done. Need to finish.'
  }
  Assert-Block 'rewrite-after-stamp-needs-new-verify' $out 'LOOP ENGINEERING'

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  Add-KaTurnSrcWrite 'src/core/LayerOps.cpp'
  Add-KaTurnGraph 'explore'
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'Change is done. Need to finish.'
  }
  Assert-Block 'explore-is-not-ship-graph' $out 'GRAPH ENGINEERING'

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    subagent_type = 'ka-implementer'
    lastAssistantMessage = 'Change is done. Need to finish.'
  }
  Assert-Allow 'snake-case-expert-skips-parent-loop' $out

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  Add-KaTurnSrcWrite 'tests/test_workflow.cpp'
  $koDone = -join [char[]](0xC644, 0xB8CC, 0xD588, 0xC2B5, 0xB2C8, 0xB2E4)  # wan-ryo-haet-seum-ni-da
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = $koDone
  }
  Assert-Block 'korean-done-without-stamp' $out 'LOOP ENGINEERING'

  Reset-KaTurnState
  Add-KaTurnSrcWrite 'src/app/MainWindow.cpp'
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'This is not implemented and remains unfixed.'
  }
  Assert-Allow 'negated-implemented-is-not-done' $out

  Reset-KaTurnState
  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'working'
    backgroundTasks = @(@{ type = 'subagent'; status = 'running'; description = 'ka-scout' })
  }
  Assert-Block 'wait-for-experts' $out 'background experts'

  $out = Invoke-StopGate @{
    reason = 'end_turn'
    lastAssistantMessage = 'working'
    backgroundTasks = @(@{ type = 'subagent'; status = 'success'; description = 'ka-scout' })
  }
  Assert-Allow 'success-status-is-not-busy' $out

  $pre = Join-Path $here 'pre-tool.ps1'
  function Invoke-PreTool([hashtable]$evt) {
    $json = $evt | ConvertTo-Json -Compress -Depth 6
    $tmp = Join-Path $state ("pre-tool-in-{0}.json" -f [guid]::NewGuid().ToString('n'))
    [System.IO.File]::WriteAllText($tmp, $json, [System.Text.UTF8Encoding]::new($false))
    try {
      $out = Get-Content -LiteralPath $tmp -Raw -Encoding UTF8 | & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $pre 2>$null
      return [string]$out
    } finally {
      Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
  }
  function Assert-Deny([string]$name, [string]$out, [string]$needle) {
    if ($out -notmatch '"decision"\s*:\s*"deny"') {
      throw "FAIL $name : expected deny, got: $out"
    }
    if ($needle -and $out -notmatch [regex]::Escape($needle)) {
      throw "FAIL $name : deny missing '$needle': $out"
    }
    Write-Host "PASS $name (deny $needle)"
  }
  $out = Invoke-PreTool @{
    toolName = 'run_terminal_command'
    toolInput = @{ command = 'git status' }
  }
  Assert-Allow 'pre-git-status' $out
  $out = Invoke-PreTool @{
    toolName = 'run_terminal_command'
    toolInput = @{ command = 'git commit -m x' }
  }
  Assert-Deny 'pre-git-commit' $out 'commit'
  $out = Invoke-PreTool @{
    toolName = 'run_terminal_command'
    toolInput = @{ command = 'git push origin HEAD' }
  }
  Assert-Allow 'pre-git-push' $out
  $out = Invoke-PreTool @{
    toolName = 'run_terminal_command'
    toolInput = @{ command = 'git push --force origin HEAD' }
  }
  Assert-Deny 'pre-git-force-push' $out 'force'
  $out = Invoke-PreTool @{
    toolName = 'search_replace'
    toolInput = @{ file_path = 'src/core/LayerOps.cpp'; new_string = 'proj->removeAllMapLayers();' }
  }
  Assert-Deny 'pre-wipe-layers' $out 'domain layers'

  $code0 = Get-KaToolExitCode ('{"exitCode":0}' | ConvertFrom-Json)
  $code1 = Get-KaToolExitCode ('{"exitCode":1}' | ConvertFrom-Json)
  $codeN = Get-KaToolExitCode ('{"toolResult":"no code here"}' | ConvertFrom-Json)
  if ($null -eq $code0 -or $code0 -ne 0) { throw "FAIL exit-code-0: $code0" }
  if ($null -eq $code1 -or $code1 -ne 1) { throw "FAIL exit-code-1: $code1" }
  if ($null -ne $codeN) { throw "FAIL exit-code-null: $codeN" }
  Write-Host 'PASS Get-KaToolExitCode'
}
catch {
  Write-Host $_
  $failed = 1
}
finally {
  foreach ($pair in @(
      @{ src = (Get-KaTurnSrcFile); bak = $bakSrc },
      @{ src = (Get-KaTurnGraphFile); bak = $bakGraph },
      @{ src = (Get-KaVerifyStampFile); bak = $bakStamp }
    )) {
    if (Test-Path -LiteralPath $pair.bak) {
      Copy-Item -LiteralPath $pair.bak -Destination $pair.src -Force
      Remove-Item -LiteralPath $pair.bak -Force
    } elseif (-not $hadStamp -and $pair.src -eq (Get-KaVerifyStampFile) -and (Test-Path -LiteralPath $pair.src)) {
      Remove-Item -LiteralPath $pair.src -Force
    }
  }
}

if ($failed -ne 0) { exit 1 }
Write-Host 'self-check-graph-loop: all PASS'
exit 0
