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
player_cpp = (ROOT / "native/src/player/player/Player.cpp").read_text(encoding="utf-8")
ytdlp_h = (ROOT / "native/src/YtDlpManager.h").read_text(encoding="utf-8")
lifecycle_h = (ROOT / "native/src/common/LifecycleEvidence.h").read_text(encoding="utf-8")
render_thread_cpp = (ROOT / "native/src/player/render/PlayBackRenderThread.cpp").read_text(encoding="utf-8")
render_thread_h = (ROOT / "native/src/player/render/PlayBackRenderThread.h").read_text(encoding="utf-8")
render_h = (ROOT / "native/src/player/render/PlayBackRender.h").read_text(encoding="utf-8")

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
    and "uint64_t startupLoadedLoadId = 0;" in models_h
    and "uint64_t startupVideoEvidenceLoadId = 0;" in models_h
    and "bool startupVideoEvidenceArmed = false;" in models_h
    and "uint32_t startupRestartCount = 0;" in models_h,
    "#33: startup transaction/evidence identity state is missing",
)
require(
    "bool startupMpvIdleReady = false;" in models_h,
    "#33: MPV idle startup readiness state is missing",
)
require(
    "m.pendingseektime = -1.0;" in command_cpp
    and "m.startupLoadId += 1;" in command_cpp
    and "m.startupLoadedLoadId = 0;" in command_cpp
    and "m.startupVideoEvidenceLoadId = 0;" in command_cpp
    and "m.startupVideoEvidenceArmed = false;" in command_cpp,
    "#33: direct LoadFile must reset stale seek/evidence state and advance startup load id",
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
    "m_renderer->BindStartupState(m_state.get())" in session_cpp
    and "BindStartupState(PlayerStateSystem* state)" in render_h
    and "m_startupState" in render_thread_h,
    "#33: render telemetry must bind the canonical startup load state",
)
require(
    "event=VIDEO_EVIDENCE_ARM" in observer_cpp
    and "boundary=video_reconfig" in observer_cpp
    and "startupLoadedLoadId = m.startupLoadId;" in observer_cpp,
    "#33: video evidence must arm only after the matching loaded/video-reconfig boundary",
)
require(
    "event=FIRST_VIDEO_FRAME" in render_thread_cpp
    and "armedLoadIdBeforeRender" in render_thread_cpp
    and "startupVideoEvidenceArmed" in render_thread_cpp
    and "mpv_render_context_render" in render_thread_cpp
    and "MarkAsReady" in render_thread_cpp
    and "armed_before_render=1" in render_thread_cpp
    and "boundary=video_reconfig" in render_thread_cpp
    and "playback.startupVideoEvidenceArmed = false;" in render_thread_cpp,
    "#33: first video-frame evidence must pre-arm, revalidate, publish, and disarm the exact load",
)

arm_snapshot_index = render_thread_cpp.find("uint64_t armedLoadIdBeforeRender = 0;")
render_call_index = render_thread_cpp.find(
    "mpv_render_context_render(this->state.render_ctx, params.data());",
    arm_snapshot_index,
)
ready_index = render_thread_cpp.find(
    "MarkAsReady(index, currentFrameId);",
    render_call_index,
)
first_video_marker_index = render_thread_cpp.find(
    "event=FIRST_VIDEO_FRAME",
    ready_index,
)
require(
    arm_snapshot_index >= 0
    and render_call_index > arm_snapshot_index
    and ready_index > render_call_index
    and first_video_marker_index > ready_index,
    "#33: AUD-33-VIDEO-01 requires arm snapshot before render and FIRST_VIDEO_FRAME only after FBO publication",
)
require(
    "event=CLI_LOAD_DEFERRED" in main_cpp
    and "event=CLI_LOAD_DISPATCH" in main_cpp
    and "startupMpvIdleReady" in main_cpp
    and "readiness=mpv_idle" in main_cpp,
    "#33: command-line startup must wait for MPV idle readiness before media dispatch",
)
require(
    "completedStartupCycles" not in main_cpp,
    "#33: startup dispatch must not depend on an arbitrary main-loop cycle count",
)
require(
    "commander->LoadFile(Url);" not in main_cpp,
    "#33: argv media must not be loaded directly during bootstrap",
)
require(
    "ResolveExecutableForPlayback" in player_cpp
    and '"script-opts"' in player_cpp
    and "script-opts-append" not in player_cpp
    and "ytdl_hook-ytdl_path=" in player_cpp
    and "event=YTDL_PATH_RESOLVED" in player_cpp
    and "event=YTDL_PATH_MISSING" in player_cpp,
    "#33: libmpv startup must bind a deterministic yt-dlp executable before initialization",
)
require(
    "CreateFileA(" in lifecycle_h
    and "FILE_APPEND_DATA" in lifecycle_h
    and "FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE" in lifecycle_h,
    "#33: startup diagnostic evidence must use explicit shared append semantics",
)
require(
    "GetModuleFileNameW" in ytdlp_h
    and 'configRoot / "Debug" / "yt-dlp.exe"' in ytdlp_h
    and 'configRoot / "Release" / "yt-dlp.exe"' in ytdlp_h
    and 'cwd / "bin" / "Debug" / "yt-dlp.exe"' in ytdlp_h
    and 'cwd / "bin" / "Release" / "yt-dlp.exe"' in ytdlp_h,
    "#33: yt-dlp resolver must cover packaged executable and Debug/Release development layouts",
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
    "event=MPV_IDLE_READY",
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
        "YTDL_PATH_RESOLVED",
        "YTDL_PATH_MISSING",
        "MPV_IDLE_READY",
        "YTDL_PATH_RESOLVED -> CLI_LOAD_DEFERRED -> MPV_IDLE_READY -> CLI_LOAD_DISPATCH -> LOAD_REQUEST -> START_FILE -> FILE_LOADED -> VIDEO_EVIDENCE_ARM -> FIRST_VIDEO_FRAME",
        "VIDEO_EVIDENCE_ARM",
        "FIRST_VIDEO_FRAME",
        "DYNAMIC_CONFIG_APPLY_BEGIN",
        "issue33-summary.json",
        '^\\[BRG5-DIAG\\] category=STARTUP ',
        "-WorkingDirectory $root",
        "StartupTimeoutSeconds = 60",
        "extra PLAYBACK_RESTART loop observed",
        "pending_seek_before=-1",
        "Assert-NoExistingImPlayer",
        'Get-Process -Name "Im_player"',
        "[System.IO.FileShare]::ReadWrite",
        "[System.IO.StreamReader]::new",
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
