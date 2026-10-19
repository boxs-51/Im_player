# Observed architecture of Im_player at alpha integration baseline

Snapshot: `alpha@20e354758efa9211494f945ace90335549cafc63`. Evidence level: GitHub source and existing CI metadata; no fresh locally executed Windows video/audio session during this audit.

## Supported build graph
- `CMakeLists.txt` builds a Windows-only C++17/MSVC executable `Im_player` with CMake >=4.2.
- Canonical presets/toolchain/dependencies: `CMakePresets.json`, `deps.lock.json`, `vcpkg.json`, `cmake/ImPlayerDependencies.cmake`, `docs/BUILDING.md`.
- The executable is wired to SDL2, ImGui, OpenGL/libmpv, C++ application components; `native/src/api/api_manager.cpp` is explicitly `EXCLUDED-PROTOTYPE` in CMake and `deps.lock.json`. The `native/src/backends/vid/` Python tree is present but not part of the canonical CMake executable target.
- Ubuntu CI runs static sanity; the supported application build and relevant runtime checks are Windows-only. Ubuntu static PASS is not proof the Windows application works.

## Source-observed component/data flow
```text
native/src/main1.cpp
  -> WindowInitializer / WindowController / WindowManager / WindowRuntime
  -> PlayerManager -> PlayerSession::Init(WindowRuntime*)
       -> Player / mpv event+command classes
       -> PlayBackRender -> PlayBackRenderThread -> mpv_render_context
                           -> OpenGL FBO pool -> SDL/ImGui UI
       -> AudioCaptureManager (named pipe from mpv)
                           -> SPSC raw PCM ring
       -> AudioProcessor (process loop + decoupled analysis loop)
                           -> SPSC processed PCM ring
       -> AudioOutputWorker -> IAudioOutputDevice / SDL output
```
The diagram summarizes observed source wiring, not a formal proof of exclusive ownership or absence of concurrency races.

## Actual coupling / ownership to preserve or investigate
- `PlayerManager.cpp` stores `unique_ptr<PlayerSession>` in its session registry. `PlayerSession::Init(WindowRuntime*)` binds a window runtime to Player/renderer/audio. A separate `SessionCoordinator` is drawn in the refactor master plan but no matching implementation path was observed in the inspected source tree. **Plan != implemented.**
- `PlayBackRender.cpp` creates `mpv_render_context`, registers a callback through `RenderUpdateCallbackState`, and creates `PlayBackRenderThread`. `PlayBackRenderThread.cpp` starts/joins its thread and calls `mpv_render_context_render`; lifetime gates are implemented but #25/#43 remain open for resource and resize correctness.
- `AudioCaptureManager.cpp` reads MPV AO bytes through a named pipe and publishes blocks with sequence/generation/PTS; the currently observed generation advance is seek-edge driven. This does not establish a unified LOAD/TRACK/AUDIO_RECONFIG discontinuity model (#30).
- `AudioProcessor.cpp` has `ProcessLoop` and `AnalysisLoop` with a separate analysis queue, while `AudioOutputWorker.cpp` manages an output device, queue/underflow and recovery. A device-offline path still drops processed blocks (diagnostic emission added in #54); #42's first causal boundary is not proven.
- Audio filter control has `native/src/player/audio/filter/af_m_*.cpp`; unit contract, desired-vs-applied state and convergence need separate verification (#28, #32, #29).

## Existing automated oracles
Eight C++ tests under `tests/`: audio media clock, queue underflow, queue write planner, telemetry math, visualizer snapshot, PCM accumulator, realtime pacer, and render callback lifetime. `scripts/ci_sanity.py` is a separate static gate. Prior #54/#58 Windows CI jobs succeeded on the **separate PR HEADs**, not this combined commit.

## Known open gaps (not claimed newly reproduced)
- Audio timeline dropout #42, unified discontinuities #30, seek recovery #44.
- Adaptive measurement/control #28/#32/#29.
- Render resource pressure and resize epoch #25/#43.
- Single-instance argv forwarding #37.
- BRG phase handoff #9 and parent audit #24.
Root causes and severity inherited from issue reports require exact-alpha reproduction before new fixes.
