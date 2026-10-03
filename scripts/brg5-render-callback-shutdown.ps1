[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [string]$VideoPath = "",
    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 3,
    [ValidateRange(1, 30)]
    [int]$CallbackTimeoutSeconds = 10,
    [ValidateRange(100, 5000)]
    [int]$ActivePlaybackDelayMilliseconds = 700,
    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"
$artifactDir = Join-Path $root "artifacts\brg5"
$configKey = $Configuration.ToLowerInvariant()
$lifecycleLog = Join-Path $artifactDir "brg5e-render-callback-$configKey.log"
$evidenceJson = Join-Path $artifactDir "brg5e-render-callback-$configKey.json"
$scenario = "real-video-render-callback-active-shutdown"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) { throw "[AUD-8-02] Missing executable: $exe" }
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

$videoSource = "USER_SUPPLIED"
if ([string]::IsNullOrWhiteSpace($VideoPath)) {
    $fixture = Join-Path $artifactDir "brg5e-render-fixture.y4m"
    & python.exe (Join-Path $PSScriptRoot "brg5_generate_video_fixture.py") $fixture --seconds 12 --fps 30 --width 160 --height 90
    if ($LASTEXITCODE -ne 0) {
        throw "[AUD-8-02] deterministic video fixture generation failed"
    }
    $VideoPath = $fixture
    $videoSource = "GENERATED_Y4M"
}

$resolvedVideo = (Resolve-Path $VideoPath -ErrorAction Stop).Path
Remove-Item $lifecycleLog -Force -ErrorAction SilentlyContinue
Remove-Item $evidenceJson -Force -ErrorAction SilentlyContinue

if (-not ("BRG5.RenderShutdownNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace BRG5 {
    public static class RenderShutdownNative {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(
            IntPtr hWnd,
            uint Msg,
            IntPtr wParam,
            IntPtr lParam
        );

        public const uint WM_CLOSE = 0x0010;
    }
}
"@
}

function Assert-Alive([System.Diagnostics.Process]$Process, [string]$Case) {
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "[AUD-8-02] $Case failed: process exited early, exitCode=$($Process.ExitCode)"
    }
}

function Wait-MainWindow([System.Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 50; ++$i) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[AUD-8-02] process exited before main window became available, exitCode=$($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }
        Start-Sleep -Milliseconds 200
    }
    throw "[AUD-8-02] timed out waiting for main window"
}

function Find-Line([string[]]$Lines, [string]$Token) {
    foreach ($line in $Lines) {
        if ($line.Contains($Token)) {
            return $line
        }
    }
    return $null
}

function Find-LineIndex([string[]]$Lines, [string]$Token) {
    for ($i = 0; $i -lt $Lines.Count; ++$i) {
        if ($Lines[$i].Contains($Token)) {
            return $i
        }
    }
    return -1
}

$cases = [ordered]@{}
$result = "FAIL"
$failure = $null
$p = $null
$hwnd = [IntPtr]::Zero
$callbackCount = 0
$callbackState = $null
$firstCallbackMarker = $null
$preCloseMarker = $null
$quiescentMarker = $null
$previousLifecycleLog = [Environment]::GetEnvironmentVariable("IM_PLAYER_LIFECYCLE_LOG", [EnvironmentVariableTarget]::Process)

Write-Host "[AUD-8-02] start commit=$commit configuration=$Configuration video=$resolvedVideo"

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $lifecycleLog
    $quotedVideo = '"' + $resolvedVideo + '"'
    $p = Start-Process -FilePath $exe -ArgumentList $quotedVideo -WorkingDirectory $root -PassThru

    $hwnd = Wait-MainWindow $p
    Start-Sleep -Seconds $StartupSeconds
    Assert-Alive $p "video-startup"
    $cases["real_video_playback"] = "PASS_PROCESS_ALIVE"

    $deadline = [DateTime]::UtcNow.AddSeconds($CallbackTimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Assert-Alive $p "waiting-for-render-callback"

        if (Test-Path $lifecycleLog) {
            $probeLines = @(Get-Content $lifecycleLog -ErrorAction SilentlyContinue)
            $firstCallbackMarker = Find-Line $probeLines "callback_first"
            if ($firstCallbackMarker -and $firstCallbackMarker.Contains("category=RENDER_CALLBACK")) {
                break
            }
        }

        Start-Sleep -Milliseconds 200
    }

    if (-not $firstCallbackMarker -or -not $firstCallbackMarker.Contains("callback_first")) {
        throw "[AUD-8-02] no real libmpv render update callback observed before timeout"
    }

    $cases["real_render_callback_observed"] = "PASS_CALLBACK_FIRST_MARKER"

    Start-Sleep -Milliseconds $ActivePlaybackDelayMilliseconds
    Assert-Alive $p "active-video-before-shutdown"
    $cases["shutdown_requested_during_active_video"] = "PASS_PROCESS_ALIVE_AFTER_CALLBACK"

    if (-not [BRG5.RenderShutdownNative]::PostMessage($hwnd, [BRG5.RenderShutdownNative]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw "[AUD-8-02] failed to post WM_CLOSE to saved main HWND"
    }

    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[AUD-8-02] shutdown timeout"
    }
    if ($p.ExitCode -ne 0) {
        throw "[AUD-8-02] shutdown exitCode=$($p.ExitCode)"
    }
    $cases["clean_shutdown"] = "PASS_EXIT_0"

    if (-not (Test-Path $lifecycleLog)) {
        throw "[AUD-8-02] lifecycle log missing: $lifecycleLog"
    }

    $lines = @(Get-Content $lifecycleLog -ErrorAction Stop)

    $firstIndex = Find-LineIndex $lines "callback_first"
    $preCloseIndex = Find-LineIndex $lines "shutdown_pre_close"
    $quiescentIndex = Find-LineIndex $lines "shutdown_quiescent"

    if ($firstIndex -lt 0) { throw "[AUD-8-02] callback_first marker missing after shutdown" }
    if ($preCloseIndex -lt 0) { throw "[AUD-8-02] shutdown_pre_close marker missing" }
    if ($quiescentIndex -lt 0) { throw "[AUD-8-02] shutdown_quiescent marker missing" }
    if (-not ($firstIndex -lt $preCloseIndex -and $preCloseIndex -lt $quiescentIndex)) {
        throw "[AUD-8-02] render callback/shutdown marker order invalid"
    }

    $firstCallbackMarker = $lines[$firstIndex]
    $preCloseMarker = $lines[$preCloseIndex]
    $quiescentMarker = $lines[$quiescentIndex]

    $countMatch = [regex]::Match($preCloseMarker, "callback_count=(\d+)")
    if (-not $countMatch.Success) {
        throw "[AUD-8-02] callback_count missing from shutdown_pre_close marker"
    }
    $callbackCount = [int64]$countMatch.Groups[1].Value
    if ($callbackCount -le 0) {
        throw "[AUD-8-02] shutdown_pre_close callback_count must be > 0"
    }

    $firstState = [regex]::Match($firstCallbackMarker, "state=(\S+)")
    $preState = [regex]::Match($preCloseMarker, "state=(\S+)")
    $quiescentState = [regex]::Match($quiescentMarker, "state=(\S+)")
    if (
        -not $firstState.Success -or
        -not $preState.Success -or
        -not $quiescentState.Success -or
        $firstState.Groups[1].Value -ne $preState.Groups[1].Value -or
        $preState.Groups[1].Value -ne $quiescentState.Groups[1].Value
    ) {
        throw "[AUD-8-02] render callback state identity changed across shutdown evidence"
    }
    $callbackState = $preState.Groups[1].Value

    $inFlightMatch = [regex]::Match($quiescentMarker, "in_flight=(\d+)")
    if (-not $inFlightMatch.Success) {
        throw "[AUD-8-02] in_flight missing from shutdown_quiescent marker"
    }
    if ([int64]$inFlightMatch.Groups[1].Value -ne 0) {
        throw "[AUD-8-02] callback gate not quiescent after close"
    }

    $cases["callback_count_before_shutdown"] = "PASS_GT_0"
    $cases["callback_state_identity"] = "PASS_STABLE"
    $cases["callback_shutdown_order"] = "PASS_FIRST_PRE_CLOSE_QUIESCENT"
    $cases["callback_quiescence"] = "PASS_IN_FLIGHT_0"

    & python.exe $validator $lifecycleLog
    if ($LASTEXITCODE -ne 0) {
        throw "[AUD-8-02] lifecycle validation failed"
    }
    $cases["lifecycle_validation"] = "PASS"

    $result = "PASS"
    Write-Host "[AUD-8-02] PASS commit=$commit configuration=$Configuration callback_count=$callbackCount"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[AUD-8-02] ERROR: $failure"
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    throw
}
finally {
    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }

    $evidence = [ordered]@{
        schema = "AUD-8-02-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        video_path = $resolvedVideo
        video_source = $videoSource
        callback_state = $callbackState
        callback_count_before_shutdown = $callbackCount
        result = $result
        failure = $failure
        cases = $cases
        markers = [ordered]@{
            callback_first = $firstCallbackMarker
            shutdown_pre_close = $preCloseMarker
            shutdown_quiescent = $quiescentMarker
        }
        lifecycle_log = $lifecycleLog
    }

    $evidence | ConvertTo-Json -Depth 8 | Set-Content -Path $evidenceJson -Encoding UTF8
    Write-Host "[AUD-8-02] evidence=$evidenceJson result=$result"
}
