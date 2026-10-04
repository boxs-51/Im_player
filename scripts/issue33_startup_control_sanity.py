#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
errors = []

def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)

command_h = (ROOT / "native/src/player/command/PlaybackCommand.h").read_text(encoding="utf-8")
command_cpp = (ROOT / "native/src/player/command/PlaybackCommand.cpp").read_text(encoding="utf-8")
models_h = (ROOT / "native/src/player/PlayerDataModels.h").read_text(encoding="utf-8")
observer_cpp = (ROOT / "native/src/player/event/PlaybackObserver.cpp").read_text(encoding="utf-8")
session_cpp = (ROOT / "native/src/player/session/PlayerSession.cpp").read_text(encoding="utf-8")
main_cpp = (ROOT / "native/src/main1.cpp").read_text(encoding="utf-8")

require(
    'extraFlags = "replace"' in command_h,
    "#33: direct LoadFile default must use replace semantics",
)
require(
    'extraFlags = "append-play"' not in command_h,
    "#33: append-play must not be the direct-load default",
)
require(
    "double pendingseektime = -1.0;" in models_h,
    "#33: fresh PlaybackModel must start with no pending seek",
)
require(
    "uint64_t startupLoadId = 0;" in models_h
    and "uint32_t startupRestartCount = 0;" in models_h,
    "#33: startup transaction identity/counter is missing",
)
require(
    "m.pendingseektime = -1.0;" in command_cpp
    and "m.startupLoadId += 1;" in command_cpp,
    "#33: direct LoadFile must clear stale pending seek and advance startup load id",
)
require(
    "Uint64 lastSeekRequestTime = 0;" in command_cpp,
    "#33: delayed seek debounce must preserve the full Uint64 timestamp",
)
require(
    "bool lastSeekRequestTime" not in command_cpp,
    "#33: delayed seek timestamp must never narrow to bool",
)
require(
    "std::make_unique<PlaybackCommand>(*m_player, *m_state)" in session_cpp,
    "#33: PlaybackCommand must be bound to the session state",
)
require(
    "event=CLI_LOAD_DEFERRED" in main_cpp
    and "event=CLI_LOAD_DISPATCH" in main_cpp
    and "completedStartupCycles > 0" in main_cpp,
    "#33: command-line startup must defer media dispatch until after a main-loop cycle",
)
require(
    "commander->LoadFile(Url);" not in main_cpp,
    "#33: argv media must not be loaded directly during bootstrap",
)

for marker in (
    "event=LOAD_REQUEST",
    "event=LOAD_COMMAND_RESULT",
):
    require(marker in command_cpp, f"#33: startup command marker missing: {marker}")

for marker in (
    "event=START_FILE",
    "event=FILE_LOADED",
    "event=PLAYBACK_RESTART",
    "event=SEEK_REQUEST",
    "event=SEEK",
    "event=DYNAMIC_CONFIG_APPLY_BEGIN",
    "event=END_FILE",
    "timing=post_file_loaded",
):
    require(marker in observer_cpp, f"#33: startup observer marker missing: {marker}")

require(
    command_cpp.count('EmitDiagnostic(\n        "STARTUP"') >= 2,
    "#33: startup command markers must persist to the lifecycle evidence sink",
)
require(
    observer_cpp.count('EmitDiagnostic(\n                "STARTUP"') >= 6,
    "#33: startup observer markers must persist to the lifecycle evidence sink",
)
cold_url_harness = ROOT / "scripts/issue33-cold-url-smoke.ps1"
require(
    cold_url_harness.is_file(),
    "#33: repeated cold URL startup harness is missing",
)
if cold_url_harness.is_file():
    harness_text = cold_url_harness.read_text(encoding="utf-8")
    for token in (
        "Iterations = 20",
        "flags=replace",
        "pending_seek=-1",
        "event=SEEK_REQUEST",
        "CLI_LOAD_DEFERRED",
        "CLI_LOAD_DISPATCH",
        "CLI_LOAD_DEFERRED -> CLI_LOAD_DISPATCH -> LOAD_REQUEST -> START_FILE -> FILE_LOADED",
        "DYNAMIC_CONFIG_APPLY_BEGIN",
        "issue33-summary.json",
        '^\\[BRG5-DIAG\\] category=STARTUP ',
        "-WorkingDirectory $root",
        "StartupTimeoutSeconds = 60",
        "Assert-NoExistingImPlayer",
        'Get-Process -Name "Im_player"',
    ):
        require(token in harness_text, f"#33: cold URL harness contract missing: {token}")

restart_index = observer_cpp.find("event=PLAYBACK_RESTART")
seek_request_index = observer_cpp.find("event=SEEK_REQUEST")
require(
    restart_index >= 0 and seek_request_index > restart_index,
    "#33: pending format-switch seek must be traceable after PLAYBACK_RESTART",
)

if errors:
    for error in errors:
        print(f"::error::{error}")
    print(f"[ISSUE-33] startup control sanity FAIL ({len(errors)} finding(s))")
    sys.exit(1)

print("[ISSUE-33] startup control sanity PASS")
