[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("Local", "ColdUrl", "Seek", "Pressure", "SlowAnalysis")]
    [string]$Scenario = "Local",

    [string]$MediaPath = "",
    [string]$MediaUrl = "",

    [ValidateRange(5, 900)]
    [int]$ObserveSeconds = 20,

    [ValidateRange(5, 120)]
    [int]$StartupTimeoutSeconds = 30,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10,

    [switch]$SkipBuild,

    [switch]$HidePlayerWindow
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$buildScript = Join-Path $PSScriptRoot "build.ps1"
$generator = Join-Path $PSScriptRoot "issue26_generate_av_fixture.py"
$artifactRoot = Join-Path $root "artifacts\issue31-backpressure"
$configKey = $Configuration.ToLowerInvariant()
$scenarioKey = $Scenario.ToLowerInvariant()
$commit = (& git.exe -C $root rev-parse HEAD).Trim()
$shortSha = $commit.Substring(0, [Math]::Min(12, $commit.Length))
$outDir = Join-Path $artifactRoot "$scenarioKey-$configKey-$shortSha"
$log = Join-Path $outDir "trace.log"
$summaryPath = Join-Path $outDir "summary.json"

if (-not $SkipBuild) {
    if (-not (Test-Path $buildScript)) {
        throw "[ISSUE31] Build wrapper missing: $buildScript"
    }
    Write-Host "[ISSUE31] exact-head incremental build start commit=$commit configuration=$Configuration"
    & powershell.exe -ExecutionPolicy Bypass -File $buildScript -Configuration $Configuration -SkipBootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "[ISSUE31] exact-head incremental build failed commit=$commit configuration=$Configuration"
    }
    Write-Host "[ISSUE31] exact-head incremental build PASS commit=$commit configuration=$Configuration"
}
if (-not (Test-Path $exe)) {
    throw "[ISSUE31] Missing executable after exact-head build: $exe"
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item $log, $summaryPath -Force -ErrorAction SilentlyContinue

function Parse-KeyValues([string]$Line) {
    $map = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, '(?<key>[A-Za-z0-9_]+)=(?<value>[^\s]+)')) {
        $map[$match.Groups['key'].Value] = $match.Groups['value'].Value
    }
    return $map
}

function Get-Percentile([double[]]$Values, [double]$Percentile) {
    if (-not $Values -or $Values.Count -eq 0) { return $null }
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Ceiling(($Percentile / 100.0) * $sorted.Count) - 1
    $index = [Math]::Max(0, [Math]::Min($sorted.Count - 1, $index))
    return [double]$sorted[$index]
}

function Get-MaxGapUs([uint64[]]$Times) {
    if (-not $Times -or $Times.Count -lt 2) { return [uint64]0 }
    [uint64]$maxGap = 0
    for ($i = 1; $i -lt $Times.Count; ++$i) {
        if ($Times[$i] -gt $Times[$i - 1]) {
            [uint64]$gap = $Times[$i] - $Times[$i - 1]
            if ($gap -gt $maxGap) { $maxGap = $gap }
        }
    }
    return $maxGap
}

function Wait-MainWindow([System.Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 100; ++$i) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[ISSUE31] process exited before main window became available exitCode=$($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 200
    }
    throw "[ISSUE31] timed out waiting for main window"
}

function Test-SharedLogContains([string]$Path, [string]$Pattern) {
    if (-not (Test-Path $Path)) { return $false }

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite
        )
        $reader = [System.IO.StreamReader]::new($stream)
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($line -match $Pattern) { return $true }
        }
        return $false
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}

function Wait-ForProcessorEvidence([System.Diagnostics.Process]$Process) {
    $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 250
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[ISSUE31] process exited before processor evidence exitCode=$($Process.ExitCode)"
        }
        if (Test-SharedLogContains $log '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=PROCESSOR_PUBLISH_PROCESSED ') {
            return
        }
    }

    $traceArmed = Test-SharedLogContains $log '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=TRACE_ARMED '
    $firstPcm = Test-SharedLogContains $log '^\[AUDIO-TELEMETRY\] FIRST_COMPLETE_PCM_BLOCK '
    $firstProcessed = Test-SharedLogContains $log '^\[AUDIO-TELEMETRY\] FIRST_PROCESSED_BLOCK '
    throw "[ISSUE31] PROCESSOR_PUBLISH_PROCESSED evidence not observed within $StartupTimeoutSeconds seconds trace_armed=$traceArmed first_pcm=$firstPcm first_processed=$firstProcessed"
}

if (($Scenario -eq "Seek" -or $HidePlayerWindow) -and -not ("Issue31.NativeWindow" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace Issue31 {
    public static class NativeWindow {
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        public const uint WM_CLOSE = 0x0010;
        public const uint WM_KEYDOWN = 0x0100;
        public const uint WM_KEYUP = 0x0101;
        public const int SW_HIDE = 0;

        public static bool PostKey(IntPtr hwnd, int vk, bool extended) {
            uint scan = MapVirtualKey((uint)vk, 0);
            long down = 1L | ((long)scan << 16);
            if (extended) down |= (1L << 24);
            long up = down | (1L << 30) | (1L << 31);
            return PostMessage(hwnd, WM_KEYDOWN, (IntPtr)vk, (IntPtr)down)
                && PostMessage(hwnd, WM_KEYUP, (IntPtr)vk, (IntPtr)up);
        }
    }
}
"@
}

function Send-SeekKey([IntPtr]$Hwnd, [int]$VirtualKey) {
    [Issue31.NativeWindow]::SetForegroundWindow($Hwnd) | Out-Null
    if (-not [Issue31.NativeWindow]::PostKey($Hwnd, $VirtualKey, $true)) {
        throw "[ISSUE31] failed to post seek key vk=$VirtualKey"
    }
}

$source = $null
if ($Scenario -eq "ColdUrl") {
    if ([string]::IsNullOrWhiteSpace($MediaUrl)) {
        throw "[ISSUE31] ColdUrl requires -MediaUrl"
    }
    if ($MediaUrl.Contains("\")) {
        throw "[ISSUE31] ColdUrl contains literal backslash escapes. Pass the URL exactly, e.g. https://www.youtube.com/watch?v=..."
    }
    $source = $MediaUrl
}
else {
    if ([string]::IsNullOrWhiteSpace($MediaPath)) {
        if (-not (Test-Path $generator)) {
            throw "[ISSUE31] deterministic fixture generator missing: $generator"
        }
        $fixture = Join-Path $outDir "issue31-av-fixture.avi"
        $fixtureSeconds = [Math]::Max(90, $ObserveSeconds + 30)
        & python.exe $generator $fixture --seconds $fixtureSeconds --sample-rate 44100
        if ($LASTEXITCODE -ne 0) {
            throw "[ISSUE31] deterministic A/V fixture generation failed"
        }
        $MediaPath = $fixture
    }
    $source = (Resolve-Path $MediaPath -ErrorAction Stop).Path
}

$previousLog = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_LIFECYCLE_LOG",
    [EnvironmentVariableTarget]::Process
)
$previousTrace = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_AUDIO_BACKPRESSURE_TRACE",
    [EnvironmentVariableTarget]::Process
)
$previousPressureDelay = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS",
    [EnvironmentVariableTarget]::Process
)
$previousAnalysisDelay = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_AUDIO_ANALYSIS_DELAY_MS",
    [EnvironmentVariableTarget]::Process
)

$p = $null
$result = "FAIL"
$failure = $null
$seekActions = @()

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $log
    $env:IM_PLAYER_AUDIO_BACKPRESSURE_TRACE = "1"
    if ($Scenario -eq "Pressure") {
        $env:IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS = "250"
    } else {
        Remove-Item Env:IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS -ErrorAction SilentlyContinue
    }
    if ($Scenario -eq "SlowAnalysis") {
        $env:IM_PLAYER_AUDIO_ANALYSIS_DELAY_MS = "100"
    } else {
        Remove-Item Env:IM_PLAYER_AUDIO_ANALYSIS_DELAY_MS -ErrorAction SilentlyContinue
    }

    $quotedSource = '"' + $source + '"'
    Write-Host "[ISSUE31] start commit=$commit configuration=$Configuration scenario=$Scenario source=$source hide_player_window=$HidePlayerWindow"
    if ($HidePlayerWindow) {
        $p = Start-Process -FilePath $exe -ArgumentList $quotedSource -WorkingDirectory $root -WindowStyle Hidden -PassThru
    } else {
        $p = Start-Process -FilePath $exe -ArgumentList $quotedSource -WorkingDirectory $root -PassThru
    }
    $hwnd = Wait-MainWindow $p
    if ($HidePlayerWindow) {
        [Issue31.NativeWindow]::ShowWindow($hwnd, [Issue31.NativeWindow]::SW_HIDE) | Out-Null
        Write-Host "[ISSUE31] player window hidden hwnd=$hwnd"
    }
    Wait-ForProcessorEvidence $p

    if ($Scenario -eq "Seek") {
        Start-Sleep -Seconds 3
        Send-SeekKey $hwnd 0x27
        $seekActions += "RIGHT"
        Start-Sleep -Seconds 2
        Send-SeekKey $hwnd 0x25
        $seekActions += "LEFT"
        Start-Sleep -Seconds 2
        Send-SeekKey $hwnd 0x27
        $seekActions += "RIGHT"
    }

    $deadline = [DateTime]::UtcNow.AddSeconds($ObserveSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Seconds 1
        $p.Refresh()
        if ($p.HasExited) {
            throw "[ISSUE31] process exited during observation exitCode=$($p.ExitCode)"
        }
    }

    if ($HidePlayerWindow) {
        if (-not [Issue31.NativeWindow]::PostMessage(
                $hwnd,
                [Issue31.NativeWindow]::WM_CLOSE,
                [IntPtr]::Zero,
                [IntPtr]::Zero)) {
            throw "[ISSUE31] hidden-window WM_CLOSE request failed"
        }
        Write-Host "[ISSUE31] hidden-window WM_CLOSE posted hwnd=$hwnd"
    } elseif (-not $p.CloseMainWindow()) {
        throw "[ISSUE31] CloseMainWindow returned false"
    }

    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[ISSUE31] shutdown timeout"
    }
    if ($p.ExitCode -ne 0) {
        throw "[ISSUE31] shutdown exitCode=$($p.ExitCode)"
    }

    if (-not (Test-Path $log)) {
        throw "[ISSUE31] evidence log missing"
    }

    [string[]]$lines = @(Get-Content -Path $log | ForEach-Object { [string]$_ })
    $traceArmed = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=TRACE_ARMED '
    })
    $acquire = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=PROCESSOR_ACQUIRE_RAW '
    })
    $analyze = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=PROCESSOR_ANALYZE_END '
    })
    $publish = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=PROCESSOR_PUBLISH_PROCESSED '
    })
    $drops = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] PROCESSOR_PUBLISH_DROP '
    })
    $underflows = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] (ERROR )?SDL_QUEUE_UNDERFLOW '
    })
    $pressureArmed = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] OUTPUT_PRESSURE_ARMED '
    })
    $analysisPolicyArmed = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] ANALYSIS_POLICY_ARMED '
    })
    $analysisEnqueue = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] BACKPRESSURE stage=ANALYSIS_ENQUEUE '
    })
    $analysisDrops = @($lines | Where-Object {
        $_ -match '^\[AUDIO-TELEMETRY\] ANALYSIS_DROP '
    })

    if ($acquire.Count -lt 5 -or $analyze.Count -lt 5 -or $publish.Count -lt 5) {
        throw "[ISSUE31] insufficient processor evidence acquire=$($acquire.Count) analyze=$($analyze.Count) publish=$($publish.Count)"
    }

    $acquireTimes = @()
    $capturePublishTimes = @()
    $captureToProcessorAcquireTimes = @()
    $acquireByBlock = @{}
    foreach ($line in $acquire) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("t_us")) {
            [uint64]$acquireTime = [uint64]$kv["t_us"]
            $acquireTimes += $acquireTime
            if ($kv.Contains("capture_publish_us")) {
                [uint64]$capturePublishTime = [uint64]$kv["capture_publish_us"]
                if ($capturePublishTime -gt 0) {
                    $capturePublishTimes += $capturePublishTime
                    if ($acquireTime -ge $capturePublishTime) {
                        $captureToProcessorAcquireTimes += [double]($acquireTime - $capturePublishTime)
                    }
                }
            }
            if ($kv.Contains("generation") -and $kv.Contains("sequence")) {
                $key = "$($kv["generation"]):$($kv["sequence"])"
                $acquireByBlock[$key] = $acquireTime
            }
        }
    }

    if ($capturePublishTimes.Count -ne $acquire.Count -or
        $captureToProcessorAcquireTimes.Count -ne $acquire.Count) {
        throw "[ISSUE31] incomplete capture->processor evidence acquire=$($acquire.Count) capture_publish=$($capturePublishTimes.Count) capture_to_acquire=$($captureToProcessorAcquireTimes.Count)"
    }

    $durations = @()
    $rawOccupancies = @()
    foreach ($line in $analyze) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("duration_us")) { $durations += [double]$kv["duration_us"] }
        if ($kv.Contains("raw_ring_size")) { $rawOccupancies += [double]$kv["raw_ring_size"] }
    }

    $processedOccupancies = @()
    $retries = @()
    $publishTimes = @()
    $processorServiceTimes = @()
    foreach ($line in $publish) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("processed_ring_size")) { $processedOccupancies += [double]$kv["processed_ring_size"] }
        if ($kv.Contains("retries")) { $retries += [double]$kv["retries"] }
        if ($kv.Contains("t_us")) {
            [uint64]$publishTime = [uint64]$kv["t_us"]
            $publishTimes += $publishTime
            if ($kv.Contains("generation") -and $kv.Contains("sequence")) {
                $key = "$($kv["generation"]):$($kv["sequence"])"
                if ($acquireByBlock.ContainsKey($key) -and $publishTime -ge [uint64]$acquireByBlock[$key]) {
                    $processorServiceTimes += [double]($publishTime - [uint64]$acquireByBlock[$key])
                }
            }
        }
    }

    $dropMetadataComplete = 0
    foreach ($line in $drops) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("retries")) { $retries += [double]$kv["retries"] }
        if ($kv.Contains("reason") -and $kv["reason"] -eq "processed_ring_full" -and
            $kv.Contains("sequence") -and $kv.Contains("generation") -and
            $kv.Contains("pts") -and $kv.Contains("retries")) {
            $dropMetadataComplete++
        }
    }

    $analysisDropMetadataComplete = 0
    $analysisQueueOccupancies = @()
    foreach ($line in $analysisEnqueue) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("analysis_queue_size")) {
            $analysisQueueOccupancies += [double]$kv["analysis_queue_size"]
        }
    }
    foreach ($line in $analysisDrops) {
        $kv = Parse-KeyValues $line
        if ($kv.Contains("analysis_queue_size")) {
            $analysisQueueOccupancies += [double]$kv["analysis_queue_size"]
        }
        if ($kv.Contains("reason") -and $kv["reason"] -eq "analysis_queue_full" -and
            $kv.Contains("sequence") -and $kv.Contains("generation") -and $kv.Contains("pts")) {
            $analysisDropMetadataComplete++
        }
    }

    if ($Scenario -eq "Pressure") {
        if ($pressureArmed.Count -lt 1) {
            throw "[ISSUE31] pressure scenario missing OUTPUT_PRESSURE_ARMED evidence"
        }
        if ($drops.Count -lt 1) {
            throw "[ISSUE31] pressure scenario did not exercise PROCESSOR_PUBLISH_DROP"
        }
        if ($dropMetadataComplete -lt 1) {
            throw "[ISSUE31] pressure drop evidence missing reason/sequence/generation/pts/retries"
        }
        if (-not $retries.Count -or (($retries | Measure-Object -Maximum).Maximum -le 0)) {
            throw "[ISSUE31] pressure scenario did not observe publish retries"
        }
    }

    if ($Scenario -eq "SlowAnalysis") {
        if ($analysisPolicyArmed.Count -lt 1) {
            throw "[ISSUE31] slow-analysis scenario missing ANALYSIS_POLICY_ARMED evidence"
        }
        if ($analysisDrops.Count -lt 1) {
            throw "[ISSUE31] slow-analysis scenario did not drop bounded analysis work"
        }
        if ($analysisDropMetadataComplete -ne $analysisDrops.Count) {
            throw "[ISSUE31] slow-analysis drop metadata incomplete complete=$analysisDropMetadataComplete drops=$($analysisDrops.Count)"
        }
        if ($drops.Count -ne 0) {
            throw "[ISSUE31] slow-analysis must not drop audible processed blocks drops=$($drops.Count)"
        }
        if ($publish.Count -ne $acquire.Count) {
            throw "[ISSUE31] slow-analysis forwarding not lossless acquire=$($acquire.Count) publish=$($publish.Count)"
        }
    }

    if ($Scenario -ne "SlowAnalysis" -and $analysisDrops.Count -ne 0) {
        throw "[ISSUE31] normal scenario unexpectedly dropped analysis work scenario=$Scenario drops=$($analysisDrops.Count)"
    }

    [uint64]$maxCapturePublishGapUs = Get-MaxGapUs $capturePublishTimes
    [uint64]$maxRawAcquireGapUs = Get-MaxGapUs $acquireTimes
    [uint64]$maxPublishGapUs = Get-MaxGapUs $publishTimes
    [uint64]$publishAcquireGapDeltaUs = if ($maxPublishGapUs -ge $maxRawAcquireGapUs) {
        $maxPublishGapUs - $maxRawAcquireGapUs
    } else {
        $maxRawAcquireGapUs - $maxPublishGapUs
    }

    $summary = [ordered]@{
        schema = "ISSUE31-BACKPRESSURE-MEASUREMENT-v2"
        commit = $commit
        configuration = $Configuration
        scenario = $Scenario
        source = $source
        observe_seconds = $ObserveSeconds
        exact_head_build_performed = (-not $SkipBuild)
        player_window_hidden = [bool]$HidePlayerWindow
        trace_enabled = $true
        trace_armed_count = $traceArmed.Count
        seek_actions = $seekActions
        output_pressure_delay_ms = if ($Scenario -eq "Pressure") { 250 } else { 0 }
        output_pressure_armed_count = $pressureArmed.Count
        analysis_delay_ms = if ($Scenario -eq "SlowAnalysis") { 100 } else { 0 }
        analysis_policy_armed_count = $analysisPolicyArmed.Count
        analysis_queue_observed_high_water = if ($analysisQueueOccupancies.Count) { ($analysisQueueOccupancies | Measure-Object -Maximum).Maximum } else { 0 }
        analysis_drop_count = $analysisDrops.Count
        analysis_drop_metadata_complete_count = $analysisDropMetadataComplete
        acquire_samples = $acquire.Count
        analyze_samples = $analyze.Count
        publish_samples = $publish.Count
        processor_duration_us_p50 = Get-Percentile $durations 50
        processor_duration_us_p95 = Get-Percentile $durations 95
        processor_duration_us_p99 = Get-Percentile $durations 99
        processor_duration_us_max = if ($durations.Count) { ($durations | Measure-Object -Maximum).Maximum } else { $null }
        processor_service_us_p95 = Get-Percentile $processorServiceTimes 95
        processor_service_us_p99 = Get-Percentile $processorServiceTimes 99
        processor_service_us_max = if ($processorServiceTimes.Count) { ($processorServiceTimes | Measure-Object -Maximum).Maximum } else { $null }
        max_capture_publish_gap_us = $maxCapturePublishGapUs
        capture_to_processor_acquire_us_p95 = Get-Percentile $captureToProcessorAcquireTimes 95
        capture_to_processor_acquire_us_p99 = Get-Percentile $captureToProcessorAcquireTimes 99
        capture_to_processor_acquire_us_max = if ($captureToProcessorAcquireTimes.Count) { ($captureToProcessorAcquireTimes | Measure-Object -Maximum).Maximum } else { $null }
        max_raw_acquire_gap_us = $maxRawAcquireGapUs
        raw_ring_observed_high_water = if ($rawOccupancies.Count) { ($rawOccupancies | Measure-Object -Maximum).Maximum } else { $null }
        processed_ring_observed_high_water = if ($processedOccupancies.Count) { ($processedOccupancies | Measure-Object -Maximum).Maximum } else { $null }
        publish_retries_max = if ($retries.Count) { ($retries | Measure-Object -Maximum).Maximum } else { $null }
        processor_publish_drop_count = $drops.Count
        processor_publish_drop_metadata_complete_count = $dropMetadataComplete
        sdl_underflow_count = $underflows.Count
        max_processor_publish_gap_us = $maxPublishGapUs
        publish_vs_acquire_gap_delta_us = $publishAcquireGapDeltaUs
        root_cause = "NOT_INFERRED"
        result = "PASS_EVIDENCE_CAPTURED"
        evidence_log = $log
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryPath -Encoding UTF8
    $result = "PASS_EVIDENCE_CAPTURED"

    Write-Host "[ISSUE31] PASS_EVIDENCE_CAPTURED commit=$commit scenario=$Scenario analyze=$($analyze.Count) publish=$($publish.Count) drops=$($drops.Count) underflows=$($underflows.Count) max_capture_publish_gap_us=$maxCapturePublishGapUs max_acquire_gap_us=$maxRawAcquireGapUs max_publish_gap_us=$maxPublishGapUs gap_delta_us=$publishAcquireGapDeltaUs"
    Write-Host "[ISSUE31] summary=$summaryPath"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[ISSUE31] ERROR: $failure"
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }

    $failureSummary = [ordered]@{
        schema = "ISSUE31-BACKPRESSURE-MEASUREMENT-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $Scenario
        source = $source
        observe_seconds = $ObserveSeconds
        exact_head_build_performed = (-not $SkipBuild)
        player_window_hidden = [bool]$HidePlayerWindow
        root_cause = "NOT_INFERRED"
        result = "FAIL"
        failure = $failure
        evidence_log = $log
    }
    $failureSummary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryPath -Encoding UTF8
    throw
}
finally {
    if ($null -eq $previousLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLog
    }

    if ($null -eq $previousTrace) {
        Remove-Item Env:IM_PLAYER_AUDIO_BACKPRESSURE_TRACE -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_AUDIO_BACKPRESSURE_TRACE = $previousTrace
    }

    if ($null -eq $previousPressureDelay) {
        Remove-Item Env:IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_AUDIO_OUTPUT_PRESSURE_DELAY_MS = $previousPressureDelay
    }

    if ($null -eq $previousAnalysisDelay) {
        Remove-Item Env:IM_PLAYER_AUDIO_ANALYSIS_DELAY_MS -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_AUDIO_ANALYSIS_DELAY_MS = $previousAnalysisDelay
    }
}
