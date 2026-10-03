# BRG-3 MPV Render Callback Lifetime Validation

Canonical Issue: #6  
Policy: IP-POL-001 v1.1.0  
Coordination Policy: IP-COORD-001 v1.0.0  
Build Policy: IP-BUILD-001 v1.0.0

## Canonical development checkpoint

```text
DEVELOPMENT_BASE = dad9b5cf362d9dddf0125609def2f022dd467acc
PARENT_HEAD      = NONE
AUTHORITY_SCOPE  = MPV render callback lifetime + shutdown ordering only
DRIFT_CLASS      = MATERIAL from #7 lifecycle landing, now refreshed
WAVE_ID          = not-assigned
INTEGRATION_STATUS = NOT_ENROLLED
```

#7 / BRG-4 is integrated on the development baseline and provides controlled
PlayerManager/WindowManager teardown. The governance landing from #16/#17 is
NON_MATERIAL to BRG-3 runtime semantics.

## Lifetime invariant

The MPV update callback must never dereference a destroyed
`PlayBackRenderThread`.

Canonical shutdown ordering:

```text
close callback admission gate
wait for callbacks admitted before close
clear callback -> worker pointer
stop + join render worker
detach MPV update callback
wait defensively for callback quiescence
destroy render worker
free mpv_render_context
final callback quiescence
destroy callback state
```

The callback-state object intentionally outlives both render-worker destruction
and MPV render-context destruction.

The render worker is joined before callback detachment because libmpv requires
`mpv_render_*` calls for a render context not to run concurrently. This avoids
racing `mpv_render_context_set_update_callback()` with
`mpv_render_context_render()`.

## Window teardown ordering

`WindowRuntime::~WindowRuntime()` first stops/joins `UIRenderThread`, then
shuts down the MPV render path while the graphics backend/context remains alive.
This removes the previously observed race where UI rendering could still reach
render/session state during teardown.

## Focused concurrency regression

```powershell
ctest --test-dir .\build\windows-msvc-x64 -C Debug -R brg3_render_callback_lifetime_gate --output-on-failure
```

The focused test races callback entry against gate closure for 200 rounds and
verifies that protected lifetime is never entered after quiescence.

## Runtime shutdown stress

After building the exact commit:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test-shutdown.ps1 -Configuration Debug -Iterations 20
powershell -ExecutionPolicy Bypass -File .\scripts\test-shutdown.ps1 -Configuration Release -Iterations 20
```

Every iteration must remain alive through startup, accept a normal main-window
close request, exit before timeout and return `ExitCode=0`.

## Lifecycle evidence markers

A successful render shutdown emits the ordered markers:

```text
[RenderShutdown] callback gate closed
[RenderShutdown] pre-stop callback quiescence established
[RenderShutdown] render worker stopped and joined
[RenderShutdown] mpv update callback detached
[RenderShutdown] post-detach callback quiescence established
[RenderShutdown] render worker destroyed
[RenderShutdown] mpv render context freed
[RenderShutdown] callback state destroyed
```

Historical signal/phase/thread-role diagnostics were localization-only evidence
and are intentionally removed from the clean candidate.
