[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\windows-msvc-x64"
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$mpv = Join-Path $root "bin\$Configuration\libmpv-2.dll"

if (-not (Test-Path $exe)) {
    throw "[BRG-2] Smoke hook missing executable: $exe"
}
if (-not (Test-Path $mpv)) {
    throw "[BRG-2] Smoke hook missing libmpv runtime: $mpv"
}

# Extension point for BRG-5 (#8): when CTest tests are registered in the
# configured build tree, they automatically become part of this hook.
$ctestFile = Join-Path $buildDir "CTestTestfile.cmake"
if (Test-Path $ctestFile) {
    & ctest.exe --test-dir $buildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG-2] CTest smoke hook failed for $Configuration"
    }
}
else {
    Write-Host "[BRG-2] No CTestTestfile.cmake yet; BRG-5 may extend this hook."
}

# BRG-3 runtime lifetime extension. When the canonical shutdown harness exists,
# exercise repeated real startup -> normal main-window close -> clean process
# exit on the exact configuration built by this CI job.
$shutdownHarness = Join-Path $root "scripts\test-shutdown.ps1"
if (Test-Path $shutdownHarness) {
    & $shutdownHarness -Configuration $Configuration -Iterations 20 *>&1
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG-3] Repeated normal-close shutdown stress failed for $Configuration"
    }
}

$commit = (& git.exe -C $root rev-parse HEAD).Trim()
Write-Host "[BRG-2] Smoke hook PASS commit=$commit configuration=$Configuration"
