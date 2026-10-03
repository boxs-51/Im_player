# BRG-5 Runtime Evidence Contract

Canonical Issue: #8  
Policy: IP-POL-001 v1.1.0  
Coordination Policy: IP-COORD-001 v1.0.0  
Build Policy: IP-BUILD-001 v1.0.0

## Development checkpoint

```text
DEVELOPMENT_BASE = 3cf30a91467adf03def1f48b08c454f83e179c1d
BRANCH           = issue/8-runtime-smoke-stress-matrix
AUTHORITY        = BRG-5 runtime smoke/stress + lifecycle evidence only
PHASE0_CHANGES   = FORBIDDEN
```

BRG-1 through BRG-4 are completed prerequisites. BRG-5 records executable
baseline behavior; it does not redesign ownership, threading, render scheduling,
audio architecture, or window/session coordination.

## Lifecycle marker format

BRG5-A standardizes lifecycle evidence as:

```text
[Lifecycle] phase=<PHASE> component=<COMPONENT> id=<IDENTITY> pid=<PID> tid=<TID>
```

Canonical phases:

```text
CREATE
START
STOP
JOIN
DESTROY
```

The marker records the thread that performed the lifecycle transition. The
component identity is stable for the object's lifetime and should use the
existing session/window/thread name when available.

Required components and applicable phases:

| Component | Required phases |
| --- | --- |
| Window | CREATE, START, STOP, DESTROY |
| PlayerSession | CREATE, START, STOP, DESTROY |
| MPVRenderContext | CREATE, START, STOP, DESTROY |
| UIRenderThread | CREATE, START, STOP, JOIN, DESTROY |
| PlayBackRenderThread | CREATE, START, STOP, JOIN, DESTROY |
| AudioCaptureManager | CREATE, START, STOP, JOIN, DESTROY |
| AudioProcessor | CREATE, START, STOP, JOIN, DESTROY |
| AudioOutputWorker | CREATE, START, STOP, JOIN, DESTROY |

`JOIN` is required only for components that own a worker thread. It must be
emitted after the corresponding `std::thread::join()` returns.

## Evidence sink

Markers always go to `OutputDebugStringA`.

For executable BRG-5 evidence, the harness sets:

```powershell
$env:IM_PLAYER_LIFECYCLE_LOG = "<absolute-path-to-log>"
```

When the variable is present, markers are also appended to that file. If the
variable is absent, no lifecycle evidence file I/O occurs.

This preserves normal runtime behavior while allowing deterministic evidence
capture during BRG-5 runs.

## Evidence rules

Every BRG-5 run must record:

```text
commit=<full SHA>
configuration=<Debug|Release>
scenario=<scenario name>
command=<exact command>
result=<PASS|FAIL>
lifecycle_log=<path>
```

Failures must include reproduction details and be classified as either:
- BRG blocker;
- pre-existing baseline debt;
- Phase 0/1+ debt outside BRG authority.

## Runtime lifecycle validation

Each local lifecycle capture must also pass:

```powershell
python .\scripts\brg5_validate_lifecycle.py .\artifacts\brg5\lifecycle-debug.log
python .\scripts\brg5_validate_lifecycle.py .\artifacts\brg5\lifecycle-release.log
```

The validator fails when:
- a required phase is missing;
- phase order is invalid;
- a component changes identity between CREATE/START/STOP/JOIN/DESTROY;
- a smoke log contains lifecycle markers from more than one process.

## BRG5-A gate

BRG5-A is complete when:
1. all required components emit standardized lifecycle markers at their
   existing lifecycle boundaries;
2. each object keeps one stable identity across all applicable phases;
3. worker JOIN is emitted only after the actual thread join returns;
4. Debug and Release lifecycle logs pass the runtime validator;
5. no ownership/control-flow redesign is introduced;
6. static sanity verifies marker coverage.
