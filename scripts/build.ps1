[CmdletBinding()]
param(
    [ValidateSet("Debug","Release","All")]
    [string]$Configuration = "All",
    [switch]$Clean,
    [switch]$SkipBootstrap
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Run([string]$Exe, [string[]]$Args) {
    & $Exe @Args
    if ($LASTEXITCODE -ne 0) { throw "[BRG-1] Command failed: $Exe $($Args -join ' ')" }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw "[BRG-1] Windows is required."
}

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\windows-msvc-x64"

if (-not $SkipBootstrap) {
    & (Join-Path $PSScriptRoot "bootstrap-deps.ps1")
    if ($LASTEXITCODE -ne 0) { throw "[BRG-1] Dependency bootstrap failed." }
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item $buildDir -Recurse -Force
}

Run "cmake.exe" @("--preset","windows-msvc-x64")

$configs = if ($Configuration -eq "All") { @("Debug","Release") } else { @($Configuration) }
foreach ($cfg in $configs) {
    $preset = if ($cfg -eq "Debug") { "windows-msvc-x64-debug" } else { "windows-msvc-x64-release" }
    Run "cmake.exe" @("--build","--preset",$preset)
}

$commit = (& git.exe -C $root rev-parse HEAD).Trim()
Write-Host "[BRG-1] Build wrapper complete. commit=$commit configuration=$Configuration CI=NOT_AVAILABLE"
