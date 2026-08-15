$ErrorActionPreference = "Stop"
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

$tool = [string]$evt.toolName
$inputObj = $evt.toolInput
$blob = ""
if ($null -ne $inputObj) {
    $blob = ($inputObj | ConvertTo-Json -Compress -Depth 8)
}

if ($blob -match 'opencode\.json' -or $blob -match '[\\/]\.agents[\\/]' -or $blob -match 'sisyphus') {
    Deny "Blocked OpenCode / .agents / Sisyphus path. Use Grok-native .grok/ only."
}

if ($blob -match 'secrets\.ini' -and $tool -match 'search_replace|write|Write') {
    Deny "Do not write config/secrets.ini (VWorld keys stay in VworldSettings / local secrets)."
}

$cmd = ""
if ($inputObj -and $inputObj.command) { $cmd = [string]$inputObj.command }
if ($cmd -match 'rm\s+-rf\s+(/|\*)' -or $cmd -match 'Remove-Item\s+-Recurse\s+-Force\s+C:\\') {
    Deny "Blocked destructive filesystem command."
}

Write-Output '{"decision":"allow"}'
exit 0
