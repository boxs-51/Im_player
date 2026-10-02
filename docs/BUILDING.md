# Building Im_player - Canonical BRG-1 Contract

**Policy:** IP-POL-001  
**Build Policy:** IP-BUILD-001  
**Canonical Issue:** #4

## Supported baseline

| Item | Canonical value |
|---|---|
| Host | Windows x64 |
| Generator | Visual Studio 18 2026 |
| Toolset family | v145 |
| CMake | 4.2.0+ |
| ABI | MSVC x64 |
| C++ | C++17 |
| vcpkg triplet | x64-windows |
| Configure authority | CMakePresets.json |

The exact installed MSVC patch version is captured in ignored
`deps/toolchain.actual.json` and must be included in PASS evidence.

## Prerequisites

Install Visual Studio 18 2026 with Desktop development with C++, CMake 4.2+,
Git, PowerShell 5.1+, and 7-Zip.

Root-level `vcpkg/` and `libs/` are not valid prerequisites.

## Locked dependencies

Machine-readable authority: `deps.lock.json`.

vcpkg is pinned to:

`f05ff9f0adbd630c4fada9072b4a68eeeb7d738a`

Resolved ports at that baseline:

| Port | Version |
|---|---|
| SDL2 | 2.32.10#1 |
| FreeType | 2.14.3 |
| CPR | 1.14.2 |
| OpenSSL | 3.6.5 |

vcpkg owns headers and binaries for those dependencies.

libmpv is pinned to:

- provider: `shinchiro/mpv-winbuild-cmake`
- release: `20260928`
- asset: `mpv-dev-x86_64-20260928-git-e470f8986e.7z`
- SHA-256: `81795d759e01016f1550fd71651a1a5d59ab5c28ef31c0b6793224e9cff39459`
- mpv commit identity: `e470f8986e`
- expected client API: `2.5`

The bootstrap verifies the archive hash and client API. If the package does
not contain an MSVC import library, it deterministically creates
`libmpv-2.dll.lib` from `libmpv-2.dll` exports using the installed v145 tools.

## Fresh-clone commands

```powershell
git checkout <exact-commit-under-test>
powershell -ExecutionPolicy Bypass -File .\scripts\bootstrap-deps.ps1
cmake --preset windows-msvc-x64
cmake --build --preset windows-msvc-x64-debug
cmake --build --preset windows-msvc-x64-release
```

Equivalent wrapper:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration All -Clean
```

## Generated dependency layout

```text
deps/
  cache/
  mpv/
    include/mpv/
    libmpv-2.dll
    libmpv-2.dll.lib
  toolchain.actual.json
  vcpkg/
  vcpkg_installed/
```

`deps/` is generated and ignored. `deps.lock.json` is tracked.

## Runtime closure

CMake explicitly copies `libmpv-2.dll` next to `Im_player.exe`.
vcpkg uses app-local deployment for its runtime DLLs.

A compile/link success is not L2 PASS. The executable must actually be
launched on Windows without missing-DLL/resource errors.

## Header authority

Package include directories are prepended before `native/include`, preventing
legacy vendored copies of mpv/SDL2/FreeType/CPR/OpenSSL from silently
overriding the canonical packages.

Both existing SDL include forms (`<SDL.h>` and `<SDL2/SDL.h>`) resolve from
the vcpkg installation, not from the legacy vendored copy.

The legacy copies are retained in BRG-1 to avoid unrelated mass cleanup.

## Source graph

`native/src/api/api_manager.cpp` is `EXCLUDED-PROTOTYPE`.

BRG-1 does not activate the API/Gemini prototype.

## IDE rule

`CMakePresets.json` is the canonical configure source.
Legacy `CMakeSettings.json` is removed because it defined a conflicting path.
VS Code must use the CMake Tools configuration provider instead of a separate
manual toolchain definition.

## Evidence status

Until the fresh-clone commands are executed on the exact PR head:

```text
L0 Configure = NEEDS-RUNTIME-PROOF
L1 Debug     = NEEDS-RUNTIME-PROOF
L1 Release   = NEEDS-RUNTIME-PROOF
L2 Launch    = NEEDS-RUNTIME-PROOF
CI           = NOT_AVAILABLE (BRG-2 #5)
```

Documentation or static review alone must not convert these statuses to PASS.
