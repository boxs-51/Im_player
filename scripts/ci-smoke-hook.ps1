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

# BRG-3 runtime policy:
# GitHub-hosted Windows runners are non-interactive service environments and
# are not authoritative for desktop startup/main-window-close behavior.
# They still enforce compile/link plus the deterministic focused lifetime CTest
# above. Canonical L2/L4 GUI runtime proof is collected on an interactive local
# Windows desktop using scripts/test-shutdown.ps1 on the exact commit under test.
Write-Host "[BRG-3] Interactive GUI shutdown stress: NOT_RUN_IN_CI"
Write-Host "[BRG-3] Canonical runtime proof: local interactive Windows exact-head evidence"
Write-Host "[BRG-3] Local command: scripts/test-shutdown.ps1 -Configuration $Configuration -Iterations 20"
Write-Host "[BRG5-B] Interactive startup/shutdown matrix: NOT_RUN_IN_CI"
Write-Host "[BRG5-B] Local command: scripts/brg5-startup-shutdown.ps1 -Configuration $Configuration -Iterations 1"

$commit = (& git.exe -C $root rev-parse HEAD).Trim()
Write-Host "[BRG-2] Smoke hook PASS commit=$commit configuration=$Configuration"
