# Full release build: rolls ONE release seed, then builds the client and the server in Release.
#
# A Release server admits a client only when its Build ID matches and its build proof verifies, and the
# proof is keyed by the release seed, so the client and the server must come from the same seed. This is
# the ONLY path that rolls a new seed; any other non-Debug build reuses the current one (each .vcxproj's
# EnsureReleaseSeed target writes a seed only when none exists). Each channel has its own seed file.
#
# Usage: build_release.ps1 [-Environment <DEV|TEST|PROD>] [-FreshSource]
#   build_release.ps1                              # DEV release pair
#   build_release.ps1 -Environment TEST -FreshSource
#   build_release.ps1 -Deploy                      # also deploy the pair into <repo>\game (dev game folder)
#
# A release pair is NOT deployed by default: <repo>\game is the dev game folder and a release build must not
# replace its binaries unasked. -Deploy deploys it, -DeployDryRun only previews that deploy.
#
# A failed build leaves the rolled seed in place; run again so nothing ships from a half-built pair.
param(
    [ValidateSet("DEV", "TEST", "PROD")]
    [string]$Environment = "DEV",

    [switch]$FreshSource,

    [switch]$Deploy,

    [switch]$DeployDryRun
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$buildIdScript = Join-Path $scriptDir "build_tools\gen_build_id.py"
$seedScript = Join-Path $scriptDir "build_tools\gen_release_seed.py"
# Shared with both .vcxproj files ($(HbBuildRoot)generated\<channel>\, sources\build_tools\layout.props) and
# the CMake default (HB_RELEASE_SEED_DIR, versioning.cmake); <repo>\build\ is git-ignored
$seedHeader = Join-Path $repoRoot ("build\generated\" + $Environment.ToLower() + "\release_seed_generated.h")
$env:PYTHONDONTWRITEBYTECODE = "1"

# Find Python the same way build.ps1 does: default install paths for 3.14 down to 3.10, then PATH
$pythonPath = @(14, 13, 12, 11, 10) | ForEach-Object { "C:\Python3$_\python.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $pythonPath) {
    $pythonPath = Get-Command python -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}
if (-not $pythonPath) {
    Write-Host "build_release: Python not found. Install Python 3.10+ to a default location." -ForegroundColor Red
    exit 1
}

# TEST/PROD: refuse before rolling a seed, while the tree is still exactly as checked out. build.ps1 repeats
# the check, also before anything writes to the tree.
if ($Environment -ne "DEV") {
    $checkArgs = @($buildIdScript, "--environment", $Environment, "--configuration", "Release", "--check-only")
    if ($FreshSource) { $checkArgs += "--fresh-source" }
    # the check's `verified head:` line is for build.ps1, which runs its own check and passes it on
    & $pythonPath @checkArgs | Where-Object { $_ -notmatch "^verified head: " } | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "build_release: the $Environment source check failed; nothing built" -ForegroundColor Red
        exit 1
    }
}

& $pythonPath $seedScript --mode roll --environment $Environment --output $seedHeader
if ($LASTEXITCODE -ne 0) {
    Write-Host "build_release: seed roll failed; nothing built" -ForegroundColor Red
    exit 1
}

# No deploy unless asked: -DeployDryRun previews, -Deploy writes into <repo>\game
$noDeploy = -not ($Deploy -or $DeployDryRun)
& (Join-Path $scriptDir "build.ps1") -Target All -Config Release -Environment $Environment -FreshSource:$FreshSource -NoDeploy:$noDeploy -DeployDryRun:$DeployDryRun
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Host "build_release: the build failed; the pair is incomplete, run again" -ForegroundColor Red
    exit $exitCode
}

Write-Host "build_release: client and server built on one $Environment seed"
exit 0
