#!/usr/bin/env python3
"""BRG-2 repository sanity checks.

This script intentionally avoids dependency installation. It validates that the
tracked build contract is internally consistent before the expensive Windows
jobs start.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "artifacts" / "ci"
REPORT_DIR.mkdir(parents=True, exist_ok=True)

errors: list[str] = []
facts: dict[str, object] = {}


def fail(message: str) -> None:
    errors.append(message)


def load_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        fail(f"cannot parse {path.relative_to(ROOT)}: {exc}")
        return {}


def extract_function_body(text: str, marker: str) -> str:
    start = text.find(marker)
    if start < 0:
        fail(f"BRG-4 contract marker missing: {marker}")
        return ""

    brace = text.find("{", start)
    if brace < 0:
        fail(f"BRG-4 contract body missing: {marker}")
        return ""

    depth = 0
    for index in range(brace, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:index]

    fail(f"BRG-4 contract body is unterminated: {marker}")
    return ""


def token_is_under_lock(body: str, token: str, lock_token: str) -> bool:
    depth = 0
    lock_depths: list[int] = []

    for line in body.splitlines():
        stripped = line.strip()

        # Account for leading scope exits before evaluating this line.
        leading_closes = len(stripped) - len(stripped.lstrip("}"))
        if leading_closes:
            depth = max(0, depth - leading_closes)
            lock_depths = [item for item in lock_depths if item <= depth]

        if lock_token in line:
            lock_depths.append(depth)

        if token in line:
            return any(item <= depth for item in lock_depths)

        # Approximate lexical scope depth. The BRG-4 target functions use
        # ordinary block scopes without braces in strings on the target lines.
        depth += line.count("{") - line.count("}") + leading_closes
        lock_depths = [item for item in lock_depths if item <= depth]

    fail(f"BRG-4 contract token missing: {token}")
    return False


required = [
    "CMakeLists.txt",
    "CMakePresets.json",
    "vcpkg.json",
    "deps.lock.json",
    "cmake/ImPlayerDependencies.cmake",
    "scripts/bootstrap-deps.ps1",
    "scripts/build.ps1",
    "docs/BUILDING.md",
    "docs/policy/PROJECT_EXECUTION_POLICY.md",
    "docs/policy/BUILD_POLICY.md",
]
missing_required = [p for p in required if not (ROOT / p).is_file()]
if missing_required:
    fail("missing required files: " + ", ".join(missing_required))

if (ROOT / "CMakeSettings.json").exists():
    fail("legacy CMakeSettings.json must remain absent")

lock = load_json(ROOT / "deps.lock.json")
manifest = load_json(ROOT / "vcpkg.json")
presets = load_json(ROOT / "CMakePresets.json")

lock_commit = lock.get("vcpkg", {}).get("commit")
manifest_baseline = manifest.get("builtin-baseline")
if lock_commit != manifest_baseline:
    fail(
        f"vcpkg baseline mismatch: deps.lock={lock_commit!r} "
        f"vcpkg.json={manifest_baseline!r}"
    )

toolchain = lock.get("toolchain", {})
if toolchain.get("platform") != "Windows":
    fail("deps.lock toolchain.platform must be Windows")
if toolchain.get("architecture") != "x64":
    fail("deps.lock toolchain.architecture must be x64")
if toolchain.get("generator") != "Visual Studio 18 2026":
    fail("deps.lock generator must remain Visual Studio 18 2026")
if toolchain.get("toolset") != "v145":
    fail("deps.lock toolset must remain v145")
if toolchain.get("vcpkgTriplet") != "x64-windows":
    fail("deps.lock vcpkgTriplet must remain x64-windows")

configure = {
    item.get("name"): item for item in presets.get("configurePresets", [])
}.get("windows-msvc-x64")
if not configure:
    fail("missing configure preset windows-msvc-x64")
else:
    if configure.get("generator") != "Visual Studio 18 2026":
        fail("configure preset generator drifted")
    toolset_value = configure.get("toolset", {}).get("value", "")
    if not str(toolset_value).startswith("v145"):
        fail("configure preset toolset must start with v145")
    cache = configure.get("cacheVariables", {})
    if cache.get("VCPKG_TARGET_TRIPLET") != "x64-windows":
        fail("configure preset VCPKG_TARGET_TRIPLET drifted")

minimum = presets.get("cmakeMinimumRequired", {})
minimum_text = ".".join(str(minimum.get(k, 0)) for k in ("major", "minor", "patch"))
if minimum_text != toolchain.get("cmakeMinimum"):
    fail(
        f"CMake minimum mismatch: presets={minimum_text} "
        f"deps.lock={toolchain.get('cmakeMinimum')}"
    )

cmake_path = ROOT / "CMakeLists.txt"
cmake_text = cmake_path.read_text(encoding="utf-8")
source_refs = set(
    re.findall(
        r"(?m)^\s*(native/[A-Za-z0-9_./-]+\.(?:c|cc|cpp|cxx))\s*$",
        cmake_text,
    )
)
missing_refs = sorted(ref for ref in source_refs if not (ROOT / ref).is_file())
if missing_refs:
    fail("CMake references missing sources: " + ", ".join(missing_refs))

impl_suffixes = {".c", ".cc", ".cpp", ".cxx"}
native_impl = {
    p.relative_to(ROOT).as_posix()
    for p in (ROOT / "native" / "src").rglob("*")
    if p.is_file() and p.suffix.lower() in impl_suffixes
}
source_graph = lock.get("sourceGraph", {})
allowed_states = {"EXCLUDED-PROTOTYPE", "REMOVED"}
for path, state in source_graph.items():
    if state not in allowed_states:
        fail(f"unsupported sourceGraph state for {path}: {state}")
    if state != "REMOVED" and not (ROOT / path).is_file():
        fail(f"sourceGraph entry does not exist: {path}")

unclassified = sorted(native_impl - source_refs - set(source_graph))
if unclassified:
    fail(
        "native/src implementations are neither BUILD nor explicitly classified: "
        + ", ".join(unclassified)
    )

tracked_deps = subprocess.run(
    ["git", "-C", str(ROOT), "ls-files", "deps"],
    check=True,
    text=True,
    capture_output=True,
).stdout.splitlines()
unexpected_generated_deps = [
    p
    for p in tracked_deps
    if p.startswith(("deps/vcpkg/", "deps/vcpkg_installed/", "deps/mpv/", "deps/cache/"))
]
if unexpected_generated_deps:
    fail(
        "generated dependency content must not be tracked: "
        + ", ".join(unexpected_generated_deps[:20])
    )

# BRG-4 manager lifecycle regression contract.
# These focused structural checks intentionally fail closed if the lock
# boundaries are rewritten and require a fresh lifecycle audit.
player_manager_cpp = (ROOT / "native/src/player/session/PlayerManager.cpp").read_text(
    encoding="utf-8"
)
player_manager_h = (ROOT / "native/src/player/session/PlayerManager.h").read_text(
    encoding="utf-8"
)
window_manager_h = (ROOT / "native/src/windows/WindowManager.h").read_text(
    encoding="utf-8"
)
font_manager_cpp = (ROOT / "native/src/FontManager.cpp").read_text(encoding="utf-8")
main_cpp = (ROOT / "native/src/main1.cpp").read_text(encoding="utf-8")

create_session_body = extract_function_body(
    player_manager_cpp, "PlayerSession *PlayerManager::CreateSession"
)
destroy_session_body = extract_function_body(
    player_manager_cpp, "void PlayerManager::DestroySession(const std::string &id)"
)
with_session_body = extract_function_body(
    player_manager_h, "void WithSession(const std::string& id, Func&& func)"
)
destroy_window_body = extract_function_body(
    window_manager_h, "void DestroyWindow(WindowId id)"
)
font_load_body = extract_function_body(
    font_manager_cpp, "bool FontManager::EnsureFontDataLoaded"
)
font_shutdown_body = extract_function_body(
    font_manager_cpp, "void FontManager::Shutdown"
)

manager_lock = "std::lock_guard<std::mutex> lock(m_sessionsMutex)"
window_lock = "std::lock_guard<std::mutex> lock(m_windowsMutex)"
font_lock = "std::lock_guard<std::mutex> lock(m_mutex)"

if token_is_under_lock(create_session_body, "session->Init(runtime)", manager_lock):
    fail("BRG-4: PlayerSession::Init must execute outside m_sessionsMutex")
if token_is_under_lock(destroy_session_body, "detached.reset()", manager_lock):
    fail("BRG-4: PlayerSession destruction must execute outside m_sessionsMutex")
if token_is_under_lock(with_session_body, "func(*session)", manager_lock):
    fail("BRG-4: WithSession callback must execute outside m_sessionsMutex")
if token_is_under_lock(destroy_window_body, "runtime.reset()", window_lock):
    fail("BRG-4: WindowRuntime destruction must execute outside m_windowsMutex")
if token_is_under_lock(font_load_body, "std::ifstream fi(filePath", font_lock):
    fail("BRG-4: font filesystem I/O must execute outside FontManager mutex")
if not token_is_under_lock(
    font_load_body, "desc->fileData = std::move(loadedData)", font_lock
):
    fail("BRG-4: font data publication must execute under FontManager mutex")
if "generation = m_generation;" not in font_load_body:
    fail("BRG-4: lazy font load must snapshot the registry generation")
if "generation != m_generation" not in font_load_body:
    fail("BRG-4: stale lazy font load must be rejected after lifecycle change")
if "++m_generation;" not in font_shutdown_body:
    fail("BRG-4: FontManager shutdown must invalidate in-flight lazy loads")

destroy_all_index = main_cpp.find("PlayerManager::GetInstance().DestroyAllSessions();")
font_shutdown_index = main_cpp.find("FontManager::Instance().Shutdown();")
if destroy_all_index < 0 or font_shutdown_index < 0:
    fail("BRG-4: explicit PlayerManager/FontManager shutdown markers missing")
elif destroy_all_index > font_shutdown_index:
    fail("BRG-4: Player sessions must be destroyed before FontManager shutdown")

if "m_pendingSessionIds.erase(sessionId);" not in create_session_body:
    fail("BRG-4: pending session reservation cleanup missing")

facts["brg4_lifecycle_contract"] = "PASS" if not errors else "FAIL"

commit = subprocess.run(
    ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
    check=True,
    text=True,
    capture_output=True,
).stdout.strip()

facts.update(
    {
        "commit": commit,
        "required_file_count": len(required),
        "cmake_source_ref_count": len(source_refs),
        "native_src_implementation_count": len(native_impl),
        "explicit_source_graph": source_graph,
        "vcpkg_baseline": lock_commit,
        "generator": toolchain.get("generator"),
        "toolset": toolchain.get("toolset"),
        "cmake_minimum": toolchain.get("cmakeMinimum"),
        "result": "FAIL" if errors else "PASS",
        "errors": errors,
    }
)

report = REPORT_DIR / "static-sanity.json"
report.write_text(json.dumps(facts, indent=2) + "\n", encoding="utf-8")

print(f"[BRG-2] commit={commit}")
print(
    f"[BRG-2] source refs={len(source_refs)} "
    f"native/src implementations={len(native_impl)}"
)
print(f"[BRG-2] vcpkg={lock_commit}")
print(
    f"[BRG-2] toolchain={toolchain.get('generator')} "
    f"{toolchain.get('toolset')} CMake>={toolchain.get('cmakeMinimum')}"
)

if errors:
    for item in errors:
        print(f"::error::{item}")
    print(f"[BRG-2] static sanity FAIL ({len(errors)} finding(s))")
    sys.exit(1)

print("[BRG-2] static sanity PASS")
