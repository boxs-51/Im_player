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
configuration=<Debug|Release|both|N/A>
toolchain=<exact|N/A>
command_or_run=<exact command / CI run / static validation>
result=<PASS|FAIL|N/A>
artifact_or_log=<reference>
```

### IP-BUILD-001 gates

For each gate, mark **PASS** or **N/A with a reason**. N/A is valid only when the PR cannot affect that layer.

- [ ] L0 Configure: PASS / N/A — reason:
- [ ] L1 Debug: PASS / N/A — reason:
- [ ] L1 Release: PASS / N/A — reason:
- [ ] L2 Startup/Shutdown: PASS / N/A — reason:
- [ ] Required L3 functional scenarios: PASS / N/A — reason:
- [ ] Required L4 concurrency/lifetime stress: PASS / N/A — reason:
- [ ] Required L5 specialized validation: PASS / N/A — reason:
- [ ] Runtime dependency closure: PASS / N/A — reason:

## Governance gates

- [ ] Canonical Issue contract is still current; no unapproved scope drift.
- [ ] I re-read the canonical Issue and dependency/auditor updates before requesting review.
- [ ] P0 blockers = 0.
- [ ] P1 findings are resolved or have an explicit accepted deferral.
- [ ] Required CI is PASS on the exact head SHA when CI is available, or CI is explicitly NOT_AVAILABLE.
- [ ] Dependency/stack order is valid.
- [ ] Rollback path is documented.
- [ ] This PR does not smuggle unrelated cleanup/refactor into the issue.

## Reviewer / Auditor notes

List unresolved questions or explicit accepted deviations here.
