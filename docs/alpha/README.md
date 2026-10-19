# Alpha engineering documentation — 2026-10-19

**Scope:** non-production `alpha`. This is an integration and engineering work branch, NOT a release authorization or claim that existing P1 bugs are fixed.

## Canonical entry points
- [Current observed architecture](ARCHITECTURE_CURRENT.md): source-backed snapshot, **not** future architecture.
- [Document audit and authority classification](DOCUMENT_AUDIT.md): approved/current/proposed/historical and drift evidence.
- [Integration ledger](INTEGRATION_LEDGER.md): immutable 3-parent merge snapshot and excluded evidence-only PR.
- [Linear issue execution plan](ISSUE_SEQUENCE.md): ordered stages and acceptance/stop gates.
- [S00 coordination issue](https://github.com/boxs-51/Im_player/issues/61).
- [Build instructions](../BUILDING.md), [CI](../CI.md), [master refactor proposal](../IM_PLAYER_REFACTOR_MASTER_PLAN.md).

## Revisions
Origin: main `e9b72924facd816c671c52c567c9e513a24077a4`.
Initial integrated alpha commit: `20e354758efa9211494f945ace90335549cafc63` (parents: main, #54, #58).

Subsequent doc and CI commits advance alpha; always pin the **new** exact alpha HEAD for actual test evidence. Individual PR-head green cannot substitute for post-integration alpha green.

## Gate invariant
No main merge or deployment, no self-certified independent review, no root-cause claims without runtime correlation, no weakening regression/security/test assertions. Preserve old audits and checkpoint links as provenance, not as current runtime results.
