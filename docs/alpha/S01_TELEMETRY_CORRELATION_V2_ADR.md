# Issue #42 — Telemetry Correlation v2: Thiết kế trước khi kiểm chứng audio vật lý

**Trạng thái:** PROPOSED / DESIGN ONLY — không phải quyền sửa C++ hoặc merge.  
**Repository:** `boxs-51/Im_player`; tham chiếu PR #62 **DRAFT** HEAD `6cddc8cd47c15dee776eea99bd7c15e0d677f224`; base `alpha@f9e0168dc46194f7dde6112ca6ebba02a5b52c9b`.  
**Goal ID:** `I42-S01-CORRELATION-V2`; **contract:** `C0-DESIGN`.  
**MUST NOT:** không chỉnh runtime, giảm oracle, khẳng định âm thanh đã phát, dùng Computer Bridge, chạy host physical, merge/deploy, hoặc mở S02.

## 1. Bằng chứng hiện tại (đọc ở exact HEAD trên)

| Nguồn | Quan sát | Khoảng trống |
|---|---|---|
| `native/src/player/session/PlayerManager.cpp` | `PlayerSession` có string ID tùy chọn/generator; phiên đã hủy có thể được tạo lại với cùng tên | String ID không đại diện một incarnation duy nhất và không tự gắn với PCM |
| `native/src/player/session/PlayerSession.cpp` | Khởi tạo `PlayerStateSystem`, `PlaybackCommand`, audio; ghi `START/STOP` cho `PlayerSession` | Lifecycle entry hiện có `component,phase,id,pid,tid`, thiếu `load_id`/`t_us`/clock domain |
| `native/src/player/command/PlaybackCommand.cpp` (khoảng dòng 65–90) | `startupLoadId` tăng ở new load, **không** tăng khi early retry | `load_id` là local state, có thể trùng giữa session; retry cần ID lần thử riêng |
| `native/src/player/audio/AudioCaptureManager.cpp` (khoảng dòng 543–620) | `m_activeGeneration++` lúc seek; gán `sequence,generation,startupLoadId,capture*Micros` lên `AudioBlock` | Generation thuộc audio pipeline; cần token cho cả session và load. Không tự thay hợp đồng generation của issue #30 |
| `native/src/player/audio/AudioTypes.h` (khoảng dòng 66–84) | `AudioBlock` có trường sequence, generation, startupLoadId và timestamp | Chưa có run/session incarnation key |
| `native/src/player/audio/AudioTelemetry.h` | `AudioTelemetryNowMicros()` dùng C++ `steady_clock`; audio và lifecycle dùng mutex riêng | Log append order không phản ánh chính xác happens-before; steady epoch không dùng so sánh giữa process |
| `native/src/player/audio/AudioOutputWorker.cpp` | Có hai đường underflow (`idle_observer`, `segment`); raw ERROR luôn được phát, bản structured có điều kiện | Raw không có `load_id`/`t_us`; last-successful-block identity có thể cũ khi shutdown |
| `native/src/player/audio/AudioCaptureManager.cpp`; `AudioOutputWorker.cpp` | `PIPE_DISCONNECTED`, `OUTPUT_DEVICE_OFFLINE_DROP` không có `load_id` theo marker native hiện tại | Ghép chúng với underflow của một load chỉ bằng thời gian là sai |
| `scripts/issue42_first_boundary.py` | S01 v1 tách STOP epochs trong thứ tự log, giữ marker unscoped/raw là INCONCLUSIVE | Chưa có bằng chứng định danh có authority và fence shutdown; không thể chứng minh causality |

Các đường dẫn/line anchors nằm trong commit đã ghim; đây là quan sát mã, không phải bằng chứng physical. Log có thể chứa URL/path tư nhân; chỉ publish fixtures đã khử dữ liệu nhạy cảm.

## 2. Kiến trúc được đề xuất

```text
PlayerManager (tạo incarnation UUID ngẫu nhiên trên mỗi session mới)
     |  run_uid / session_uid + label (label không phải khóa)
PlayerSession correlation owner --[active lease / STOP fence]--> lifecycle events
     |
PlaybackCommand (local load_id + retry_attempt, new-media boundary)
     |
AudioCaptureManager (stamp correlation token trên mỗi AudioBlock sau khi chọn load/generation)
     |  [run_uid, session_uid, load_id, generation, block_seq]
     v
SPSC raw ring -> AudioProcessor (copy token nguyên vẹn) -> processed ring
     |                                                    |
     +---- stage events                                    v
                                                AudioOutputWorker / SDL underflow
                                                   | block-bound token nếu có
                                                   | nếu không: UNBOUND, không đoán
                                                   v
                                 per-thread bounded telemetry queues (không I/O hot path)
                                                   |
                                 loss-aware async sink -> JSONL v2 evidence
                                                   |
                           S01 v2 analyzer: strict key + clock + lifecycle fence
                                                   |
                                    OBSERVED / INCONCLUSIVE + cause UNKNOWN
```

### Khóa và trách nhiệm sở hữu

Định danh chuẩn một block là **`(run_uid, session_uid, load_id, generation, block_sequence)`**. `run_uid` là token ngẫu nhiên 128-bit tạo một lần trên tiến trình/instance; `session_uid` là incarnation ngẫu nhiên 128-bit tạo mỗi lần một `PlayerSession` thực sự được khởi tạo, **không sử dụng** string label hay pointer làm khóa. Chỉ dùng ngẫu nhiên để tránh trùng, không phải credential hay chữ ký bảo mật. `load_id` là counter của `PlaybackCommand` trong session; `retry_attempt` tăng cho retry *trong cùng load*, **không** tạo load mới. `generation` được lấy nguyên từ nguồn `AudioBlock`/quy tắc #30, tuyệt đối không gộp generation của hai session/load. `block_sequence` là sequence từ producer, không yêu cầu bắt đầu lại tại seek/load (theo mã hiện tại sequence chạy trong capture lifetime).

Trên các event không thuộc block (ví dụ pipe disconnect, thiết bị offline chưa có block): producer **phải** lấy snapshot typed context từ owner ở thời điểm xảy ra sự kiện hoặc để `correlation_status="UNBOUND"`. Việc có một `load_id` trong bộ nhớ `PlayerStateSystem` không đủ để chứng minh pipe event thuộc chính load đó, nếu transport chưa có acquisition/ownership fence. Event có `correlation_status="BOUND"` chỉ khi gắn qua token có authority (owner tạo và worker nhận theo đường truyền có xác nhận). Legacy/unscoped không được nâng lên BOUND chỉ bằng thời gian.

### Timestamp và trình tự

Sử dụng `event_mono_us` là `steady_clock` tại thời điểm quan sát sự kiện; `clock_domain = "process_steady_v1"`, `run_uid` phân vùng đồng hồ. `capture_publish_us`, `capture_read_us` cùng miền thời gian; `pts`/`mpv_time_pos` là **media clock** riêng và không được trừ trực tiếp từ `event_mono_us`. `SDL_GetTicks64` trong diagnostic legacy là một nguồn khác, không được đồng nhất. Ghi thêm `emitter_id` (ổn định cho incarnation của component), `emitter_seq` (tăng đơn điệu trong emitter) và `event_id` (duy nhất trong run). Không suy ra happens-before giữa hai thread chỉ từ thứ tự append, timestamp bằng nhau, hoặc event counter. Kết luận causal boundary cần explicit block lineage, fence hoặc chứng cứ stage, không chỉ timestamp gần nhau.

### Lifecycle fence / shutdown

Một context bất biến chứa correlation key cùng `lifecycle_epoch`; owner giữ trạng thái atomic `ACTIVE -> STOPPING -> STOPPED`. Event trong hot path chỉ đánh dấu `playback_phase="ACTIVE"` nếu có **active lease/fence snapshot** đã được owner xác nhận tại observation point. Sau khi STOPPING bắt đầu, worker không được tạo sự kiện mang trạng thái `ACTIVE` mới, ngay cả khi snapshot flags mpv còn 0. In-flight event đã bắt đầu trước STOP được lưu với lease/epoch riêng và cần xác minh token, không tin thứ tự ghi file. Khi không xác định được thời điểm so với STOP, trả `INCONCLUSIVE` (không đoán teardown/mid-playback).

**Yêu cầu tối thiểu implementation sau duyệt:** dữ liệu token dạng POD/trivially-copyable trong `AudioBlock`, không cấp phát hoặc I/O đồng bộ trên audio hot path; writer thread xử lý JSONL. Dùng bounded ring + `lost_event_count`/sequence gaps; mất telemetry cần được ghi rõ và không được trở thành false PASS. Không phát URL, access token, local file path, raw PCM trong telemetry v2. Không thay đổi logic audio, sampling/pacing, buffering hoặc generation thuộc issue #30.

## 3. Schema v2 (dự thảo, JSON Lines)

Một dòng cho một sự kiện, được sinh từ snapshot token của producer, không bằng cách grep chuỗi log khác:

```json
{"schema":"IM_PLAYER_AUDIO_V2","event_id":"r1:e1:1","run_uid":"r1","session_uid":"session-incarnation-a","session_label":"window-A","load_id":1,"retry_attempt":0,"generation":3,"block_sequence":101,"event_type":"PROCESSOR_ACQUIRE_RAW","component":"AudioProcessor","emitter_id":"e1","emitter_seq":1,"event_mono_us":3000010,"clock_domain":"process_steady_v1","lifecycle_epoch":1,"playback_phase":"ACTIVE","correlation_status":"BOUND","capture_publish_us":3000000}
```

- Cấu trúc bắt buộc: `schema,event_id,run_uid,session_uid,load_id,retry_attempt,generation,event_type,component,emitter_id,emitter_seq,event_mono_us,clock_domain,lifecycle_epoch,playback_phase,correlation_status`; `block_sequence` cho block-bound stage; `capture_publish_us` chỉ ở stage phù hợp.
- Event **không có khóa nguồn** phải dùng `correlation_status="UNBOUND"`, để `session_uid/load_id/generation` là null nếu chưa biết; không được giả `0` là khóa hợp lệ.
- STOP/START dùng `event_type="SESSION_STOP"` hoặc `"SESSION_START"`, có `session_uid`, `lifecycle_epoch`, `event_mono_us` và `fence_seq`; STOP có `playback_phase="STOPPING"`.
- Underflow có `event_type="SDL_QUEUE_UNDERFLOW"`, `source="idle_observer|segment"`, trạng thái `paused/core_idle/idle_active/seeking/pause_for_cache` cùng `playback_phase`; nếu không có block token thì `UNBOUND`.
- Kết quả bộ phân tích đề xuất: `schema="ISSUE42-S01-OBSERVATION-v2"`, `classification="...|INCONCLUSIVE|NO_UNDERFLOW_OBSERVED"`, `binding_status="VERIFIED|UNVERIFIED"`, `selected_event_id`, `reason_codes`, `root_cause="UNKNOWN"`, `audible_output_verified=false`, counts raw/structured/lost.

### Thuật toán verifier fail-closed

1. Validate schema/version/types/clock/domain/unique event IDs; duplicate conflicting payload ⇒ `INCONCLUSIVE` và `CONFLICTING_EVENT_ID`.
2. Xây registry `run_uid -> session_uid -> load_id -> generation`; không liên kết theo `session_label`, pointer, PID, `load_id` đơn độc.
3. Xác minh transition: session lifetime, load-created/closed, retry_attempt, generation change; nguồn/trạng thái STOP fence; event mất key/khác clock ⇒ unverified.
4. Với mỗi underflow theo *observation event time*, ưu tiên candidate có BOUND identity, active lease, đầy đủ state; không dùng appender order để chứng nhận trước/sau STOP. Dùng block lineage và capture publish của **cùng composite key** trong cùng clock domain và window thời gian. No duplicate event counting (raw+rich pairing cần explicit common event ID).
5. Unbound, lost telemetry, late/stale source, cross-session, cross-load, cross-generation, mixed-process clock, unknown playback flags ⇒ `INCONCLUSIVE` hoặc giữ marker *quan sát* mà không gán nguyên nhân.
6. Dù matched, chỉ trả phân loại **observed boundary**; `root_cause=UNKNOWN`, `audible_output_verified=false` cho đến khi physical E2E có bằng chứng độc lập.

## 4. Các phương án (ADR-42-TEL-01 — PROPOSED)

| Phương án | Đánh đổi | Nhận định |
|---|---|---|
| A. Tiếp tục join v1 bằng `timestamp + load_id` | Rẻ nhưng cross-session, lifecycle races, unscoped continuity và clock collision không loại trừ được | Không đủ cho yêu cầu |
| B. Thêm token vào log string, không truyền qua block | Dễ bổ sung ở ít emitter nhưng vẫn lấy current state không đồng bộ, gán nhầm các block cũ sau seek | Không chọn |
| **C. POD correlation token truyền cùng AudioBlock + typed event/sink** | Cần thay đổi source nhiều vị trí, test thread/race, memory budget rõ | **Đề xuất** |
| D. Gắn distributed tracing toàn hệ thống | Khối lượng lớn, chi phí trên real-time path, không cần thiết cho S01 | Hoãn |

## 5. RED-first test matrix (T1 tests-only, không sửa runtime)

| Test | Đầu vào đối kháng | Oracle bắt buộc |
|---|---|---|
| R01 | Hai `session_uid` cùng `load_id/generation`, interleave markers | Không trộn capture của session khác |
| R02 | Hủy rồi tạo lại cùng `session_label` | Incarnation mới không dùng evidence cũ |
| R03 | `load_id` giống nhau ở hai session | Khóa composite, không join theo load_id |
| R04 | Seek đổi generation; underflow dùng gen trước | Không ghép qua generation |
| R05 | Retry trong cùng load, `retry_attempt++` | Vẫn cùng load, phân biệt attempt |
| R06 | STOP rồi underflow có `paused=0` | STOPPING override flags; `INCONCLUSIVE` |
| R07 | Event trước STOP được flush sau STOP | Không tin thứ tự dòng file; cần lease/fence |
| R08 | `process_steady_v1` run khác hoặc `SDL_GetTicks64` | Không trừ/so sánh cross-clock |
| R09 | Raw underflow thiếu token nhưng rich underflow đầy đủ | Raw giữ observation, không đếm đôi; không gán |
| R10 | Pipe/disconnected và offline/drop không có load token | Báo UNBOUND, không nâng classification |
| R11 | Trùng event_id với payload khác | CONFLICTING_EVENT_ID / fail closed |
| R12 | Đứt event_seq/buffer tràn | LOST_TELEMETRY, không PASS giả |
| R13 | Hai event cùng timestamp nhưng khác emitter | Không coi append order là causality |
| R14 | Legacy v1 | Giữ nguyên analyzer v1, không xóa oracles S01 |
| R15 | Quan sát valid same-key capture gap + full active lease | BOUND observed gap, **vẫn** cause UNKNOWN/audible false |
| R16 | Cùng session/load nhưng generation 0 và 1 bị lặp block_seq | Không collision, dùng full composite key |

Các test trong `test_issue42_telemetry_v2_redfirst.py` đính kèm gọi **API công khai hiện tại** `analyze(raw)` để có thể thêm vào repo khi được phép và kỳ vọng bắt đầu RED theo *đúng behavior*, không phải do thiếu module. Dùng `python -m unittest discover -s tests -p 'test_issue42_*.py' -v` trên GitHub Ubuntu/Windows; ngoài ra cần source-level kiểm tra propagation token qua actual `AudioBlock`. Không sửa assertion/skip test khi đỏ.

## 6. Các gate và kế hoạch chuyển tiếp

- **D0 — DESIGN:** đối chiếu chính xác owner/ABI/clock/identity, chốt schema & R01–R16, review compatibility legacy S01. Chưa dùng RED của fixture như bằng chứng production.
- **T1 — RED FIRST:** tạo nhánh tests-only kế thừa exact HEAD PR #62, thêm tests/fixtures và CI path trigger, chạy GitHub Linux+Windows. Chỉ nhận **semantic RED** của target invariants, giữ 44 legacy controls PASS, không sửa test khi sửa.
- **T2 — IMPLEMENT (chỉ khi được phê duyệt riêng):** thêm correlation token/typed event C++ ở đúng producers, wiring capture→processor→output; thêm lifecycle fence và async sink loss accounting; parser v2 dual-read v1. Không đổi audio path timing/PTS oracles.
- **T3 — GREEN:** cùng frozen tests, native Windows CTest, semantic canary, Debug/Release build, property/fault-injection/race/stress, không có false PASS, exact SHA 6/6 CI hoặc phù hợp workflow mới; self-review trước independent review theo quyết định chủ sở hữu.
- **T4 — PHYSICAL (chưa cấp quyền):** Windows real playback Debug+Release, affected ColdUrl + local known-good + deterministic fixture; underflow capture đủ run/session/load/gen/clock/STOP fence/sequence; kiểm tra first broken boundary. Nếu root cause chưa có evidence vẫn UNKNOWN, giữ S02 HOLD.

**Rollback:** v2 để sau feature flag đo đạc, bảo toàn v1; tắt v2 emitter không ảnh hưởng PCM/data plane. Bản báo cáo v2 có thể được đảo mà không thay đổi playback; không xóa legacy oracles. Không merge/deploy khi thiếu owner authorization và physical gate.

## 7. Acceptance và ranh giới

- **Design artifact PASS** khi schema/ownership/fail-closed/compatibility/matrix/ADR đã được review tính nhất quán.
- **T1 runtime RED NOT_RUN** trong báo cáo này; không tạo PR/CI hay chạm source từ một yêu cầu design-only. Bộ test đã được syntax-check tại môi trường tài liệu, nhưng chưa phải kết quả GitHub runner.
- **S01 physical BLOCKED / issue #42 UNKNOWN**, không gọi `NO_UNDERFLOW_OBSERVED` là bằng chứng không có dropout, không xác nhận nghe được tiếng vì GUI hosted CI không kiểm chứng playback.

## 8. REQ/provenance và acceptance cho bản thiết kế

| Requirement | Contract (MUST) | Artifact chứng minh DESIGN | Proof sau triển khai |
|---|---|---|---|
| REQ-001 | `(run_uid,session_uid,load_id,generation)` là khóa tương quan; không dùng label/pointer | §2–3, R01–R04/R16 | Linux/Windows native-token tests |
| REQ-002 | Giữ retry cùng load nhưng phân biệt attempt | §2–3, R05 | Unit + integration owner transition |
| REQ-003 | Mỗi event có process clock-domain, original event-time và emitter-order; không so sánh mpv/SDL ticks trực tiếp | §2–3, R08/R13 | Clock-boundary tests |
| REQ-004 | Sau STOP không được chứng nhận ACTIVE bằng cờ playback stale | §2–3, R06/R07 | Concurrent stop/drain tests trên Windows |
| REQ-005 | Raw/unbound/lost/duplicate conflicts => fail-closed | §3, R09–R12 | Fault injection + negative canary |
| REQ-006 | Không sửa generation #30, không làm yếu v1 #42 | §2, §5 R14, §6 rollback | CTest, current 44 S01 tests unchanged |
| REQ-007 | Không coi telemetry là bằng chứng đã phát âm thanh | §3, §6, R15 | Physical E2E tách riêng, hiện BLOCKED |

**Nguồn tham chiếu exact HEAD:**
- [PlayerManager.cpp](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/session/PlayerManager.cpp#L14-L32)
- [PlaybackCommand.cpp](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/command/PlaybackCommand.cpp#L65-L90)
- [AudioTypes.h](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/audio/AudioTypes.h#L62-L86)
- [AudioCaptureManager.cpp](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/audio/AudioCaptureManager.cpp#L535-L620)
- [AudioOutputWorker.cpp](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/audio/AudioOutputWorker.cpp#L376-L412)
- [LifecycleEvidence.h](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/common/LifecycleEvidence.h#L12-L70)
- [AudioTelemetry.h](https://github.com/boxs-51/Im_player/blob/6cddc8cd47c15dee776eea99bd7c15e0d677f224/native/src/player/audio/AudioTelemetry.h#L15-L20)

**Reviewer status:** SELF-DESIGN-CHECK, không có independent review. **Architectural ADR status:** PROPOSED (not approved). **Gate:** `DESIGN_DELIVERED`; `IMPLEMENTATION_NOT_STARTED`; `RUNNER_RED_NOT_RUN`; `PHYSICAL_BLOCKED`.
## T2 CLI admission fence — 2026-10-21

At the frozen test-only T2 HEAD `52fe9eae3b295f0a4eead472d6b1623b44c84559`, [S01 RED #39606265614](https://github.com/boxs-51/Im_player/actions/runs/39606265614) on **Ubuntu and Windows** independently ran **62 tests, 1 expected ERROR, 61 PASS**. This exposed the absence of `producer_attestation` for a structurally valid v2 gap and legacy v1 CLI admission logic indexing `playback_context_verified` on a v2 report. The 61 older checks remained PASS.

The bounded fix reports `producer_attestation=UNVERIFIED` on **all** v2 results and returns exit code **2** for `--require-reproduction` with v2 input, whether the synthetic gap classification is positive or inconclusive. No traceback and no promotion to real audio output. The original v1 CLI evaluation remains unchanged in effect; `--require-reproduction` does not become a physical proof for v1 either. Future authenticated producer and Windows physical admission must be designed, tested and separately approved. The frozen 62 test assertions were not edited.

## T2 additional provenance hardening — 2026-10-21

Frozen RED-first HEAD `15e3370c1e133cc77cdefd2acdbe14275e04eba4`, [S01 #39610494133](https://github.com/boxs-51/Im_player/actions/runs/39610494133): **67 tests, 4 semantic FAIL + 63 PASS** on BOTH hosted Ubuntu and Windows. The four previously unguarded conditions were distinct event IDs sharing a single emitter sequence, stale acquisition `active_lease`, a generation owner timestamp preceding its owning load timestamp, and `load_id=0` being treated as bound. The fifth test is a positive control and passed. Existing 62 tests remained intact and PASS.

Correction requires a unique `(run_uid, session_uid, emitter_id, emitter_seq) → event_id` mapping, strict owner time ordering `SESSION_START < LOAD_STARTED < GENERATION_STARTED < UNDERFLOW` within one process clock domain, rejecting `load_id=0`, and rejecting explicitly mismatched acquisition leases. Missing acquisition lease in older synthetic fixtures is not accepted as OS attestation; v2 `producer_attestation` remains `UNVERIFIED` and CLI physical reproduction stays denied. This correction is an analyzer-only bounded experiment, not a trusted native producer lineage claim. Preserve all 67 frozen tests and all native C++.

## Native session lineage T2a — 2026-10-21

Frozen [S01 RED #39617101257](https://github.com/boxs-51/Im_player/actions/runs/39617101257) at test-only `56f2ffe87772a84fa1734de0eeacc1a8bf496ccd` reproduced **72 tests: 7 native source assertions FAIL and all 67 earlier tests PASS**, on both GitHub Ubuntu and Windows runners. The test harness converts missing native header to an assertion, not a FileNotFoundError.

This first native observational slice mints noncryptographic process-run and session-incarnation UIDs, binds the session before the capture thread starts, adds both IDs to AudioBlock metadata without changing PCM contents, preserves original load/generation/PTS and SPSC copying, and reports `run_uid/session_uid` on capture→processor→output stages and idle/segment underflow paths. The reported run ID is a best-effort diagnostic marker (not guaranteed collision-free and not a trusted producer credential). The emitted records still use legacy key/value formatting, **not** authenticated v2 JSONL. Any analyzer `binding_status=VERIFIED` only means structural fixture consistency; `producer_attestation=UNVERIFIED` and `--require-reproduction` fail-closed remain unchanged. No audio device or affected-media physical playback is exercised by hosted CI.
