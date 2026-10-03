[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateRange(1, 100)]
    [int]$Iterations = 20,

    [ValidateRange(1, 30)]
    [int]$StartupSeconds = 2,

    [ValidateRange(1, 30)]
    [int]$ExitTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "bin\$Configuration\Im_player.exe"

if (-not (Test-Path $exe)) {
    throw "[BRG-3] Missing executable: $exe"
}

$commit = (& git.exe -C $root rev-parse HEAD).Trim()
Write-Host "[BRG-3] shutdown stress start commit=$commit configuration=$Configuration iterations=$Iterations"

for ($i = 1; $i -le $Iterations; ++$i) {
    $p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
    Start-Sleep -Seconds $StartupSeconds
    $p.Refresh()

    if ($p.HasExited) {
        throw "[BRG-3] iteration=$i failed: process exited during startup, exitCode=$($p.ExitCode)"
    }

    if (-not $p.CloseMainWindow()) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[BRG-3] iteration=$i failed: CloseMainWindow() returned false"
    }

    if (-not $p.WaitForExit($ExitTimeoutSeconds * 1000)) {
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        throw "[BRG-3] iteration=$i failed: shutdown timeout"
    }

    if ($p.ExitCode -ne 0) {
        throw "[BRG-3] iteration=$i failed: exitCode=$($p.ExitCode)"
    }

    Write-Host "[BRG-3] iteration=$i PASS exitCode=0"
}

Write-Host "[BRG-3] shutdown stress PASS commit=$commit configuration=$Configuration iterations=$Iterations"
