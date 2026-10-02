# IP-COORD-001 — Multi-Agent Coordination & Integration Wave Policy

> **Policy ID:** IP-COORD-001  
> **Version:** 1.0.0  
> **Parent authority:** [IP-POL-001](./PROJECT_EXECUTION_POLICY.md)  
> **Status:** CANONICAL after merge to `main`  
> **Reference source:** concepts adapted for Im_player from `boxs-51/assistant` Issue #85 v2.5; that external Issue is not authority for this repository.

## 1. Purpose

Policy này định nghĩa cách nhiều agent, Issue và PR phối hợp song song mà vẫn giữ:
- one canonical workspace per technical scope;
- explicit ownership/authority;
- exact-head evidence;
- deterministic dependency order;
- low re-anchor churn;
- a separate boundary between autonomous planning and merge authority.

IP-COORD-001 chỉ quản lý **coordination/integration planning**. Nó không cấp runtime, lifecycle, build, destructive hoặc architecture authority ngoài canonical Issue.

## 2. Core authority rule

Mỗi technical deliverable có đúng một canonical Issue.

Cross-issue discussion, auditor notice, coordinator plan, Wave enrollment hoặc main movement **không chuyển ownership bằng implication**.

Nếu Issue A phát hiện dependency lên Issue B:
- A có thể đọc/notify/challenge;
- B vẫn quyết định scope kỹ thuật của B;
- unresolved authority conflict => affected path = HOLD;
- scope transfer chỉ hợp lệ khi canonical Issues ghi explicit handoff.

## 3. Multi-agent roles

### Owner / Implementer
- owns implementation and state transitions inside canonical scope;
- re-reads canonical Issue before major transitions;
- publishes exact evidence;
- reports material cross-track changes.

### Independent Auditor
- read-only correctness/dependency/stage-gate role by default;
- may inspect related active Issues/PRs;
- may post findings/notices to affected canonical workspaces;
- may block with evidence-based P0/P1;
- does not silently implement fixes or broaden authority.

### Reviewer / Integrator
- validates scope, exact-head evidence, review state, mergeability and dependency order;
- executes merge only with valid authority.

### Wave Coordinator
- owns only the integration-plan record;
- discovers candidates, freezes manifest/order, classifies drift, maintains HOLD/READY state;
- may create/update a Wave issue autonomously before authorization;
- cannot change member technical scope or treat READY as merge permission.

## 4. Structured cross-issue notices

Dùng các prefix sau để agent khác parse nhanh:

```text
[MERGE-CANDIDATE]  exact PR/HEAD is approaching or reached readiness
[DEPENDENCY]       inbound/outbound dependency or required order
[NO-IMPACT]        inspected cross-track change is NON_MATERIAL
[DRIFT]            assumption/evidence changed
[HOLD]             candidate/path cannot advance
[UNBLOCK]          recorded blocker is closed on exact evidence
[ORDER-PROPOSAL]   proposed merge sequence + rationale
[WAVE-ENROLLMENT]  candidate assigned to a Wave
[WAVE-EXCLUSION]   candidate intentionally deferred
[HANDOFF]          explicit authority/responsibility transfer
```

Notices phải chứa facts/material impact, không spam status lặp lại.

## 5. Stable development baseline

Mỗi production/runtime/high-risk candidate ghi:

```text
DEVELOPMENT_BASE = <exact sha>
PARENT_HEAD      = <exact parent head | NONE>
AUTHORITY_SCOPE  = <frozen scope summary>
DRIFT_CLASS      = <NON_MATERIAL | MATERIAL | UNKNOWN>
```

Candidate có thể tiếp tục development/audit trên baseline ổn định cho tới khi có MATERIAL drift.

## 6. Drift classification

### NON_MATERIAL

Không bắt buộc re-anchor chỉ vì `main` tiến lên, nếu thay đổi:
- path-disjoint và không thay đổi shared contract;
- docs/governance/evidence-only không đổi production semantics của candidate;
- không đổi CMake/dependency/toolchain assumptions của candidate;
- không đổi ownership/lifetime/concurrency/Window/Player/MPV/audio/render contract mà candidate phụ thuộc;
- không đổi parent semantics;
- không gây merge conflict hoặc invalidate tests.

### MATERIAL

Bắt buộc refresh/re-anchor hoặc fresh affected evidence khi có:
- changed-path overlap có semantic impact;
- shared API/contract/ownership change;
- lifecycle/threading/locking/callback/render/audio semantics change trên dependency path;
- CMake/toolchain/dependency/runtime-DLL change ảnh hưởng candidate;
- parent head semantic change;
- merge conflict;
- new P0/P1;
- evidence no longer reproducible on candidate assumptions.

Nếu chưa đủ evidence => `DRIFT_CLASS=UNKNOWN` và integration path = HOLD cho tới khi classify.

## 7. Operating modes

```text
SINGLE_LANE
PARALLEL_LANES
```

SINGLE_LANE dùng khi chỉ một candidate/dependency chain đang được integrate.

PARALLEL_LANES dùng khi nhiều Issue/PR độc lập hoặc partially dependent cùng phát triển. Mỗi lane giữ canonical owner và stable baseline riêng; integration được đồng bộ bằng Wave.

## 8. Autonomous coordination authority

Trước merge authorization, owner/auditor/coordinator được phép tự động:
1. inspect active Issues/PRs;
2. resolve exact base/head/main/CI/review/thread state;
3. classify dependency, merge class and drift;
4. post structured notices;
5. negotiate order;
6. create/revise a Wave coordinator Issue;
7. add/remove/defer/reorder pre-authorization members;
8. freeze exact candidate HEADs when gates pass;
9. move plan through DISCOVERY/NEGOTIATING/PLANNED/READY/HOLD;
10. maintain a next-wave queue.

Không cần user command cho các coordination steps này.

## 9. Merge planning state machine

```text
DISCOVERY
  -> NEGOTIATING
  -> PLANNED
  -> READY
  -> AUTHORIZED
  -> MERGING
  -> COMPLETE

Any pre-COMPLETE state -> HOLD when a required gate fails.
```

READY chỉ có nghĩa manifest/gates đã frozen/green. READY **không phải merge authority**.

## 10. Integration Wave manifest

Mỗi Wave phải ghi tối thiểu:

```text
WAVE_ID
MODE                    = SINGLE_LANE | PARALLEL_LANES
ENTRY_MAIN              = exact canonical main
MEMBERS                 = ordered PR list
ISSUE_OWNER             = canonical Issue per member
CANDIDATE_HEAD          = exact audited HEAD per member
MERGE_CLASS             = governance/docs | build/ci | production | high-risk | mixed
DEPENDENCIES            = hard parent/order constraints
DRIFT_CLASS             = current disposition
BLOCKERS                = P0/P1/CI/review/authority
EXCLUSIONS / NEXT_WAVE
AUTHORIZATION_MODE
AUTHORIZATION_STATE     = OPEN | READY | AUTHORIZED | MERGING | COMPLETE | HOLD
```

Một PR đã enroll không được merge standalone cho tới khi bị explicit WAVE-EXCLUSION trước authorization hoặc Wave hoàn tất/cancel.

## 11. Wave readiness

Wave = READY chỉ khi mọi member:
- scope/authority frozen;
- exact candidate HEAD recorded;
- required independent audit disposition complete;
- required CI/evidence GREEN on exact head;
- unresolved blocking P0/P1 = 0;
- review threads clean as required;
- dependency/order recorded;
- drift class is current;
- mechanically mergeable or has documented integration step to become mergeable.

Blocked independent member nên được defer/exclude thay vì freeze các READY member không phụ thuộc.

## 12. Merge authorization boundary

Autonomous planning != autonomous production merge.

Wave chứa production/mixed/high-risk member yêu cầu explicit authorization:

```text
Xác nhận MERGE WAVE <WAVE_ID>
```

Equivalent wording chỉ hợp lệ khi nêu exact Wave ID và frozen manifest.

Authorization chỉ áp dụng cho exact manifest/order/heads đã READY. Semantic head change, scope expansion, dependency-significant reorder, MATERIAL drift hoặc new P0/P1 invalidates affected member authorization.

Governance/docs-only work ngoài Wave tiếp tục theo normal IP-POL-001 merge gate. Khi đã enroll, Wave trở thành integration path.

## 13. In-wave sequencing

Dependent/high-risk:

```text
merge predecessor with expected-head guard
-> verify canonical main / required health
-> classify drift for next member
-> re-anchor only if MATERIAL
-> merge next member
```

Independent path-disjoint members:
- previous member landing may be NON_MATERIAL;
- no automatic re-anchor requirement;
- still recheck mergeability + exact-head CI/audit + cross-drift facts.

Only one Wave should be AUTHORIZED/MERGING at a time unless an explicit IP-POL-001 exception exists.

## 14. Expected-head and evidence rules

Immediately before merge:
- verify PR HEAD equals frozen candidate HEAD or documented equivalent re-anchor;
- verify required CI/audit/reviews remain green;
- verify dependency predecessors landed as expected;
- verify no new blocker/material drift;
- merge with expected-head SHA guard.

CI rerun without semantic head change does not invalidate manifest. Semantic code change always requires fresh affected evidence.

## 15. Handoff protocol

Explicit handoff format:

```text
[HANDOFF]
FROM_ISSUE=<#>
TO_ISSUE=<#>
AUTHORITY=<exact scope>
EFFECTIVE_BASE=<sha>
DEPENDENCIES=<refs>
OPEN_RISKS=<refs|NONE>
ACCEPTANCE=<what receiving owner acknowledges>
```

Không có handoff record => ownership unchanged.

## 16. Noise control

Agents should notify other workspaces only when:
- dependency/order changed;
- drift classification changed;
- blocker opened/closed;
- candidate became READY/HOLD;
- Wave enrollment/exclusion changed;
- authority handoff is needed.

Routine CI progress without decision impact stays in canonical Issue/PR.

## 17. Interaction with IP-BUILD-001

Build/test claims remain governed by IP-BUILD-001.

Coordination may classify a build-policy change as MATERIAL but cannot waive L0–L5, exact toolchain, dependency or CI requirements.

## 18. Active BRG relationship

During Baseline Recovery:
- #3 remains BRG parent authority;
- #4–#9 remain technical child authorities;
- #6/#7 may coordinate dependency/order via this policy;
- #8 remains blocked until required predecessors satisfy their contracts;
- Wave planning cannot declare BRG PASS;
- #9 remains the only BRG handoff authority defined by #3.

## 19. Reference adoption note

Concepts in this policy were informed by the working multi-agent governance in `boxs-51/assistant` Issue #85 v2.5:
- stable baseline;
- MATERIAL/NON_MATERIAL drift;
- cross-issue coordination mesh;
- autonomous pre-authorization planning;
- Integration Waves.

This repository intentionally keeps its own policy IDs, issue lifecycle, build gates and BRG authority.

## 20. Backward-compatible adoption

IP-COORD-001 v1.0.0 là additive coordination governance.

- active Issues/PRs opened before this policy is canonical are grandfathered for initial template/reference fields;
- missing historical coordination fields alone does not invalidate CLAIM/READY/CI/runtime evidence;
- policy-doc landing with zero shared technical semantic impact should normally classify NON_MATERIAL for active production candidates;
- active work adopts the coordination checkpoint at the next relevant state transition, refresh, dependency negotiation or Wave enrollment;
- no existing technical authority is widened by adoption;
- no candidate is forced to re-anchor solely to rewrite metadata;
- future new Issues/PRs must use the current templates and references.

This compatibility rule is why IP-POL-001 moves from v1.0.0 to v1.1.0 rather than a breaking major-version reset: technical authority, P0/P1 gates, exact-evidence requirements and merge safety are preserved; the change adds coordination capabilities and metadata.
