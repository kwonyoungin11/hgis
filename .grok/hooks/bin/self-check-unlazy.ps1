# Self-check: unlazy is a first-class Grok skill + Stop hook in this repo.
# Does not touch product src. Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File .grok/hooks/bin/self-check-unlazy.ps1
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..')).Path
$skillDir = Join-Path $repo '.grok\skills\unlazy'
$stopHookJs = Join-Path $skillDir 'scripts\stop-hook.mjs'
$wrapper = Join-Path $here 'unlazy-stop.ps1'
$hookJson = Join-Path $repo '.grok\hooks\ka-hgis.json'
$skillMd = Join-Path $skillDir 'SKILL.md'

$failed = 0
function Fail([string]$name, [string]$detail) {
  $script:failed++
  Write-Host "FAIL $name : $detail"
}
function Pass([string]$name) { Write-Host "PASS $name" }

if (-not (Test-Path -LiteralPath $stopHookJs)) {
  Fail 'skill-scripts' "missing $stopHookJs"
} else { Pass 'skill-scripts' }

if (-not (Test-Path -LiteralPath $skillMd)) {
  Fail 'skill-md' "missing $skillMd"
} else {
  $md = Get-Content -LiteralPath $skillMd -Raw
  if ($md -notmatch '(?m)^name:\s*unlazy\s*$') { Fail 'skill-name' 'SKILL.md frontmatter name is not unlazy' }
  else { Pass 'skill-name' }
  if ($md -match 'install-hooks\.mjs' -and $md -notmatch '(?i)do not run install-hooks') {
    Fail 'no-claude-install' 'SKILL.md still tells the agent to install Claude Code hooks'
  } else { Pass 'no-claude-install' }
  if ($md -notmatch 'Grok') { Fail 'grok-native' 'SKILL.md does not mention Grok' }
  else { Pass 'grok-native' }
}

if (-not (Test-Path -LiteralPath $wrapper)) {
  Fail 'wrapper' "missing $wrapper"
} else { Pass 'wrapper' }

if (-not (Test-Path -LiteralPath $hookJson)) {
  Fail 'hook-json' "missing $hookJson"
} else {
  $jsonText = Get-Content -LiteralPath $hookJson -Raw
  if ($jsonText -notmatch 'unlazy-stop') { Fail 'hook-registered' 'ka-hgis.json Stop does not call unlazy-stop' }
  else { Pass 'hook-registered' }
  if ($jsonText -match '\.claude\\settings') { Fail 'no-claude-settings' 'ka-hgis.json still points at .claude/settings' }
  else { Pass 'no-claude-settings' }
}

$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) {
  Fail 'node' 'node is not on PATH (unlazy gate-check and Stop hook need Node >= 16)'
} elseif (-not (Test-Path -LiteralPath $stopHookJs)) {
  Fail 'stop-hook-behavior' 'cannot run stop-hook.mjs (skill missing)'
} else {
  $savedScope = $env:UNLAZY_SCOPE
  $savedWs = $env:GROK_WORKSPACE_ROOT
  Remove-Item Env:UNLAZY_SCOPE -ErrorAction SilentlyContinue
  Remove-Item Env:GROK_WORKSPACE_ROOT -ErrorAction SilentlyContinue
  $tmp = Join-Path $repo ('.grok\.state\unlazy-self-check-' + [guid]::NewGuid().ToString('n'))
  $tmp = [System.IO.Path]::GetFullPath($tmp)
  New-Item -ItemType Directory -Path $tmp | Out-Null
  try {
    function Invoke-ProcessStdin([string]$fileName, [string]$arguments, [string]$cwd, [string]$stdin) {
      $psi = New-Object System.Diagnostics.ProcessStartInfo
      $psi.FileName = $fileName
      $psi.Arguments = $arguments
      $psi.WorkingDirectory = $cwd
      $psi.UseShellExecute = $false
      $psi.RedirectStandardInput = $true
      $psi.RedirectStandardOutput = $true
      $psi.RedirectStandardError = $true
      $psi.CreateNoWindow = $true
      $p = [System.Diagnostics.Process]::Start($psi)
      $utf8NoBom = New-Object System.Text.UTF8Encoding $false
      $bytes = $utf8NoBom.GetBytes($stdin)
      $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
      $p.StandardInput.BaseStream.Flush()
      $p.StandardInput.Close()
      $stdout = $p.StandardOutput.ReadToEnd()
      $script:LastHookErr = [string]$p.StandardError.ReadToEnd()
      $p.WaitForExit()
      $script:LastHookCode = $p.ExitCode
      return [string]$stdout
    }

    function Invoke-UnlazyStop([string]$cwd, [string]$sessionId) {
      $payload = @{
        cwd = $cwd
        workspaceRoot = $cwd
        sessionId = $sessionId
        hookEventName = 'stop'
      } | ConvertTo-Json -Compress
      $nodeExe = [string]$node.Source
      $script:LastHookPayload = [string]$payload
      return Invoke-ProcessStdin $nodeExe ("`"" + $stopHookJs + "`"") $cwd $payload
    }

    $empty = Invoke-UnlazyStop $tmp 'self-check-session'
    if ($empty -match '"decision"\s*:\s*"block"') {
      Fail 'empty-allow' "expected allow with no pipeline, got: $empty"
    } else { Pass 'empty-allow' }

    $gates = @"
# Gates: self-check

OWNS: tmp/**

- [ ] G1: unmet on purpose
  EVIDENCE: pending
"@
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText((Join-Path $tmp 'GATES.md'), $gates, $utf8)
    $blocked = Invoke-UnlazyStop $tmp 'self-check-session'
    if ($blocked -notmatch '"decision"\s*:\s*"block"') {
      Fail 'unmet-block' "expected block with unmet GATES.md, got: $blocked"
    } elseif ($blocked -notmatch 'unlazy') {
      Fail 'unmet-block' "block JSON missing unlazy reason: $blocked"
    } else { Pass 'unmet-block' }

    function Invoke-UnlazyWrapper([string]$cwd, [string]$sessionId) {
      $payload = @{
        cwd = $cwd
        workspaceRoot = $cwd
        sessionId = $sessionId
        hookEventName = 'stop'
        reason = 'end_turn'
      } | ConvertTo-Json -Compress
      $args = '-NoProfile -ExecutionPolicy Bypass -File "' + $wrapper + '"'
      return Invoke-ProcessStdin 'powershell.exe' $args $cwd $payload
    }

    $legacy = Invoke-UnlazyWrapper $tmp 'self-check-session'
    if ($legacy -match '"decision"\s*:\s*"block"') {
      Fail 'wrapper-legacy-allow' "root GATES.md must not arm Grok Stop, got: $legacy"
    } else { Pass 'wrapper-legacy-allow' }

    $scopeDir = Join-Path $tmp '.unlazy\ka-hgis'
    New-Item -ItemType Directory -Path $scopeDir -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $scopeDir 'GATES.md'), $gates, $utf8)
    $scoped = Invoke-UnlazyWrapper $tmp 'self-check-session'
    if ($scoped -notmatch '"decision"\s*:\s*"block"') {
      Fail 'wrapper-scope-block' "expected block with scoped .unlazy pipeline, got: $scoped"
    } else { Pass 'wrapper-scope-block' }

    Remove-Item -LiteralPath (Join-Path $tmp '.unlazy') -Recurse -Force
    $bogus = Join-Path $tmp '.unlazy\_not-a-scope'
    New-Item -ItemType Directory -Path $bogus -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $bogus 'GATES.md'), $gates, $utf8)
    $invalid = Invoke-UnlazyWrapper $tmp 'self-check-session'
    if ($invalid -match '"decision"\s*:\s*"block"') {
      Fail 'wrapper-invalid-scope-allow' "invalid .unlazy dir plus root GATES.md must not block, got: $invalid"
    } else { Pass 'wrapper-invalid-scope-allow' }

    Remove-Item -LiteralPath (Join-Path $tmp '.unlazy') -Recurse -Force -ErrorAction SilentlyContinue
    foreach ($id in @('ka-hgis', 'uiux-opt')) {
      $d = Join-Path $tmp ('.unlazy\' + $id)
      New-Item -ItemType Directory -Path $d -Force | Out-Null
      [System.IO.File]::WriteAllText((Join-Path $d 'GATES.md'), $gates, $utf8)
    }
    $ambiguous = Invoke-UnlazyWrapper $tmp 'self-check-session'
    if ($ambiguous -match '"decision"\s*:\s*"block"') {
      Fail 'wrapper-multi-allow' "unbound leftover scopes must not trap Stop, got: $ambiguous"
    } else { Pass 'wrapper-multi-allow' }

    [System.IO.File]::WriteAllText((Join-Path $tmp '.unlazy\ka-hgis\session'), "self-check-session`n", $utf8)
    $bound = Invoke-UnlazyWrapper $tmp 'self-check-session'
    if ($bound -notmatch '"decision"\s*:\s*"block"') {
      Fail 'wrapper-session-bind' "session-bound ka-hgis among leftovers must block, got: $bound"
    } else { Pass 'wrapper-session-bind' }

    Remove-Item -LiteralPath (Join-Path $tmp '.unlazy\ka-hgis\session') -Force
    $oldScope = $env:UNLAZY_SCOPE
    $env:UNLAZY_SCOPE = 'ka-hgis'
    try {
      $picked = Invoke-UnlazyWrapper $tmp 'self-check-session'
    } finally {
      if ($null -eq $oldScope) { Remove-Item Env:UNLAZY_SCOPE -ErrorAction SilentlyContinue }
      else { $env:UNLAZY_SCOPE = $oldScope }
    }
    if ($picked -notmatch '"decision"\s*:\s*"block"') {
      Fail 'wrapper-env-scope' "UNLAZY_SCOPE=ka-hgis among leftovers must block, got: $picked"
    } else { Pass 'wrapper-env-scope' }
  } finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    if ($null -eq $savedScope -or $savedScope -eq '') {
      Remove-Item Env:UNLAZY_SCOPE -ErrorAction SilentlyContinue
    } else {
      $env:UNLAZY_SCOPE = $savedScope
    }
    if ($null -eq $savedWs -or $savedWs -eq '') {
      Remove-Item Env:GROK_WORKSPACE_ROOT -ErrorAction SilentlyContinue
    } else {
      $env:GROK_WORKSPACE_ROOT = $savedWs
    }
  }
}

$grok = Get-Command grok -ErrorAction SilentlyContinue
if ($grok) {
  $inspectRaw = & grok inspect --json 2>$null
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace([string]$inspectRaw)) {
    Fail 'grok-inspect' 'grok inspect --json failed'
  } else {
    $blob = ($inspectRaw | Out-String)
    $vendorNeedle = 'agents' + [char]92 + 'skills' + [char]92 + 'unlazy'
    if ($blob -notmatch 'grok.{1,12}skills.{1,12}unlazy.{1,12}SKILL\.md') {
      Fail 'grok-inspect' 'inspect JSON missing .grok/skills/unlazy/SKILL.md'
    } elseif ($blob.Contains($vendorNeedle) -or $blob.Contains(('agents/skills/unlazy'))) {
      Fail 'grok-inspect' 'inspect JSON still lists the vendor-dir unlazy SKILL.md'
    } else { Pass 'grok-inspect' }
  }
} else {
  Write-Host 'SKIP grok-inspect (grok CLI not on PATH)'
}

if ($failed -gt 0) {
  Write-Host "self-check-unlazy FAILED ($failed)"
  exit 1
}
Write-Host 'self-check-unlazy ok'
exit 0
