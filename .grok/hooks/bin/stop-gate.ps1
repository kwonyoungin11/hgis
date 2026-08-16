$ErrorActionPreference = "Continue"
$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }

try { $evt = $raw | ConvertFrom-Json } catch { exit 0 }

# Only genuine turn ends. Session-end / cancel must not loop.
if ([string]$evt.reason -ne "end_turn") { exit 0 }
if ($evt.stopHookActive -eq $true) { exit 0 }
if ($evt.subagentType) { exit 0 }

$root = $env:GROK_WORKSPACE_ROOT
if (-not $root) { $root = $env:CLAUDE_PROJECT_DIR }
if (-not $root) { exit 0 }

Push-Location $root
try {
    $dirty = git diff --name-only HEAD -- src tests CMakeLists.txt 2>$null
} catch {
    $dirty = @()
}
Pop-Location

$srcHits = @($dirty | Where-Object { $_ -match '\.(cpp|h|hpp)$' -or $_ -eq 'CMakeLists.txt' })
if ($srcHits.Count -eq 0) { exit 0 }

$msg = [string]$evt.lastAssistantMessage
if ([string]::IsNullOrWhiteSpace($msg)) { exit 0 }

# Only nudge when the model is claiming done, not when it is asking or planning.
if ($msg -notmatch '완료|고쳤|수정했|통과|fixed|passed|done\.|구현했|빌드 성공') { exit 0 }

$stamp = Join-Path $root ".grok\.state\last-verify"
if (Test-Path $stamp) {
    $stampTime = (Get-Item $stamp).LastWriteTimeUtc
    $newerSrc = $false
    foreach ($rel in $srcHits) {
        $p = Join-Path $root $rel
        if (Test-Path $p) {
            if ((Get-Item $p).LastWriteTimeUtc -gt $stampTime) { $newerSrc = $true }
        } else {
            $newerSrc = $true
        }
    }
    if (-not $newerSrc) { exit 0 }
}

$reason = "LOOP ENGINEERING: src/ C++ changed and the reply claims 완료/fixed, but there is no newer cmake/ctest/smoke stamp. Run cmake --build build --config Release and ctest (or /ka-hgis-verify), then finish."
$payload = @{ decision = "block"; reason = $reason } | ConvertTo-Json -Compress
Write-Output $payload
exit 0
