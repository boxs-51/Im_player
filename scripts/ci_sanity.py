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
    "docs/policy/MULTI_AGENT_COORDINATION_POLICY.md",
    "docs/policy/BUILD_POLICY.md",
    ".github/ISSUE_TEMPLATE/canonical_work_item.yml",
    ".github/ISSUE_TEMPLATE/integration_wave.yml",
    ".github/pull_request_template.md",
    "native/src/common/LifecycleEvidence.h",
    "docs/BRG5_RUNTIME_EVIDENCE.md",
    "scripts/brg5_lifecycle_sanity.py",
    "scripts/brg5_validate_lifecycle.py",
    "scripts/brg5-startup-shutdown.ps1",
    "scripts/brg5-playback-window-smoke.ps1",
    "scripts/brg5_generate_media_fixture.py",
    "scripts/brg5_generate_video_fixture.py",
    "scripts/brg5-multi-window-smoke.ps1",
    "scripts/brg5-lifecycle-audio-stress.ps1",
    "scripts/brg5-render-callback-shutdown.ps1",
    "scripts/issue19-event-render-stress.ps1",
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
window_factory_h = (ROOT / "native/src/windows/WindowFactory.h").read_text(
    encoding="utf-8"
)
hotkey_handler_cpp = (ROOT / "native/src/hotkey_handler.cpp").read_text(
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
if "retiredRegistry.swap(m_fontRegistry);" not in font_shutdown_body:
    fail("BRG-4: FontManager shutdown must detach registry ownership under lock")
if "retiredList.swap(m_fontList);" not in font_shutdown_body:
    fail("BRG-4: FontManager shutdown must detach font-list ownership under lock")
if "fileData->clear()" in font_shutdown_body or "fileData.reset()" in font_shutdown_body:
    fail("BRG-4: FontManager shutdown must not mutate shared descriptor font data")

destroy_all_index = main_cpp.find("PlayerManager::GetInstance().DestroyAllSessions();")
font_shutdown_index = main_cpp.find("FontManager::Instance().Shutdown();")
if destroy_all_index < 0 or font_shutdown_index < 0:
    fail("BRG-4: explicit PlayerManager/FontManager shutdown markers missing")
elif destroy_all_index > font_shutdown_index:
    fail("BRG-4: Player sessions must be destroyed before FontManager shutdown")

if "m_pendingSessionIds.erase(sessionId);" not in create_session_body:
    fail("BRG-4: pending session reservation cleanup missing")

if "new WindowRuntime();" in window_factory_h:
    fail("BRG-5: WindowRuntime must not be constructed before stable WindowId assignment")
if "runtime->info.id = nextId++" in window_factory_h:
    fail("BRG-5: Window identity must not mutate after lifecycle CREATE/START")
if "const WindowId id = nextId++;" not in window_factory_h or "new WindowRuntime(id)" not in window_factory_h:
    fail("BRG-5: WindowFactory must pass the final WindowId into WindowRuntime construction")

popup_handler_match = re.search(
    r"bool\s+HandlePopupHotkeys\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
    hotkey_handler_cpp,
    re.S,
)
if not popup_handler_match:
    fail("BRG-5: HandlePopupHotkeys body not found")
else:
    popup_handler_body = popup_handler_match.group("body")
    if "e->key.keysym.mod" not in popup_handler_body:
        fail("BRG-5: popup hotkeys must use event-local SDL modifier snapshot")
    if re.search(
        r"SDL_Keymod\s+mod\s*=\s*SDL_GetModState\s*\(",
        popup_handler_body,
    ):
        fail("BRG-5: popup hotkeys must not use timing-dependent global SDL modifier state")

facts["brg4_lifecycle_contract"] = "PASS" if not errors else "FAIL"

# Issue #19: per-window ImGui context/backend serialization contract.
# Fail closed if event handling and rendering stop sharing the same per-window
# mutex, or if the render-side lock no longer spans the complete ImGui frame.
window_runtime_h = (ROOT / "native/src/windows/WindowRuntime.h").read_text(
    encoding="utf-8"
)
window_event_cpp = (ROOT / "native/src/windows/Evevnt.cpp").read_text(
    encoding="utf-8"
)
ui_render_cpp = (ROOT / "native/src/windows/UIRenderThread.cpp").read_text(
    encoding="utf-8"
)

if "mutable std::mutex imguiMutex;" not in window_runtime_h:
    fail("#19: per-window imguiMutex ownership boundary missing")

event_body = extract_function_body(
    window_event_cpp, "void HandleWindowRuntimeEvent(WindowRuntime *runtime, const SDL_Event *e)"
)
render_body = extract_function_body(ui_render_cpp, "void UIRenderThread::Run()")
event_imgui_lock = "std::lock_guard<std::mutex> imguiLock(runtime->imguiMutex)"
render_imgui_lock = "std::lock_guard<std::mutex> imguiLock(currentWindow->imguiMutex)"

for token in (
    "ImGui::SetCurrentContext(imguiCtx)",
    "runtime->resource.graphicsBackend->ProcessEvent(e)",
):
    if not token_is_under_lock(event_body, token, event_imgui_lock):
        fail(f"#19: event-side ImGui mutation must be under per-window imguiMutex: {token}")

for token in (
    "ImGui::SetCurrentContext(currentWindow->resource.imguiCtx)",
    "m_graphicsBackend->BeginFrame(currentWindow->resource.sdlWindow)",
    "currentWindow->renderer->RenderUI(currentWindow, *snapshot)",
    "m_graphicsBackend->EndFrame(currentWindow->resource.sdlWindow)",
    "m_graphicsBackend->SwapWindow(currentWindow->resource.sdlWindow)",
):
    if not token_is_under_lock(render_body, token, render_imgui_lock):
        fail(f"#19: full ImGui frame must remain under per-window imguiMutex: {token}")

facts["issue19_imgui_serialization_contract"] = "PASS" if not errors else "FAIL"

# AUD-19-01: focused event/render stress harness must remain capable of driving
# real window-targeted mouse + keyboard pressure, resize, multi-window close,
# and lifecycle-validated shutdown on the exact local candidate.
issue19_stress_harness = (
    ROOT / "scripts/issue19-event-render-stress.ps1"
).read_text(encoding="utf-8")

for token in (
    "WM_MOUSEMOVE",
    "WM_LBUTTONDOWN",
    "WM_LBUTTONUP",
    "Invoke-EventBurst",
    'cases["secondary_close_during_event_burst"]',
    'cases["main_close_during_event_burst"]',
    'cases["ui_render_thread_started_before_injection"]',
    "phase=START component=UIRenderThread ",
    "POSTMESSAGE_WINDOW_TARGETED_MOUSE_KEYBOARD",
    "brg5_validate_lifecycle.py",
):
    if token not in issue19_stress_harness:
        fail(f"#19: focused event/render stress harness contract missing: {token}")

facts["issue19_event_render_stress_contract"] = "PASS" if not errors else "FAIL"

# BRG-3 MPV render callback lifetime regression contract.
# Keep callback userdata alive through detach/context destruction and ensure
# UI rendering is quiesced before render teardown begins.
playback_render_cpp = (ROOT / "native/src/player/render/PlayBackRender.cpp").read_text(
    encoding="utf-8"
)
window_controller_cpp = (ROOT / "native/src/windows/WindowController.cpp").read_text(
    encoding="utf-8"
)

render_shutdown_body = extract_function_body(
    playback_render_cpp, "void PlayBackRender::Shutdown"
)
render_update_body = extract_function_body(
    playback_render_cpp, "void PlayBackRender::HandleRenderUpdate"
)
window_runtime_destructor = extract_function_body(
    window_controller_cpp, "WindowRuntime::~WindowRuntime"
)

def require_order(body: str, first: str, second: str, message: str) -> None:
    first_index = body.find(first)
    second_index = body.find(second)
    if first_index < 0 or second_index < 0:
        fail(f"BRG-3: missing ordering marker for {message}")
    elif first_index > second_index:
        fail(f"BRG-3: invalid ordering: {message}")

if "RenderCallbackLifetimeGate.h" not in (
    ROOT / "native/src/player/render/PlayBackRender.h"
).read_text(encoding="utf-8"):
    fail("BRG-3: PlayBackRender must own the callback lifetime gate")

if "callbackState->lifetime.Enter()" not in render_update_body:
    fail("BRG-3: render callback must enter lifetime gate before worker access")
require_order(
    render_update_body,
    "callbackState->lifetime.Enter()",
    "callbackState->thread.load",
    "callback gate entry must precede worker snapshot",
)

if "acceptedCallbacks.fetch_add" not in render_update_body:
    fail("BRG-5: real render callback activity counter missing")
if "callback_first" not in render_update_body:
    fail("BRG-5: first real render callback evidence marker missing")
require_order(
    render_update_body,
    "callbackState->lifetime.Enter()",
    "acceptedCallbacks.fetch_add",
    "callback gate entry must precede callback activity count",
)
require_order(
    render_update_body,
    "acceptedCallbacks.fetch_add",
    "callbackState->thread.load",
    "callback activity count must precede worker request",
)

if "m_renderThread.get());" in playback_render_cpp:
    fail("BRG-3: raw render-thread pointer must not be registered as MPV callback userdata")
if "&PlayBackRender::HandleRenderUpdate" not in playback_render_cpp:
    fail("BRG-3: MPV update callback must use the guarded static handler")
if "m_updateCallbackState.get()" not in playback_render_cpp:
    fail("BRG-3: MPV callback userdata must be the stable callback-state object")

if "shutdown_pre_close" not in render_shutdown_body:
    fail("BRG-5: shutdown callback-count evidence marker missing")
if "shutdown_quiescent" not in render_shutdown_body:
    fail("BRG-5: shutdown callback quiescence evidence marker missing")
require_order(
    render_shutdown_body,
    "shutdown_pre_close",
    "m_updateCallbackState->lifetime.Close();",
    "callback-count snapshot must precede callback gate close",
)
require_order(
    render_shutdown_body,
    "m_updateCallbackState->lifetime.Close();",
    "shutdown_quiescent",
    "callback quiescence marker must follow callback gate close",
)

require_order(
    render_shutdown_body,
    "m_updateCallbackState->lifetime.Close();",
    "m_updateCallbackState->thread.store(nullptr",
    "callback admission must close before worker pointer is cleared",
)
require_order(
    render_shutdown_body,
    "m_updateCallbackState->thread.store(nullptr",
    "m_renderThread->Stop();",
    "callback worker pointer must clear before render worker stop/join",
)
require_order(
    render_shutdown_body,
    "m_renderThread->Stop();",
    "mpv_render_context_set_update_callback(m_render_ctx, nullptr, nullptr);",
    "render worker must stop/join before MPV callback detach",
)
require_order(
    render_shutdown_body,
    "mpv_render_context_set_update_callback(m_render_ctx, nullptr, nullptr);",
    "m_renderThread.reset();",
    "MPV callback detach must precede render worker destruction",
)
require_order(
    render_shutdown_body,
    "m_renderThread.reset();",
    "mpv_render_context_free(m_render_ctx);",
    "render worker destruction must precede MPV render-context free",
)
require_order(
    render_shutdown_body,
    "mpv_render_context_free(m_render_ctx);",
    "m_updateCallbackState.reset();",
    "callback state must outlive MPV render-context destruction",
)

require_order(
    window_runtime_destructor,
    "resource.uiRenderThread->Stop();",
    "renderer->Shutdown();",
    "UIRenderThread must stop/join before PlayBackRender shutdown",
)

require_order(
    window_runtime_destructor,
    "ImGui::SetCurrentContext(resource.imguiCtx);",
    "resource.graphicsBackend->Shutdown(true);",
    "window ImGui context must be current before backend shutdown",
)
require_order(
    window_runtime_destructor,
    "resource.graphicsBackend->Shutdown(true);",
    "ImGui::DestroyContext(resource.imguiCtx);",
    "backend shutdown must precede ImGui context destruction",
)

gate_header_text = (
    ROOT / "native/src/player/render/RenderCallbackLifetimeGate.h"
).read_text(encoding="utf-8")

if not (ROOT / "native/src/player/render/RenderCallbackLifetimeGate.h").is_file():
    fail("BRG-3: callback lifetime gate header is missing")
if not (ROOT / "tests/render_callback_lifetime_test.cpp").is_file():
    fail("BRG-3: focused callback lifetime test is missing")
if "brg3_render_callback_lifetime_gate" not in cmake_text:
    fail("BRG-3: focused callback lifetime CTest is not registered")

last_leave = gate_header_text.find("if (previous == 1)")
wait_lock = gate_header_text.find(
    "std::lock_guard<std::mutex> lock(m_waitMutex)", last_leave
)
notify = gate_header_text.find("m_waitCv.notify_all()", last_leave)
if last_leave < 0 or wait_lock < 0 or notify < 0 or wait_lock > notify:
    fail(
        "BRG-3: final callback leave must synchronize m_waitMutex before "
        "quiescence notification"
    )

facts["brg3_callback_lifetime_contract"] = "PASS" if not errors else "FAIL"

# Issue #20 visualizer snapshot concurrency contract.
# Fail closed if the guarded publication boundary or focused stress gate drifts.
audio_processor_h = (ROOT / "native/src/player/audio/AudioProcessor.h").read_text(
    encoding="utf-8"
)
audio_processor_cpp = (ROOT / "native/src/player/audio/AudioProcessor.cpp").read_text(
    encoding="utf-8"
)
visualizer_snapshot_h = (
    ROOT / "native/src/player/audio/AudioVisualizerSnapshot.h"
).read_text(encoding="utf-8")

if "m_visualizerFrames[2]" in audio_processor_h or "m_writeIndex" in audio_processor_h:
    fail("Issue #20: unsafe visualizer double-buffer publication must remain removed")
if "AudioVisualizerFrame m_workingVisualizerFrame;" not in audio_processor_h:
    fail("Issue #20: processor-only working visualizer frame is missing")
if "AudioVisualizerSnapshot m_visualizerSnapshot;" not in audio_processor_h:
    fail("Issue #20: guarded published visualizer snapshot is missing")
if "m_visualizerSnapshot.Read(outFrame);" not in audio_processor_cpp:
    fail("Issue #20: UI visualizer reads must use the guarded snapshot")
if "AudioVisualizerFrame& frame = m_workingVisualizerFrame;" not in audio_processor_cpp:
    fail("Issue #20: DSP must operate on the processor-only working frame")
if audio_processor_cpp.count("m_visualizerSnapshot.Publish(frame);") < 2:
    fail("Issue #20: all AnalyzeBlock exits must publish through the guarded snapshot")
if "std::lock_guard<std::mutex> lock(m_mutex);" not in visualizer_snapshot_h:
    fail("Issue #20: visualizer snapshot must retain the mutex boundary")
if "swap(m_publishedFrame, producerFrame);" not in visualizer_snapshot_h:
    fail("Issue #20: visualizer publication must swap only under the snapshot mutex")
if not (ROOT / "tests/audio_visualizer_snapshot_test.cpp").is_file():
    fail("Issue #20: focused visualizer concurrency stress test is missing")
if "issue20_audio_visualizer_snapshot_gate" not in cmake_text:
    fail("Issue #20: focused visualizer CTest is not registered")

facts["issue20_visualizer_snapshot_contract"] = "PASS" if not errors else "FAIL"

# Issue #26: canonical raw PCM transport/output contract.
audio_capture_h = (ROOT / "native/src/player/audio/AudioCaptureManager.h").read_text(
    encoding="utf-8"
)
audio_capture_cpp = (ROOT / "native/src/player/audio/AudioCaptureManager.cpp").read_text(
    encoding="utf-8"
)
audio_output_h = (ROOT / "native/src/player/audio/AudioOutputWorker.h").read_text(
    encoding="utf-8"
)
audio_output_cpp = (ROOT / "native/src/player/audio/AudioOutputWorker.cpp").read_text(
    encoding="utf-8"
)
audio_types_h = (ROOT / "native/src/player/audio/AudioTypes.h").read_text(
    encoding="utf-8"
)
pcm_accumulator_h = (
    ROOT / "native/src/player/audio/PcmFrameAccumulator.h"
).read_text(encoding="utf-8")

for token in (
    'SetRequiredMpvProperty("ao-pcm-waveheader", "no")',
    'SetRequiredMpvProperty("audio-format", "float")',
    'SetRequiredMpvProperty("audio-samplerate", "48000")',
    'SetRequiredMpvProperty("audio-channels", "stereo")',
    'SetRequiredMpvProperty("ao-pcm-file", m_pipeName.c_str())',
    'SetRequiredMpvProperty("ao", "pcm")',
    "PcmFrameAccumulator accumulator;",
    "accumulator.DrainFrames(",
    "writeSlot->format.sampleRate = kCanonicalAudioSampleRate;",
    "writeSlot->format.channels = kCanonicalAudioChannels;",
):
    if token not in audio_capture_cpp:
        fail(f"Issue #26: canonical PCM capture contract missing: {token}")

for token in (
    "kCanonicalAudioSampleRate = 48000",
    "kCanonicalAudioChannels = 2",
    "kCanonicalAudioBytesPerFrame",
):
    if token not in audio_types_h:
        fail(f"Issue #26: canonical PCM format constant missing: {token}")

for token in (
    "ReadyFrames()",
    "DrainFrames(",
    "PendingBytes()",
):
    if token not in pcm_accumulator_h:
        fail(f"Issue #26: PCM frame accumulator contract missing: {token}")

for token in (
    "blocksDroppedFormatMismatch",
    "queueUnderflowEvents",
    "GetQueuedSizeBytes()",
    "queuedMilliseconds",
):
    if token not in audio_output_cpp and token not in audio_output_h:
        fail(f"Issue #26: output telemetry/format guard missing: {token}")

if "mpv_get_property_string" not in audio_capture_cpp or "Effective MPV audio property" not in audio_capture_cpp:
    fail("Issue #26: effective MPV transport properties must be recorded")

sdl_audio_cpp = (ROOT / "native/src/player/audio/SdlAudioDevice.cpp").read_text(
    encoding="utf-8"
)
for token in (
    "obtainedSpec.freq != desiredSpec.freq",
    "obtainedSpec.format != AUDIO_F32SYS",
    "obtainedSpec.channels != desiredSpec.channels",
    "Obtained format violates canonical contract",
):
    if token not in sdl_audio_cpp:
        fail(f"Issue #26: SDL obtained-format contract missing: {token}")

for token in (
    "[AUDIO_TIMELINE] PIPE_CONNECTED",
    "[AUDIO_TIMELINE] FIRST_PIPE_BYTES",
    "[AUDIO_TIMELINE] FIRST_COMPLETE_PCM_BLOCK",
):
    if token not in audio_capture_cpp:
        fail(f"Issue #26: capture startup timeline marker missing: {token}")

for token in (
    "[AUDIO_TIMELINE] FIRST_PROCESSED_BLOCK",
    "[AUDIO_TIMELINE] FIRST_SDL_WRITE",
    "[AUDIO_TIMELINE] FIRST_NONZERO_SDL_QUEUE",
    "estimatedAudibleHeadPts",
    "estimatedAvOffsetSeconds",
    "SYNC_SAMPLE index=",
    "syncSampleCount",
    "maxAbsAvOffsetSeconds",
):
    if token not in audio_output_cpp and token not in audio_output_h:
        fail(f"Issue #26: output startup/sync telemetry missing: {token}")

if not (ROOT / "native/src/player/audio/AudioTelemetry.h").is_file():
    fail("Issue #26: audio telemetry helper is missing")
if not (ROOT / "tests/pcm_frame_accumulator_test.cpp").is_file():
    fail("Issue #26: focused PCM framing test is missing")
if not (ROOT / "tests/audio_telemetry_math_test.cpp").is_file():
    fail("Issue #26: focused audio telemetry math test is missing")
if not (ROOT / "native/src/player/audio/PcmRealtimePacer.h").is_file():
    fail("Issue #26: realtime PCM pacer is missing")
if not (ROOT / "tests/pcm_realtime_pacer_test.cpp").is_file():
    fail("Issue #26: focused realtime pacer test is missing")
if "issue26_pcm_frame_accumulator_gate" not in cmake_text:
    fail("Issue #26: focused PCM framing CTest is not registered")
if "issue26_audio_telemetry_math_gate" not in cmake_text:
    fail("Issue #26: focused telemetry math CTest is not registered")
if "issue26_pcm_realtime_pacer_gate" not in cmake_text:
    fail("Issue #26: focused realtime pacer CTest is not registered")

audio_telemetry_h = (
    ROOT / "native/src/player/audio/AudioTelemetry.h"
).read_text(encoding="utf-8")
for token in (
    "[AUDIO-TELEMETRY]",
    "IM_PLAYER_LIFECYCLE_LOG",
    "AudioTelemetryEvidenceMutex",
):
    if token not in audio_telemetry_h:
        fail(f"Issue #26: durable audio telemetry evidence sink missing: {token}")

issue26_runtime_harness = (
    ROOT / "scripts/issue26-pcm-runtime-smoke.ps1"
).read_text(encoding="utf-8")
for token in (
    "44100-mono",
    "44100-stereo",
    "48000-mono",
    "48000-stereo",
    "FIRST_NONZERO_SDL_QUEUE",
    "startup timeline order invalid",
    "effective property mismatch",
    "WAVE RIFF header reached raw PCM consumer",
    "published block violates canonical contract",
    "runtime summary reports loss/error",
    "SDL queue high-water exceeded 150ms",
    "short-run A/V offset exceeded 500ms",
    "realtime PCM pacer did not engage",
    "telemetry_error_count",
):
    if token not in issue26_runtime_harness:
        fail(f"Issue #26: runtime normalization harness contract missing: {token}")

if not (ROOT / "scripts/issue26_generate_pcm_fixture.py").is_file():
    fail("Issue #26: normalization fixture generator is missing")

issue26_av_fixture_path = ROOT / "scripts/issue26_generate_av_fixture.py"
if not issue26_av_fixture_path.is_file():
    fail("Issue #26: deterministic long-run A/V fixture generator is missing")
else:
    issue26_av_fixture = issue26_av_fixture_path.read_text(encoding="utf-8")
    for token in (
        'stream.write(b"AVI ")',
        'b"vids"',
        'b"auds"',
        'WAVE_FORMAT_PCM',
        'BI_RGB',
    ):
        if token not in issue26_av_fixture:
            fail(f"Issue #26: A/V fixture contract missing: {token}")

    av_fixture_selftest_path = REPORT_DIR / "issue26-av-fixture-selftest.avi"
    av_fixture_selftest = subprocess.run(
        [
            sys.executable,
            str(issue26_av_fixture_path),
            str(av_fixture_selftest_path),
            "--seconds",
            "5",
        ],
        text=True,
        capture_output=True,
    )
    if av_fixture_selftest.returncode != 0:
        detail = (
            av_fixture_selftest.stderr or av_fixture_selftest.stdout
        ).strip()
        fail("Issue #26: A/V fixture self-test generation failed: " + detail)
    elif not av_fixture_selftest_path.is_file():
        fail("Issue #26: A/V fixture self-test output missing")
    else:
        fixture_prefix = av_fixture_selftest_path.read_bytes()[:65536]
        if fixture_prefix[:4] != b"RIFF" or fixture_prefix[8:12] != b"AVI ":
            fail("Issue #26: A/V fixture self-test RIFF/AVI signature invalid")
        for marker in (b"vids", b"auds", b"movi"):
            if marker not in fixture_prefix:
                fail(
                    "Issue #26: A/V fixture self-test missing marker: "
                    + marker.decode("ascii")
                )
        av_fixture_selftest_path.unlink(missing_ok=True)

issue26_longrun_harness_path = ROOT / "scripts/issue26-av-drift-longrun.ps1"
if not issue26_longrun_harness_path.is_file():
    fail("Issue #26: >=10-minute A/V drift harness is missing")
else:
    issue26_longrun_harness = issue26_longrun_harness_path.read_text(
        encoding="utf-8"
    )
    for token in (
        "PlaybackSeconds = 610",
        "issue26_generate_av_fixture.py",
        "fixture_container = \"AVI\"",
        "measured sync span below 600s",
        "insufficient sync samples",
        "non-sequential sync index",
        "non-monotonic sync timestamp",
        "max absolute A/V offset exceeded 500ms",
        "per-write max absolute A/V offset exceeded 500ms",
        "10-minute A/V drift delta exceeded 250ms",
        "periodic SDL queue exceeded 150ms",
        "capture_dropped",
        "ring_overflows",
        "format_mismatch",
        "underflows",
        "write_failures",
        "ISSUE26-AV-DRIFT-LONGRUN-v1",
    ):
        if token not in issue26_longrun_harness:
            fail(f"Issue #26: long-run drift contract missing: {token}")

audio_cpp = (ROOT / "native/src/player/audio/Audio.cpp").read_text(
    encoding="utf-8"
)
if "[AUDIO-TELEMETRY]" not in (
    ROOT / "native/src/player/audio/AudioTelemetry.h"
).read_text(encoding="utf-8"):
    fail("Issue #26: audio telemetry evidence prefix missing")
if "SUMMARY capture_blocks=" not in audio_cpp:
    fail("Issue #26: end-of-run audio telemetry summary missing")
for token in (
    "sync_samples=%llu",
    "max_abs_av_offset_s=%.6f",
    "snapshot_phase=shutdown_entry",
):
    if token not in audio_cpp:
        fail(f"Issue #26: long-run summary field missing: {token}")
if "sample_rate=%u channels=%u format=float32" not in audio_capture_cpp:
    fail("Issue #26: runtime AudioBlock format evidence missing")
for token in (
    "PcmRealtimePacer realtimePacer;",
    "realtimePacer.BeforePublish",
    "realtimePacer.OnPublished",
    "backpressureWaits",
    "PCM_PACING_ACTIVE",
):
    if token not in audio_capture_cpp and token not in (
        ROOT / "native/src/player/audio/AudioCaptureManager.h"
    ).read_text(encoding="utf-8"):
        fail(f"Issue #26: capture pacing contract missing: {token}")

facts["issue26_pcm_transport_contract"] = "PASS" if not errors else "FAIL"

# BRG-5 lifecycle evidence contract.
# Run the focused coverage checker as part of the canonical static sanity gate
# so lifecycle instrumentation cannot silently drift out of the BRG-5 matrix.
brg5_sanity = subprocess.run(
    [sys.executable, str(ROOT / "scripts/brg5_lifecycle_sanity.py")],
    text=True,
    capture_output=True,
)
if brg5_sanity.returncode != 0:
    detail = (brg5_sanity.stderr or brg5_sanity.stdout).strip()
    fail("BRG-5 lifecycle marker coverage failed: " + detail)
facts["brg5_lifecycle_contract"] = (
    "PASS" if brg5_sanity.returncode == 0 else "FAIL"
)

brg5_validator_selftest = subprocess.run(
    [
        sys.executable,
        str(ROOT / "scripts/brg5_validate_lifecycle.py"),
        "--self-test",
    ],
    text=True,
    capture_output=True,
)
if brg5_validator_selftest.returncode != 0:
    detail = (
        brg5_validator_selftest.stderr or brg5_validator_selftest.stdout
    ).strip()
    fail("BRG-5 lifecycle validator self-test failed: " + detail)
facts["brg5_lifecycle_validator_selftest"] = (
    "PASS" if brg5_validator_selftest.returncode == 0 else "FAIL"
)

render_shutdown_harness = (
    ROOT / "scripts/brg5-render-callback-shutdown.ps1"
).read_text(encoding="utf-8")
if "callback_count did not advance before shutdown" not in render_shutdown_harness:
    fail("BRG-5: AUD-8-02 must fail when callback activity does not advance")
if "PASS_PRE_CLOSE_GT_FIRST" not in render_shutdown_harness:
    fail("BRG-5: AUD-8-02 callback-advancement PASS evidence marker missing")
if "callback_count_first_observed" not in render_shutdown_harness:
    fail("BRG-5: AUD-8-02 must record the first observed callback count")

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
