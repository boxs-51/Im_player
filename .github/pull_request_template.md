# Pull Request Contract

**Canonical Issue:** Closes #

**Policy:** IP-POL-001  
**Build Policy:** IP-BUILD-001

> Required references:
> - https://github.com/boxs-51/Im_player/blob/main/docs/policy/PROJECT_EXECUTION_POLICY.md
> - https://github.com/boxs-51/Im_player/blob/main/docs/policy/BUILD_POLICY.md

## Baseline / Lineage

- Base branch:
- Base SHA:
- Head SHA:
- Depends on Issue/PR:

## Scope

What this PR changes:

## Non-goals

What this PR intentionally does not change:

## Key files / subsystems

-

## Risk

- Lifetime/concurrency:
- Build/dependency:
- Runtime/regression:
- Rollback:

## Validation evidence

```text
tested_commit=<full SHA>
configuration=<Debug|Release|both>
toolchain=<exact>
command_or_run=<exact command / CI run>
result=<PASS|FAIL>
artifact_or_log=<reference>
```

### IP-BUILD-001 gates

- [ ] L0 Configure is PASS or N/A with reason.
- [ ] L1 Debug is PASS.
- [ ] L1 Release is PASS.
- [ ] L2 Startup/Shutdown is PASS when runnable.
- [ ] Required L3 functional scenarios are PASS.
- [ ] Required L4 concurrency/lifetime stress is PASS for thread/callback/lifetime changes.
- [ ] Required L5 specialized validation is PASS when applicable.
- [ ] Runtime dependency closure is verified when build/runtime dependencies changed.

## Governance gates

- [ ] Canonical Issue contract is still current; no unapproved scope drift.
- [ ] I re-read the canonical Issue and dependency/auditor updates before requesting review.
- [ ] P0 blockers = 0.
- [ ] P1 findings are resolved or have an explicit accepted deferral.
- [ ] Required CI is PASS on the exact head SHA when CI is available.
- [ ] Dependency/stack order is valid.
- [ ] Rollback path is documented.
- [ ] This PR does not smuggle unrelated cleanup/refactor into the issue.

## Reviewer / Auditor notes

List unresolved questions or explicit accepted deviations here.
