[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [ValidateRange(5, 30)]
    [int]$FixtureSeconds = 5,

    [ValidateRange(10, 60)]
    [int]$TimeoutSeconds = 25
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$artifactDir = Join-Path $root "artifacts\issue33\audio-only-eof"
$fixture = Join-Path $artifactDir "audio-only-$FixtureSeconds-sec.wav"
$logPath = Join-Path $artifactDir ("{0}-audio-only-eof.log" -f $Configuration.ToLowerInvariant())
$generator = Join-Path $PSScriptRoot "issue26_generate_pcm_fixture.py"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) {
    throw "[ISSUE-33-AUDIO-EOF] missing executable: $exe"
}

New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
Remove-Item $logPath -Force -ErrorAction SilentlyContinue

& python.exe $generator $fixture --seconds $FixtureSeconds --sample-rate 44100 --channels 1
if ($LASTEXITCODE -ne 0) {
    throw "[ISSUE-33-AUDIO-EOF] deterministic WAV generation failed"
}

$previousLog = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_LIFECYCLE_LOG",
    [EnvironmentVariableTarget]::Process)
$p = $null

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $logPath
    $quotedFixture = "`"$fixture`""
    $p = Start-Process -FilePath $exe -ArgumentList $quotedFixture -WorkingDirectory $root -PassThru

    $startupDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $mediaStartedObserved = $false
    while ([DateTime]::UtcNow -lt $startupDeadline) {
        $p.Refresh()
        if ($p.HasExited) {
            throw "[ISSUE-33-AUDIO-EOF] process exited before audio startup, exitCode=$($p.ExitCode)"
        }

        if (Test-Path $logPath) {
            $text = Get-Content -Path $logPath -Raw -ErrorAction SilentlyContinue
            if ($text -match "event=STARTUP_MEDIA_STARTED load_id=1 .*source=audio_pcm") {
                $mediaStartedObserved = $true
                break
            }
        }
        Start-Sleep -Milliseconds 200
    }

    if (-not $mediaStartedObserved) {
        throw "[ISSUE-33-AUDIO-EOF] timed out waiting for audio media-start boundary"
    }

    # Cross the known deterministic fixture duration. With historical
    # keep-open semantics restored after startup, mpv may stay loaded/paused
    # instead of emitting END_FILE immediately, so END_FILE itself is optional.
    $eofHorizon = [DateTime]::UtcNow.AddSeconds($FixtureSeconds + 2)
    while ([DateTime]::UtcNow -lt $eofHorizon) {
        $p.Refresh()
        if ($p.HasExited) {
            throw "[ISSUE-33-AUDIO-EOF] process exited before normal EOF horizon, exitCode=$($p.ExitCode)"
        }
        Start-Sleep -Milliseconds 200
    }

    $evidence = Get-Content -Path $logPath -Raw

    if ($evidence -notmatch "event=STARTUP_MEDIA_STARTED load_id=1 .*source=audio_pcm") {
        throw "[ISSUE-33-AUDIO-EOF] missing audio any-media startup marker"
    }
    if ($evidence -notmatch "event=STARTUP_KEEP_OPEN_RESTORE load_id=1 .*result=0") {
        throw "[ISSUE-33-AUDIO-EOF] startup keep-open override was not restored"
    }
    if ($evidence -match "event=END_FILE load_id=1 " -and
        $evidence -notmatch "event=END_FILE load_id=1 .*startup_media_started=1") {
        throw "[ISSUE-33-AUDIO-EOF] observed END_FILE lost media-started state"
    }
    if ($evidence -match "event=FIRST_VIDEO_FRAME ") {
        throw "[ISSUE-33-AUDIO-EOF] WAV fixture unexpectedly produced video evidence"
    }
    if ($evidence -match "event=LOAD_RETRY_REQUEST ") {
        throw "[ISSUE-33-AUDIO-EOF] normal audio-only EOF incorrectly triggered startup retry"
    }
    if ($evidence -match "event=EARLY_TERMINAL_FAILURE_DETECTED ") {
        throw "[ISSUE-33-AUDIO-EOF] normal audio-only EOF incorrectly classified as early terminal"
    }
    if ($evidence -match "event=LEGACY_PLAYLIST_RETRY ") {
        throw "[ISSUE-33-AUDIO-EOF] legacy playlist retry executed during direct audio-only load"
    }

    Write-Host "[ISSUE-33-AUDIO-EOF] PASS configuration=$Configuration head=$commit fixture=$fixture"
}
finally {
    if ($p) {
        try {
            $p.Refresh()
            if (-not $p.HasExited) {
                Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
                $p.WaitForExit(5000) | Out-Null
            }
        } catch {}
    }

    if ($null -eq $previousLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLog
    }
}
