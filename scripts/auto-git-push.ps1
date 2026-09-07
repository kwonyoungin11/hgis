# Auto Git Push Script for ka-hgis
# Periodically commits working-tree state into 'backup/auto-save' branch
# and pushes to remote GitHub repository without touching current working tree/index.

param(
  [string]$Remote = "origin",
  [string]$BackupBranch = "backup/auto-save"
)

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$LogDir = Join-Path $RepoRoot "logs"
if (-not (Test-Path $LogDir)) {
  New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}
$LogFile = Join-Path $LogDir "auto-git-push.log"

function Write-Log([string]$Message) {
  $time = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $line = "[$time] $Message"
  Write-Output $line
  try {
    Add-Content -Path $LogFile -Value $line -Encoding utf8
    # Trim log if over 100KB
    if ((Get-Item $LogFile).Length -gt 100000) {
      $lines = Get-Content $LogFile -Tail 200
      Set-Content -Path $LogFile -Value $lines -Encoding utf8
    }
  } catch {}
}

if (-not (Test-Path (Join-Path $RepoRoot ".git"))) {
  Write-Log "ERROR: Not a git repository: $RepoRoot"
  exit 1
}

$tempIndex = Join-Path $env:TEMP "git_autopush_idx_$PID"

try {
  $currentHead = (& git rev-parse HEAD)
  if ($LASTEXITCODE -ne 0 -or -not $currentHead) {
    Write-Log "ERROR: Failed to resolve HEAD"
    exit 1
  }
  $currentHead = $currentHead.Trim()

  $currentBranch = (& git rev-parse --abbrev-ref HEAD)
  if ($currentBranch) { $currentBranch = $currentBranch.Trim() } else { $currentBranch = "detached" }

  $env:GIT_INDEX_FILE = $tempIndex
  & git read-tree HEAD
  & git add -A
  $tree = (& git write-tree)
  if ($LASTEXITCODE -ne 0 -or -not $tree) {
    Write-Log "ERROR: Failed to write git tree"
    exit 1
  }
  $tree = $tree.Trim()

  $lastBackupTree = ""
  $lastBackupCommit = (& git rev-parse "refs/heads/$BackupBranch" 2>$null)
  if ($LASTEXITCODE -eq 0 -and $lastBackupCommit) {
    $lastBackupTree = (& git rev-parse "$($lastBackupCommit.Trim())^{tree}" 2>$null)
    if ($lastBackupTree) { $lastBackupTree = $lastBackupTree.Trim() }
  }

  if ($lastBackupTree -and ($lastBackupTree -eq $tree)) {
    Write-Log "No changes detected since last backup. Skipping push."
    exit 0
  }

  $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
  $msg = "auto-backup: $timestamp (base: $currentBranch @ $currentHead)"
  $commit = (& git commit-tree $tree -p $currentHead -m $msg)
  if ($LASTEXITCODE -ne 0 -or -not $commit) {
    Write-Log "ERROR: Failed to create backup commit"
    exit 1
  }
  $commit = $commit.Trim()

  # Update local backup ref
  & git update-ref "refs/heads/$BackupBranch" $commit

  # Push to remote (redirect standard output, let git push run)
  $pushOutput = & git push $Remote "${BackupBranch}:${BackupBranch}" --force *>&1 | Out-String
  if ($LASTEXITCODE -eq 0) {
    Write-Log "SUCCESS: Backed up $commit to $Remote/$BackupBranch ($msg)"
  } else {
    Write-Log "WARNING: Git push returned non-zero ($LASTEXITCODE): $($pushOutput.Trim())"
  }
} catch {
  Write-Log "ERROR: $_"
} finally {
  $env:GIT_INDEX_FILE = $null
  if (Test-Path $tempIndex) {
    Remove-Item -Force $tempIndex -ErrorAction SilentlyContinue
  }
}
