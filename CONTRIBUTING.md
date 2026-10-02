# Contributing to Im_player

All repository work is governed by:

1. [IP-POL-001 — Canonical Issue Execution & Integration Policy](docs/policy/PROJECT_EXECUTION_POLICY.md)
2. [IP-BUILD-001 — Canonical Build, Dependency & Validation Policy](docs/policy/BUILD_POLICY.md)

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
MERGE_READY
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
