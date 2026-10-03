[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [string]$MediaPath = "",

    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 4,

    [ValidateRange(100, 5000)]
    [int]$ActionDelayMilliseconds = 700,

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
$lifecycleLog = Join-Path $artifactDir "brg5c-playback-window-$configKey.log"
$evidenceJson = Join-Path $artifactDir "brg5c-playback-window-$configKey.json"
$scenario = "local-media-playback-window-controls"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) { throw "[BRG5-C] Missing executable: $exe" }

New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

$mediaSource = "USER_SUPPLIED"
if ([string]::IsNullOrWhiteSpace($MediaPath)) {
    $fixture = Join-Path $artifactDir "brg5c-fixture.wav"
    & python.exe (Join-Path $PSScriptRoot "brg5_generate_media_fixture.py") $fixture --seconds 60
    if ($LASTEXITCODE -ne 0) {
        throw "[BRG5-C] deterministic media fixture generation failed"
    }
    $MediaPath = $fixture
    $mediaSource = "GENERATED_PCM_WAV"
}

$resolvedMedia = (Resolve-Path $MediaPath -ErrorAction Stop).Path
Remove-Item $lifecycleLog -Force -ErrorAction SilentlyContinue
Remove-Item $evidenceJson -Force -ErrorAction SilentlyContinue

if (-not ("BRG5.NativeWindow" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace BRG5 {
    public static class NativeWindow {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

        [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint flags);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);

        public const uint WM_KEYDOWN = 0x0100;
        public const uint WM_KEYUP = 0x0101;
        public const uint SWP_NOZORDER = 0x0004;
        public const uint SWP_NOACTIVATE = 0x0010;
        public const int SW_MINIMIZE = 6;
        public const int SW_RESTORE = 9;

        public static bool PostKey(IntPtr hwnd, int vk, bool extended) {
            uint scan = MapVirtualKey((uint)vk, 0);
            long down = 1L | ((long)scan << 16);
            if (extended) down |= (1L << 24);
            long up = down | (1L << 30) | (1L << 31);
            bool a = PostMessage(hwnd, WM_KEYDOWN, (IntPtr)vk, (IntPtr)down);
            bool b = PostMessage(hwnd, WM_KEYUP, (IntPtr)vk, (IntPtr)up);
            return a && b;
        }
    }
}
"@
}

function Assert-Alive([System.Diagnostics.Process]$Process, [string]$Case) {
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "[BRG5-C] $Case failed: process exited early, exitCode=$($Process.ExitCode)"
    }
}

function Wait-MainWindow([System.Diagnostics.Process]$Process) {
    for ($i = 0; $i -lt 50; ++$i) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "[BRG5-C] process exited before main window became available, exitCode=$($Process.ExitCode)"
        }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) { return $Process.MainWindowHandle }
        Start-Sleep -Milliseconds 200
    }
    throw "[BRG5-C] timed out waiting for main window"
}

function Get-Rect([IntPtr]$Hwnd) {
    $rect = New-Object BRG5.NativeWindow+RECT
    if (-not [BRG5.NativeWindow]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "[BRG5-C] GetWindowRect failed"
    }
    return $rect
}

function Rect-Width($Rect) { return $Rect.Right - $Rect.Left }
function Rect-Height($Rect) { return $Rect.Bottom - $Rect.Top }

function Send-Key([IntPtr]$Hwnd, [int]$VirtualKey, [bool]$Extended = $false) {
    [BRG5.NativeWindow]::SetForegroundWindow($Hwnd) | Out-Null
    if (-not [BRG5.NativeWindow]::PostKey($Hwnd, $VirtualKey, $Extended)) {
        throw "[BRG5-C] failed to post key vk=$VirtualKey"
    }
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
}

$cases = [ordered]@{}
$result = "FAIL"
$failure = $null
$previousLifecycleLog = [Environment]::GetEnvironmentVariable("IM_PLAYER_LIFECYCLE_LOG", [EnvironmentVariableTarget]::Process)
$p = $null

Write-Host "[BRG5-C] start commit=$commit configuration=$Configuration media=$resolvedMedia"

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $lifecycleLog
    $quotedMedia = "`"$resolvedMedia`""
    $p = Start-Process -FilePath $exe -ArgumentList $quotedMedia -WorkingDirectory $root -PassThru
    $hwnd = Wait-MainWindow $p
    Start-Sleep -Seconds $StartupSeconds
    Assert-Alive $p "media-startup"
    $cases["play_local_media"] = "PASS_PROCESS_ALIVE"

    Send-Key $hwnd 0x20
    Assert-Alive $p "pause"
    $cases["pause"] = "PASS_INPUT_PATH_ALIVE"

    Send-Key $hwnd 0x20
    Assert-Alive $p "resume"
    $cases["resume"] = "PASS_INPUT_PATH_ALIVE"

    Send-Key $hwnd 0x27 $true
    Assert-Alive $p "seek-forward"
    $cases["seek_forward"] = "PASS_INPUT_PATH_ALIVE"

    Send-Key $hwnd 0x25 $true
    Assert-Alive $p "seek-backward"
    $cases["seek_backward"] = "PASS_INPUT_PATH_ALIVE"

    for ($i = 0; $i -lt 3; ++$i) {
        Send-Key $hwnd 0x27 $true
        Send-Key $hwnd 0x25 $true
    }
    Assert-Alive $p "rapid-seek"
    $cases["rapid_seek"] = "PASS_INPUT_PATH_ALIVE"

    $beforeResize = Get-Rect $hwnd
    $beforeW = Rect-Width $beforeResize
    $beforeH = Rect-Height $beforeResize
    $targetW = if ($beforeW -gt 900) { $beforeW - 120 } else { $beforeW + 120 }
    $targetH = if ($beforeH -gt 560) { $beforeH - 80 } else { $beforeH + 80 }
    if (-not [BRG5.NativeWindow]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, $targetW, $targetH, [BRG5.NativeWindow]::SWP_NOZORDER -bor [BRG5.NativeWindow]::SWP_NOACTIVATE)) {
        throw "[BRG5-C] resize SetWindowPos failed"
    }
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
    $afterResize = Get-Rect $hwnd
    if ((Rect-Width $afterResize) -eq (Rect-Width $beforeResize) -and (Rect-Height $afterResize) -eq (Rect-Height $beforeResize)) {
        throw "[BRG5-C] resize did not change window geometry"
    }
    Assert-Alive $p "resize"
    $cases["resize"] = "PASS_GEOMETRY_CHANGED"

    [BRG5.NativeWindow]::ShowWindow($hwnd, [BRG5.NativeWindow]::SW_MINIMIZE) | Out-Null
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
    if (-not [BRG5.NativeWindow]::IsIconic($hwnd)) { throw "[BRG5-C] minimize state not observed" }
    Assert-Alive $p "minimize"
    $cases["minimize"] = "PASS_ICONIC"

    [BRG5.NativeWindow]::ShowWindow($hwnd, [BRG5.NativeWindow]::SW_RESTORE) | Out-Null
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
    if ([BRG5.NativeWindow]::IsIconic($hwnd)) { throw "[BRG5-C] restore state not observed" }
    Assert-Alive $p "restore"
    $cases["restore"] = "PASS_NOT_ICONIC"

    $beforeFullscreen = Get-Rect $hwnd
    Send-Key $hwnd 0x7A
    $fullscreenRect = Get-Rect $hwnd
    Assert-Alive $p "fullscreen-enter"
    if ((Rect-Width $fullscreenRect) -eq (Rect-Width $beforeFullscreen) -and (Rect-Height $fullscreenRect) -eq (Rect-Height $beforeFullscreen)) {
        throw "[BRG5-C] fullscreen geometry change not observed"
    }
    $cases["fullscreen_enter"] = "PASS_GEOMETRY_CHANGED"

    Send-Key $hwnd 0x7A
    $restoredRect = Get-Rect $hwnd
    Assert-Alive $p "fullscreen-exit"
    if ((Rect-Width $restoredRect) -eq (Rect-Width $fullscreenRect) -and (Rect-Height $restoredRect) -eq (Rect-Height $fullscreenRect)) {
        throw "[BRG5-C] fullscreen restore geometry change not observed"
    }
    $cases["fullscreen_exit"] = "PASS_GEOMETRY_CHANGED"

    if (-not $p.CloseMainWindow()) { throw "[BRG5-C] CloseMainWindow returned false" }
    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[BRG5-C] shutdown timeout"
    }
    if ($p.ExitCode -ne 0) { throw "[BRG5-C] shutdown exitCode=$($p.ExitCode)" }
    $cases["clean_shutdown"] = "PASS_EXIT_0"

    if (-not (Test-Path $lifecycleLog)) { throw "[BRG5-C] lifecycle log missing: $lifecycleLog" }
    & python.exe $validator $lifecycleLog
    if ($LASTEXITCODE -ne 0) { throw "[BRG5-C] lifecycle validation failed" }
    $cases["lifecycle_validation"] = "PASS"

    $result = "PASS"
    Write-Host "[BRG5-C] PASS commit=$commit configuration=$Configuration scenario=$scenario"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[BRG5-C] ERROR: $failure"
    if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    throw
}
finally {
    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }

    $evidence = [ordered]@{
        schema = "BRG5-C-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        media_path = $resolvedMedia
        media_source = $mediaSource
        result = $result
        failure = $failure
        cases = $cases
        lifecycle_log = $lifecycleLog
    }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -Path $evidenceJson -Encoding UTF8
    Write-Host "[BRG5-C] evidence=$evidenceJson result=$result"
}
