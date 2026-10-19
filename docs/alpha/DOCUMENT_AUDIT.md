# Documentation reconciliation (PDRM scoped audit)

Scope: source/tree/docs observed on `alpha@20e354758efa9211494f945ace90335549cafc63`; 2026-10-19. This document describes evidence and discrepancies, not a new policy or user authorization.

| Artifact | Classification | Reconciliation |
|---|---|---|
| `CMakeLists.txt`, `CMakePresets.json`, `deps.lock.json` | CURRENT_STATE (executable contract) | Windows/MSVC C++17/CMake 4.2, pinned dependencies; source graph excludes API prototype. |
| `.github/workflows/brg-ci.yml` | CURRENT_STATE | Prior initial alpha has cache step from PR #58, while trigger originally covered only main. Expand alpha triggers with unchanged gates and capture new exact-head CI. |
| `docs/BUILDING.md` | MIXED_UNCLASSIFIED before normalization | Build instructions useful; old bottom "CI NOT_AVAILABLE" evidence status is historical after workflow established. Replace with source-separated current observations. |
| `docs/CI.md` | MIXED_UNCLASSIFIED before normalization | Workflow trigger/cache description lagged integrated alpha. Bring it into alignment without changing negative canary or required check identity. |
| `docs/IM_PLAYER_REFACTOR_MASTER_PLAN.md` | PLANNED_PROPOSED | SessionCoordinator, unified ownership and phases 0–12 describe **target** architecture; do not treat as shipped. |
| `docs/BRG3_RENDER_CALLBACK_LIFETIME.md`, `docs/BRG5_RUNTIME_EVIDENCE.md` | MIXED_UNCLASSIFIED | Implementation/evidence contracts plus earlier checkpoints; test assertions still relevant, dated run evidence is not fresh alpha proof. |
| `docs/IM_PLAYER_*_AUDIT_2026-08-15.md`, `docs/LOCK GRAPH & DEADLOCK AUDIT — Im_player.md` | HISTORICAL_REFERENCE with findings to revalidate | Keep records, re-check each assertion against alpha before assigning root cause. |
| `docs/old/`, `.checkpoint/`, `.plan/` | HISTORICAL_REFERENCE or PLANNED_PROPOSED | Preserve archives; do not silently inherit runtime verdict/approval. |
| `docs/policy/*.md`, `CONTRIBUTING.md` | NORMATIVE_APPROVED process reference on main | Still present; a generic stage assertion cannot override current user-directed non-production alpha integration or issue-specific release gates. |
| `README` | DOC_STALE_CONFIRMED (empty landing page) | Populate with alpha workflow/build/current-vs-target links. |

## Required evidence tiers
1. STATIC: exact Git SHA, file/graph diff, test-contract review.
2. CI: real alpha commit, static sanity, Windows Debug/Release, final BRG gate and logs.
3. PHYSICAL: reproducible media playback, audio gap/seek/resize/shutdown scenarios and first-broken-boundary evidence on Windows.
4. REVIEW: separate human/independent review for critical fixes where required.

STATIC is satisfied for the original six-file consolidation. Integrated-head CI and affected PHYSICAL checks are not yet certified merely by the individual PR's historical green.

## Architecture parity and migration
Legacy Windows/PlayerSession association and the proposed SessionCoordinator coexist only as current code versus a **proposal**, not two confirmed production architectures. Before introducing coordinator/adapters, record window↔player API, ownership, callback affinity, load/seek lifecycle, cleanup, rollback, and the test oracle that prevents regression. Preserve deliberate transitional coupling until parity has proof.

## Prioritization
Reconcile top-level docs/CI first, then runtime tracing and bug boundaries, then Phase 0 ownership matrix. Do not rewrite historic audit files to claim present PASS or delete old content without evidence.
