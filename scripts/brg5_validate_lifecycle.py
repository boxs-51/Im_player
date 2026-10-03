#!/usr/bin/env python3
"""Validate BRG-5 lifecycle evidence emitted by Im_player."""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()

    if not args.log.is_file():
        fail(f"lifecycle log missing: {args.log}")
        return 1

    groups: dict[tuple[str, str], list[str]] = defaultdict(list)
    pids: set[str] = set()
    parsed = 0

    for raw in args.log.read_text(encoding="utf-8-sig").splitlines():
        match = LINE_RE.match(raw.strip())
        if not match:
            continue
        parsed += 1
        component = match.group("component")
        identity = match.group("identity")
        phase = match.group("phase")
        pid = match.group("pid")
        if not identity:
            fail(f"{component}: empty identity")
            return 1
        groups[(component, identity)].append(phase)
        pids.add(pid)

    if parsed == 0:
        fail("no lifecycle markers parsed")
        return 1

    errors: list[str] = []

    if len(pids) != 1:
        errors.append(f"expected one process per smoke log, got pid set={sorted(pids)}")

    for component, expected in REQUIRED.items():
        matching = [
            (identity, phases)
            for (group_component, identity), phases in groups.items()
            if group_component == component and "CREATE" in phases
        ]

        if not matching:
            errors.append(f"{component}: no lifecycle beginning with CREATE")
            continue

        for identity, phases in matching:
            missing = [phase for phase in expected if phase not in phases]
            if missing:
                errors.append(
                    f"{component} id={identity}: missing phases {missing}; got {phases}"
                )
                continue

            positions = [phases.index(phase) for phase in expected]
            if positions != sorted(positions):
                errors.append(
                    f"{component} id={identity}: invalid order {phases}; "
                    f"expected {' -> '.join(expected)}"
                )

        orphan_groups = [
            (identity, phases)
            for (group_component, identity), phases in groups.items()
            if group_component == component and "CREATE" not in phases
        ]
        for identity, phases in orphan_groups:
            errors.append(
                f"{component} id={identity}: phases {phases} have no matching CREATE; "
                "identity changed during lifecycle"
            )

    if errors:
        for error in errors:
            fail(error)
        return 1

    print(
        f"[BRG5-A] lifecycle runtime validation PASS "
        f"log={args.log} markers={parsed}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
