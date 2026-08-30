# Shared this-turn graph/loop state for ka-hgis hooks.
# Turn files live in .grok/.state/ (gitignored). last-verify is cross-turn.

$ErrorActionPreference = 'Continue'

function Get-KaRepoRoot {
  $root = $env:GROK_WORKSPACE_ROOT
  if (-not $root) { $root = $env:CLAUDE_PROJECT_DIR }
  if (-not $root) {
    $here = $PSScriptRoot
    if ($here) {
      $cand = Join-Path $here '..\..\..'
      try { $root = (Resolve-Path -LiteralPath $cand).Path } catch { $root = '' }
    }
  }
  if (-not $root) { $root = (Get-Location).Path }
  return $root
}

function Get-KaStateDir {
  $dir = Join-Path (Get-KaRepoRoot) '.grok\.state'
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  return $dir
}

function Get-KaTurnSrcFile { Join-Path (Get-KaStateDir) 'turn-src-writes.txt' }
function Get-KaTurnGraphFile { Join-Path (Get-KaStateDir) 'turn-graph.txt' }
function Get-KaVerifyStampFile { Join-Path (Get-KaStateDir) 'last-verify' }

function Reset-KaTurnState {
  $src = Get-KaTurnSrcFile
  $graph = Get-KaTurnGraphFile
  Set-Content -Path $src -Value '' -Encoding ascii
  Set-Content -Path $graph -Value '' -Encoding ascii
}

function Test-KaProductSrcPath([string]$path) {
  if ([string]::IsNullOrWhiteSpace($path)) { return $false }
  $n = $path -replace '\\', '/'
  if ($n -match '(^|/)CMakeLists\.txt$') { return $true }
  if ($n -match '\.qss$') { return $true }
  if ($n -match '(^|/)src/') { return $true }
  if ($n -match '(^|/)tests/') { return $true }
  return $false
}

function Add-KaTurnSrcWrite([string]$path) {
  if (-not (Test-KaProductSrcPath $path)) { return }
  $n = ($path -replace '\\', '/').Trim()
  $file = Get-KaTurnSrcFile
  $existing = @()
  if (Test-Path -LiteralPath $file) {
    $existing = @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
  }
  if ($existing -contains $n) {
    if (Test-Path -LiteralPath $file) {
      (Get-Item -LiteralPath $file).LastWriteTimeUtc = [datetime]::UtcNow.AddSeconds(1)
    }
    return
  }
  Add-Content -LiteralPath $file -Value $n -Encoding ascii
}

function Add-KaTurnGraph([string]$kind) {
  if ([string]::IsNullOrWhiteSpace($kind)) { return }
  $k = $kind.Trim().ToLowerInvariant()
  $file = Get-KaTurnGraphFile
  $existing = @()
  if (Test-Path -LiteralPath $file) {
    $existing = @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
  }
  if ($existing -contains $k) { return }
  Add-Content -LiteralPath $file -Value $k -Encoding ascii
}

function Get-KaTurnSrcWrites {
  $file = Get-KaTurnSrcFile
  if (-not (Test-Path -LiteralPath $file)) { return @() }
  return @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
}

function Get-KaTurnGraph {
  $file = Get-KaTurnGraphFile
  if (-not (Test-Path -LiteralPath $file)) { return @() }
  return @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
}

function Set-KaVerifyStamp {
  $stamp = Get-KaVerifyStampFile
  Get-Date -Format o | Set-Content -LiteralPath $stamp -Encoding ascii
}

function Test-KaVerifyStampNewerThanWrites {
  $writes = Get-KaTurnSrcWrites
  if ($writes.Count -eq 0) { return $true }
  $stamp = Get-KaVerifyStampFile
  if (-not (Test-Path -LiteralPath $stamp)) { return $false }
  $stampTime = (Get-Item -LiteralPath $stamp).LastWriteTimeUtc
  $log = Get-KaTurnSrcFile
  if (Test-Path -LiteralPath $log) {
    if ((Get-Item -LiteralPath $log).LastWriteTimeUtc -gt $stampTime) { return $false }
  }
  return $true
}

function Test-KaTurnHasTestWrite {
  foreach ($w in @(Get-KaTurnSrcWrites)) {
    $n = $w -replace '\\', '/'
    if ($n -match '(^|/)tests/') { return $true }
  }
  return $false
}

function Test-KaTurnTouchesProductCpp {
  foreach ($w in @(Get-KaTurnSrcWrites)) {
    $n = $w -replace '\\', '/'
    if ($n -match '(^|/)src/(app|core)/') { return $true }
  }
  return $false
}

function Test-KaTurnTouchesAppAndCore {
  $writes = Get-KaTurnSrcWrites
  $app = $false
  $core = $false
  foreach ($w in $writes) {
    $n = $w -replace '\\', '/'
    if ($n -match '(^|/)src/app/') { $app = $true }
    if ($n -match '(^|/)src/core/') { $core = $true }
  }
  return ($app -and $core)
}

function Test-KaIsExpertEvent($evt) {
  if ($null -eq $evt) { return $false }
  foreach ($k in @('subagentType', 'subagent_type', 'agentType', 'agent_type')) {
    $v = Get-KaNamed $evt $k
    if ($null -ne $v -and [string]$v -ne '') { return $true }
  }
  return $false
}

function Test-KaKnownGraphSpawn {
  $graph = @(Get-KaTurnGraph)
  foreach ($g in $graph) {
    $n = [string]$g
    if ($n -match '^(ka-scout|ka-implementer|ka-reviewer|ka-tester|ka-debugger|ka-architect|qgis-api|qgis-expert|gis-protocol|field-check|arcgis-expert|ka-developer|ka-color|ka-symbol|ka-ui|ka-ux|designer|document-specialist|workflow|ka-ship|ka-council|ka-verify)$') {
      return $true
    }
  }
  return $false
}

function Get-KaToolName($evt) {
  foreach ($k in @('toolName', 'tool_name', 'name')) {
    if ($evt.PSObject.Properties.Name -contains $k) {
      $v = [string]$evt.$k
      if ($v) {
        if ($v -match '[./]') { return ($v -split '[./]')[-1] }
        return $v
      }
    }
  }
  return ''
}

function Get-KaToolInput($evt) {
  foreach ($k in @('toolInput', 'tool_input', 'input')) {
    if ($evt.PSObject.Properties.Name -contains $k -and $null -ne $evt.$k) {
      return $evt.$k
    }
  }
  return $null
}

function Get-KaNamed($obj, [string]$name) {
  if ($null -eq $obj) { return $null }
  if ($obj -is [System.Collections.IDictionary]) {
    foreach ($key in @($obj.Keys)) {
      if ([string]$key -eq $name) { return $obj[$key] }
    }
    return $null
  }
  $prop = $obj.PSObject.Properties[$name]
  if ($prop) { return $prop.Value }
  return $null
}

function Get-KaToolExitCode($evt) {
  if ($null -eq $evt) { return $null }
  foreach ($k in @('exitCode', 'exit_code')) {
    $v = Get-KaNamed $evt $k
    if ($null -ne $v -and [string]$v -ne '') {
      try { return [int]$v } catch { }
    }
  }
  foreach ($k in @('toolResult', 'tool_result', 'toolResponse', 'tool_response')) {
    $tr = Get-KaNamed $evt $k
    if ($null -eq $tr) { continue }
    if ($tr -is [int] -or $tr -is [long] -or $tr -is [decimal]) { return [int]$tr }
    if ($tr -is [string]) {
      if ($tr -match '(?i)exit(?:[_\s]?code)["\s:=]+(\d+)') { return [int]$Matches[1] }
      continue
    }
    foreach ($ek in @('exitCode', 'exit_code', 'code')) {
      $v = Get-KaNamed $tr $ek
      if ($null -ne $v -and [string]$v -ne '') {
        try { return [int]$v } catch { }
      }
    }
    $o = Get-KaNamed $tr 'output'
    if ($null -ne $o) {
      $s = [string]$o
      if ($s -match '(?i)exit(?:[_\s]?code)["\s:=]+(\d+)') { return [int]$Matches[1] }
    }
  }
  return $null
}
