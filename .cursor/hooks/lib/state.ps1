# Cursor hook state for ka-hgis auto review.
# ASCII-only so Windows PowerShell 5.1 parses it regardless of console CP.
$ErrorActionPreference = 'Continue'

function Get-KaCursorRepoRoot {
  $here = $PSScriptRoot
  if ($here) {
    $cand = Join-Path $here '..\..\..'
    try { return (Resolve-Path -LiteralPath $cand).Path } catch { }
  }
  return (Get-Location).Path
}

function Get-KaCursorStateDir {
  $dir = Join-Path (Get-KaCursorRepoRoot) '.cursor\.state'
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  return $dir
}

function Get-KaCursorWritesFile { Join-Path (Get-KaCursorStateDir) 'turn-src-writes.txt' }
function Get-KaCursorPendingFile { Join-Path (Get-KaCursorStateDir) 'review-pending' }
function Get-KaCursorSecretFile { Join-Path (Get-KaCursorStateDir) 'secret-flags.txt' }

function Reset-KaCursorTurnState {
  $dir = Get-KaCursorStateDir
  foreach ($name in @('turn-src-writes.txt', 'review-pending', 'secret-flags.txt')) {
    $p = Join-Path $dir $name
    if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue }
  }
}

function Test-KaCursorProductSrcPath([string]$path) {
  if ([string]::IsNullOrWhiteSpace($path)) { return $false }
  $n = $path -replace '\\', '/'
  if ($n -match '(^|/)(\.cursor|\.grok|docs|build|dist)/') { return $false }
  if ($n -match '(^|/)CMakeLists\.txt$') { return $true }
  if ($n -match '\.qss$') { return $true }
  if ($n -match '(^|/)src/') { return $true }
  if ($n -match '(^|/)tests/') { return $true }
  return $false
}

function Add-KaCursorSrcWrite([string]$path) {
  if (-not (Test-KaCursorProductSrcPath $path)) { return $false }
  $n = ($path -replace '\\', '/').Trim()
  $file = Get-KaCursorWritesFile
  $existing = @()
  if (Test-Path -LiteralPath $file) {
    $existing = @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
  }
  if ($existing -notcontains $n) {
    Add-Content -LiteralPath $file -Value $n -Encoding ascii
  }
  Set-Content -Path (Get-KaCursorPendingFile) -Value '1' -Encoding ascii
  return $true
}

function Test-KaCursorReviewPending {
  $p = Get-KaCursorPendingFile
  if (-not (Test-Path -LiteralPath $p)) { return $false }
  $v = (Get-Content -LiteralPath $p -ErrorAction SilentlyContinue | Select-Object -First 1)
  return ($v -eq '1')
}

function Clear-KaCursorReviewPending {
  $p = Get-KaCursorPendingFile
  if (Test-Path -LiteralPath $p) {
    Set-Content -Path $p -Value '0' -Encoding ascii
  }
}

function Get-KaCursorSrcWrites {
  $file = Get-KaCursorWritesFile
  if (-not (Test-Path -LiteralPath $file)) { return @() }
  return @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
}

function Test-KaCursorSecretText([string]$text) {
  if ([string]::IsNullOrWhiteSpace($text)) { return $false }
  if ($text -match '(?i)BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY') { return $true }
  if ($text -match '(?i)AKIA[0-9A-Z]{16}') { return $true }
  if ($text -match '(?i)(api[_-]?key|secret|token|password)\s*[:=]\s*[''"][^''"]{12,}') { return $true }
  if ($text -match '(?i)vworld.{0,40}[A-Za-z0-9]{20,}') { return $true }
  return $false
}

function Add-KaCursorSecretFlag([string]$path) {
  $n = ($path -replace '\\', '/').Trim()
  Add-Content -LiteralPath (Get-KaCursorSecretFile) -Value $n -Encoding ascii
}

function Get-KaCursorSecretFlags {
  $file = Get-KaCursorSecretFile
  if (-not (Test-Path -LiteralPath $file)) { return @() }
  return @(Get-Content -LiteralPath $file -ErrorAction SilentlyContinue | Where-Object { $_ })
}

function Get-KaCursorStdinJson {
  try { [Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false) } catch { }
  $raw = [Console]::In.ReadToEnd()
  if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
  try { return ($raw | ConvertFrom-Json) } catch { return $null }
}

function Write-KaCursorJson($obj) {
  $json = $obj | ConvertTo-Json -Compress
  [Console]::Out.Write($json)
}
