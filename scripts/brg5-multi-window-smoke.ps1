[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [ValidateRange(200, 5000)]
    [int]$ActionDelayMilliseconds = 900,

    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 3,

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
$lifecycleLog = Join-Path $artifactDir "brg5d-multi-window-$configKey.log"
$evidenceJson = Join-Path $artifactDir "brg5d-multi-window-$configKey.json"
$scenario = "main-plus-secondary-window-baseline"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()

if (-not (Test-Path $exe)) { throw "[BRG5-D] Missing executable: $exe" }
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
Remove-Item $lifecycleLog -Force -ErrorAction SilentlyContinue
Remove-Item $evidenceJson -Force -ErrorAction SilentlyContinue

if (-not ("BRG5.MultiWindowNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace BRG5 {
    public static class MultiWindowNative {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

        public class WindowInfo {
            public IntPtr Handle;
            public string Title;
            public bool Visible;
        }

        [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll")] static extern int GetWindowTextLength(IntPtr hWnd);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
        [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
        [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT {
            public uint type;
            public INPUTUNION U;
        }

        [StructLayout(LayoutKind.Explicit)]
        public struct INPUTUNION {
            [FieldOffset(0)] public MOUSEINPUT mi;
            [FieldOffset(0)] public KEYBDINPUT ki;
            [FieldOffset(0)] public HARDWAREINPUT hi;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public UIntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public UIntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct HARDWAREINPUT {
            public uint uMsg;
            public ushort wParamL;
            public ushort wParamH;
        }

        public const uint WM_KEYDOWN = 0x0100;
        public const uint WM_KEYUP = 0x0101;
        public const uint WM_CLOSE = 0x0010;
        public const uint SWP_NOZORDER = 0x0004;
        public const uint SWP_NOACTIVATE = 0x0010;
        public const int SW_RESTORE = 9;
        public const uint INPUT_KEYBOARD = 1;
        public const uint KEYEVENTF_KEYUP = 0x0002;

        public static INPUT KeyInput(ushort vk, bool keyUp) {
            var input = new INPUT();
            input.type = INPUT_KEYBOARD;
            input.U.ki.wVk = vk;
            input.U.ki.wScan = 0;
            input.U.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
            input.U.ki.time = 0;
            input.U.ki.dwExtraInfo = UIntPtr.Zero;
            return input;
        }

        public static uint LastSendInputCount { get; private set; }
        public static int LastSendInputError { get; private set; }
        public static string LastSendStage { get; private set; }
        public static int InputSize { get { return Marshal.SizeOf(typeof(INPUT)); } }

        public static bool SendKeyStage(ushort vk, bool keyUp, string stage) {
            var inputs = new INPUT[] { KeyInput(vk, keyUp) };
            LastSendStage = stage;
            LastSendInputCount = SendInput(
                1,
                inputs,
                Marshal.SizeOf(typeof(INPUT))
            );
            LastSendInputError = LastSendInputCount == 1
                ? 0
                : Marshal.GetLastWin32Error();
            return LastSendInputCount == 1;
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
                result.Add(new WindowInfo { Handle = hwnd, Title = sb.ToString(), Visible = visible });
                return true;
            }, IntPtr.Zero);
            return result;
        }

        public static bool PostKey(IntPtr hwnd, int vk, bool down) {
            uint scan = MapVirtualKey((uint)vk, 0);
            long lp = 1L | ((long)scan << 16);
            if (!down) lp |= (1L << 30) | (1L << 31);
            return PostMessage(hwnd, down ? WM_KEYDOWN : WM_KEYUP, (IntPtr)vk, (IntPtr)lp);
        }
    }
}
"@
}

function Assert-Alive([System.Diagnostics.Process]$Process, [string]$Case) {
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "[BRG5-D] $Case failed: process exited early, exitCode=$($Process.ExitCode)"
    }
}

function Get-Rect([IntPtr]$Hwnd) {
    $rect = New-Object BRG5.MultiWindowNative+RECT
    if (-not [BRG5.MultiWindowNative]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "[BRG5-D] GetWindowRect failed hwnd=$Hwnd"
    }
    return $rect
}

function Rect-Signature($Rect) {
    return "$($Rect.Left),$($Rect.Top),$($Rect.Right),$($Rect.Bottom)"
}

function Get-VisibleWindows([System.Diagnostics.Process]$Process) {
    return [BRG5.MultiWindowNative]::EnumerateProcessWindows([uint32]$Process.Id, $true)
}

function Wait-VisibleWindowCount([System.Diagnostics.Process]$Process, [int]$Expected, [string]$Case) {
    for ($i = 0; $i -lt 30; ++$i) {
        Assert-Alive $Process $Case
        $windows = @(Get-VisibleWindows $Process)
        if ($windows.Count -eq $Expected) { return $windows }
        Start-Sleep -Milliseconds 200
    }
    $actual = @(Get-VisibleWindows $Process)
    throw "[BRG5-D] $Case expected visible windows=$Expected actual=$($actual.Count)"
}

function Invoke-KeyStage([int]$VirtualKey, [bool]$KeyUp, [string]$Stage) {
    if (-not [BRG5.MultiWindowNative]::SendKeyStage(
        [uint16]$VirtualKey,
        $KeyUp,
        $Stage
    )) {
        $sent = [BRG5.MultiWindowNative]::LastSendInputCount
        $errorCode = [BRG5.MultiWindowNative]::LastSendInputError
        $inputSize = [BRG5.MultiWindowNative]::InputSize
        throw "[BRG5-D] SendInput stage=$Stage failed sent=$sent/1 error=$errorCode inputSize=$inputSize"
    }
}

function Send-CtrlP([IntPtr]$Hwnd) {
    # BRG5-D needs the P key to enter SDL through the target window's normal
    # Win32 message path. Prior SendInput probes were accepted by Windows but
    # produced no SDL P KEYDOWN in this application (hotkey diagnostics empty).
    #
    # With production popup handling now using e->key.keysym.mod, queue the
    # complete Ctrl+P sequence to the saved main HWND in order. SDL snapshots
    # the modifier state on the P event, so the later Ctrl-up cannot erase it.
    [BRG5.MultiWindowNative]::ShowWindow(
        $Hwnd,
        [BRG5.MultiWindowNative]::SW_RESTORE
    ) | Out-Null

    $focused = $false
    for ($attempt = 1; $attempt -le 10; ++$attempt) {
        [BRG5.MultiWindowNative]::SetForegroundWindow($Hwnd) | Out-Null
        Start-Sleep -Milliseconds 100
        if ([BRG5.MultiWindowNative]::GetForegroundWindow() -eq $Hwnd) {
            $focused = $true
            break
        }
    }

    if (-not $focused) {
        throw "[BRG5-D] could not focus main window for staged Ctrl+P PostMessage"
    }

    if (-not [BRG5.MultiWindowNative]::PostKey($Hwnd, 0x11, $true)) {
        throw "[BRG5-D] failed to post CTRL_DOWN to main HWND"
    }
    Start-Sleep -Milliseconds 80

    if (-not [BRG5.MultiWindowNative]::PostKey($Hwnd, 0x50, $true)) {
        throw "[BRG5-D] failed to post P_DOWN to main HWND"
    }
    Start-Sleep -Milliseconds 80

    if (-not [BRG5.MultiWindowNative]::PostKey($Hwnd, 0x50, $false)) {
        throw "[BRG5-D] failed to post P_UP to main HWND"
    }
    Start-Sleep -Milliseconds 80

    if (-not [BRG5.MultiWindowNative]::PostKey($Hwnd, 0x11, $false)) {
        throw "[BRG5-D] failed to post CTRL_UP to main HWND"
    }

    Start-Sleep -Milliseconds $ActionDelayMilliseconds
}

$cases = [ordered]@{}
$result = "FAIL"
$failure = $null
$p = $null
$secondaryHandle = [IntPtr]::Zero
$mainTitle = $null
$mainRectBeforeSecondaryResizeSignature = $null
$mainRectAfterSecondaryResizeSignature = $null
$secondaryRectBeforeResizeSignature = $null
$secondaryRectAfterResizeSignature = $null
$hotkeyDiagnostics = @()
$previousLifecycleLog = [Environment]::GetEnvironmentVariable("IM_PLAYER_LIFECYCLE_LOG", [EnvironmentVariableTarget]::Process)

Write-Host "[BRG5-D] start commit=$commit configuration=$Configuration scenario=$scenario"

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $lifecycleLog
    $p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
    Start-Sleep -Seconds $StartupSeconds
    Assert-Alive $p "startup"

    $initial = @(Wait-VisibleWindowCount $p 1 "initial-main-window")
    $mainHandle = $initial[0].Handle
    $mainTitle = $initial[0].Title
    $cases["main_window_visible"] = "PASS_ONE_VISIBLE"

    Send-CtrlP $mainHandle
    $two = @(Wait-VisibleWindowCount $p 2 "open-secondary")
    $secondary = $two | Where-Object { $_.Handle -ne $mainHandle } | Select-Object -First 1
    if (-not $secondary) { throw "[BRG5-D] secondary window not identified" }
    $secondaryHandle = $secondary.Handle
    $cases["open_main_plus_secondary"] = "PASS_TWO_VISIBLE"
    $cases["render_both_baseline"] = "PASS_BOTH_VISIBLE_PROCESS_ALIVE"

    # Attribute this assertion only to the secondary-resize transition.
    # Do not compare against the startup geometry: opening the secondary is a
    # separate transition and may legitimately cause unrelated window-manager
    # settling before the resize case begins.
    $mainRectBeforeSecondaryResize = Get-Rect $mainHandle
    $secondaryBefore = Get-Rect $secondaryHandle
    $mainRectBeforeSecondaryResizeSignature = Rect-Signature $mainRectBeforeSecondaryResize
    $secondaryRectBeforeResizeSignature = Rect-Signature $secondaryBefore

    $secondaryW = $secondaryBefore.Right - $secondaryBefore.Left
    $secondaryH = $secondaryBefore.Bottom - $secondaryBefore.Top
    $targetW = if ($secondaryW -gt 520) { $secondaryW - 90 } else { $secondaryW + 90 }
    $targetH = if ($secondaryH -gt 400) { $secondaryH - 60 } else { $secondaryH + 60 }
    if (-not [BRG5.MultiWindowNative]::SetWindowPos($secondaryHandle, [IntPtr]::Zero, 0, 0, $targetW, $targetH, [BRG5.MultiWindowNative]::SWP_NOZORDER -bor [BRG5.MultiWindowNative]::SWP_NOACTIVATE)) {
        throw "[BRG5-D] secondary resize failed"
    }
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
    Assert-Alive $p "resize-secondary"

    $secondaryAfter = Get-Rect $secondaryHandle
    $mainRectAfterSecondaryResize = Get-Rect $mainHandle
    $secondaryRectAfterResizeSignature = Rect-Signature $secondaryAfter
    $mainRectAfterSecondaryResizeSignature = Rect-Signature $mainRectAfterSecondaryResize

    if ($secondaryRectAfterResizeSignature -eq $secondaryRectBeforeResizeSignature) {
        throw "[BRG5-D] secondary geometry did not change"
    }
    if ($mainRectAfterSecondaryResizeSignature -ne $mainRectBeforeSecondaryResizeSignature) {
        throw "[BRG5-D] main geometry changed during secondary resize: before=$mainRectBeforeSecondaryResizeSignature after=$mainRectAfterSecondaryResizeSignature"
    }
    $cases["resize_secondary_main_continues"] = "PASS_ISOLATED_GEOMETRY"

    if (-not [BRG5.MultiWindowNative]::PostMessage($secondaryHandle, [BRG5.MultiWindowNative]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)) {
        throw "[BRG5-D] failed to post close to secondary"
    }
    Start-Sleep -Milliseconds $ActionDelayMilliseconds
    $afterSecondaryClose = @(Wait-VisibleWindowCount $p 1 "close-secondary-hides")
    if ($afterSecondaryClose[0].Handle -ne $mainHandle) {
        throw "[BRG5-D] main window not preserved after secondary close"
    }
    Assert-Alive $p "main-after-secondary-close"
    $cases["close_secondary_main_valid"] = "PASS_SECONDARY_HIDDEN_MAIN_ALIVE"

    Send-CtrlP $mainHandle
    $reopened = @(Wait-VisibleWindowCount $p 2 "reopen-secondary")
    $reopenedSecondary = $reopened | Where-Object { $_.Handle -ne $mainHandle } | Select-Object -First 1
    if (-not $reopenedSecondary) { throw "[BRG5-D] reopened secondary not identified" }
    if ($reopenedSecondary.Handle -ne $secondaryHandle) {
        throw "[BRG5-D] secondary HWND changed; expected hide/reuse baseline"
    }
    $cases["reopen_secondary_reuses_window"] = "PASS_SAME_HWND"
    $cases["request_render_heartbeat_isolation"] = "PASS_OBSERVED_MAIN_ALIVE_SECONDARY_REUSABLE"

    # Do not use Process.CloseMainWindow() in a multi-window process.
    # After the secondary is reopened, .NET may select that top-level HWND;
    # the application intentionally handles secondary WM_CLOSE as hide-only.
    # Target the saved main HWND explicitly so this assertion measures the
    # application's main-window shutdown contract rather than .NET selection.
    if (-not [BRG5.MultiWindowNative]::PostMessage(
        $mainHandle,
        [BRG5.MultiWindowNative]::WM_CLOSE,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    )) {
        throw "[BRG5-D] failed to post WM_CLOSE to saved main HWND"
    }
    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[BRG5-D] main shutdown timeout after explicit main WM_CLOSE"
    }
    if ($p.ExitCode -ne 0) { throw "[BRG5-D] main shutdown exitCode=$($p.ExitCode)" }
    $cases["close_main_clean_shutdown"] = "PASS_EXIT_0"

    if (-not (Test-Path $lifecycleLog)) { throw "[BRG5-D] lifecycle log missing: $lifecycleLog" }
    & python.exe $validator $lifecycleLog
    if ($LASTEXITCODE -ne 0) { throw "[BRG5-D] lifecycle validation failed" }
    $cases["lifecycle_validation"] = "PASS"

    $result = "PASS"
    Write-Host "[BRG5-D] PASS commit=$commit configuration=$Configuration scenario=$scenario"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[BRG5-D] ERROR: $failure"
    if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    throw
}
finally {
    if (Test-Path $lifecycleLog) {
        $hotkeyDiagnostics = @(
            Get-Content $lifecycleLog -ErrorAction SilentlyContinue |
                Where-Object { $_.StartsWith("[BRG5-DIAG]") }
        )
    }

    if ($null -eq $previousLifecycleLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    } else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLifecycleLog
    }

    $evidence = [ordered]@{
        schema = "BRG5-D-v1"
        commit = $commit
        configuration = $Configuration
        scenario = $scenario
        input_injection = "POSTMESSAGE_STAGED_CTRL_P_EVENT_LOCAL_MODIFIER"
        input_diagnostics = [ordered]@{
            delivery = "POSTMESSAGE_WM_KEYDOWN_UP"
            modifier_source = "SDL_EVENT_KEYSYM_MOD"
            target = "SAVED_MAIN_HWND"
        }
        hotkey_diagnostics = $hotkeyDiagnostics
        result = $result
        failure = $failure
        main_window_title = if ($mainTitle) { $mainTitle } else { $null }
        secondary_hwnd = if ($secondaryHandle -ne [IntPtr]::Zero) { $secondaryHandle.ToInt64() } else { $null }
        resize_geometry = [ordered]@{
            main_before_secondary_resize = $mainRectBeforeSecondaryResizeSignature
            main_after_secondary_resize = $mainRectAfterSecondaryResizeSignature
            secondary_before_resize = $secondaryRectBeforeResizeSignature
            secondary_after_resize = $secondaryRectAfterResizeSignature
        }
        cases = $cases
        lifecycle_log = $lifecycleLog
    }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -Path $evidenceJson -Encoding UTF8
    Write-Host "[BRG5-D] evidence=$evidenceJson result=$result"
}
