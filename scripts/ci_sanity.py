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
    "docs/policy/MULTI_AGENT_COORDINATION_POLICY.md",
    "docs/policy/BUILD_POLICY.md",
    ".github/ISSUE_TEMPLATE/canonical_work_item.yml",
    ".github/ISSUE_TEMPLATE/integration_wave.yml",
    ".github/pull_request_template.md",
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
