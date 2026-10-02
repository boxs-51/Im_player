# Continuous Integration — BRG-2 Contract

Canonical Issue: #5  
Policy: IP-POL-001 v1.0.0  
Build Policy: IP-BUILD-001 v1.0.0

## Canonical workflow

`.github/workflows/brg-ci.yml`

The workflow runs on pull requests targeting `main`, pushes to `main`, and
manual dispatch.

The canonical required aggregate context is:

`BRG CI Gate`

It depends on:

- `Static sanity`
- `Windows build (Debug)`
- `Windows build (Release)`

Both Windows jobs run on the pinned GitHub-hosted label
`windows-2025-vs2026` and verify Visual Studio 18 / MSVC 14.5x at runtime.

CMake 4.2.0 is installed explicitly. The workflow does not silently fall back
to the runner image's preinstalled CMake.

## Exact commit rule

For pull requests, checkout is pinned to `github.event.pull_request.head.sha`,
not GitHub's synthetic merge ref. Each job compares the checked-out commit to
that expected SHA and fails on mismatch.

For pushes/manual runs, the expected commit is `github.sha`.

## Dependency rule

The workflow executes `scripts/bootstrap-deps.ps1` from the checked-out tree.

Initial BRG-2 intentionally disables vcpkg binary caching with:

`VCPKG_BINARY_SOURCES=clear`

This makes the first canonical proof independent of an opaque dependency
binary cache. A future cache may be introduced only with an explicit Issue and
evidence that cache keys preserve dependency correctness.

## Failure evidence

Each Windows job records:

- exact Git SHA;
- runner/image context;
- Visual Studio installation;
- MSVC toolset version;
- CMake version;
- dependency bootstrap log;
- configure log;
- configuration-specific build log;
- smoke-hook log;
- generated `deps/toolchain.actual.json` when available.

Failure evidence is uploaded for debugging. Successful build artifacts are also
uploaded with the exact SHA in the artifact name.

## Smoke hook

`scripts/ci-smoke-hook.ps1` is the BRG-5 extension point.

BRG-2 currently verifies that the built executable and pinned libmpv runtime
exist. If CTest is registered in the build tree, the hook automatically runs
CTest for the current configuration.

BRG-5 (#8) may extend this script with startup/shutdown and functional smoke
without changing the stable `BRG CI Gate` context.

## Negative canary

BRG-2 validation may temporarily add:

`.github/ci/force-build-failure`

When this marker exists, the Windows jobs deliberately request a nonexistent
CMake target. The expected result is a red workflow and red `BRG CI Gate`.
The marker must be deleted before the final green candidate is reviewed.

## Branch protection

After the workflow is proven deterministic, `main` should require PR-based
integration and the `BRG CI Gate` context.

If the connected automation identity cannot mutate repository rules/branch
protection, that limitation must be recorded on Issue #5 and the repository
owner must apply the guardrail in GitHub settings.
