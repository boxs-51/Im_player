[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateRange(610, 1800)]
    [int]$PlaybackSeconds = 620,

    [ValidateRange(640, 1900)]
    [int]$FixtureSeconds = 650,

    [ValidateRange(5, 60)]
    [int]$WarmupSeconds = 10,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"
$generator = Join-Path $PSScriptRoot "issue26_generate_av_fixture.py"
$artifactRoot = Join-Path $root "artifacts\issue26-longrun"
$configKey = $Configuration.ToLowerInvariant()
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if ($FixtureSeconds -lt ($PlaybackSeconds + 20)) {
    throw "[ISSUE26-LONGRUN] FixtureSeconds must exceed PlaybackSeconds by at least 20 seconds"
}
if (($PlaybackSeconds - $WarmupSeconds) -lt 605) {
    throw "[ISSUE26-LONGRUN] playback window must leave at least 605s after warm-up"
}
if (-not (Test-Path $exe)) {
    throw "[ISSUE26-LONGRUN] Missing executable: $exe"
}

New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null

$fixture = Join-Path $artifactRoot "av-44100-mono-$FixtureSeconds-sec.avi"
$log = Join-Path $artifactRoot "$configKey-drift.log"
$json = Join-Path $artifactRoot "$configKey-drift-summary.json"
Remove-Item $fixture, $log, $json -Force -ErrorAction SilentlyContinue

# The AVI contains a real video timeline plus 44.1 kHz mono PCM audio.
# This exercises mpv resample/remix into the canonical 48 kHz stereo float32
# pipe while the video clock remains active for the entire drift interval.
& python.exe $generator $fixture --seconds $FixtureSeconds --sample-rate 44100
if ($LASTEXITCODE -ne 0) {
    throw "[ISSUE26-LONGRUN] fixture generation failed"
}

function Wait-MainWindow([System.Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 50; ++$i) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[ISSUE26-LONGRUN] process exited before main window became available exitCode=$($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 200
    }
    throw "[ISSUE26-LONGRUN] timed out waiting for main window"
}

function Parse-KeyValues([string]$Line) {
    $map = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, '(?<key>[A-Za-z0-9_]+)=(?<value>[^\s]+)')) {
        $map[$match.Groups['key'].Value] = $match.Groups['value'].Value
    }
    return $map
}

$previousLifecycleLog = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_LIFECYCLE_LOG",
    [EnvironmentVariableTarget]::Process
)

$p = $null
try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $log
    $quotedMedia = '"' + $fixture + '"'

    Write-Host "[ISSUE26-LONGRUN] start commit=$commit configuration=$Configuration playback_seconds=$PlaybackSeconds fixture_seconds=$FixtureSeconds"
    $p = Start-Process -FilePath $exe -ArgumentList $quotedMedia -WorkingDirectory $root -PassThru
    $null = Wait-MainWindow $p

    # Fail early if the generated A/V container cannot reach the external
    # speaker path. The >=10-minute measurement clock starts only after the
    # first durable sync sample exists.
    $startupDeadline = [DateTime]::UtcNow.AddSeconds(20)
    $firstSyncObserved = $false
    while ([DateTime]::UtcNow -lt $startupDeadline) {
        Start-Sleep -Milliseconds 500
        $p.Refresh()
        if ($p.HasExited) {
            throw "[ISSUE26-LONGRUN] process exited before first sync sample exitCode=$($p.ExitCode)"
        }
        if (Test-Path $log) {
            $firstSyncObserved = $null -ne (
                Select-String -Path $log -Pattern '^\[AUDIO-TELEMETRY\] SYNC_SAMPLE index=1 ' -Quiet
            )
            if ($firstSyncObserved) {
                break
            }
        }
    }
    if (-not $firstSyncObserved) {
        throw "[ISSUE26-LONGRUN] first SYNC_SAMPLE not observed within 20s; A/V fixture did not reach paced output"
    }

    Write-Host "[ISSUE26-LONGRUN] first sync observed; starting observation window seconds=$PlaybackSeconds warmup_seconds=$WarmupSeconds"
    $deadline = [DateTime]::UtcNow.AddSeconds($PlaybackSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Seconds 5
        $p.Refresh()
        if ($p.HasExited) {
            throw "[ISSUE26-LONGRUN] process exited before drift interval completed exitCode=$($p.ExitCode)"
        }
    }

    # Freeze phase-scoped underflow evidence before intentionally initiating
    # shutdown. The first WarmupSeconds belong to startup, not steady-state.
    $preCloseLines = @()
    if (Test-Path $log) {
        $preCloseLines = @(Get-Content -Path $log | ForEach-Object { [string]$_ })
    }

    $firstSyncLine = @($preCloseLines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] SYNC_SAMPLE index=1 '
    }) | Select-Object -First 1
    if (-not $firstSyncLine) {
        throw "[ISSUE26-LONGRUN] cannot phase underflows without first SYNC_SAMPLE"
    }

    $firstSyncKv = Parse-KeyValues ([string]$firstSyncLine)
    $preCloseFirstSyncUs = [uint64]$firstSyncKv["t_us"]
    $preCloseWarmupCutoffUs =
        $preCloseFirstSyncUs +
        ([uint64]$WarmupSeconds * [uint64]1000000)

    [uint64]$pendingEmptyObservedUs = 0
    [uint64]$startupUnderflowCount = 0
    [uint64]$steadyUnderflowCount = 0
    [uint64]$preCloseUnderflowCount = 0

    foreach ($line in $preCloseLines) {
        if ($line -match '^\[AUDIO-TELEMETRY\] SDL_QUEUE_EMPTY_OBSERVED ') {
            $observedKv = Parse-KeyValues $line
            if ($observedKv.Contains("t_us")) {
                $pendingEmptyObservedUs = [uint64]$observedKv["t_us"]
            }
            continue
        }

        if ($line -match '^\[AUDIO-TELEMETRY\] ERROR SDL_QUEUE_UNDERFLOW ') {
            $underflowKv = Parse-KeyValues $line
            if ($pendingEmptyObservedUs -eq 0 -or
                -not $underflowKv.Contains("empty_us")) {
                throw "[ISSUE26-LONGRUN] cannot phase underflow without observed t_us + empty_us"
            }

            $underflowEventUs =
                $pendingEmptyObservedUs +
                [uint64]$underflowKv["empty_us"]
            ++$preCloseUnderflowCount

            if ($underflowEventUs -lt $preCloseWarmupCutoffUs) {
                ++$startupUnderflowCount
            } else {
                ++$steadyUnderflowCount
            }

            $pendingEmptyObservedUs = 0
        }
    }

    Write-Host "[ISSUE26-LONGRUN] observation complete pre_close_underflows=$preCloseUnderflowCount startup_underflows=$startupUnderflowCount steady_underflows=$steadyUnderflowCount warmup_cutoff_us=$preCloseWarmupCutoffUs"
    if ($steadyUnderflowCount -ne 0) {
        throw "[ISSUE26-LONGRUN] steady-state sustained underflow detected after warm-up count=$steadyUnderflowCount"
    }

    if (-not $p.CloseMainWindow()) {
        throw "[ISSUE26-LONGRUN] CloseMainWindow returned false"
    }
    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[ISSUE26-LONGRUN] shutdown timeout"
    }
    if ($p.ExitCode -ne 0) {
        throw "[ISSUE26-LONGRUN] shutdown exitCode=$($p.ExitCode)"
    }
}
finally {
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }
}

if (-not (Test-Path $log)) {
    throw "[ISSUE26-LONGRUN] evidence log missing"
}

& python.exe $validator $log
if ($LASTEXITCODE -ne 0) {
    throw "[ISSUE26-LONGRUN] lifecycle validation failed"
}

[string[]]$lines = @(Get-Content -Path $log | ForEach-Object { [string]$_ })
$anchorLines = @($lines | Where-Object {
    $_ -match '^\[AUDIO-TELEMETRY\] AUDIO_MEDIA_CLOCK_ANCHOR '
})
if ($anchorLines.Count -ne 1) {
    throw "[ISSUE26-LONGRUN] expected exactly one independent audio media clock anchor actual=$($anchorLines.Count)"
}

$syncLines = @($lines | Where-Object {
    $_ -match '^\[AUDIO-TELEMETRY\] SYNC_SAMPLE '
})
if ($syncLines.Count -lt 100) {
    throw "[ISSUE26-LONGRUN] insufficient sync samples count=$($syncLines.Count) required>=100"
}

$samples = @()
foreach ($line in $syncLines) {
    $kv = Parse-KeyValues $line
    foreach ($required in @("index", "t_us", "queued_ms", "av_offset_s")) {
        if (-not $kv.Contains($required)) {
            throw "[ISSUE26-LONGRUN] malformed sync sample missing=$required line=$line"
        }
    }
    $samples += [pscustomobject]@{
        Index = [uint64]$kv["index"]
        TimeUs = [uint64]$kv["t_us"]
        QueuedMs = [double]$kv["queued_ms"]
        AvOffsetSeconds = [double]$kv["av_offset_s"]
        Raw = $line
    }
}

for ($i = 0; $i -lt $samples.Count; ++$i) {
    $expectedIndex = [uint64]($i + 1)
    if ($samples[$i].Index -ne $expectedIndex) {
        throw "[ISSUE26-LONGRUN] non-sequential sync index position=$i expected=$expectedIndex actual=$($samples[$i].Index)"
    }
    if ($i -gt 0 -and $samples[$i].TimeUs -le $samples[$i - 1].TimeUs) {
        throw "[ISSUE26-LONGRUN] non-monotonic sync timestamp position=$i previous=$($samples[$i - 1].TimeUs) actual=$($samples[$i].TimeUs)"
    }
}

$allFirst = $samples[0]
$allLast = $samples[$samples.Count - 1]
$warmupCutoffUs =
    $allFirst.TimeUs + ([uint64]$WarmupSeconds * [uint64]1000000)
$steadySamples = @($samples | Where-Object {
    $_.TimeUs -ge $warmupCutoffUs
})

if ($steadySamples.Count -lt 100) {
    throw "[ISSUE26-LONGRUN] insufficient steady sync samples after warm-up count=$($steadySamples.Count) required>=100"
}

$first = $steadySamples[0]
$last = $steadySamples[$steadySamples.Count - 1]
$spanSeconds = ([double]($last.TimeUs - $first.TimeUs)) / 1000000.0
if ($spanSeconds -lt 600.0) {
    throw "[ISSUE26-LONGRUN] measured steady sync span below 600s actual=$spanSeconds warmup_s=$WarmupSeconds"
}

$steadyBaselineOffsetSeconds = $first.AvOffsetSeconds
$maxRelativeDriftSeconds = 0.0
$maxQueuedMs = 0.0
foreach ($sample in $steadySamples) {
    $relativeDrift = [math]::Abs(
        $sample.AvOffsetSeconds - $steadyBaselineOffsetSeconds
    )
    if ($relativeDrift -gt $maxRelativeDriftSeconds) {
        $maxRelativeDriftSeconds = $relativeDrift
    }
    if ($sample.QueuedMs -gt $maxQueuedMs) {
        $maxQueuedMs = $sample.QueuedMs
    }
}
$driftDeltaSeconds = [math]::Abs(
    $last.AvOffsetSeconds - $first.AvOffsetSeconds
)

if ($maxRelativeDriftSeconds -gt 0.500) {
    throw "[ISSUE26-LONGRUN] steady A/V drift excursion exceeded 500ms actual_s=$maxRelativeDriftSeconds baseline_s=$steadyBaselineOffsetSeconds"
}
if ($driftDeltaSeconds -gt 0.250) {
    throw "[ISSUE26-LONGRUN] 10-minute steady A/V drift delta exceeded 250ms actual_s=$driftDeltaSeconds"
}
if ($maxQueuedMs -gt 150.0) {
    throw "[ISSUE26-LONGRUN] steady periodic SDL queue exceeded 150ms actual_ms=$maxQueuedMs"
}

$summaryLines = @($lines | Where-Object {
    $_ -match '^\[AUDIO-TELEMETRY\] SUMMARY '
})
if ($summaryLines.Count -ne 1) {
    throw "[ISSUE26-LONGRUN] expected exactly one SUMMARY marker actual=$($summaryLines.Count)"
}
$summaryLine = [string]$summaryLines[0]
$summary = Parse-KeyValues $summaryLine

foreach ($required in @(
    "capture_dropped",
    "ring_overflows",
    "format_mismatch",
    "underflows",
    "write_failures",
    "queue_high_water_bytes",
    "sync_samples",
    "first_sync_us",
    "last_sync_us",
    "first_sync_offset_s",
    "last_sync_offset_s",
    "max_abs_av_offset_s",
    "snapshot_phase"
)) {
    if (-not $summary.Contains($required)) {
        throw "[ISSUE26-LONGRUN] SUMMARY missing field=$required"
    }
}

if ([uint64]$summary["capture_dropped"] -ne 0 -or
    [uint64]$summary["ring_overflows"] -ne 0 -or
    [uint64]$summary["format_mismatch"] -ne 0 -or
    [uint64]$summary["write_failures"] -ne 0) {
    throw "[ISSUE26-LONGRUN] SUMMARY reports transport loss/error marker=$summaryLine"
}
# Underflow acceptance is phase-scoped: steady playback must have zero
# sustained underflows before CloseMainWindow(). Any additional underflow
# recorded only during teardown remains diagnostic evidence.

if ([string]$summary["snapshot_phase"] -ne "shutdown_entry") {
    throw "[ISSUE26-LONGRUN] unexpected summary snapshot phase=$($summary['snapshot_phase'])"
}

$bytesPerSecond = 48000.0 * 2.0 * 4.0
$summaryQueueHighWaterMs =
    1000.0 * [double]([uint64]$summary["queue_high_water_bytes"]) / $bytesPerSecond
if ($summaryQueueHighWaterMs -gt 150.0) {
    throw "[ISSUE26-LONGRUN] SUMMARY SDL queue high-water exceeded 150ms actual_ms=$summaryQueueHighWaterMs"
}

$summaryUnderflows = [uint64]$summary["underflows"]
$shutdownOnlyUnderflows =
    [int64]$summaryUnderflows - [int64]$preCloseUnderflowCount
if ($shutdownOnlyUnderflows -lt 0) {
    throw "[ISSUE26-LONGRUN] summary underflow count regressed below pre-close evidence"
}

$summarySyncSamples = [uint64]$summary["sync_samples"]
$summaryFirstUs = [uint64]$summary["first_sync_us"]
$summaryLastUs = [uint64]$summary["last_sync_us"]
$summaryFirstOffset = [double]$summary["first_sync_offset_s"]
$summaryLastOffset = [double]$summary["last_sync_offset_s"]
$summaryMaxAbsOffset = [double]$summary["max_abs_av_offset_s"]

# Absolute startup/per-write offset remains diagnostic evidence. The #26
# long-run verdict is relative steady-state drift after warm-up; initial
# alignment remains a #33 integration concern.

if ($summarySyncSamples -ne [uint64]$samples.Count) {
    throw "[ISSUE26-LONGRUN] summary/sample count mismatch summary=$summarySyncSamples log=$($samples.Count)"
}
if ($summaryFirstUs -ne $allFirst.TimeUs -or $summaryLastUs -ne $allLast.TimeUs) {
    throw "[ISSUE26-LONGRUN] summary/all-sample timestamp mismatch"
}
if ([math]::Abs($summaryFirstOffset - $allFirst.AvOffsetSeconds) -gt 0.000001 -or
    [math]::Abs($summaryLastOffset - $allLast.AvOffsetSeconds) -gt 0.000001) {
    throw "[ISSUE26-LONGRUN] summary/all-sample offset mismatch"
}

$result = [ordered]@{
    schema = "ISSUE26-AV-DRIFT-LONGRUN-v1"
    commit = $commit
    configuration = $Configuration
    fixture_container = "AVI"
    video_codec = "BI_RGB"
    video_fps = 10
    video_width = 64
    video_height = 36
    source_sample_rate = 44100
    source_channels = 1
    requested_playback_seconds = $PlaybackSeconds
    warmup_seconds = $WarmupSeconds
    total_sync_sample_count = $samples.Count
    steady_sync_sample_count = $steadySamples.Count
    measured_steady_sync_span_seconds = $spanSeconds
    startup_first_av_offset_seconds = $allFirst.AvOffsetSeconds
    startup_last_av_offset_seconds = $allLast.AvOffsetSeconds
    steady_baseline_av_offset_seconds = $steadyBaselineOffsetSeconds
    steady_last_av_offset_seconds = $last.AvOffsetSeconds
    drift_delta_seconds = $driftDeltaSeconds
    max_steady_relative_drift_seconds = $maxRelativeDriftSeconds
    max_per_write_abs_av_offset_diagnostic_seconds = $summaryMaxAbsOffset
    max_steady_periodic_queue_ms = $maxQueuedMs
    summary_queue_high_water_ms = $summaryQueueHighWaterMs
    capture_dropped = [uint64]$summary["capture_dropped"]
    ring_overflows = [uint64]$summary["ring_overflows"]
    format_mismatch = [uint64]$summary["format_mismatch"]
    underflows = [uint64]$summary["underflows"]
    write_failures = [uint64]$summary["write_failures"]
    pre_close_underflows = [uint64]$preCloseUnderflowCount
    shutdown_only_underflows = $shutdownOnlyUnderflows
    snapshot_phase = [string]$summary["snapshot_phase"]
    result = "PASS"
    evidence_log = $log
}
$result | ConvertTo-Json -Depth 6 | Set-Content -Path $json -Encoding UTF8

Write-Host "[ISSUE26-LONGRUN] PASS commit=$commit configuration=$Configuration pre_close_underflows=$preCloseUnderflowCount shutdown_only_underflows=$shutdownOnlyUnderflows"
Write-Host "[ISSUE26-LONGRUN] steady_span_s=$spanSeconds total_samples=$($samples.Count) steady_samples=$($steadySamples.Count) warmup_s=$WarmupSeconds startup_first_offset_s=$($allFirst.AvOffsetSeconds) steady_baseline_offset_s=$steadyBaselineOffsetSeconds steady_last_offset_s=$($last.AvOffsetSeconds) drift_delta_s=$driftDeltaSeconds max_relative_drift_s=$maxRelativeDriftSeconds max_steady_queue_ms=$maxQueuedMs absolute_per_write_offset_diagnostic_s=$summaryMaxAbsOffset summary_queue_high_water_ms=$summaryQueueHighWaterMs"
Write-Host "[ISSUE26-LONGRUN] summary=$json"
