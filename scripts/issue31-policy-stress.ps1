[CmdletBinding()]
param(
    [ValidateRange(600, 900)]
    [int]$LongRunSeconds = 660,

    [ValidateRange(20, 300)]
    [int]$StressSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$harness = Join-Path $PSScriptRoot "issue31-backpressure-measurement.ps1"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()
$shortSha = $commit.Substring(0, [Math]::Min(12, $commit.Length))
$resultPath = Join-Path $root "artifacts\issue31-policy-stress-$shortSha.txt"

if (-not (Test-Path $harness)) {
    throw "[ISSUE31] Missing harness: $harness"
}

function Invoke-Issue31Evidence {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("Debug", "Release")]
        [string]$Configuration,

        [Parameter(Mandatory = $true)]
        [ValidateSet("Local", "SlowAnalysis", "Pressure")]
        [string]$Scenario,

        [Parameter(Mandatory = $true)]
        [int]$ObserveSeconds,

        [switch]$SkipBuild
    )

    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $harness,
        "-Configuration", $Configuration,
        "-Scenario", $Scenario,
        "-ObserveSeconds", $ObserveSeconds,
        "-HidePlayerWindow"
    )
    if ($SkipBuild) {
        $args += "-SkipBuild"
    }

    Write-Host "[ISSUE31] policy-stress start commit=$commit configuration=$Configuration scenario=$Scenario observe_s=$ObserveSeconds skip_build=$SkipBuild"
    & powershell.exe @args
    if ($LASTEXITCODE -ne 0) {
        throw "[ISSUE31] policy-stress failed configuration=$Configuration scenario=$Scenario exit=$LASTEXITCODE"
    }
    Write-Host "[ISSUE31] policy-stress PASS configuration=$Configuration scenario=$Scenario"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resultPath) | Out-Null
Remove-Item $resultPath -Force -ErrorAction SilentlyContinue

foreach ($configuration in @("Debug", "Release")) {
    Invoke-Issue31Evidence -Configuration $configuration -Scenario "Local" -ObserveSeconds $LongRunSeconds
    Invoke-Issue31Evidence -Configuration $configuration -Scenario "SlowAnalysis" -ObserveSeconds $StressSeconds -SkipBuild
    Invoke-Issue31Evidence -Configuration $configuration -Scenario "Pressure" -ObserveSeconds $StressSeconds -SkipBuild
}

@(
    "PASS"
    "commit=$commit"
    "long_run_seconds=$LongRunSeconds"
    "stress_seconds=$StressSeconds"
    "Debug.Local=PASS"
    "Debug.SlowAnalysis=PASS"
    "Debug.Pressure=PASS"
    "Release.Local=PASS"
    "Release.SlowAnalysis=PASS"
    "Release.Pressure=PASS"
) | Set-Content -Path $resultPath -Encoding UTF8

Write-Host "[ISSUE31] policy-stress matrix PASS commit=$commit"
Write-Host "[ISSUE31] result=$resultPath"
