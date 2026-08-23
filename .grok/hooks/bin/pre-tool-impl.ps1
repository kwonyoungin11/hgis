$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'graph-loop-state.ps1')

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) {
    Write-Output '{"decision":"allow"}'
    exit 0
}

try {
    $evt = $raw | ConvertFrom-Json
} catch {
    Write-Output '{"decision":"allow"}'
    exit 0
}

function Deny([string]$reason) {
    $o = @{ decision = "deny"; reason = $reason } | ConvertTo-Json -Compress
    Write-Output $o
    exit 2
}

$tool = Get-KaToolName $evt
if (-not $tool) { $tool = [string]$evt.toolName }
$inputObj = Get-KaToolInput $evt
if ($null -eq $inputObj) { $inputObj = $evt.toolInput }
$blob = ""
if ($null -ne $inputObj) {
    $blob = ($inputObj | ConvertTo-Json -Compress -Depth 8)
}

$bannedVendor = '.' + 'agents'
$bannedJson = 'opencode' + '.json'
$bannedTrail = 'sisy' + 'phus'
if ($blob.Contains($bannedJson) -or $blob.Contains($bannedVendor) -or $blob.Contains($bannedTrail)) {
    Deny "Blocked OpenCode / vendor agent-dir / trailers. Use Grok-native .grok/ only."
}

$secretName = 'secrets' + '.ini'
if ($blob.Contains($secretName) -and $tool -match '(?i)search_replace|write|edit') {
    Deny "Do not write local API-key ini (VWorld keys stay in VworldSettings / local secrets)."
}

$path = ""
if ($null -ne $inputObj) {
    foreach ($k in @('file_path', 'filePath', 'path', 'target_file', 'targetFile')) {
        if ($inputObj.PSObject.Properties.Name -contains $k) {
            $path = [string]$inputObj.$k
            if ($path) { break }
        }
    }
}
$norm = ($path -replace '\\', '/')
$wipeFn = 'removeAllMap' + 'Layers'
if ($norm -match '(^|/)src/(app|core)/' -and $blob.Contains($wipeFn + '(')) {
    Deny "Blocked map-layer wipe in src/app or src/core. loadSurveyLayers must drop domain layers only."
}

$cmd = ""
if ($inputObj -and $inputObj.command) { $cmd = [string]$inputObj.command }
if ($cmd -match 'rm\s+-rf\s+(/|\*)' -or $cmd -match 'Remove-Item\s+-Recurse\s+-Force\s+C:\\') {
    Deny "Blocked destructive filesystem command."
}
if ($cmd -match '\bgit(\.exe)?\b' -and $cmd -match '\bcommit\b') {
    Deny "Do not git commit unless the user asked. After an explicit request use .\scripts\commit.ps1."
}
if ($cmd -match '\bgit(\.exe)?\b' -and $cmd -match '\bpush\b' -and $cmd -match '(--force|-f)\b') {
    Deny "Never force-push."
}
if ($cmd -match '\breset\s+--hard\b') {
    Deny "Do not git reset --hard. Uncommitted files must not be discarded."
}

Write-Output '{"decision":"allow"}'
exit 0
