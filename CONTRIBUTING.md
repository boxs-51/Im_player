# Contributing to Im_player

All repository work is governed by:

1. [IP-POL-001 — Canonical Issue Execution & Integration Policy](docs/policy/PROJECT_EXECUTION_POLICY.md)
2. [IP-COORD-001 — Multi-Agent Coordination & Integration Wave Policy](docs/policy/MULTI_AGENT_COORDINATION_POLICY.md)
3. [IP-BUILD-001 — Canonical Build, Dependency & Validation Policy](docs/policy/BUILD_POLICY.md)

## Required workflow

```text
Issue Template
    ↓
DRAFT → TRIAGED → READY
    ↓
dedicated branch
    ↓
implementation + evidence
    ↓
Pull Request Template
    ↓
review / audit / CI
    ↓
drift classification + optional Integration Wave
    ↓
MERGE_READY / Wave READY
    ↓
merge
    ↓
post-merge main evidence
    ↓
DONE
```

Do not implement non-trivial repository changes without a canonical Issue.

Do not commit feature/bug/refactor work directly to `main`.

All PASS/FIXED/DONE claims require exact-commit evidence.

Build and dependency claims must follow IP-BUILD-001.

If an exception is required, use the explicit `POLICY_EXCEPTION` format in IP-POL-001 §16.


## Multi-agent coordination

When multiple Issues/PRs are active:
- each technical scope keeps one canonical Issue;
- agents may inspect related work and post structured IP-COORD-001 notices;
- classify main/dependency drift as MATERIAL or NON_MATERIAL before deciding to re-anchor;
- use an Integration Wave when coordinated ordering reduces merge/revalidation churn;
- Wave READY does not authorize merge;
- wave-enrolled PRs integrate only through that manifest;
- expected-head SHA guards are mandatory at merge.
