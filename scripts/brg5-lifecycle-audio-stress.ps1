[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [ValidateRange(5, 50)]
    [int]$Iterations = 5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\windows-msvc-x64"
$artifactRoot = Join-Path $root "artifacts\brg5"
$configKey = $Configuration.ToLowerInvariant()
$commit = (& git.exe -C $root rev-parse HEAD).Trim()
$shortCommit = $commit.Substring(0, 12)
$stressDir = Join-Path $artifactRoot "stress\$configKey-$shortCommit"
$summaryPath = Join-Path $stressDir "brg5e-summary.json"
$playbackScript = Join-Path $PSScriptRoot "brg5-playback-window-smoke.ps1"
$multiWindowScript = Join-Path $PSScriptRoot "brg5-multi-window-smoke.ps1"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"

New-Item -ItemType Directory -Force -Path $stressDir | Out-Null
Remove-Item (Join-Path $stressDir "iter-*") -Force -ErrorAction SilentlyContinue
Remove-Item $summaryPath -Force -ErrorAction SilentlyContinue

$playbackSourceJson = Join-Path $artifactRoot "brg5c-playback-window-$configKey.json"
$playbackSourceLog = Join-Path $artifactRoot "brg5c-playback-window-$configKey.log"
$multiSourceJson = Join-Path $artifactRoot "brg5d-multi-window-$configKey.json"
$multiSourceLog = Join-Path $artifactRoot "brg5d-multi-window-$configKey.log"

$scenario = "repeated-lifecycle-audio-stress"
$result = "FAIL"
$failure = $null
$focusedRegression = "NOT_RUN"
$iterationResults = [System.Collections.Generic.List[object]]::new()
$started = [DateTimeOffset]::UtcNow

function Invoke-CheckedScript(
    [string]$ScriptPath,
    [string]$Label
) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG5-E] $Label failed with exit code $LASTEXITCODE"
    }
}

function Require-ResultPass([string]$JsonPath, [string]$Label) {
    if (-not (Test-Path $JsonPath)) {
        throw "[BRG5-E] $Label evidence missing: $JsonPath"
    }
    $evidence = Get-Content $JsonPath -Raw | ConvertFrom-Json
    if ($evidence.result -ne "PASS") {
        throw "[BRG5-E] $Label evidence result=$($evidence.result)"
    }
    return $evidence
}

function Archive-And-Validate(
    [int]$Iteration,
    [string]$Kind,
    [string]$SourceJson,
    [string]$SourceLog
) {
    $ordinal = "{0:D3}" -f $Iteration
    $destJson = Join-Path $stressDir "iter-$ordinal-$Kind.json"
    $destLog = Join-Path $stressDir "iter-$ordinal-$Kind.log"

    if (-not (Test-Path $SourceJson)) {
        throw "[BRG5-E] source JSON missing for $Kind iteration $Iteration"
    }
    if (-not (Test-Path $SourceLog)) {
        throw "[BRG5-E] source lifecycle log missing for $Kind iteration $Iteration"
    }

    Copy-Item $SourceJson $destJson -Force
    Copy-Item $SourceLog $destLog -Force

    & python.exe $validator $destLog
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG5-E] archived lifecycle validation failed kind=$Kind iteration=$Iteration"
    }

    return [ordered]@{
        json = $destJson
        lifecycle_log = $destLog
    }
}

Write-Host "[BRG5-E] start commit=$commit configuration=$Configuration iterations=$Iterations scenario=$scenario"

try {
    & ctest.exe --test-dir $buildDir -C $Configuration -R brg3_render_callback_lifetime_gate --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG5-E] focused BRG-3 callback lifetime regression failed"
    }
    $focusedRegression = "PASS"

    for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
        Write-Host "[BRG5-E] iteration=$iteration/$Iterations playback-cycle start"

        Remove-Item $playbackSourceJson -Force -ErrorAction SilentlyContinue
        Remove-Item $playbackSourceLog -Force -ErrorAction SilentlyContinue
        Invoke-CheckedScript $playbackScript "playback-cycle iteration $iteration"
        $playbackEvidence = Require-ResultPass $playbackSourceJson "playback-cycle iteration $iteration"
        $playbackArchive = Archive-And-Validate $iteration "playback" $playbackSourceJson $playbackSourceLog

        $requiredPlaybackCases = @(
            "play_local_media",
            "pause",
            "resume",
            "seek_forward",
            "seek_backward",
            "rapid_seek",
            "clean_shutdown",
            "lifecycle_validation"
        )
        foreach ($case in $requiredPlaybackCases) {
            if ($null -eq $playbackEvidence.cases.$case -or -not [string]$playbackEvidence.cases.$case.StartsWith("PASS")) {
                throw "[BRG5-E] playback iteration $iteration missing PASS case: $case"
            }
        }

        Write-Host "[BRG5-E] iteration=$iteration/$Iterations multi-window-cycle start"

        Remove-Item $multiSourceJson -Force -ErrorAction SilentlyContinue
        Remove-Item $multiSourceLog -Force -ErrorAction SilentlyContinue
        Invoke-CheckedScript $multiWindowScript "multi-window-cycle iteration $iteration"
        $multiEvidence = Require-ResultPass $multiSourceJson "multi-window-cycle iteration $iteration"
        $multiArchive = Archive-And-Validate $iteration "multi-window" $multiSourceJson $multiSourceLog

        $requiredMultiCases = @(
            "open_main_plus_secondary",
            "resize_secondary_main_continues",
            "close_secondary_main_valid",
            "reopen_secondary_reuses_window",
            "close_main_clean_shutdown",
            "lifecycle_validation"
        )
        foreach ($case in $requiredMultiCases) {
            if ($null -eq $multiEvidence.cases.$case -or -not [string]$multiEvidence.cases.$case.StartsWith("PASS")) {
                throw "[BRG5-E] multi-window iteration $iteration missing PASS case: $case"
            }
        }

        $iterationResults.Add([ordered]@{
            iteration = $iteration
            playback = [ordered]@{
                result = "PASS"
                json = $playbackArchive.json
                lifecycle_log = $playbackArchive.lifecycle_log
                player_session_create_destroy = "PASS_ONE_SHOT_GRAMMAR"
                play_seek_close = "PASS"
                render_callback_shutdown_activity = "PASS_PLAYBACK_CLOSE_EXIT_0"
                audio_start_seek_stop = "PASS_AUDIO_WORKER_LIFECYCLE_VALIDATED"
            }
            multi_window = [ordered]@{
                result = "PASS"
                json = $multiArchive.json
                lifecycle_log = $multiArchive.lifecycle_log
                window_create_hide_destroy = "PASS"
            }
        }) | Out-Null

        Write-Host "[BRG5-E] iteration=$iteration/$Iterations PASS"
    }

    $result = "PASS"
    Write-Host "[BRG5-E] PASS commit=$commit configuration=$Configuration iterations=$Iterations"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[BRG5-E] ERROR: $failure"
    throw
}
finally {
    $completed = [DateTimeOffset]::UtcNow
    $summary = [ordered]@{
        schema = "BRG5-E-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        iterations_requested = $Iterations
        iterations_completed = $iterationResults.Count
        focused_brg3_regression = $focusedRegression
        process_model = "FRESH_PROCESS_PER_SCENARIO_PER_ITERATION"
        lifecycle_grammar = "ONE_SHOT_EXACT_SEQUENCE"
        result = $result
        failure = $failure
        started_utc = $started.ToString("o")
        completed_utc = $completed.ToString("o")
        duration_seconds = [Math]::Round(($completed - $started).TotalSeconds, 3)
        iteration_results = $iterationResults
    }
    $summary | ConvertTo-Json -Depth 10 | Set-Content -Path $summaryPath -Encoding UTF8
    Write-Host "[BRG5-E] summary=$summaryPath result=$result completed=$($iterationResults.Count)/$Iterations"
}
