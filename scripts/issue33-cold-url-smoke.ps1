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
    schema = "ISSUE33-COLD-URL-v1"
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

function Find-StartupIndex([string[]]$Lines, [string]$Token) {
    for ($i = 0; $i -lt $Lines.Count; $i++) {
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
        startup_log = $logPath
        failure = $null
    }

    try {
        $quotedMediaUrl = '"' + $MediaUrl + '"'
        $process = Start-Process -FilePath $exe -ArgumentList $quotedMediaUrl -WorkingDirectory $root -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)

        $startup = @()
        while ([DateTime]::UtcNow -lt $deadline) {
            if ($process.HasExited) {
                throw "process exited during startup, exitCode=$($process.ExitCode)"
            }

            $startup = Get-StartupLines $logPath
            $hasDeferred = ($startup | Where-Object { $_ -match "event=CLI_LOAD_DEFERRED " }).Count -gt 0
            $hasIdleReady = ($startup | Where-Object { $_ -match "event=MPV_IDLE_READY " }).Count -gt 0
            $hasDispatch = ($startup | Where-Object { $_ -match "event=CLI_LOAD_DISPATCH " }).Count -gt 0
            $hasLoad = ($startup | Where-Object { $_ -match "event=LOAD_REQUEST " }).Count -gt 0
            $hasStart = ($startup | Where-Object { $_ -match "event=START_FILE " }).Count -gt 0
            $hasLoaded = ($startup | Where-Object { $_ -match "event=FILE_LOADED " }).Count -gt 0
            $hasRestart = ($startup | Where-Object { $_ -match "event=PLAYBACK_RESTART " }).Count -gt 0

            if ($hasDeferred -and $hasIdleReady -and $hasDispatch -and $hasLoad -and $hasStart -and $hasLoaded -and $hasRestart) { break }
            Start-Sleep -Milliseconds 100
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
        if (($startup | Where-Object { $_ -match "event=PLAYBACK_RESTART " }).Count -eq 0) {
            throw "PLAYBACK_RESTART marker not observed"
        }

        Start-Sleep -Milliseconds $PostStartupObserveMilliseconds
        $startup = Get-StartupLines $logPath

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

        $ytdlIndex = Find-StartupIndex $startup "event=YTDL_PATH_RESOLVED "
        $deferredIndex = Find-StartupIndex $startup "event=CLI_LOAD_DEFERRED "
        $idleReadyIndex = Find-StartupIndex $startup "event=MPV_IDLE_READY "
        $dispatchIndex = Find-StartupIndex $startup "event=CLI_LOAD_DISPATCH "
        $loadIndex = Find-StartupIndex $startup "event=LOAD_REQUEST "
        $startIndex = Find-StartupIndex $startup "event=START_FILE "
        $loadedIndex = Find-StartupIndex $startup "event=FILE_LOADED "
        $configBeginIndex = Find-StartupIndex $startup "event=DYNAMIC_CONFIG_APPLY_BEGIN "
        $configEndIndex = Find-StartupIndex $startup "event=DYNAMIC_CONFIG_APPLY_END "

        if ($ytdlIndex -lt 0 -or $deferredIndex -le $ytdlIndex -or $idleReadyIndex -le $deferredIndex -or $dispatchIndex -le $idleReadyIndex -or $loadIndex -le $dispatchIndex -or $startIndex -le $loadIndex -or $loadedIndex -le $startIndex) {
            throw "startup event order is not YTDL_PATH_RESOLVED -> CLI_LOAD_DEFERRED -> MPV_IDLE_READY -> CLI_LOAD_DISPATCH -> LOAD_REQUEST -> START_FILE -> FILE_LOADED"
        }
        if ($configBeginIndex -le $loadedIndex -or $configEndIndex -le $configBeginIndex) {
            throw "dynamic config timing markers are not post-FILE_LOADED ordered"
        }

        $restartLines = @($startup | Where-Object { $_ -match "event=PLAYBACK_RESTART " })
        $maxRestart = 0
        foreach ($line in $restartLines) {
            if ($line -match "count=(\d+)") {
                $maxRestart = [Math]::Max($maxRestart, [int]$Matches[1])
            }
        }
        $run.playback_restart_count = $maxRestart

        if (-not $process.CloseMainWindow()) {
            throw "CloseMainWindow returned false"
        }
        if (-not $process.WaitForExit(10000)) {
            throw "process did not exit within 10s after close"
        }
        if ($process.ExitCode -ne 0) {
            throw "process exitCode=$($process.ExitCode)"
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
        $tail = Get-StartupLines $logPath | Select-Object -Last 20
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
