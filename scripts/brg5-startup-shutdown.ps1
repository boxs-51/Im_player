[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateRange(1, 100)]
    [int]$Iterations = 1,

    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 2,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\windows-msvc-x64"
$artifactDir = Join-Path $root "artifacts\brg5"
$shutdownScript = Join-Path $PSScriptRoot "test-shutdown.ps1"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()
$configKey = $Configuration.ToLowerInvariant()
$lifecycleLog = Join-Path $artifactDir "brg5b-startup-shutdown-$configKey.log"
$evidenceJson = Join-Path $artifactDir "brg5b-startup-shutdown-$configKey.json"
$scenario = "startup-clean-shutdown-no-playback"

New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
Remove-Item $lifecycleLog -Force -ErrorAction SilentlyContinue
Remove-Item $evidenceJson -Force -ErrorAction SilentlyContinue

$ctestCommand = "ctest --test-dir build/windows-msvc-x64 -C $Configuration -R brg3_render_callback_lifetime_gate --output-on-failure"
$shutdownCommand = "powershell -ExecutionPolicy Bypass -File scripts/test-shutdown.ps1 -Configuration $Configuration -Iterations $Iterations -StartupSeconds $StartupSeconds -ExitTimeoutSeconds $ExitTimeoutSeconds"
$validatorCommand = "python scripts/brg5_validate_lifecycle.py artifacts/brg5/brg5b-startup-shutdown-$configKey.log"

$result = "FAIL"
$failure = $null
$focusedRegression = "NOT_RUN"
$lifecycleValidation = "NOT_RUN"
$previousLifecycleLog = [Environment]::GetEnvironmentVariable("IM_PLAYER_LIFECYCLE_LOG", [EnvironmentVariableTarget]::Process)

Write-Host "[BRG5-B] start commit=$commit configuration=$Configuration scenario=$scenario iterations=$Iterations"

try {
    & ctest.exe --test-dir $buildDir -C $Configuration -R brg3_render_callback_lifetime_gate --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "[BRG5-B] focused BRG-3 regression failed for $Configuration" }
    $focusedRegression = "PASS"

    $env:IM_PLAYER_LIFECYCLE_LOG = $lifecycleLog
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $shutdownScript -Configuration $Configuration -Iterations $Iterations -StartupSeconds $StartupSeconds -ExitTimeoutSeconds $ExitTimeoutSeconds
    if ($LASTEXITCODE -ne 0) { throw "[BRG5-B] startup/shutdown harness failed for $Configuration" }
    if (-not (Test-Path $lifecycleLog)) { throw "[BRG5-B] lifecycle log missing: $lifecycleLog" }

    & python.exe $validator $lifecycleLog
    if ($LASTEXITCODE -ne 0) { throw "[BRG5-B] lifecycle validation failed for $Configuration" }
    $lifecycleValidation = "PASS"

    $result = "PASS"
    Write-Host "[BRG5-B] PASS commit=$commit configuration=$Configuration scenario=$scenario"
}
catch {
    $failure = $_.Exception.Message
    Write-Error $failure
    throw
}
finally {
    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    }
    else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }

    $evidence = [ordered]@{
        schema = "BRG5-B-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        iterations = $Iterations
        startup_seconds = $StartupSeconds
        exit_timeout_seconds = $ExitTimeoutSeconds
        focused_brg3_regression = $focusedRegression
        lifecycle_validation = $lifecycleValidation
        result = $result
        failure = $failure
        lifecycle_log = $lifecycleLog
        ctest_command = $ctestCommand
        runtime_command = $shutdownCommand
        validator_command = $validatorCommand
    }

    $evidence | ConvertTo-Json -Depth 4 | Set-Content -Path $evidenceJson -Encoding UTF8
    Write-Host "[BRG5-B] evidence=$evidenceJson result=$result"
}
