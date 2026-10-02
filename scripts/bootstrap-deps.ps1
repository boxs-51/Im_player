[CmdletBinding()]
param([switch]$Force)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Run([string]$Exe, [string[]]$ArgumentList) {
    & $Exe @ArgumentList
    if ($LASTEXITCODE -ne 0) { throw "[BRG-1] Failed: $Exe $($ArgumentList -join ' ')" }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
    throw "[BRG-1] Windows x64 is required."
}

$root = Split-Path -Parent $PSScriptRoot
$lock = Get-Content (Join-Path $root "deps.lock.json") -Raw | ConvertFrom-Json

$gitCmd = Get-Command git.exe -ErrorAction Stop
$sevenCmd = Get-Command 7z.exe -ErrorAction SilentlyContinue
$seven = if ($sevenCmd) { $sevenCmd.Source } else { Join-Path $env:ProgramFiles "7-Zip\7z.exe" }
if (-not (Test-Path $seven)) { throw "[BRG-1] 7-Zip is required." }

$pf86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$vswhere = Join-Path $pf86 "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "[BRG-1] Visual Studio Installer/vswhere is required." }

$vs = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $vs) { throw "[BRG-1] Visual Studio C++ tools not found." }

$tool = Get-ChildItem (Join-Path $vs "VC\Tools\MSVC") -Directory |
    Where-Object { $_.Name -like "14.5*" } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $tool) { throw "[BRG-1] v145/MSVC 14.5x toolset not found." }

$bin = Join-Path $tool.FullName "bin\Hostx64\x64"
$dumpbin = Join-Path $bin "dumpbin.exe"
$libexe = Join-Path $bin "lib.exe"
$cl = Join-Path $bin "cl.exe"
foreach ($p in @($dumpbin,$libexe,$cl)) { if (-not (Test-Path $p)) { throw "[BRG-1] Missing: $p" } }

$deps = Join-Path $root "deps"
$cache = Join-Path $deps "cache"
$vcpkg = Join-Path $deps "vcpkg"
$mpv = Join-Path $deps "mpv"
New-Item -ItemType Directory -Force -Path $deps,$cache | Out-Null

if (-not (Test-Path (Join-Path $vcpkg ".git"))) {
    if (Test-Path $vcpkg) { Remove-Item $vcpkg -Recurse -Force }
    Run $gitCmd.Source @("clone","--filter=blob:none","--no-checkout",$lock.vcpkg.repository,$vcpkg)
}
Run $gitCmd.Source @("-C",$vcpkg,"fetch","--depth","1","origin",$lock.vcpkg.commit)
Run $gitCmd.Source @("-C",$vcpkg,"checkout","--detach",$lock.vcpkg.commit)
$actualVcpkg = (& $gitCmd.Source -C $vcpkg rev-parse HEAD).Trim()
if ($actualVcpkg -ne $lock.vcpkg.commit) { throw "[BRG-1] vcpkg commit mismatch." }
Run (Join-Path $vcpkg "bootstrap-vcpkg.bat") @("-disableMetrics")

$archive = Join-Path $cache $lock.libmpv.asset
if ($Force -or -not (Test-Path $archive) -or
    (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.libmpv.sha256.ToLowerInvariant()) {
    Invoke-WebRequest -UseBasicParsing -Uri $lock.libmpv.url -OutFile $archive
}
$hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -ne $lock.libmpv.sha256.ToLowerInvariant()) { throw "[BRG-1] libmpv SHA-256 mismatch." }

$tmp = Join-Path $deps ".mpv-extract"
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
Run $seven @("x",$archive,"-o$tmp","-y")

$dll = Get-ChildItem $tmp -Recurse -File -Filter $lock.libmpv.runtimeDll | Select-Object -First 1
$client = Get-ChildItem $tmp -Recurse -File -Filter client.h | Where-Object { $_.Directory.Name -eq "mpv" } | Select-Object -First 1
if (-not $dll -or -not $client) { throw "[BRG-1] Incomplete libmpv package." }

if (Test-Path $mpv) { Remove-Item $mpv -Recurse -Force }
New-Item -ItemType Directory -Force -Path $mpv | Out-Null
Copy-Item $client.Directory.Parent.FullName (Join-Path $mpv "include") -Recurse
Copy-Item $dll.FullName (Join-Path $mpv $lock.libmpv.runtimeDll)

$header = Get-Content (Join-Path $mpv "include\mpv\client.h") -Raw
if ($header -notmatch 'MPV_CLIENT_API_VERSION\s+MPV_MAKE_VERSION\((\d+),\s*(\d+)\)') { throw "[BRG-1] Cannot read mpv API." }
$api = "$($Matches[1]).$($Matches[2])"
if ($api -ne $lock.libmpv.clientApi) { throw "[BRG-1] mpv API mismatch: $api" }

$implib = Join-Path $mpv $lock.libmpv.msvcImportLibrary
$packaged = Get-ChildItem $tmp -Recurse -File -Filter $lock.libmpv.msvcImportLibrary | Select-Object -First 1
if ($packaged) { Copy-Item $packaged.FullName $implib }
if (-not (Test-Path $implib)) {
    $exports = & $dumpbin /exports (Join-Path $mpv $lock.libmpv.runtimeDll)
    $names = @($exports | ForEach-Object {
        if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') { $Matches[1] }
    } | Where-Object { $_ } | Sort-Object -Unique)
    if ($names.Count -lt 10) { throw "[BRG-1] Unexpected libmpv export table." }
    $def = Join-Path $mpv "libmpv-2.def"
    @("LIBRARY $($lock.libmpv.runtimeDll)","EXPORTS") + $names | Set-Content $def -Encoding ASCII
    Run $libexe @("/def:$def","/out:$implib","/machine:x64")
}

[ordered]@{
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    visualStudioInstallation = $vs
    msvcToolsetVersion = $tool.Name
    clFileVersion = (Get-Item $cl).VersionInfo.FileVersion
    vcpkgCommit = $actualVcpkg
    libmpvAsset = $lock.libmpv.asset
    libmpvSha256 = $hash
    libmpvClientApi = $api
} | ConvertTo-Json | Set-Content (Join-Path $deps "toolchain.actual.json") -Encoding UTF8

Remove-Item $tmp -Recurse -Force
Write-Host "[BRG-1] Bootstrap complete: vcpkg=$actualVcpkg mpv=$($lock.libmpv.asset) MSVC=$($tool.Name)"
