# Unified Build Script for Helbreath 3.82
# Usage: build.ps1 [-Target <Game|Server|All>] [-Config <Debug|Release>]
#
# Examples:
#   build.ps1                           # Build Game with Debug-SFML x64 (default)
#   build.ps1 -Target Server            # Build Server
#   build.ps1 -Target Game -Config Release
#   build.ps1 -Target All               # Build all projects
#   build.ps1 -Target All -Config Release -Environment TEST -FreshSource
#   build.ps1 -Target All -DeployDryRun # build, then only preview the deploy
#
# -Environment picks the versioning channel (DEV by default). TEST and PROD also need -Config Release,
# a clean Git tree and -FreshSource, the operator's assertion that the source was freshly obtained from
# Git (sources/build_tools/gen_build_id.py enforces all three).
#
# Layout: the solution is <repo>\Helbreath.sln. Intermediates and logs go to <repo>\build\, exes and libs
# to <repo>\bin\<project>\<Config>\. After a successful build, sources\build_tools\deploy.py assembles
# <repo>\game\client and/or <repo>\game\server (it never overwrites runtime state); -NoDeploy skips it,
# -DeployDryRun only prints what it would do.
param(
    [ValidateSet("Game", "Server", "All")]
    [string]$Target = "Game",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [ValidateSet("DEV", "TEST", "PROD")]
    [string]$Environment = "DEV",

    [switch]$FreshSource,

    [switch]$NoDeploy,

    [switch]$DeployDryRun
)

$Renderer = "SFML"
$Platform = "x64"

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$solutionPath = Join-Path $repoRoot "Helbreath.sln"
$logDir = Join-Path $repoRoot "build\logs"
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }
# Keep the build tools from writing __pycache__ into sources\build_tools
$env:PYTHONDONTWRITEBYTECODE = "1"

# Find MSBuild — search common Visual Studio install locations
$msbuildCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)
# Prefer vswhere (finds any installed VS version, newest first); fall back to the fixed list
$msbuildPath = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $msbuildPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $msbuildPath) {
    $msbuildPath = $msbuildCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $msbuildPath) {
    Write-Host "ERROR: MSBuild.exe not found. Install Visual Studio 2022 or newer, or Build Tools." -ForegroundColor Red
    exit 1
}

# Configuration string matches the solution configs (Debug|x64, Release|x64)
$configString = $Config
switch ($Target) {
    "Server" { $logFile = Join-Path $logDir "build_server.log" }
    "All"    { $logFile = Join-Path $logDir "build_all.log" }
    default  { $logFile = Join-Path $logDir "build_game.log" }
}

# Delete old log
if (Test-Path $logFile) { Remove-Item $logFile -Force }

Write-Host "============================================"
Write-Host "Building: $Target | Config: $configString | Platform: $Platform | Environment: $Environment"
Write-Host "============================================"

# Find Python — search default install paths for 3.14 down to 3.10
$pythonPath = @(14, 13, 12, 11, 10) | ForEach-Object { "C:\Python3$_\python.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $pythonPath) {
    # Try PATH as fallback
    $pythonPath = Get-Command python -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}
if (-not $pythonPath) {
    Write-Host "ERROR: Python not found. Install Python 3.10+ to a default location." -ForegroundColor Red
    exit 1
}

# TEST/PROD: check the whole tree ONCE, before anything below writes to it (MSBuild modifies tracked
# files), then tell every project's generator which HEAD the check passed at. The generator
# still refuses a changed HEAD or changed source at compile time.
$verifiedHead = ""
if ($Environment -ne "DEV") {
    $checkArgs = @("$scriptDir\build_tools\gen_build_id.py", "--environment", $Environment, "--configuration", $configString, "--check-only")
    if ($FreshSource) { $checkArgs += "--fresh-source" }
    $checkOutput = & $pythonPath @checkArgs
    $checkExit = $LASTEXITCODE
    $checkOutput | Where-Object { $_ -notmatch "^verified head: " } | ForEach-Object { Write-Host $_ }
    $headLines = @($checkOutput | Where-Object { $_ -cmatch '^verified head: [0-9a-f]{40,64}$' })
    if ($checkExit -ne 0 -or $headLines.Count -ne 1) {
        Write-Host "BUILD STOPPED: the $Environment source check failed" -ForegroundColor Red
        exit 1
    }
    $null = $headLines[0] -cmatch '^verified head: ([0-9a-f]{40,64})$'
    $verifiedHead = $Matches[1]
}

# Determine MSBuild target
$msbuildTarget = if ($Target -eq "All") { "" } else { "/t:$Target" }

# Run MSBuild
$msbuildArgs = @(
    $solutionPath
    "/p:Configuration=$configString"
    "/p:Platform=$Platform"
    "/p:HbEnvironment=$Environment"
    "/p:HbPython=$pythonPath"
    "/nologo"
    "/v:minimal"
    "/consoleloggerparameters:Summary;ErrorsOnly;WarningsOnly"
    "/fileLogger"
    "/fileloggerparameters:LogFile=$logFile;Verbosity=normal;Encoding=UTF-8"
)
if ($msbuildTarget) { $msbuildArgs += $msbuildTarget }
# Appended verbatim to the gen_build_id.py command line by both .vcxproj files
if ($FreshSource) { $msbuildArgs += "/p:HbFreshSourceSwitch= --fresh-source" }
if ($verifiedHead) { $msbuildArgs += "/p:HbCleanVerifiedSwitch= --clean-verified $verifiedHead" }

& $msbuildPath @msbuildArgs
$exitCode = $LASTEXITCODE

# The console logger shows only errors and warnings, so echo the Build ID from the log: one line when
# every project agrees, a visible mismatch when they do not
if (Test-Path $logFile) {
    Get-Content $logFile | Where-Object { $_ -match "^\s*build id: " } | ForEach-Object { $_.Trim() } |
        Select-Object -Unique | ForEach-Object { Write-Host $_ }
}

# Show summary
Write-Host ""
if ($exitCode -eq 0) {
    Write-Host "BUILD SUCCEEDED" -ForegroundColor Green
} else {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    Write-Host "Check log: $logFile"
    # Show errors from log
    if (Test-Path $logFile) {
        Write-Host ""
        Write-Host "=== Errors ===" -ForegroundColor Red
        Get-Content $logFile | Select-String "error" | Select-Object -First 20
    }
}

# Deploy: assemble game\<target> from bin\, staged\ and (later) the generated DBs. Only after a good build.
if ($exitCode -eq 0 -and -not $NoDeploy) {
    $deployTarget = switch ($Target) { "Server" { "server" } "All" { "all" } default { "client" } }
    $deployArgs = @("$scriptDir\build_tools\deploy.py", "--target", $deployTarget, "--config", $configString, "--platform", "windows")
    if ($DeployDryRun) { $deployArgs += "--dry-run" }
    Write-Host ""
    & $pythonPath @deployArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "DEPLOY FAILED (the build itself succeeded)" -ForegroundColor Red
        $exitCode = $LASTEXITCODE
    }
}

exit $exitCode
