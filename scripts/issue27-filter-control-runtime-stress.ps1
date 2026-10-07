[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedCommit,

    [string]$MediaPath = "",

    [ValidateRange(15, 300)]
    [int]$StressSeconds = 45,

    [ValidateRange(5, 1000)]
    [int]$MinEnqueueCount = 30,

    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 3,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10,

    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"
$buildScript = Join-Path $PSScriptRoot "build.ps1"
$fixtureGenerator = Join-Path $PSScriptRoot "issue26_generate_av_fixture.py"
$artifactRoot = Join-Path $root "artifacts\issue27-filter-control"
$commit = (& git.exe -C $root rev-parse HEAD).Trim()
$configKey = $Configuration.ToLowerInvariant()
$shortSha = $commit.Substring(0, [Math]::Min(12, $commit.Length))
$outDir = Join-Path $artifactRoot "$configKey-$shortSha"
$log = Join-Path $outDir "filter-control.log"
$summary = Join-Path $outDir "summary.json"

if ($commit -ne $ExpectedCommit) {
    throw "[ISSUE27] exact-head mismatch expected=$ExpectedCommit actual=$commit"
}

$trackedDirty = @(& git.exe -C $root status --porcelain --untracked-files=no)
if ($trackedDirty.Count -ne 0) {
    throw "[ISSUE27] tracked worktree is dirty; exact-head runtime evidence requires a clean tracked tree"
}

if (-not $SkipBuild) {
    if (-not (Test-Path $buildScript)) {
        throw "[ISSUE27] build wrapper missing: $buildScript"
    }
    Write-Host "[ISSUE27] exact-head build start commit=$commit configuration=$Configuration"
    & powershell.exe -ExecutionPolicy Bypass -File $buildScript -Configuration $Configuration -SkipBootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "[ISSUE27] build failed commit=$commit configuration=$Configuration"
    }
}
if (-not (Test-Path $exe)) {
    throw "[ISSUE27] executable missing: $exe"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item $log, $summary -Force -ErrorAction SilentlyContinue

if (-not ("Issue27.NativeWindow" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Issue27 {
    public static class NativeWindow {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT {
            public int Left;
            public int Top;
            public int Right;
            public int Bottom;
        }

        [DllImport("user32.dll")]
        static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

        [DllImport("user32.dll")]
        static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll")]
        static extern bool IsWindowVisible(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

        [DllImport("user32.dll")]
        public static extern uint MapVirtualKey(uint code, uint mapType);

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

        public const uint WM_KEYDOWN = 0x0100;
        public const uint WM_KEYUP = 0x0101;
        public const uint SWP_NOZORDER = 0x0004;
        public const uint SWP_NOACTIVATE = 0x0010;

        public static List<IntPtr> VisibleProcessWindows(uint pid) {
            var result = new List<IntPtr>();
            EnumWindows(delegate(IntPtr hwnd, IntPtr lp) {
                uint owner;
                GetWindowThreadProcessId(hwnd, out owner);
                if (owner == pid && IsWindowVisible(hwnd)) {
                    result.Add(hwnd);
                }
                return true;
            }, IntPtr.Zero);
            return result;
        }

        public static bool PostKey(IntPtr hwnd, int vk) {
            uint scan = MapVirtualKey((uint)vk, 0);
            long down = 1L | ((long)scan << 16);
            long up = down | (1L << 30) | (1L << 31);
            return PostMessage(hwnd, WM_KEYDOWN, (IntPtr)vk, (IntPtr)down)
                && PostMessage(hwnd, WM_KEYUP, (IntPtr)vk, (IntPtr)up);
        }
    }
}
"@
}

function Assert-Alive([System.Diagnostics.Process]$Process, [string]$Case) {
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "[ISSUE27] $Case process exited early exitCode=$($Process.ExitCode)"
    }
}

function Wait-VisibleWindows(
    [System.Diagnostics.Process]$Process,
    [int]$AtLeast,
    [string]$Case
) {
    for ($i = 0; $i -lt 100; ++$i) {
        Assert-Alive $Process $Case
        $windows = @([Issue27.NativeWindow]::VisibleProcessWindows([uint32]$Process.Id))
        if ($windows.Count -ge $AtLeast) {
            return $windows
        }
        Start-Sleep -Milliseconds 200
    }
    throw "[ISSUE27] $Case visible-window timeout expected_at_least=$AtLeast"
}

function Send-Key([IntPtr]$Hwnd, [int]$VirtualKey, [string]$Name) {
    [Issue27.NativeWindow]::SetForegroundWindow($Hwnd) | Out-Null
    Start-Sleep -Milliseconds 80
    if (-not [Issue27.NativeWindow]::PostKey($Hwnd, $VirtualKey)) {
        throw "[ISSUE27] failed to send key=$Name"
    }
    Start-Sleep -Milliseconds 250
}

function Send-CtrlP([IntPtr]$Hwnd) {
    [Issue27.NativeWindow]::SetForegroundWindow($Hwnd) | Out-Null
    if (-not [Issue27.NativeWindow]::PostKey($Hwnd, 0x11)) {
        throw "[ISSUE27] failed to send CTRL"
    }
    # SDL needs the modifier held across P, so send explicit down/up sequence.
    $scanCtrl = [Issue27.NativeWindow]::MapVirtualKey(0x11, 0)
    $scanP = [Issue27.NativeWindow]::MapVirtualKey(0x50, 0)
    $ctrlDown = [IntPtr](1L -bor ([int64]$scanCtrl -shl 16))
    $ctrlUp = [IntPtr]((1L -bor ([int64]$scanCtrl -shl 16)) -bor (1L -shl 30) -bor (1L -shl 31))
    $pDown = [IntPtr](1L -bor ([int64]$scanP -shl 16))
    $pUp = [IntPtr]((1L -bor ([int64]$scanP -shl 16)) -bor (1L -shl 30) -bor (1L -shl 31))
    [Issue27.NativeWindow]::PostMessage($Hwnd, 0x0100, [IntPtr]0x11, $ctrlDown) | Out-Null
    Start-Sleep -Milliseconds 60
    [Issue27.NativeWindow]::PostMessage($Hwnd, 0x0100, [IntPtr]0x50, $pDown) | Out-Null
    Start-Sleep -Milliseconds 60
    [Issue27.NativeWindow]::PostMessage($Hwnd, 0x0101, [IntPtr]0x50, $pUp) | Out-Null
    Start-Sleep -Milliseconds 60
    [Issue27.NativeWindow]::PostMessage($Hwnd, 0x0101, [IntPtr]0x11, $ctrlUp) | Out-Null
    Start-Sleep -Milliseconds 500
}

function Nudge-Window([IntPtr]$Hwnd, [int]$Iteration) {
    $rect = New-Object Issue27.NativeWindow+RECT
    if (-not [Issue27.NativeWindow]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "[ISSUE27] GetWindowRect failed hwnd=$Hwnd"
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $delta = if (($Iteration % 2) -eq 0) { 24 } else { -24 }
    $targetW = [Math]::Max(480, $width + $delta)
    $targetH = [Math]::Max(320, $height + $delta)
    if (-not [Issue27.NativeWindow]::SetWindowPos(
            $Hwnd,
            [IntPtr]::Zero,
            $rect.Left,
            $rect.Top,
            $targetW,
            $targetH,
            [Issue27.NativeWindow]::SWP_NOZORDER -bor [Issue27.NativeWindow]::SWP_NOACTIVATE)) {
        throw "[ISSUE27] SetWindowPos failed hwnd=$Hwnd"
    }
}

function Read-SharedLines([string]$Path) {
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
            $lines += [string]$reader.ReadLine()
        }
        return $lines
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
    }
}

function Parse-KeyValues([string]$Line) {
    $map = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, '(?<key>[A-Za-z0-9_]+)=(?<value>[^\s]+)')) {
        $map[$match.Groups['key'].Value] = $match.Groups['value'].Value
    }
    return $map
}

function Get-ControlRows([string]$Path) {
    $rows = @()
    foreach ($line in @(Read-SharedLines $Path)) {
        if ($line -notmatch '^\[BRG5-DIAG\] category=FILTER_CONTROL ') { continue }
        $kv = Parse-KeyValues $line
        $rows += [pscustomobject]@{
            Stage = if ($kv.Contains("stage")) { [string]$kv["stage"] } else { "" }
            Sequence = if ($kv.Contains("seq")) { [uint64]$kv["seq"] } else { [uint64]0 }
            Command = if ($kv.Contains("command")) { [string]$kv["command"] } else { "" }
            Tid = if ($kv.Contains("tid")) { [uint64]$kv["tid"] } else { [uint64]0 }
            Raw = $line
        }
    }
    return $rows
}

$source = $null
if ([string]::IsNullOrWhiteSpace($MediaPath)) {
    if (-not (Test-Path $fixtureGenerator)) {
        throw "[ISSUE27] deterministic fixture generator missing: $fixtureGenerator"
    }
    $fixture = Join-Path $outDir "issue27-av-fixture.avi"
    $fixtureSeconds = [Math]::Max(120, $StressSeconds + 45)
    & python.exe $fixtureGenerator $fixture --seconds $fixtureSeconds --sample-rate 44100
    if ($LASTEXITCODE -ne 0) {
        throw "[ISSUE27] fixture generation failed"
    }
    $source = $fixture
}
else {
    $source = (Resolve-Path $MediaPath -ErrorAction Stop).Path
}

$previousLog = [Environment]::GetEnvironmentVariable(
    "IM_PLAYER_LIFECYCLE_LOG",
    [EnvironmentVariableTarget]::Process
)

$p = $null
$result = "FAIL"
$failure = $null
$mainHwnd = [IntPtr]::Zero
$secondaryHwnd = [IntPtr]::Zero
$multiWindowObserved = $false

try {
    $env:IM_PLAYER_LIFECYCLE_LOG = $log
    $quotedSource = '"' + $source + '"'
    Write-Host "[ISSUE27] start commit=$commit configuration=$Configuration source=$source"
    $p = Start-Process -FilePath $exe -ArgumentList $quotedSource -WorkingDirectory $root -PassThru
    Start-Sleep -Seconds $StartupSeconds
    Assert-Alive $p "startup"

    $initialWindows = @(Wait-VisibleWindows $p 1 "main-window")
    $mainHwnd = $initialWindows[0]

    Send-Key $mainHwnd 0x54 "T"
    Write-Host "[ISSUE27] Test popup requested with T"

    Send-CtrlP $mainHwnd
    $windows = @(Wait-VisibleWindows $p 2 "secondary-window")
    $secondary = $windows | Where-Object { $_ -ne $mainHwnd } | Select-Object -First 1
    if (-not $secondary) {
        throw "[ISSUE27] secondary window not identified"
    }
    $secondaryHwnd = [IntPtr]$secondary
    $multiWindowObserved = $true

    Write-Host ""
    Write-Host "[ISSUE27] MANUAL STRESS WINDOW = $StressSeconds seconds"
    Write-Host "[ISSUE27] In the Test popup, repeatedly perform ALL of:"
    Write-Host "  1) Adaptive AI OFF -> ON at least once"
    Write-Host "  2) switch presets repeatedly while Adaptive AI is ON"
    Write-Host "  3) toggle multiple filters"
    Write-Host "  4) lock/unlock filter bypass"
    Write-Host "  5) drag one or more enabled sliders rapidly"
    Write-Host "[ISSUE27] Keep the secondary window open; this harness resizes it during stress."
    Write-Host ""

    $deadline = [DateTime]::UtcNow.AddSeconds($StressSeconds)
    $nudgeIteration = 0
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Seconds 1
        Assert-Alive $p "runtime-stress"
        if (($nudgeIteration % 3) -eq 0) {
            Nudge-Window $secondaryHwnd $nudgeIteration
        }
        ++$nudgeIteration
    }

    # Allow the main-thread owner loop to drain the final UI command burst.
    $settled = $false
    for ($i = 0; $i -lt 50; ++$i) {
        Start-Sleep -Milliseconds 100
        Assert-Alive $p "settle"
        $rows = @(Get-ControlRows $log)
        $enqueues = @($rows | Where-Object { $_.Stage -eq "ENQUEUE" })
        $applies = @($rows | Where-Object { $_.Stage -eq "APPLY" })
        if ($enqueues.Count -ge $MinEnqueueCount -and $enqueues.Count -eq $applies.Count) {
            $settled = $true
            break
        }
    }
    if (-not $settled) {
        throw "[ISSUE27] command stream did not settle enqueues=$($enqueues.Count) applies=$($applies.Count) min=$MinEnqueueCount"
    }

    if (-not $p.CloseMainWindow()) {
        throw "[ISSUE27] CloseMainWindow returned false"
    }
    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[ISSUE27] shutdown timeout"
    }
    if ($p.ExitCode -ne 0) {
        throw "[ISSUE27] shutdown exitCode=$($p.ExitCode)"
    }

    $rows = @(Get-ControlRows $log)
    $ownerBound = @($rows | Where-Object { $_.Stage -eq "OWNER_BOUND" })
    $ownerViolation = @($rows | Where-Object { $_.Stage -eq "OWNER_VIOLATION" })
    $reentrantDefers = @($rows | Where-Object { $_.Stage -eq "REENTRANT_DEFER" })
    $enqueues = @($rows | Where-Object { $_.Stage -eq "ENQUEUE" })
    $applies = @($rows | Where-Object { $_.Stage -eq "APPLY" })

    if ($ownerBound.Count -ne 1) {
        throw "[ISSUE27] expected exactly one OWNER_BOUND marker actual=$($ownerBound.Count)"
    }
    if ($ownerViolation.Count -ne 0) {
        throw "[ISSUE27] OWNER_VIOLATION observed count=$($ownerViolation.Count)"
    }
    if ($reentrantDefers.Count -lt 1) {
        throw "[ISSUE27] re-entrant guard was not exercised; toggle Adaptive AI ON and switch presets during stress"
    }
    if ($enqueues.Count -lt $MinEnqueueCount) {
        throw "[ISSUE27] insufficient UI command pressure enqueues=$($enqueues.Count) required=$MinEnqueueCount"
    }
    if ($applies.Count -ne $enqueues.Count) {
        throw "[ISSUE27] enqueue/apply mismatch enqueues=$($enqueues.Count) applies=$($applies.Count)"
    }

    $applySequences = @($applies | ForEach-Object { [uint64]$_.Sequence })
    for ($i = 1; $i -lt $applySequences.Count; ++$i) {
        if ($applySequences[$i] -le $applySequences[$i - 1]) {
            throw "[ISSUE27] non-monotonic APPLY sequence index=$i previous=$($applySequences[$i-1]) actual=$($applySequences[$i])"
        }
    }

    $enqueueSequenceSet = @($enqueues | ForEach-Object { [uint64]$_.Sequence } | Sort-Object -Unique)
    $applySequenceSet = @($applySequences | Sort-Object -Unique)
    if (($enqueueSequenceSet -join ",") -ne ($applySequenceSet -join ",")) {
        throw "[ISSUE27] applied sequence set differs from enqueued sequence set"
    }

    $ownerTids = @(
        @($ownerBound + $applies) |
            Where-Object { $_.Tid -ne 0 } |
            ForEach-Object { [uint64]$_.Tid } |
            Sort-Object -Unique
    )
    if ($ownerTids.Count -ne 1) {
        throw "[ISSUE27] expected exactly one owner/apply thread actual=$($ownerTids -join ',')"
    }

    foreach ($requiredCommand in @(
        "SetAdaptiveMode",
        "SetCurrentPreset",
        "ToggleFilter",
        "SetFilterBypassMode",
        "UpdateParam"
    )) {
        $count = @($enqueues | Where-Object { $_.Command -eq $requiredCommand }).Count
        if ($count -lt 1) {
            throw "[ISSUE27] required stress command not observed: $requiredCommand"
        }
    }

    $updateParamCount = @($enqueues | Where-Object { $_.Command -eq "UpdateParam" }).Count
    if ($updateParamCount -lt 10) {
        throw "[ISSUE27] insufficient slider pressure UpdateParam count=$updateParamCount required>=10"
    }

    $result = "PASS"
    Write-Host "[ISSUE27] PASS commit=$commit configuration=$Configuration enqueues=$($enqueues.Count) applies=$($applies.Count) reentrant_defers=$($reentrantDefers.Count) owner_tid=$($ownerTids[0])"
}
catch {
    $failure = $_.Exception.Message
    Write-Host "[ISSUE27] ERROR: $failure"
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
    throw
}
finally {
    if ($null -eq $previousLog) {
        Remove-Item Env:IM_PLAYER_LIFECYCLE_LOG -ErrorAction SilentlyContinue
    }
    else {
        $env:IM_PLAYER_LIFECYCLE_LOG = $previousLog
    }

    $rows = @(Get-ControlRows $log)
    $enqueues = @($rows | Where-Object { $_.Stage -eq "ENQUEUE" })
    $applies = @($rows | Where-Object { $_.Stage -eq "APPLY" })
    $ownerBound = @($rows | Where-Object { $_.Stage -eq "OWNER_BOUND" })
    $ownerViolation = @($rows | Where-Object { $_.Stage -eq "OWNER_VIOLATION" })
    $reentrantDefers = @($rows | Where-Object { $_.Stage -eq "REENTRANT_DEFER" })

    $evidence = [ordered]@{
        schema = "ISSUE27-FILTER-CONTROL-STRESS-v1"
        commit = $commit
        expected_commit = $ExpectedCommit
        configuration = $Configuration
        source = $source
        stress_seconds = $StressSeconds
        min_enqueue_count = $MinEnqueueCount
        multi_window_observed = $multiWindowObserved
        main_hwnd = if ($mainHwnd -ne [IntPtr]::Zero) { $mainHwnd.ToInt64() } else { $null }
        secondary_hwnd = if ($secondaryHwnd -ne [IntPtr]::Zero) { $secondaryHwnd.ToInt64() } else { $null }
        enqueue_count = $enqueues.Count
        apply_count = $applies.Count
        reentrant_defer_count = $reentrantDefers.Count
        owner_bound_count = $ownerBound.Count
        owner_violation_count = $ownerViolation.Count
        apply_sequence_monotonic = if ($result -eq "PASS") { $true } else { $null }
        single_owner_thread = if ($result -eq "PASS") { $true } else { $null }
        lifecycle_log = $log
        result = $result
        failure = $failure
    }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -Path $summary -Encoding UTF8
    Write-Host "[ISSUE27] evidence=$summary result=$result"
}
