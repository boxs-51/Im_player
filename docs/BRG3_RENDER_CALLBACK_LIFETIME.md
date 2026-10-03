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
ctest --test-dir .\build\windows-msvc-x64 -C Release -R brg3_render_callback_lifetime_gate --output-on-failure
```

The focused test races callback entry against gate closure for 200 rounds,
verifies that protected lifetime is never entered after quiescence, and repeats
the waiter/last-leaver synchronization path for 2000 rounds.

This focused CTest is deterministic and is required in BRG CI for both Debug and
Release.

## Runtime shutdown stress — canonical local interactive proof

BRG-3 desktop startup/main-window-close evidence is intentionally collected on
an **interactive local Windows desktop**. GitHub-hosted Windows runners execute
as service/non-interactive environments and are not authoritative for this GUI
lifecycle scenario.

After building the exact commit locally:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test-shutdown.ps1 -Configuration Debug -Iterations 20
powershell -ExecutionPolicy Bypass -File .\scripts\test-shutdown.ps1 -Configuration Release -Iterations 20
```

Every iteration must remain alive through startup, accept a normal main-window
close request, exit before timeout and return `ExitCode=0`.

The exact full Git SHA must be recorded with the evidence. Runtime evidence from
a different head is supporting evidence only unless the issue explicitly records
that the intervening drift is NON_MATERIAL and carries the evidence forward.

## CI / runtime evidence boundary

BRG CI remains mandatory and fail-closed for deterministic gates:

```text
Static sanity
Fresh dependency provisioning
Configure
Debug compile/link
Release compile/link
Focused BRG-3 lifetime CTest
BRG CI Gate
```

The CI smoke hook **does not launch the desktop GUI shutdown harness**. It emits
an explicit `NOT_RUN_IN_CI` marker instead. A green CI run therefore proves the
deterministic build/lifetime gates only; it must not be relabeled as BRG-3 L2
interactive runtime proof.

Canonical BRG-3 merge evidence is the combination of:
1. exact-head required CI PASS; and
2. exact-head local interactive Debug + Release shutdown stress PASS.

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
