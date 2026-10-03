#!/usr/bin/env python3
"""Validate BRG-5 lifecycle evidence emitted by Im_player.

AUD-8-01 contract:
- one lifecycle log represents exactly one process;
- each object identity is one-shot within that process;
- each object's phase list must exactly equal the canonical sequence;
- repeated stress uses fresh process/log iterations rather than restarting the
  same object identity after DESTROY.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable

LINE_RE = re.compile(
    r"^\[Lifecycle\] "
    r"phase=(?P<phase>[A-Z]+) "
    r"component=(?P<component>\S+) "
    r"id=(?P<identity>\S+) "
    r"pid=(?P<pid>\d+) "
    r"tid=(?P<tid>\d+)\s*$"
)

REQUIRED = {
    "Window": ("CREATE", "START", "STOP", "DESTROY"),
    "PlayerSession": ("CREATE", "START", "STOP", "DESTROY"),
    "MPVRenderContext": ("CREATE", "START", "STOP", "DESTROY"),
    "UIRenderThread": ("CREATE", "START", "STOP", "JOIN", "DESTROY"),
    "PlayBackRenderThread": ("CREATE", "START", "STOP", "JOIN", "DESTROY"),
    "AudioCaptureManager": ("CREATE", "START", "STOP", "JOIN", "DESTROY"),
    "AudioProcessor": ("CREATE", "START", "STOP", "JOIN", "DESTROY"),
    "AudioOutputWorker": ("CREATE", "START", "STOP", "JOIN", "DESTROY"),
}


def fail(message: str) -> None:
    print(f"[BRG5-A] ERROR: {message}", file=sys.stderr)


def lifecycle_line(
    component: str,
    identity: str,
    phase: str,
    *,
    pid: int = 100,
    tid: int = 200,
) -> str:
    return (
        f"[Lifecycle] phase={phase} component={component} id={identity} "
        f"pid={pid} tid={tid}"
    )


def validate_lines(lines: Iterable[str]) -> tuple[list[str], int]:
    groups: dict[tuple[str, str], list[str]] = defaultdict(list)
    pids: set[str] = set()
    parsed = 0
    errors: list[str] = []

    for raw in lines:
        match = LINE_RE.match(raw.strip())
        if not match:
            continue

        parsed += 1
        component = match.group("component")
        identity = match.group("identity")
        phase = match.group("phase")
        pid = match.group("pid")

        if not identity:
            errors.append(f"{component}: empty identity")
            continue

        if component not in REQUIRED:
            errors.append(f"unknown lifecycle component: {component}")
            continue

        groups[(component, identity)].append(phase)
        pids.add(pid)

    if parsed == 0:
        errors.append("no lifecycle markers parsed")
        return errors, parsed

    if len(pids) != 1:
        errors.append(f"expected one process per smoke log, got pid set={sorted(pids)}")

    for component, expected in REQUIRED.items():
        component_groups = [
            (identity, phases)
            for (group_component, identity), phases in groups.items()
            if group_component == component
        ]

        if not component_groups:
            errors.append(f"{component}: no lifecycle object observed")
            continue

        for identity, phases in component_groups:
            actual = tuple(phases)
            if actual != expected:
                errors.append(
                    f"{component} id={identity}: invalid lifecycle {list(actual)}; "
                    f"expected exactly {' -> '.join(expected)}"
                )

    return errors, parsed


def build_valid_selftest_log() -> list[str]:
    lines: list[str] = []
    for index, (component, expected) in enumerate(REQUIRED.items(), start=1):
        identity = f"{component}-selftest-{index}"
        for phase in expected:
            lines.append(lifecycle_line(component, identity, phase))
    return lines


def run_self_test() -> int:
    valid = build_valid_selftest_log()
    errors, _ = validate_lines(valid)
    if errors:
        fail("self-test valid lifecycle was rejected: " + " | ".join(errors))
        return 1

    invalid_cases: dict[str, list[str]] = {}

    post_destroy = valid.copy()
    post_destroy.append(lifecycle_line("UIRenderThread", "UIRenderThread-selftest-4", "START"))
    invalid_cases["phase_after_destroy"] = post_destroy

    duplicate_create = valid.copy()
    duplicate_create.insert(
        1,
        lifecycle_line("Window", "Window-selftest-1", "CREATE"),
    )
    invalid_cases["duplicate_create"] = duplicate_create

    reordered = valid.copy()
    window_identity = "Window-selftest-1"
    window_lines = [
        lifecycle_line("Window", window_identity, "CREATE"),
        lifecycle_line("Window", window_identity, "STOP"),
        lifecycle_line("Window", window_identity, "START"),
        lifecycle_line("Window", window_identity, "DESTROY"),
    ]
    invalid_cases["out_of_order"] = window_lines + [
        line for line in valid if f"component=Window id={window_identity} " not in line
    ]

    missing_join = [
        line
        for line in valid
        if not (
            "component=AudioProcessor id=AudioProcessor-selftest-7 " in line
            and "phase=JOIN " in line
        )
    ]
    invalid_cases["missing_join"] = missing_join

    multi_pid = valid.copy()
    multi_pid.append(
        lifecycle_line(
            "Window",
            "Window-second-process",
            "CREATE",
            pid=101,
        )
    )
    invalid_cases["multiple_processes"] = multi_pid

    for name, lines in invalid_cases.items():
        invalid_errors, _ = validate_lines(lines)
        if not invalid_errors:
            fail(f"self-test invalid case unexpectedly passed: {name}")
            return 1

    print(
        "[BRG5-A] lifecycle validator self-test PASS "
        f"grammar=one-shot cases={len(invalid_cases) + 1}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, nargs="?")
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run deterministic valid/invalid lifecycle grammar tests",
    )
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    if args.log is None:
        parser.error("log path is required unless --self-test is used")

    if not args.log.is_file():
        fail(f"lifecycle log missing: {args.log}")
        return 1

    errors, parsed = validate_lines(
        args.log.read_text(encoding="utf-8-sig").splitlines()
    )

    if errors:
        for error in errors:
            fail(error)
        return 1

    print(
        f"[BRG5-A] lifecycle runtime validation PASS "
        f"log={args.log} markers={parsed} grammar=one-shot"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
