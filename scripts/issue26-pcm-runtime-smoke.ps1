[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [ValidateRange(3, 20)]
    [int]$PlaybackSeconds = 7,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"
$generator = Join-Path $PSScriptRoot "issue26_generate_pcm_fixture.py"
$artifactRoot = Join-Path $root "artifacts\issue26"
$configKey = $Configuration.ToLowerInvariant()
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) { throw "[ISSUE26] Missing executable: $exe" }
New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null

$cases = @(
    [ordered]@{ name = "44100-mono";   rate = 44100; channels = 1 },
    [ordered]@{ name = "44100-stereo"; rate = 44100; channels = 2 },
    [ordered]@{ name = "48000-mono";   rate = 48000; channels = 1 },
    [ordered]@{ name = "48000-stereo"; rate = 48000; channels = 2 }
)

function Wait-MainWindow([System.Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 50; ++$i) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[ISSUE26] process exited before main window became available exitCode=$($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 200
    }
    throw "[ISSUE26] timed out waiting for main window"
}

function Require-Marker([string[]]$Lines, [string]$Pattern, [string]$Label) {
    $matches = @($Lines | Where-Object { $_ -match $Pattern })
    if ($matches.Count -lt 1) {
        throw "[ISSUE26] missing telemetry marker: $Label"
    }
    return [string]$matches[0]
}

$previousLifecycleLog = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_LIFECYCLE_LOG",
    [EnvironmentVariableTarget]::Process
)

$results = @()
try {
    foreach ($case in $cases) {
        $name = [string]$case.name
        $rate = [int]$case.rate
        $channels = [int]$case.channels

        $fixture = Join-Path $artifactRoot "$name.wav"
        $log = Join-Path $artifactRoot "$configKey-$name.log"
        $json = Join-Path $artifactRoot "$configKey-$name.json"

        Remove-Item $fixture, $log, $json -Force -ErrorAction SilentlyContinue

        & python.exe $generator $fixture --seconds 12 --sample-rate $rate --channels $channels
        if ($LASTEXITCODE -ne 0) {
            throw "[ISSUE26] fixture generation failed case=$name"
        }

        $env:IM_PLAYER_LIFECYCLE_LOG = $log
        $quotedMedia = '"' + $fixture + '"'
        $p = $null
        try {
            Write-Host "[ISSUE26] start commit=$commit configuration=$Configuration case=$name source_rate=$rate source_channels=$channels"
            $p = Start-Process -FilePath $exe -ArgumentList $quotedMedia -WorkingDirectory $root -PassThru
            $null = Wait-MainWindow $p
            Start-Sleep -Seconds $PlaybackSeconds
            $p.Refresh()
            if ($p.HasExited) {
                throw "[ISSUE26] process exited during playback case=$name exitCode=$($p.ExitCode)"
            }

            if (-not $p.CloseMainWindow()) {
                throw "[ISSUE26] CloseMainWindow returned false case=$name"
            }
            if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
                Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
                throw "[ISSUE26] shutdown timeout case=$name"
            }
            if ($p.ExitCode -ne 0) {
                throw "[ISSUE26] shutdown exitCode=$($p.ExitCode) case=$name"
            }
        }
        finally {
            if ($p -and -not $p.HasExited) {
                Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            }
        }

        if (-not (Test-Path $log)) {
            throw "[ISSUE26] evidence log missing case=$name"
        }

        & python.exe $validator $log
        if ($LASTEXITCODE -ne 0) {
            throw "[ISSUE26] lifecycle validation failed case=$name"
        }

        [string[]]$lines = @(Get-Content -Path $log | ForEach-Object { [string]$_ })

        $expectedEffective = [ordered]@{
            "ao-pcm-waveheader" = "no"
            "audio-format" = "float"
            "audio-samplerate" = "48000"
            "audio-channels" = "stereo"
            "ao" = "pcm"
        }
        $effectiveProperties = [ordered]@{}

        foreach ($property in @(
            "ao-pcm-waveheader",
            "audio-format",
            "audio-samplerate",
            "audio-channels",
            "ao-pcm-file",
            "ao"
        )) {
            $escapedProperty = [regex]::Escape($property)
            $setMarker = Require-Marker $lines "^\[AUDIO-TELEMETRY\] MPV_PROPERTY_SET name=$escapedProperty " "MPV_PROPERTY_SET $property"
            $effectiveMarker = Require-Marker $lines "^\[AUDIO-TELEMETRY\] MPV_PROPERTY_EFFECTIVE name=$escapedProperty " "MPV_PROPERTY_EFFECTIVE $property"

            if ($effectiveMarker -notmatch " value=(.+)$") {
                throw "[ISSUE26] unable to parse effective property case=$name property=$property marker=$effectiveMarker"
            }
            $effectiveValue = [string]$Matches[1]
            $effectiveProperties[$property] = $effectiveValue

            if ($expectedEffective.Contains($property)) {
                $expectedValue = [string]$expectedEffective[$property]
                if ($effectiveValue -ne $expectedValue) {
                    throw "[ISSUE26] effective property mismatch case=$name property=$property expected=$expectedValue actual=$effectiveValue"
                }
            }

            if ($property -eq "ao-pcm-file" -and
                $effectiveValue -notmatch "^\\\\\.\\pipe\\mpv_pcm_") {
                throw "[ISSUE26] effective ao-pcm-file is not the canonical named pipe case=$name value=$effectiveValue"
            }

            if ($setMarker -notmatch " result=0$") {
                throw "[ISSUE26] required property set did not return success case=$name property=$property marker=$setMarker"
            }
        }

        $pipeConnected = Require-Marker $lines "^\[AUDIO-TELEMETRY\] PIPE_CONNECTED " "PIPE_CONNECTED"
        $pipeBytes = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_PIPE_BYTES " "FIRST_PIPE_BYTES"
        $rawPayload = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_RAW_PCM_PAYLOAD " "FIRST_RAW_PCM_PAYLOAD"
        if ($rawPayload -match "preview=\[52 49 46 46") {
            throw "[ISSUE26] WAVE RIFF header reached raw PCM consumer case=$name marker=$rawPayload"
        }

        $completeBlock = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_COMPLETE_PCM_BLOCK " "FIRST_COMPLETE_PCM_BLOCK"
        if ($completeBlock -notmatch "carry_bytes=(\d+) sample_rate=(\d+) channels=(\d+) format=(\S+)") {
            throw "[ISSUE26] unable to parse complete-block format case=$name marker=$completeBlock"
        }
        $carryBytes = [int]$Matches[1]
        $blockRate = [int]$Matches[2]
        $blockChannels = [int]$Matches[3]
        $blockFormat = [string]$Matches[4]
        if ($carryBytes -ge 8 -or $blockRate -ne 48000 -or $blockChannels -ne 2 -or $blockFormat -ne "float32") {
            throw "[ISSUE26] published block violates canonical contract case=$name marker=$completeBlock"
        }
        $processedBlock = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_PROCESSED_BLOCK " "FIRST_PROCESSED_BLOCK"
        $sdlFormat = Require-Marker $lines "^\[AUDIO-TELEMETRY\] SDL_FORMAT " "SDL_FORMAT"
        $sdlWrite = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_SDL_WRITE " "FIRST_SDL_WRITE"
        $nonzeroQueue = Require-Marker $lines "^\[AUDIO-TELEMETRY\] FIRST_NONZERO_SDL_QUEUE " "FIRST_NONZERO_SDL_QUEUE"

        if ($sdlFormat -notmatch "requested_rate=(\d+) requested_channels=(\d+) requested_format=(0x[0-9A-Fa-f]+) obtained_rate=(\d+) obtained_channels=(\d+) obtained_format=(0x[0-9A-Fa-f]+)") {
            throw "[ISSUE26] unable to parse SDL format case=$name marker=$sdlFormat"
        }
        $requestedRate = [int]$Matches[1]
        $requestedChannels = [int]$Matches[2]
        $requestedFormat = [string]$Matches[3]
        $obtainedRate = [int]$Matches[4]
        $obtainedChannels = [int]$Matches[5]
        $obtainedFormat = [string]$Matches[6]
        if ($requestedRate -ne 48000 -or
            $requestedChannels -ne 2 -or
            $obtainedRate -ne $requestedRate -or
            $obtainedChannels -ne $requestedChannels -or
            $obtainedFormat -ne $requestedFormat) {
            throw "[ISSUE26] SDL output not canonical case=$name marker=$sdlFormat"
        }

        $summaryMarker = Require-Marker $lines "^\[AUDIO-TELEMETRY\] SUMMARY " "SUMMARY"
        if ($summaryMarker -notmatch "capture_blocks=(\d+) capture_dropped=(\d+) capture_bytes=(\d+) ring_overflows=(\d+) carry_bytes=(\d+) generation=(\d+) output_blocks=(\d+) format_mismatch=(\d+) underflows=(\d+) write_failures=(\d+) queued_bytes=(\d+) queued_ms=([-+0-9.eE]+) queue_high_water_bytes=(\d+) last_written_end_pts=([-+0-9.eE]+) mpv_time_pos=([-+0-9.eE]+) audible_head_pts=([-+0-9.eE]+) av_offset_s=([-+0-9.eE]+)") {
            throw "[ISSUE26] unable to parse runtime summary case=$name marker=$summaryMarker"
        }
        $captureBlocks = [uint64]$Matches[1]
        $captureDropped = [uint64]$Matches[2]
        $captureBytes = [uint64]$Matches[3]
        $ringOverflows = [uint64]$Matches[4]
        $summaryCarryBytes = [uint64]$Matches[5]
        $generation = [uint64]$Matches[6]
        $outputBlocks = [uint64]$Matches[7]
        $formatMismatch = [uint64]$Matches[8]
        $underflows = [uint64]$Matches[9]
        $writeFailures = [uint64]$Matches[10]
        $queuedBytes = [uint32]$Matches[11]
        $queuedMs = [double]$Matches[12]
        $queueHighWaterBytes = [uint32]$Matches[13]
        $lastWrittenEndPts = [double]$Matches[14]
        $mpvTimePos = [double]$Matches[15]
        $audibleHeadPts = [double]$Matches[16]
        $avOffsetSeconds = [double]$Matches[17]

        if ($captureBlocks -eq 0 -or $captureBytes -eq 0 -or $outputBlocks -eq 0) {
            throw "[ISSUE26] runtime summary contains no audio progress case=$name marker=$summaryMarker"
        }
        if ($captureDropped -ne 0 -or $ringOverflows -ne 0 -or
            $formatMismatch -ne 0 -or $writeFailures -ne 0) {
            throw "[ISSUE26] runtime summary reports loss/error case=$name marker=$summaryMarker"
        }
        if ($summaryCarryBytes -ge 8) {
            throw "[ISSUE26] invalid final partial-frame carry case=$name carry_bytes=$summaryCarryBytes"
        }

        $errorLines = @($lines | Where-Object { $_ -match "^\[AUDIO-TELEMETRY\] ERROR " })
        if ($errorLines.Count -gt 0) {
            throw "[ISSUE26] telemetry error case=$name first=$($errorLines[0])"
        }

        $orderedPatterns = @(
            "^\[AUDIO-TELEMETRY\] PIPE_CONNECTED ",
            "^\[AUDIO-TELEMETRY\] FIRST_PIPE_BYTES ",
            "^\[AUDIO-TELEMETRY\] FIRST_COMPLETE_PCM_BLOCK ",
            "^\[AUDIO-TELEMETRY\] FIRST_PROCESSED_BLOCK ",
            "^\[AUDIO-TELEMETRY\] FIRST_SDL_WRITE ",
            "^\[AUDIO-TELEMETRY\] FIRST_NONZERO_SDL_QUEUE "
        )
        $lastIndex = -1
        foreach ($pattern in $orderedPatterns) {
            $index = -1
            for ($i = 0; $i -lt $lines.Count; ++$i) {
                if ($lines[$i] -match $pattern) {
                    $index = $i
                    break
                }
            }
            if ($index -lt 0 -or $index -le $lastIndex) {
                throw "[ISSUE26] startup timeline order invalid case=$name pattern=$pattern index=$index previous=$lastIndex"
            }
            $lastIndex = $index
        }

        $caseResult = [ordered]@{
            schema = "ISSUE26-PCM-RUNTIME-v1"
            commit = $commit
            configuration = $Configuration
            case = $name
            source_sample_rate = $rate
            source_channels = $channels
            result = "PASS"
            pipe_connected = $pipeConnected
            first_pipe_bytes = $pipeBytes
            first_raw_pcm_payload = $rawPayload
            first_complete_pcm_block = $completeBlock
            first_processed_block = $processedBlock
            effective_mpv_properties = $effectiveProperties
            block_sample_rate = $blockRate
            block_channels = $blockChannels
            block_format = $blockFormat
            partial_frame_carry_bytes = $carryBytes
            sdl_format = $sdlFormat
            sdl_requested_format = $requestedFormat
            sdl_obtained_format = $obtainedFormat
            first_sdl_write = $sdlWrite
            first_nonzero_sdl_queue = $nonzeroQueue
            runtime_summary = $summaryMarker
            capture_blocks = $captureBlocks
            capture_dropped = $captureDropped
            ring_overflows = $ringOverflows
            output_blocks = $outputBlocks
            format_mismatch = $formatMismatch
            underflows = $underflows
            write_failures = $writeFailures
            queued_bytes_at_shutdown = $queuedBytes
            queued_ms_at_shutdown = $queuedMs
            queue_high_water_bytes = $queueHighWaterBytes
            last_written_end_pts = $lastWrittenEndPts
            mpv_time_pos = $mpvTimePos
            audible_head_pts = $audibleHeadPts
            av_offset_seconds = $avOffsetSeconds
            lifecycle_validation = "PASS"
            telemetry_error_count = 0
            evidence_log = $log
        }
        $caseResult | ConvertTo-Json -Depth 6 | Set-Content -Path $json -Encoding UTF8
        $results += [pscustomobject]$caseResult
        Write-Host "[ISSUE26] PASS case=$name configuration=$Configuration evidence=$json"
    }
}
finally {
    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }
}

$summaryPath = Join-Path $artifactRoot "$configKey-summary.json"
$summary = [ordered]@{
    schema = "ISSUE26-PCM-RUNTIME-SUMMARY-v1"
    commit = $commit
    configuration = $Configuration
    cases_requested = $cases.Count
    cases_completed = $results.Count
    result = if ($results.Count -eq $cases.Count) { "PASS" } else { "FAIL" }
    cases = $results
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -Path $summaryPath -Encoding UTF8

if ($results.Count -ne $cases.Count) {
    throw "[ISSUE26] runtime matrix incomplete completed=$($results.Count)/$($cases.Count)"
}

Write-Host "[ISSUE26] PASS commit=$commit configuration=$Configuration cases=$($results.Count)/$($cases.Count)"
Write-Host "[ISSUE26] summary=$summaryPath"
