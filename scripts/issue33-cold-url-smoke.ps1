[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [Parameter(Mandatory = $true)]
    [string]$MediaUrl,

    [ValidateRange(1, 100)]
    [int]$Iterations = 20,

    [ValidateRange(5, 180)]
    [int]$StartupTimeoutSeconds = 60,

    [ValidateRange(100, 5000)]
    [int]$PostStartupObserveMilliseconds = 1000
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
if (-not (Test-Path $exe)) {
    throw "[ISSUE-33] missing executable: $exe"
}

$commit = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) {
    throw "[ISSUE-33] unable to resolve git HEAD"
}

$shortSha = $commit.Substring(0, [Math]::Min(12, $commit.Length))
$outDir = Join-Path $root "artifacts\issue33\cold-url\$($Configuration.ToLowerInvariant())-$shortSha"
New-Item -ItemType Directory -Force $outDir | Out-Null

function Assert-NoExistingImPlayer {
    $existing = @(Get-Process -Name "Im_player" -ErrorAction SilentlyContinue)
    if ($existing.Count -gt 0) {
        $pids = ($existing | ForEach-Object { $_.Id }) -join ","
        throw "[ISSUE-33] precondition failed: existing Im_player process detected pid=$pids. Close all Im_player.exe instances before cold-start validation."
    }
}

Assert-NoExistingImPlayer

$summary = [ordered]@{
    schema = "ISSUE33-COLD-URL-v2-COMBINED"
    commit = $commit
    configuration = $Configuration
    iterations_requested = $Iterations
    iterations_passed = 0
    result = "FAIL"
    runs = @()
}

function Get-StartupLines([string]$Path) {
    if (-not (Test-Path $Path)) { return @() }

    # The application opens IM_PLAYER_LIFECYCLE_LOG for append on every
    # diagnostic marker. Use an explicitly shared reader so polling cannot
    # transiently block fopen_s("ab") and silently drop startup evidence.
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
        $lines = @()
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($line -match "^\[BRG5-DIAG\] category=STARTUP ") {
                $lines += $line
            }
        }
        return @($lines)
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}

# STARTUP/audio/video evidence may be polled live because every writer uses explicit shared Win32 append semantics.
function Get-EvidenceLines([string]$Path) {
    if (-not (Test-Path $Path)) { return @() }

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
        $lines = @()
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($line -match "^\[BRG5-DIAG\] category=STARTUP " -or
                $line -match "^\[AUDIO-TELEMETRY\] ") {
                $lines += $line
            }
        }
        return @($lines)
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}

function Find-StartupIndex([string[]]$Lines, [string]$Token) {
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i].Contains($Token)) { return $i }
    }
    return -1
}

function Find-LastStartupIndex([string[]]$Lines, [string]$Token) {
    for ($i = $Lines.Count - 1; $i -ge 0; $i--) {
        if ($Lines[$i].Contains($Token)) { return $i }
    }
    return -1
}


for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
    Assert-NoExistingImPlayer
    $logPath = Join-Path $outDir ("run-{0:D2}.log" -f $iteration)
    if (Test-Path $logPath) { Remove-Item -Force $logPath }

    $oldLifecycleLog = $env:IM_PLAYER_LIFECYCLE_LOG
    $env:IM_PLAYER_LIFECYCLE_LOG = $logPath
    $process = $null
    $run = [ordered]@{
        iteration = $iteration
        result = "FAIL"
        load_id = $null
        playback_restart_count = $null
        mpv_internal_seek_count = 0
        startup_retry_count = 0
        combined_trace = $null
        startup_log = $logPath
        failure = $null
    }

    try {
        $quotedMediaUrl = '"' + $MediaUrl + '"'
        $process = Start-Process -FilePath $exe -ArgumentList $quotedMediaUrl -WorkingDirectory $root -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
        $evidence = @()
        while ([DateTime]::UtcNow -lt $deadline) {
            if ($process.HasExited) {
                throw "process exited during startup, exitCode=$($process.ExitCode)"
            }

            $evidence = Get-EvidenceLines $logPath
            $hasDeferred = ($evidence | Where-Object { $_ -match "event=CLI_LOAD_DEFERRED " }).Count -gt 0
            $hasIdleReady = ($evidence | Where-Object { $_ -match "event=MPV_IDLE_READY " }).Count -gt 0
            $hasDispatch = ($evidence | Where-Object { $_ -match "event=CLI_LOAD_DISPATCH " }).Count -gt 0
            $hasLoad = ($evidence | Where-Object { $_ -match "event=LOAD_REQUEST " }).Count -gt 0
            $hasStart = ($evidence | Where-Object { $_ -match "event=START_FILE " }).Count -gt 0
            $hasLoaded = ($evidence | Where-Object { $_ -match "event=FILE_LOADED " }).Count -gt 0
            $hasFirstPcm = ($evidence | Where-Object { $_ -match "^\[AUDIO-TELEMETRY\] FIRST_COMPLETE_PCM_BLOCK " }).Count -gt 0
            $hasFirstProcessed = ($evidence | Where-Object { $_ -match "^\[AUDIO-TELEMETRY\] FIRST_PROCESSED_BLOCK " }).Count -gt 0
            $hasFirstSdlWrite = ($evidence | Where-Object { $_ -match "^\[AUDIO-TELEMETRY\] FIRST_SDL_WRITE " }).Count -gt 0
            $hasFirstNonzeroQueue = ($evidence | Where-Object { $_ -match "^\[AUDIO-TELEMETRY\] FIRST_NONZERO_SDL_QUEUE " }).Count -gt 0
            $hasVideoEvidenceArm = ($evidence | Where-Object { $_ -match "event=VIDEO_EVIDENCE_ARM " }).Count -gt 0
            $hasFirstVideoFrame = ($evidence | Where-Object { $_ -match "event=FIRST_VIDEO_FRAME " }).Count -gt 0

            if ($hasDeferred -and $hasIdleReady -and $hasDispatch -and $hasLoad -and $hasStart -and $hasLoaded -and $hasFirstPcm -and $hasFirstProcessed -and $hasFirstSdlWrite -and $hasFirstNonzeroQueue -and $hasVideoEvidenceArm -and $hasFirstVideoFrame) {
                break
            }
            Start-Sleep -Milliseconds 100
        }

        Start-Sleep -Milliseconds $PostStartupObserveMilliseconds

        if (-not $process.CloseMainWindow()) {
            throw "CloseMainWindow returned false"
        }
        if (-not $process.WaitForExit(10000)) {
            throw "process did not exit within 10s after close"
        }
        if ($process.ExitCode -ne 0) {
            throw "process exitCode=$($process.ExitCode)"
        }

        $startup = Get-StartupLines $logPath
        $missingYtdl = @($startup | Where-Object { $_ -match "event=YTDL_PATH_MISSING" })
        if ($missingYtdl.Count -gt 0) {
            throw "yt-dlp executable was not resolved before MPV initialization"
        }
        $resolvedYtdl = @($startup | Where-Object { $_ -match "event=YTDL_PATH_RESOLVED " })
        if ($resolvedYtdl.Count -eq 0) {
            throw "YTDL_PATH_RESOLVED marker not observed"
        }
        if ($resolvedYtdl[0] -notmatch "result=0") {
            throw "MPV rejected deterministic yt-dlp path option: $($resolvedYtdl[0])"
        }

        if (($startup | Where-Object { $_ -match "event=CLI_LOAD_DEFERRED " }).Count -eq 0) {
            throw "CLI_LOAD_DEFERRED marker not observed"
        }
        if (($startup | Where-Object { $_ -match "event=MPV_IDLE_READY " }).Count -eq 0) {
            throw "MPV_IDLE_READY marker not observed"
        }
        if (($startup | Where-Object { $_ -match "event=CLI_LOAD_DISPATCH " }).Count -eq 0) {
            throw "CLI_LOAD_DISPATCH marker not observed"
        }
        if (($startup | Where-Object { $_ -match "event=LOAD_REQUEST " }).Count -eq 0) {
            throw "LOAD_REQUEST marker not observed"
        }
        if (($startup | Where-Object { $_ -match "event=START_FILE " }).Count -eq 0) {
            throw "START_FILE marker not observed"
        }
        if (($startup | Where-Object { $_ -match "event=FILE_LOADED " }).Count -eq 0) {
            $endFile = @($startup | Where-Object { $_ -match "event=END_FILE " } | Select-Object -Last 1)
            if ($endFile.Count -gt 0) {
                throw "FILE_LOADED not observed; $($endFile[0])"
            }
            throw "FILE_LOADED not observed within $StartupTimeoutSeconds seconds after START_FILE"
        }
        $videoEvidenceArms = @($startup | Where-Object { $_ -match "event=VIDEO_EVIDENCE_ARM " })
        if ($videoEvidenceArms.Count -ne 1) {
            throw "expected exactly one VIDEO_EVIDENCE_ARM marker, observed=$($videoEvidenceArms.Count)"
        }
        if ($videoEvidenceArms[0] -notmatch "boundary=video_reconfig") {
            throw "VIDEO_EVIDENCE_ARM missing video-reconfig boundary attribution"
        }

        $firstVideoFrames = @($startup | Where-Object { $_ -match "event=FIRST_VIDEO_FRAME " })
        if ($firstVideoFrames.Count -ne 1) {
            throw "expected exactly one FIRST_VIDEO_FRAME marker, observed=$($firstVideoFrames.Count)"
        }
        if ($firstVideoFrames[0] -notmatch "armed_before_render=1") {
            throw "FIRST_VIDEO_FRAME missing armed-before-render proof"
        }
        if ($firstVideoFrames[0] -notmatch "boundary=video_reconfig") {
            throw "FIRST_VIDEO_FRAME missing video-reconfig boundary attribution"
        }
        $loadLine = $startup | Where-Object { $_ -match "event=LOAD_REQUEST " } | Select-Object -First 1
        if ($loadLine -notmatch "flags=replace") {
            throw "direct startup did not use replace semantics"
        }
        if ($loadLine -notmatch "load_id=(\d+)") {
            throw "LOAD_REQUEST missing load_id"
        }
        $loadId = [UInt64]$Matches[1]
        $run.load_id = $loadId

        $startLine = $startup | Where-Object { $_ -match "event=START_FILE " } | Select-Object -First 1
        if ($startLine -notmatch "pending_seek=-1\.000") {
            throw "fresh START_FILE has armed pending seek"
        }

        $foreignLoad = $startup | Where-Object {
            $_ -match "load_id=(\d+)" -and [UInt64]$Matches[1] -ne $loadId
        }
        if ($foreignLoad.Count -gt 0) {
            throw "multiple startup load_id values observed in one fresh-process run"
        }

        $seekRequests = @($startup | Where-Object { $_ -match "event=SEEK_REQUEST " })
        if ($seekRequests.Count -gt 0) {
            throw "default startup emitted SEEK_REQUEST"
        }

        $retryLines = @($startup | Where-Object { $_ -match "event=LOAD_RETRY_REQUEST " })
        if ($retryLines.Count -gt 1) {
            throw "LOAD_RETRY_REQUEST count exceeded bounded startup retry policy: $($retryLines.Count)"
        }
        $run.startup_retry_count = $retryLines.Count

        $retryExhausted = @($startup | Where-Object { $_ -match "event=EARLY_EOF_RETRY_EXHAUSTED " })
        if ($retryExhausted.Count -gt 0) {
            throw "EARLY_EOF_RETRY_EXHAUSTED is a startup failure"
        }

        $retryIndex = Find-StartupIndex $startup "event=LOAD_RETRY_REQUEST "
        if ($retryLines.Count -eq 1) {
            if ($retryLines[0] -notmatch "load_id=$loadId\b" -or
                $retryLines[0] -notmatch "attempt=2" -or
                $retryLines[0] -notmatch "reason=early_eof_before_first_frame") {
                throw "LOAD_RETRY_REQUEST missing bounded same-load early-EOF attribution"
            }

            $earlyEofIndex = Find-StartupIndex $startup "event=EARLY_EOF_DETECTED "
            if ($earlyEofIndex -lt 0 -or $earlyEofIndex -ge $retryIndex) {
                throw "LOAD_RETRY_REQUEST is not preceded by EARLY_EOF_DETECTED"
            }
        }

        $ytdlIndex = Find-StartupIndex $startup "event=YTDL_PATH_RESOLVED "
        $deferredIndex = Find-StartupIndex $startup "event=CLI_LOAD_DEFERRED "
        $idleReadyIndex = Find-StartupIndex $startup "event=MPV_IDLE_READY "
        $dispatchIndex = Find-StartupIndex $startup "event=CLI_LOAD_DISPATCH "
        $loadIndex = Find-StartupIndex $startup "event=LOAD_REQUEST "
        $startIndex = if ($retryIndex -ge 0) { Find-LastStartupIndex $startup "event=START_FILE " } else { Find-StartupIndex $startup "event=START_FILE " }
        $loadedIndex = if ($retryIndex -ge 0) { Find-LastStartupIndex $startup "event=FILE_LOADED " } else { Find-StartupIndex $startup "event=FILE_LOADED " }
        $videoEvidenceArmIndex = Find-StartupIndex $startup "event=VIDEO_EVIDENCE_ARM "
        $firstVideoIndex = Find-StartupIndex $startup "event=FIRST_VIDEO_FRAME "
        $configBeginIndex = Find-StartupIndex $startup "event=DYNAMIC_CONFIG_APPLY_BEGIN "
        $configEndIndex = Find-StartupIndex $startup "event=DYNAMIC_CONFIG_APPLY_END "
        $configDeferIndex = Find-StartupIndex $startup "event=DYNAMIC_CONFIG_DEFER "
        $formatDiscoveredIndex = Find-StartupIndex $startup "event=YTDL_FORMAT_DISCOVERED "

        if ($ytdlIndex -lt 0 -or $deferredIndex -le $ytdlIndex -or $idleReadyIndex -le $deferredIndex -or $dispatchIndex -le $idleReadyIndex -or $loadIndex -le $dispatchIndex -or $startIndex -le $loadIndex -or $loadedIndex -le $startIndex -or $videoEvidenceArmIndex -le $loadedIndex -or $firstVideoIndex -le $videoEvidenceArmIndex) {
            throw "startup event order is not YTDL_PATH_RESOLVED -> CLI_LOAD_DEFERRED -> MPV_IDLE_READY -> CLI_LOAD_DISPATCH -> LOAD_REQUEST -> START_FILE -> FILE_LOADED -> VIDEO_EVIDENCE_ARM -> FIRST_VIDEO_FRAME"
        }

        $hasConfigApply = $configBeginIndex -gt $loadedIndex -and $configEndIndex -gt $configBeginIndex
        $hasConfigDefer = $configDeferIndex -gt $loadedIndex
        if ($hasConfigApply) {
            throw "cold URL active load must not apply source-specific dynamic config after FILE_LOADED"
        }
        if (-not $hasConfigDefer) {
            throw "cold URL active load missing DYNAMIC_CONFIG_DEFER boundary"
        }
        $deferLine = $startup[$configDeferIndex]
        if ($deferLine -notmatch "reason=active_load_frozen") {
            throw "DYNAMIC_CONFIG_DEFER missing active_load_frozen attribution"
        }
        if ($formatDiscoveredIndex -lt 0) {
            throw "cold URL load missing YTDL_FORMAT_DISCOVERED attribution"
        }


        $appSeekLines = @($startup | Where-Object { $_ -match "event=APP_SEEK_COMMAND " })
        if ($appSeekLines.Count -gt 0) {
            throw "unexpected C++ app-issued seek during default cold startup: $($appSeekLines -join '; ')"
        }

        $restartLines = @()
        $restartScanStart = if ($retryIndex -ge 0) { $retryIndex + 1 } else { 0 }
        for ($i = $restartScanStart; $i -lt $startup.Count; $i++) {
            if ($startup[$i] -match "event=PLAYBACK_RESTART ") {
                $restartLines += $startup[$i]
            }
        }

        $maxRestart = 0
        foreach ($line in $restartLines) {
            if ($line -notmatch "pending_seek_before=-1\.000") {
                throw "PLAYBACK_RESTART observed with an armed startup seek: $line"
            }
            if ($line -match "count=(\d+)") {
                $maxRestart = [Math]::Max($maxRestart, [int]$Matches[1])
            }
        }
        $internalSeekLines = @()
        for ($i = $restartScanStart; $i -lt $startup.Count; $i++) {
            if ($startup[$i] -match "event=SEEK " -and $startup[$i] -match "source=mpv_event") {
                $internalSeekLines += $startup[$i]
            }
        }

        # #33 owns app-generated startup control churn. mpv/ytdl/timeline may
        # legitimately emit more than one PLAYBACK_RESTART while selecting and
        # opening EDL tracks. Do not fail on the raw restart count alone:
        # APP_SEEK_COMMAND / SEEK_REQUEST / armed pending seek remain hard
        # failures, while final startup milestones + bounded early-EOF policy
        # decide whether an internal restart actually broke startup.
        $run.playback_restart_count = $maxRestart
        $run.mpv_internal_seek_count = $internalSeekLines.Count

        # Audio telemetry uses a separate append sink. Read the combined evidence
        # only after process exit so the harness cannot perturb writer sharing or
        # manufacture an evidence false negative while playback is active.
        $evidence = Get-EvidenceLines $logPath
        $combinedLoadIndex = Find-StartupIndex $evidence "event=LOAD_REQUEST "
        $combinedStartIndex = if ($retryIndex -ge 0) { Find-LastStartupIndex $evidence "event=START_FILE " } else { Find-StartupIndex $evidence "event=START_FILE " }
        $combinedLoadedIndex = if ($retryIndex -ge 0) { Find-LastStartupIndex $evidence "event=FILE_LOADED " } else { Find-StartupIndex $evidence "event=FILE_LOADED " }
        $firstPcmIndex = Find-StartupIndex $evidence "FIRST_COMPLETE_PCM_BLOCK "
        $firstProcessedIndex = Find-StartupIndex $evidence "FIRST_PROCESSED_BLOCK "
        $firstSdlWriteIndex = Find-StartupIndex $evidence "FIRST_SDL_WRITE "
        $firstNonzeroQueueIndex = Find-StartupIndex $evidence "FIRST_NONZERO_SDL_QUEUE "
        $combinedVideoArmIndex = Find-StartupIndex $evidence "event=VIDEO_EVIDENCE_ARM "
        $combinedFirstVideoIndex = Find-StartupIndex $evidence "event=FIRST_VIDEO_FRAME "

        if ($combinedLoadIndex -lt 0 -or
            $combinedStartIndex -le $combinedLoadIndex -or
            $combinedLoadedIndex -le $combinedStartIndex -or
            $firstPcmIndex -le $combinedLoadedIndex -or
            $firstProcessedIndex -le $firstPcmIndex -or
            $firstSdlWriteIndex -le $firstProcessedIndex -or
            $firstNonzeroQueueIndex -le $firstSdlWriteIndex -or
            $combinedVideoArmIndex -le $combinedLoadedIndex -or
            $combinedFirstVideoIndex -le $combinedVideoArmIndex -or
            $combinedFirstVideoIndex -le $firstNonzeroQueueIndex) {
            throw "combined startup event order is not LOAD_REQUEST -> START_FILE -> FILE_LOADED -> FIRST_COMPLETE_PCM_BLOCK -> FIRST_PROCESSED_BLOCK -> FIRST_SDL_WRITE -> FIRST_NONZERO_SDL_QUEUE -> FIRST_VIDEO_FRAME with VIDEO_EVIDENCE_ARM after FILE_LOADED"
        }

        $run.combined_trace = [ordered]@{
            load_request = $combinedLoadIndex
            start_file = $combinedStartIndex
            file_loaded = $combinedLoadedIndex
            startup_retry = $retryIndex
            first_complete_pcm_block = $firstPcmIndex
            first_processed_block = $firstProcessedIndex
            first_sdl_write = $firstSdlWriteIndex
            first_nonzero_sdl_queue = $firstNonzeroQueueIndex
            video_evidence_arm = $combinedVideoArmIndex
            first_video_frame = $combinedFirstVideoIndex
        }

        $run.result = "PASS"
        $summary.iterations_passed++
    }
    catch {
        $run.failure = $_.Exception.Message
        $summary.runs += [pscustomobject]$run
        $summaryPath = Join-Path $outDir "issue33-summary.json"
        $summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryPath -Encoding UTF8

        Write-Host "[ISSUE-33] $Configuration FAIL iteration=$iteration reason=$($run.failure)"
        $tail = Get-EvidenceLines $logPath | Select-Object -Last 40
        if ($tail) { $tail | ForEach-Object { Write-Host $_ } }
        throw "[ISSUE-33] cold URL smoke failed"
    }
    finally {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
        $env:IM_PLAYER_LIFECYCLE_LOG = $oldLifecycleLog
    }

    $summary.runs += [pscustomobject]$run
}

$summary.result = "PASS"
$summaryPath = Join-Path $outDir "issue33-summary.json"
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host "[ISSUE-33] $Configuration PASS $($summary.iterations_passed)/$Iterations head=$commit"
Write-Host "[ISSUE-33] summary=$summaryPath"
