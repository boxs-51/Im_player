# Linear development queue for non-production alpha

Source: existing open issues in `boxs-51/Im_player` as read 2026-10-19; coordination [#61](https://github.com/boxs-51/Im_player/issues/61). Use **one ordered integration queue**, while independently scoped research/test branches may be prepared in parallel. Do not collapse independent problems into one source PR.

| Stage | Canonical issue(s) | Deliverable | PASS gate before proceeding |
|---|---|---|---|
| S00 | [#61](https://github.com/boxs-51/Im_player/issues/61), [#57](https://github.com/boxs-51/Im_player/issues/57) | Alpha consolidation, architecture/doc current snapshot, CI alpha, cache validation | exact integrated-alpha SHA + static sanity + Debug + Release + BRG Gate; docs reviewed; missing physical evidence explicit |
| S01 | [#42](https://github.com/boxs-51/Im_player/issues/42) | Reproduce original in-play audio gap, identify first broken PCM boundary (MPV → pipe → raw → processor → processed → SDL) | media-repro matrix and correlated per-block PTS/sequence/generation + device/pipe/queue logs; no causal repair before proof |
| S02 | [#30](https://github.com/boxs-51/Im_player/issues/30) | Unified LOAD/SEEK/TRACK/AUDIO_RECONFIG generation and reset model, only after S01 interface evidence | RED-first reset/stale-block tests, exact generation behavior, Windows integration/physical seek+reconfig proof |
| S03 | [#44](https://github.com/boxs-51/Im_player/issues/44) | Correct seek freeze / external output drain | reproduce + prove video/audio transaction and bounded recovery without silently dropping user-visible audio |
| S04 | [#42](https://github.com/boxs-51/Im_player/issues/42) | Fix actual in-play PCM continuity cause isolated in S01 | RED→GREEN targeted regression, physical repeat of original media, Debug/Release, no false boundary attribution |
| S05 | [#28](https://github.com/boxs-51/Im_player/issues/28) | Canonical audio metrics: units, valid/invalid semantics, safety input | per-metric reference fixtures + negative invalid/NaN tests, control safety review |
| S06 | [#32](https://github.com/boxs-51/Im_player/issues/32) | Filter command result, desired/applied state and failure reconciliation | injected MPV failure tests prove no false applied ACK; exact audio control regression |
| S07 | [#29](https://github.com/boxs-51/Im_player/issues/29) | Adaptive filter convergence/quality acceptance and rollback | stable controller tests, subjective quality evaluation plan + observable acceptance; no uncontrolled retuning |
| S08 | [#25](https://github.com/boxs-51/Im_player/issues/25) | Render scheduling, FBO resource ownership and pressure | CPU/GPU/VRAM baseline parity, render lifetime stress, no leak/glitches |
| S09 | [#43](https://github.com/boxs-51/Im_player/issues/43) | Resize epoch correctness and no black/flicker frames | rapid resize/fullscreen physical E2E + callback/render epoch oracle |
| S10 | [#37](https://github.com/boxs-51/Im_player/issues/37) | Single-instance forwarded argv must load exact requested media | second-process E2E / ack / rapid-start/close tests |
| S11 | [#9](https://github.com/boxs-51/Im_player/issues/9), [#3](https://github.com/boxs-51/Im_player/issues/3), [#24](https://github.com/boxs-51/Im_player/issues/24) | BRG-6 reconciled architecture/evidence and Phase 0 acceptance, then next approved master plan phase | ownership/thread/lifetime/dependency matrix, Debug/Release + required physical shutdown/render/audio tests; explicit phase gate |

## Every stage's template
1. Pin `alpha@HEAD` and scope to one canonical issue. Do not inherit stale main-era claims.
2. Define reproduction/acceptance, risks and rollback; add targeted RED-first regression for bugfixes before source repair. An unrun RED is UNVERIFIED.
3. Branch `fix/<issue>-<short-name>` from exact alpha, not main. Avoid unrelated refactors; retain existing test/security assertions.
4. Run static checks and supported Windows Debug/Release/CTest/physical matrix appropriate to the change. Linux is a static gate, not a Windows runtime substitute.
5. Review exact diff, retain raw CI/runtime evidence, and integrate to alpha in order only when stage gates pass. No merge/deploy to main.
6. Update canonical issue with `PASS|REVISE|BLOCKED|UNVERIFIED`. Only close fixed issue after corresponding runtime proof; parent #24 and Phase 0 issues remain open until all relevant children are reconciled.

## Dependency/rollback notes
- S02 follows S01's first-boundary characterization; S03/S04 use the frozen S02 discontinuity semantics. If S01 shows a different root cause, update the exact issue ordering **before** changing implementation contracts.
- S05→S06→S07 prevents adaptive feedback from relying on invalid measurements or false MPV actuation acknowledgements.
- S08→S09 checks resource/epoch ownership before resizing concurrency.
- S10 is technically separable but held in a single integration queue to make outcomes reviewable end to end.
- The 12-phase master plan is a proposal, not automatically authorized to implement after S11. Separate goals and acceptance are mandatory.
