# IP-POL-001 — Canonical Issue Execution & Integration Policy

> **Policy ID:** IP-POL-001  
> **Version:** 1.0.0  
> **Status:** CANONICAL after merge to `main`  
> **Scope:** All repository Issues, implementation work, audits, Pull Requests, integration and completion claims.  
> **Build policy:** [IP-BUILD-001](./BUILD_POLICY.md)

## 1. Purpose

Policy này là authority chính cho cách một công việc được tạo, nhận ownership, triển khai, audit, review, merge và đóng trong `boxs-51/Im_player`.

Mục tiêu:

- không sửa code trước khi contract của Issue rõ ràng;
- mọi thay đổi có traceability từ Issue → branch → PR → commit → evidence;
- không tuyên bố PASS/DONE nếu chưa có bằng chứng trên exact commit;
- dependency và cross-issue impact phải được kiểm tra trước mọi state transition quan trọng;
- tránh trộn scope, refactor ngoài yêu cầu và merge code chưa có gate;
- giữ `main` là integration branch có thể kiểm chứng.

## 2. Authority hierarchy

Khi có xung đột, thứ tự authority là:

```text
IP-POL-001
    ↓
IP-BUILD-001 (cho build/test/dependency)
    ↓
Canonical Issue contract + explicit owner decisions
    ↓
Current architecture/master-plan documents
    ↓
Audit/checkpoint/reference documents
    ↓
Implementation comments / historical notes
```

Policy điều khiển **process**. Canonical Issue điều khiển **scope kỹ thuật cụ thể**. Tài liệu cũ không tự động override Issue hiện hành.

Nếu cần vi phạm policy, phải dùng Exception Process ở §16.

## 3. Mandatory Issue contract

Mọi Issue mới phải dùng Issue Template chuẩn và có tối thiểu:

1. **Policy reference:** `IP-POL-001` và version áp dụng.
2. **Type:** BUG / FEATURE / REFACTOR / AUDIT / BUILD / CI / DOCS / GOVERNANCE.
3. **Priority/Severity:** P0 / P1 / P2 / P3.
4. **Canonical baseline:** branch + exact SHA đang được phân tích.
5. **Owner:** người chịu trách nhiệm triển khai/chốt trạng thái.
6. **Problem statement:** sự cố hoặc mục tiêu cụ thể.
7. **Evidence:** code path, log, reproduction, audit hoặc dữ liệu hỗ trợ.
8. **Scope / Non-goals.**
9. **Dependencies:** inbound + outbound dependencies.
10. **Acceptance criteria.**
11. **Validation plan:** build/test/runtime/audit cần chạy.
12. **Risk + rollback.**
13. **Integration plan:** branch/PR/dependency order.
14. **Completion evidence:** exact SHA + commands/runs/results.

Issue thiếu các trường bắt buộc không được chuyển sang READY.

## 4. Issue identity and canonical workspace

Mỗi công việc phải có **một Issue canonical**.

- Discussion triển khai, state transitions và evidence cuối cùng phải quay về Issue canonical.
- Issue khác có thể hỗ trợ/audit nhưng không được âm thầm thay đổi contract của Issue canonical.
- Nếu scope được chia nhỏ, parent Issue phải liên kết child Issues và dependency graph.
- Không được có hai Issue cùng tự nhận authority cho cùng một deliverable mà không xác định parent/canonical rõ ràng.

## 5. Roles

### Owner / Implementer

Chịu trách nhiệm:

- đọc lại Issue canonical trước khi thay đổi state quan trọng;
- theo dõi comment/audit/dependency update;
- triển khai đúng scope;
- duy trì branch/PR;
- cung cấp build/test/runtime evidence;
- không tự tuyên bố DONE khi gate chưa đủ.

### Auditor

Độc lập với implementer ở góc nhìn kiểm chứng:

- rà dependency hai chiều;
- kiểm tra contract drift;
- kiểm tra lifetime/concurrency/build/CI risk;
- phân loại finding với evidence;
- không âm thầm sửa scope của owner;
- P0/P1 finding phải được phản hồi trên Issue canonical trước merge.

### Reviewer / Integrator

Xác nhận:

- scope đúng;
- policy/build policy được tuân thủ;
- CI/evidence đúng exact head;
- dependency order hợp lệ;
- PR đủ điều kiện merge.

Một người có thể giữ nhiều role, nhưng evidence và gates không được bỏ qua vì role trùng nhau.

## 6. Severity / priority

### P0 — BLOCKER

Crash/UAF/data corruption/deadlock chắc chắn hoặc có đường dẫn mạnh; security-critical; build/release blocker; loss of ownership/lifetime correctness.

- Chặn merge của work phụ thuộc.
- Phải có dedicated Issue hoặc được scope rõ trong canonical Issue.
- Không được defer im lặng.

### P1 — HIGH

Race/lifecycle correctness, regression lớn, API/contract violation hoặc reliability risk cao.

- Phải được resolve hoặc explicitly deferred với owner + target phase trước merge.

### P2 — NORMAL

Bug/technical debt có workaround hoặc impact giới hạn.

### P3 — LOW

Cleanup, readability, optimization không blocking.

Severity phải dựa trên evidence, không dùng như nhãn cảm tính.

## 7. Issue lifecycle

Canonical states:

```text
DRAFT
  ↓
TRIAGED
  ↓
READY
  ↓
IN_PROGRESS
  ↓
READY_FOR_REVIEW
  ↓
MERGE_READY
  ↓
DONE
```

State phụ:

```text
BLOCKED
SUPERSEDED
CANCELLED
```

### DRAFT → TRIAGED

Cần:
- problem statement;
- initial evidence;
- type/severity;
- candidate scope.

### TRIAGED → READY

Cần:
- exact baseline SHA;
- owner;
- scope + non-goals;
- dependencies;
- acceptance criteria;
- validation plan;
- risk/rollback.

### READY → IN_PROGRESS

Cần:
- dedicated branch;
- base SHA recorded;
- dependency blockers clear;
- Issue được đọc lại ngay trước khi bắt đầu.

### IN_PROGRESS → READY_FOR_REVIEW

Cần:
- implementation hoàn tất theo scope;
- local/build validation theo IP-BUILD-001;
- self-review;
- known deviations ghi rõ.

### READY_FOR_REVIEW → MERGE_READY

Cần:
- PR review/audit findings xử lý;
- required CI PASS;
- exact PR head evidence;
- no unresolved blocking dependency;
- P0 = 0; P1 phải resolve hoặc có explicit accepted deferral.

### MERGE_READY → DONE

Cần:
- PR merged;
- post-merge exact `main` SHA ghi vào Issue;
- post-merge required checks PASS;
- completion evidence + residual debt links;
- acceptance criteria checked.

Không được dùng "code xong" đồng nghĩa "DONE".

## 8. Branch policy

Không triển khai feature/bug/refactor trực tiếp trên `main`.

Tên branch chuẩn:

```text
issue/<number>-<short-slug>
bug/<number>-<short-slug>
audit/<number>-<short-slug>
governance/<short-slug>
```

Mỗi branch phải ghi nhận base SHA ban đầu trên Issue/PR.

Nếu work phụ thuộc branch khác, dependency phải explicit. Không rebase/cherry-pick tùy ý làm mất lineage.

## 9. Pull Request policy

Mỗi PR phải:

- tham chiếu canonical Issue;
- tham chiếu `IP-POL-001` và `IP-BUILD-001`;
- ghi base SHA và head SHA;
- mô tả scope + non-goals;
- liệt kê files/subsystems quan trọng;
- liệt kê risk;
- ghi commands/tests + kết quả;
- khai báo dependency PR/Issue;
- có rollback note.

Ưu tiên PR nhỏ, atomic và reviewable.

Không merge khi:
- required CI đỏ/chưa chạy;
- exact head chưa được test;
- scope drift chưa được canonical Issue chấp thuận;
- P0 chưa giải quyết;
- merge làm vi phạm dependency order.

## 10. Stacked development

Stacked development được phép khi cần dependency chain, nhưng bắt buộc:

- mỗi layer có Issue/PR riêng;
- parent/base relationship explicit;
- không merge child trước dependency bắt buộc;
- sau upstream merge, downstream phải re-anchor/revalidate nếu code path liên quan thay đổi;
- evidence cũ không tự động carry-over qua head mới.

## 11. Dependency and cross-issue rule

Trước các transition READY, READY_FOR_REVIEW và MERGE_READY, owner phải rà:

- Issue canonical comments;
- linked parent/child Issues;
- active PR dependencies;
- auditor findings;
- current `main` drift;
- build/dependency policy changes.

Nếu dependency mới làm contract không còn đúng, Issue chuyển BLOCKED hoặc contract phải được update trước khi tiếp tục.

## 12. Evidence contract

Mọi claim `PASS`, `FIXED`, `MERGE_READY`, `DONE` phải có:

```text
tested_commit=<full SHA>
environment=<toolchain/runtime relevant>
command_or_run=<exact command / CI run>
result=<PASS/FAIL>
artifact_or_log=<reference when available>
```

Historical checkpoint chỉ là reference, không phải proof cho head mới.

Static audit phải được ghi là **STATIC**. Runtime behavior chưa chạy thì ghi **NEEDS-RUNTIME-PROOF**, không suy diễn thành PASS.

## 13. Change control / scope freeze

Sau READY, scope được coi là frozen.

Scope thay đổi materially phải:
1. update canonical Issue;
2. ghi lý do;
3. update dependencies/acceptance/test plan;
4. re-evaluate severity;
5. nếu quá lớn, tách child Issue.

Không dùng một bug fix PR để lén kèm architecture redesign hoặc cleanup không liên quan.

## 14. Merge and integration gate

Merge chỉ được phép khi:

- canonical Issue ở MERGE_READY;
- IP-BUILD-001 gate tương ứng PASS;
- PR head là exact tested head;
- dependencies đã merge hoặc gate cho phép rõ ràng;
- review/audit blockers = 0;
- rollback path đủ rõ;
- branch không drift khỏi base theo cách làm invalid evidence.

Sau merge phải xác nhận `main@<sha>` và required checks.

## 15. Documentation and supersession

Tài liệu audit/checkpoint/plan cũ không bị xóa chỉ vì đã lỗi thời.

Khi bị thay thế:
- đánh dấu SUPERSEDED nếu phù hợp;
- link tới authority mới;
- giữ historical evidence trừ khi chứa nội dung sai/nguy hiểm cần remove.

Canonical policy ID không đổi khi update version. Breaking governance change phải tăng major version và có governance Issue/PR.

## 16. Exception process

Chỉ owner/repository maintainer được approve exception.

Exception phải ghi trên canonical Issue/PR:

```text
POLICY_EXCEPTION
policy=IP-POL-001
rule=<section>
reason=<why>
risk=<impact>
mitigation=<controls>
approved_by=<owner>
expires=<issue/merge/date condition>
```

Exception không được implicit.

P0 safety/lifetime correctness và requirement về exact evidence không được waive chỉ để merge nhanh.

## 17. Build authority

Mọi build/dependency/test claim phải tuân thủ [IP-BUILD-001](./BUILD_POLICY.md).

Nếu Issue thay đổi CMake, vcpkg, compiler, dependency, runtime DLL hoặc CI:
- phải đánh dấu Type BUILD/CI hoặc dependency tương ứng;
- phải update build evidence;
- nếu thay đổi canonical build contract thì update IP-BUILD-001 trong cùng governance/build Issue.

## 18. Repository bootstrap / current gate

Trong giai đoạn Baseline Recovery:
- #3 là BRG canonical workspace;
- #4–#9 là child gate work;
- governance này không tự tuyên bố BRG PASS;
- BRG vẫn phải hoàn thành exact build/runtime evidence theo contract riêng.

## 19. Mandatory references

Mọi Issue/PR mới sau khi policy có hiệu lực phải chứa:

```text
Policy: IP-POL-001
Build Policy: IP-BUILD-001
```

Thiếu reference là contract incomplete.
