[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 3,
    [ValidateRange(0, 100)]
    [int]$EventDelayMilliseconds = 5,
    [ValidateRange(50, 2000)]
    [int]$MainBurstIterations = 120,
    [ValidateRange(100, 4000)]
    [int]$DualWindowBurstIterations = 260,
    [ValidateRange(50, 2000)]
    [int]$ShutdownBurstIterations = 120,
    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$validator = Join-Path $PSScriptRoot "brg5_validate_lifecycle.py"
$artifactDir = Join-Path $root "artifacts\issue19"
$configKey = $Configuration.ToLowerInvariant()
$lifecycleLog = Join-Path $artifactDir "issue19-event-render-stress-$configKey.log"
$evidenceJson = Join-Path $artifactDir "issue19-event-render-stress-$configKey.json"
$scenario = "focused-imgui-event-render-stress"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) { throw "[AUD-19-01] Missing executable: $exe" }
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
Remove-Item $lifecycleLog -Force -ErrorAction SilentlyContinue
Remove-Item $evidenceJson -Force -ErrorAction SilentlyContinue

if (-not ("ISSUE19.EventRenderNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace ISSUE19 {
    public static class EventRenderNative {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        public class WindowInfo {
            public IntPtr Handle;
            public string Title;
            public bool Visible;
        }

        [DllImport("user32.dll")]
        static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll")]
        static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll")]
        static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        static extern int GetWindowTextLength(IntPtr hWnd);

        [DllImport("user32.dll", CharSet=CharSet.Unicode)]
        static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

        [DllImport("user32.dll")]
        public static extern bool SetWindowPos(
            IntPtr hWnd,
            IntPtr after,
            int x,
            int y,
            int cx,
            int cy,
            uint flags
        );

        [DllImport("user32.dll")]
        public static extern bool PostMessage(
            IntPtr hWnd,
            uint msg,
            IntPtr wParam,
            IntPtr lParam
        );

        [DllImport("user32.dll")]
        public static extern uint MapVirtualKey(uint code, uint mapType);

        public const uint WM_KEYDOWN = 0x0100;
        public const uint WM_KEYUP = 0x0101;
        public const uint WM_MOUSEMOVE = 0x0200;
        public const uint WM_LBUTTONDOWN = 0x0201;
        public const uint WM_LBUTTONUP = 0x0202;
        public const uint WM_CLOSE = 0x0010;
        public const uint MK_LBUTTON = 0x0001;
        public const uint SWP_NOZORDER = 0x0004;
        public const uint SWP_NOACTIVATE = 0x0010;

        static IntPtr PointLParam(int x, int y) {
            long packed = ((long)(y & 0xffff) << 16) | (uint)(x & 0xffff);
            return new IntPtr(packed);
        }

        public static List<WindowInfo> EnumerateProcessWindows(uint pid, bool visibleOnly) {
            var result = new List<WindowInfo>();
            EnumWindows(delegate(IntPtr hwnd, IntPtr lp) {
                uint owner;
                GetWindowThreadProcessId(hwnd, out owner);
                if (owner != pid) return true;

                bool visible = IsWindowVisible(hwnd);
                if (visibleOnly && !visible) return true;

                int length = GetWindowTextLength(hwnd);
                var sb = new StringBuilder(length + 1);
                GetWindowText(hwnd, sb, sb.Capacity);
                result.Add(new WindowInfo {
                    Handle = hwnd,
                    Title = sb.ToString(),
                    Visible = visible
                });
                return true;
            }, IntPtr.Zero);
            return result;
        }

        public static bool PostKey(IntPtr hwnd, int vk, bool down) {
            uint scan = MapVirtualKey((uint)vk, 0);
            long lp = 1L | ((long)scan << 16);
            if (!down) lp |= (1L << 30) | (1L << 31);
            return PostMessage(
                hwnd,
                down ? WM_KEYDOWN : WM_KEYUP,
                (IntPtr)vk,
                (IntPtr)lp
            );
        }

        public static bool PostMouseMove(IntPtr hwnd, int x, int y) {
            return PostMessage(
                hwnd,
                WM_MOUSEMOVE,
                IntPtr.Zero,
                PointLParam(x, y)
            );
        }

        public static bool PostLeftClick(IntPtr hwnd, int x, int y) {
            IntPtr point = PointLParam(x, y);
            bool down = PostMessage(
                hwnd,
                WM_LBUTTONDOWN,
                (IntPtr)MK_LBUTTON,
                point
            );
            bool up = PostMessage(
                hwnd,
                WM_LBUTTONUP,
                IntPtr.Zero,
                point
            );
            return down && up;
        }
    }
}
"@
}

function Assert-Alive([System.Diagnostics.Process]$Process, [string]$Case) {
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "[AUD-19-01] $Case failed: process exited early, exitCode=$($Process.ExitCode)"
    }
}

function Get-VisibleWindows([System.Diagnostics.Process]$Process) {
    return [ISSUE19.EventRenderNative]::EnumerateProcessWindows([uint32]$Process.Id, $true)
}

function Wait-VisibleWindowCount([System.Diagnostics.Process]$Process, [int]$Expected, [string]$Case) {
    for ($i = 0; $i -lt 40; ++$i) {
        Assert-Alive $Process $Case
        $windows = @(Get-VisibleWindows $Process)
        if ($windows.Count -eq $Expected) { return $windows }
        Start-Sleep -Milliseconds 200
    }

    $actual = @(Get-VisibleWindows $Process)
    throw "[AUD-19-01] $Case expected visible windows=$Expected actual=$($actual.Count)"
}

function Get-Rect([IntPtr]$Hwnd) {
    $rect = New-Object ISSUE19.EventRenderNative+RECT
    if (-not [ISSUE19.EventRenderNative]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "[AUD-19-01] GetWindowRect failed hwnd=$Hwnd"
    }
    return $rect
}

function Resize-Window([IntPtr]$Hwnd, [string]$Case) {
    $before = Get-Rect $Hwnd
    $width = $before.Right - $before.Left
    $height = $before.Bottom - $before.Top
    $targetW = if ($width -gt 700) { $width - 72 } else { $width + 72 }
    $targetH = if ($height -gt 480) { $height - 48 } else { $height + 48 }

    if (-not [ISSUE19.EventRenderNative]::SetWindowPos(
        $Hwnd,
        [IntPtr]::Zero,
        $before.Left,
        $before.Top,
        $targetW,
        $targetH,
        [ISSUE19.EventRenderNative]::SWP_NOZORDER -bor [ISSUE19.EventRenderNative]::SWP_NOACTIVATE
    )) {
        throw "[AUD-19-01] $Case resize failed hwnd=$Hwnd"
    }

    $script:resizeCount += 1
}

function Send-CtrlP([IntPtr]$Hwnd) {
    if (-not [ISSUE19.EventRenderNative]::PostKey($Hwnd, 0x11, $true)) { throw "[AUD-19-01] failed to post CTRL_DOWN" }
    Start-Sleep -Milliseconds 60
    if (-not [ISSUE19.EventRenderNative]::PostKey($Hwnd, 0x50, $true)) { throw "[AUD-19-01] failed to post P_DOWN" }
    Start-Sleep -Milliseconds 60
    if (-not [ISSUE19.EventRenderNative]::PostKey($Hwnd, 0x50, $false)) { throw "[AUD-19-01] failed to post P_UP" }
    Start-Sleep -Milliseconds 60
    if (-not [ISSUE19.EventRenderNative]::PostKey($Hwnd, 0x11, $false)) { throw "[AUD-19-01] failed to post CTRL_UP" }
    Start-Sleep -Milliseconds 300
    $script:keyboardPairCount += 2
}

function Invoke-EventBurst {
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)]
        [IntPtr[]]$Targets,
        [Parameter(Mandatory = $true)]
        [int]$Iterations,
        [IntPtr]$ResizeTarget = [IntPtr]::Zero,
        [int]$ResizeIteration = -1,
        [IntPtr]$CloseTarget = [IntPtr]::Zero,
        [int]$CloseIteration = -1,
        [switch]$StopAfterClose,
        [Parameter(Mandatory = $true)]
        [string]$Case
    )

    if ($Targets.Count -eq 0) { throw "[AUD-19-01] $Case has no event targets" }

    for ($i = 0; $i -lt $Iterations; ++$i) {
        Assert-Alive $Process "$Case-iteration-$i"

        $target = $Targets[$i % $Targets.Count]
        $x = 24 + (($i * 13) % 160)
        $y = 24 + (($i * 7) % 100)

        if (-not [ISSUE19.EventRenderNative]::PostMouseMove($target, $x, $y)) {
            throw "[AUD-19-01] $Case mouse move failed iteration=$i hwnd=$target"
        }
        $script:mouseMoveCount += 1

        if (($i % 8) -eq 0) {
            if (-not [ISSUE19.EventRenderNative]::PostLeftClick($target, $x, $y)) {
                throw "[AUD-19-01] $Case mouse click failed iteration=$i hwnd=$target"
            }
            $script:mouseClickCount += 1
        }

        if (($i % 11) -eq 0) {
            if (-not [ISSUE19.EventRenderNative]::PostKey($target, 0x41, $true) -or -not [ISSUE19.EventRenderNative]::PostKey($target, 0x41, $false)) {
                throw "[AUD-19-01] $Case keyboard pair failed iteration=$i hwnd=$target"
            }
            $script:keyboardPairCount += 1
        }

        if ($ResizeTarget -ne [IntPtr]::Zero -and $i -eq $ResizeIteration) {
            Resize-Window $ResizeTarget "$Case-resize"
        }

        if ($CloseTarget -ne [IntPtr]::Zero -and $i -eq $CloseIteration) {
            if (-not [ISSUE19.EventRenderNative]::PostMessage(
                $CloseTarget,
                [ISSUE19.EventRenderNative]::WM_CLOSE,
                [IntPtr]::Zero,
                [IntPtr]::Zero
            )) {
                throw "[AUD-19-01] $Case close injection failed hwnd=$CloseTarget"
            }

            $script:closeDuringBurstCount += 1
            if ($StopAfterClose) { break }
        }

        if ($EventDelayMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $EventDelayMilliseconds
        }
    }
}

$cases = [ordered]@{}
$result = "FAIL"
$failure = $null
$p = $null
$mainHandle = [IntPtr]::Zero
$secondaryHandle = [IntPtr]::Zero
$mouseMoveCount = 0
$mouseClickCount = 0
$keyboardPairCount = 0
$resizeCount = 0
$closeDuringBurstCount = 0
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$previousLifecycleLog = [Environment]::GetEnvironmentVariable("IM_PLAYER_LIFECYCLE_LOG", [EnvironmentVariableTarget]::Process)

Write-Host "[AUD-19-01] start commit=$commit configuration=$Configuration scenario=$scenario"

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $lifecycleLog
    $p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
    Start-Sleep -Seconds $StartupSeconds
    Assert-Alive $p "startup"

    $initial = @(Wait-VisibleWindowCount $p 1 "initial-main-window")
    $mainHandle = $initial[0].Handle
    $cases["main_window_visible"] = "PASS_ONE_VISIBLE"

    $mainParams = @{
        Process = $p
        Targets = @($mainHandle)
        Iterations = $MainBurstIterations
        ResizeTarget = $mainHandle
        ResizeIteration = [Math]::Floor($MainBurstIterations / 2)
        Case = "main-event-render-burst"
    }
    Invoke-EventBurst @mainParams

    Assert-Alive $p "after-main-event-render-burst"
    $cases["main_mouse_keyboard_render_burst"] = "PASS_PROCESS_ALIVE"
    $cases["main_resize_during_event_burst"] = "PASS"

    Send-CtrlP $mainHandle
    $two = @(Wait-VisibleWindowCount $p 2 "open-secondary")
    $secondary = $two | Where-Object { $_.Handle -ne $mainHandle } | Select-Object -First 1
    if (-not $secondary) { throw "[AUD-19-01] secondary window not identified" }

    $secondaryHandle = $secondary.Handle
    $cases["open_secondary"] = "PASS_TWO_VISIBLE"

    $dualParams = @{
        Process = $p
        Targets = @($mainHandle, $secondaryHandle)
        Iterations = $DualWindowBurstIterations
        ResizeTarget = $secondaryHandle
        ResizeIteration = [Math]::Floor($DualWindowBurstIterations / 3)
        CloseTarget = $secondaryHandle
        CloseIteration = [Math]::Max(1, [Math]::Floor($DualWindowBurstIterations * 0.75))
        Case = "dual-window-event-render-burst"
    }
    Invoke-EventBurst @dualParams

    $one = @(Wait-VisibleWindowCount $p 1 "secondary-close-during-burst")
    if ($one[0].Handle -ne $mainHandle) {
        throw "[AUD-19-01] main window not preserved after secondary close"
    }

    Assert-Alive $p "main-after-secondary-close"
    $cases["dual_window_mouse_keyboard_render_burst"] = "PASS_PROCESS_ALIVE"
    $cases["secondary_resize_during_event_burst"] = "PASS"
    $cases["secondary_close_during_event_burst"] = "PASS_MAIN_ALIVE"

    Send-CtrlP $mainHandle
    $reopened = @(Wait-VisibleWindowCount $p 2 "reopen-secondary")
    $reopenedSecondary = $reopened | Where-Object { $_.Handle -ne $mainHandle } | Select-Object -First 1
    if (-not $reopenedSecondary) { throw "[AUD-19-01] reopened secondary not identified" }
    if ($reopenedSecondary.Handle -ne $secondaryHandle) {
        throw "[AUD-19-01] secondary HWND changed; expected hide/reuse baseline"
    }

    $cases["reopen_secondary_same_hwnd"] = "PASS"

    $shutdownParams = @{
        Process = $p
        Targets = @($mainHandle, $secondaryHandle)
        Iterations = $ShutdownBurstIterations
        CloseTarget = $mainHandle
        CloseIteration = [Math]::Max(1, [Math]::Floor($ShutdownBurstIterations * 0.75))
        StopAfterClose = $true
        Case = "shutdown-under-event-render-pressure"
    }
    Invoke-EventBurst @shutdownParams

    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[AUD-19-01] shutdown timeout after main close under event pressure"
    }
    if ($p.ExitCode -ne 0) { throw "[AUD-19-01] shutdown exitCode=$($p.ExitCode)" }

    $cases["main_close_during_event_burst"] = "PASS_EXIT_0"

    if (-not (Test-Path $lifecycleLog)) { throw "[AUD-19-01] lifecycle log missing: $lifecycleLog" }

    & python.exe $validator $lifecycleLog
    if ($LASTEXITCODE -ne 0) { throw "[AUD-19-01] lifecycle validation failed" }
    $cases["lifecycle_validation"] = "PASS"

    if ($mouseMoveCount -lt 100) { throw "[AUD-19-01] insufficient mouse move pressure count=$mouseMoveCount" }
    if ($mouseClickCount -lt 10) { throw "[AUD-19-01] insufficient mouse click pressure count=$mouseClickCount" }
    if ($keyboardPairCount -lt 10) { throw "[AUD-19-01] insufficient keyboard pressure count=$keyboardPairCount" }
    if ($resizeCount -lt 2) { throw "[AUD-19-01] expected main+secondary resize during bursts count=$resizeCount" }
    if ($closeDuringBurstCount -lt 2) { throw "[AUD-19-01] expected secondary+main close during bursts count=$closeDuringBurstCount" }

    $result = "PASS"
    Write-Host "[AUD-19-01] PASS commit=$commit configuration=$Configuration mouse_moves=$mouseMoveCount mouse_clicks=$mouseClickCount keyboard_pairs=$keyboardPairCount resize_count=$resizeCount close_during_burst_count=$closeDuringBurstCount"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[AUD-19-01] ERROR: $failure"
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    throw
}
finally {
    $stopwatch.Stop()

    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    }
    else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }

    $evidence = [ordered]@{
        schema = "ISSUE19-EVENT-RENDER-STRESS-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        input_injection = "POSTMESSAGE_WINDOW_TARGETED_MOUSE_KEYBOARD"
        event_delay_milliseconds = $EventDelayMilliseconds
        main_burst_iterations = $MainBurstIterations
        dual_window_burst_iterations = $DualWindowBurstIterations
        shutdown_burst_iterations = $ShutdownBurstIterations
        mouse_move_count = $mouseMoveCount
        mouse_click_count = $mouseClickCount
        keyboard_pair_count = $keyboardPairCount
        resize_count = $resizeCount
        close_during_burst_count = $closeDuringBurstCount
        main_hwnd = if ($mainHandle -ne [IntPtr]::Zero) { $mainHandle.ToInt64() } else { $null }
        secondary_hwnd = if ($secondaryHandle -ne [IntPtr]::Zero) { $secondaryHandle.ToInt64() } else { $null }
        duration_milliseconds = $stopwatch.ElapsedMilliseconds
        cases = $cases
        lifecycle_log = $lifecycleLog
        result = $result
        failure = $failure
    }

    $evidence | ConvertTo-Json -Depth 6 | Set-Content -Path $evidenceJson -Encoding UTF8
    Write-Host "[AUD-19-01] evidence=$evidenceJson result=$result"
}
