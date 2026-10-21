[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Debug','Release')][string]$Configuration,
    [string]$MediaPath = '',
    [string]$MediaUrl = '',
    [ValidateRange(10,900)][int]$ObserveSeconds = 180,
    [switch]$SkipBuild,
    [switch]$RequireReproduction
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sha = (& git.exe -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $sha.Length -ne 40) { throw 'Cannot pin exact git HEAD' }
if (-not [Environment]::UserInteractive -or $env:OS -ne 'Windows_NT') {
    throw '[S01] BLOCKED: requires interactive physical Windows session, not hosted headless CI'
}
$useUrl = -not [string]::IsNullOrWhiteSpace($MediaUrl)
if ($useUrl -and -not [string]::IsNullOrWhiteSpace($MediaPath)) {
    throw 'Only one of -MediaPath and -MediaUrl may be supplied'
}
if (-not $useUrl -and $MediaPath -and
    -not (Test-Path -LiteralPath $MediaPath -PathType Leaf)) {
    throw '[S01] Missing local test media input'
}
$oldContinuity = [Environment]::GetEnvironmentVariable('IM_PLAYER_AUDIO_CONTINUITY_TRACE','Process')
$oldStartup = [Environment]::GetEnvironmentVariable('IM_PLAYER_STARTUP_BOUNDARY_TRACE','Process')
try {
    $env:IM_PLAYER_AUDIO_CONTINUITY_TRACE = '1'
    $env:IM_PLAYER_STARTUP_BOUNDARY_TRACE = '1'
    $scenario = if ($useUrl) { 'ColdUrl' } else { 'Local' }
    $argsList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',
        (Join-Path $PSScriptRoot 'issue31-backpressure-measurement.ps1'),
        '-Configuration',$Configuration,'-ObserveSeconds',[string]$ObserveSeconds)
    if ($useUrl) {
        $argsList += @('-Scenario','ColdUrl','-MediaUrl',$MediaUrl)
    } else {
        $argsList += @('-Scenario','Local')
        if ($MediaPath) { $argsList += @('-MediaPath',$MediaPath) }
    }
    if ($SkipBuild) { $argsList += '-SkipBuild' }
    & powershell.exe @argsList
    if ($LASTEXITCODE -ne 0) { throw '[S01] Physical capture failed; do not claim E2E PASS' }
    $outDir = Join-Path $root ('artifacts/issue31-backpressure/' +
        $scenario.ToLowerInvariant() + '-' + $Configuration.ToLowerInvariant() +
        '-' + $sha.Substring(0,12))
    $trace = Join-Path $outDir 'trace.log'
    if (-not (Test-Path -LiteralPath $trace -PathType Leaf)) { throw '[S01] Missing trace log' }
    $report = Join-Path $outDir 's01-boundary.json'
    $analyzerArgs = @((Join-Path $PSScriptRoot 'issue42_first_boundary.py'),
        $trace,'--output',$report)
    if ($RequireReproduction) { $analyzerArgs += '--require-reproduction' }
    & python.exe @analyzerArgs
    if ($LASTEXITCODE -ne 0) { throw '[S01] Underflow/boundary evidence INCONCLUSIVE; no E2E claim' }
    Write-Host "[S01] TRACE_CAPTURED commit=$sha configuration=$Configuration report=$report"
    Write-Host '[S01] Neither a normal trace nor an underflow classifier certifies audible output or root cause.'
}
finally {
    [Environment]::SetEnvironmentVariable('IM_PLAYER_AUDIO_CONTINUITY_TRACE',$oldContinuity,'Process')
    [Environment]::SetEnvironmentVariable('IM_PLAYER_STARTUP_BOUNDARY_TRACE',$oldStartup,'Process')
}
