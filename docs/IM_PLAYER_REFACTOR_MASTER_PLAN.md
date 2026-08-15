# IM_PLAYER — REFACTOR MASTER PLAN

> **Mục đích:** Tài liệu triển khai refactor toàn bộ `Im_player` dựa trên 3 audit:
>
> 1. Audio Pipeline Audit
> 2. Video Render Pipeline Audit
> 3. Window & Player Management Audit
>
> **Nguyên tắc:** Refactor theo từng phase độc lập, mỗi phase phải build được, chạy được và có thể rollback. Không thay đổi Window + Player + Render + Audio cùng lúc.

---

# 0. MỤC TIÊU CUỐI CÙNG

Kiến trúc mục tiêu:

```text
                           ApplicationRuntime
                                  |
                         SessionCoordinator
                           /             \
                          /               \
                         v                 v
                 WindowManager        PlayerManager
                       |                   |
                       v                   v
                 WindowRuntime        PlayerSession
                       |                   |
                       v                   v
                 RenderRuntime          MPVSession
                       |              /          \
                       v             v            v
                    OpenGL         Audio         Video
                       |             |             |
                       +-------------+-------------+
                                     |
                              Command / Snapshot
```

Các nguyên tắc bắt buộc:

```text
Window != Player
Player != MPV
Runtime object != cross-thread object
ID crosses thread
Snapshot crosses thread
Command crosses thread
Raw mutable pointer không vượt thread boundary
Generation invalidates stale asynchronous work
Thread owner phải rõ ràng
Resource owner phải rõ ràng
Shutdown phải deterministic
```

---

# 1. QUY TẮC REFACTOR TOÀN DỰ ÁN

## 1.1 Không refactor tất cả cùng lúc

Không thực hiện:

```text
Window
+ Player
+ MPV
+ Audio
+ Video
+ OpenGL
+ Shutdown
```

trong một commit/phase.

Thay vào đó:

```text
Phase 0  Documentation / Baseline
Phase 1  Window Ownership
Phase 2  Player Ownership
Phase 3  Session Coordinator
Phase 4  Snapshot Architecture
Phase 5  Command/Event Architecture
Phase 6  OpenGL / Video Ownership
Phase 7  Audio Ownership
Phase 8  Unified Seek / Track / Generation
Phase 9  Shutdown
Phase 10 Multi-window
Phase 11 Performance
Phase 12 Final Hardening
```

---

# 2. NGUYÊN TẮC "ONE OWNER"

Mỗi resource phải có đúng một owner chính.

## Window

```text
WindowManager
    |
    +-- WindowRuntime
           |
           +-- SDL_Window
           +-- Window state
```

## OpenGL

```text
RenderRuntime / RenderThread
    |
    +-- GLContext
    +-- Texture
    +-- FBO
    +-- Shader
    +-- FrameBufferPool
```

## Player

```text
PlayerManager
    |
    +-- PlayerSession
           |
           +-- MPVSession
```

## Audio

```text
AudioRuntime / AudioOutputWorker
    |
    +-- SDL Audio Device
    +-- Audio processing resources
```

Không cho nhiều thread cùng coi một resource là owner.

---

# 3. THREAD OWNERSHIP MỤC TIÊU

```text
MAIN / UI THREAD
    |
    +-- SDL event pump
    +-- UI command creation
    +-- UI snapshot consumption

MPV THREAD
    |
    +-- MPV event processing
    +-- MPV commands

RENDER THREAD
    |
    +-- OpenGL context
    +-- MPV video render context
    +-- GPU resources

AUDIO THREAD
    |
    +-- Audio device
    +-- PCM output

PROCESSING / ANALYSIS THREAD
    |
    +-- Audio processing
    +-- STT / analysis nếu có
```

Không bắt buộc phải đúng số thread trên ngay lập tức. Điều bắt buộc là **owner rõ ràng**.

---

# 4. PHASE 0 — BASELINE & OWNERSHIP AUDIT

## Mục tiêu

Không thay đổi behavior.

Tạo baseline để biết hệ thống hiện tại hoạt động thế nào trước khi refactor.

## Công việc

Tạo:

```text
docs/
    REFACTOR_MASTER_PLAN.md
    OWNERSHIP.md
    THREAD_MODEL.md
    LIFETIME_MODEL.md
    DEPENDENCY_GRAPH.md
    TEST_MATRIX.md
```

Ghi lại:

```text
class
owner
thread
resource
lifetime
dependency
mutex
callback
```

## Kiểm kê tối thiểu

```text
WindowManager
WindowRuntime
WindowFactory
WindowTemplate
WindowTemplateRegistry
WindowSharedGroup

PlayerManager
PlayerSession
MPVManager
MPVSession
MPVPlayer
MPVObserver
MPVCommandDispatcher

PlayBackRender
PlayBackRenderThread
IFrameBufferPool
OpenGLFrameBufferPool
OpenGLBackend

AudioCaptureManager
AudioOutputWorker
SdlAudioDevice
SpscRingBuffer
AudioProcessor
```

## Instrumentation

Thêm log:

```text
CREATE
START
STOP
JOIN
DESTROY
```

cho:

```text
Window
Player
MPV
RenderThread
AudioThread
```

## Không làm

```text
Không đổi ownership
Không đổi synchronization
Không đổi render
Không đổi audio
Không đổi MPV behavior
```

## Definition of Done

```text
[ ] Debug build
[ ] Release build
[ ] Play
[ ] Pause
[ ] Seek
[ ] Resize
[ ] Close
[ ] Shutdown
[ ] Log lifecycle đầy đủ
```

---

# 5. PHASE 1 — WINDOW OWNERSHIP

## Mục tiêu

`WindowManager` trở thành owner thực sự của `WindowRuntime`.

## File/nhóm file

Ưu tiên:

```text
WindowManager.*
WindowRuntime.*
WindowFactory.*
WindowTemplate.*
WindowTemplateRegistry.*
WindowSharedGroup.*
```

Nếu repository có tên file khác, map theo trách nhiệm tương ứng.

## Thiết kế

```cpp
using WindowId = uint64_t;

struct WindowIdentity {
    WindowId id;
    uint64_t generation;
};
```

Registry:

```cpp
std::unordered_map<
    WindowId,
    std::unique_ptr<WindowRuntime>
>;
```

## WindowState

```cpp
enum class WindowState {
    Created,
    Initializing,
    Ready,
    Visible,
    Hidden,
    Resizing,
    Closing,
    Closed
};
```

## Quy tắc

```text
WindowManager owns WindowRuntime.
WindowRuntime owns window-local state.
WindowId crosses threads.
WindowRuntime* does not cross threads.
```

## Không sửa

```text
Player
MPV
Audio
Video
FrameBufferPool
```

## Test

```text
Create window
Destroy window
Create 2 windows
Resize
Close secondary
Close primary
```

---

# 6. PHASE 2 — PLAYER OWNERSHIP

## Mục tiêu

Tách Player khỏi Window.

## File/nhóm

```text
PlayerManager.*
PlayerSession.*
MPVManager.*
MPVSession.*
MPVPlayer.*
MPVObserver.*
MPVCommandDispatcher.*
```

## PlayerId

```cpp
using PlayerId = uint64_t;

struct PlayerIdentity {
    PlayerId id;
    uint64_t generation;
};
```

## PlayerState

```cpp
enum class PlayerState {
    Created,
    Initializing,
    Idle,
    Loading,
    Ready,
    Playing,
    Paused,
    Seeking,
    Stopping,
    Stopped,
    Error,
    Destroying
};
```

## Kiến trúc

```text
PlayerManager
    |
    +-- PlayerSession
           |
           +-- MPVSession
```

Không:

```text
WindowRuntime -> owns PlayerSession
```

## PlayerSession và MPVSession

```text
PlayerSession = logical player
MPVSession    = backend implementation
```

Điều này cho phép sau này:

```text
PlayerSession
   +-- MPV backend
   +-- FFmpeg backend
   +-- Other backend
```

## Không sửa

```text
OpenGLFrameBufferPool
AudioCaptureManager
PlayBackRenderThread
```

## Test

```text
Create Player
Load
Play
Pause
Stop
Destroy
Create two Players
```

---

# 7. PHASE 3 — SESSION COORDINATOR

## Mục tiêu

Xóa dependency hai chiều:

```text
Window <-> Player
```

Tạo:

```text
SessionCoordinator.*
```

## Binding

```cpp
struct PlayerWindowBinding {
    WindowId windowId;
    PlayerId playerId;

    uint64_t windowGeneration;
    uint64_t playerGeneration;
};
```

## API

```cpp
attach(WindowId, PlayerId);
detach(WindowId);

switchPlayer(WindowId, PlayerId);

playerForWindow(WindowId);
windowsForPlayer(PlayerId);
```

## Kiến trúc

```text
WindowManager        PlayerManager
      |                   |
      v                   v
WindowRuntime        PlayerSession
       \                 /
        \               /
         SessionCoordinator
```

## Quy tắc

Coordinator là nơi xử lý:

```text
attach
detach
switch
logical lifecycle coordination
```

Window không tự destroy Player.

Player không tự destroy Window.

## Test

```text
A -> Player1
A -> Player2
A/B -> Player1
A closes, B remains
Player1 closes, A/B detach
```

---

# 8. PHASE 4 — SNAPSHOT ARCHITECTURE

## Mục tiêu

Không cho UI/Render đọc mutable runtime state xuyên thread.

Tạo:

```text
WindowSnapshot.*
PlayerSnapshot.*
RenderSnapshot.*
```

## WindowSnapshot

```cpp
struct WindowSnapshot {
    WindowId id;
    uint64_t generation;

    WindowState state;

    int width;
    int height;

    bool visible;
    bool focused;
    bool fullscreen;
};
```

## PlayerSnapshot

```cpp
struct PlayerSnapshot {
    PlayerId id;
    uint64_t generation;

    PlayerState state;

    double position;
    double duration;

    bool paused;
    bool buffering;

    int videoWidth;
    int videoHeight;
};
```

## Quy tắc

Snapshot phải là:

```text
immutable
self-consistent
thread-safe
```

UI:

```text
Runtime -> Snapshot -> UI
```

không:

```text
UI -> Runtime mutable fields
```

## Test

Kiểm tra:

```text
Play/Pause
Seek
Resize
Rapid state changes
Multi-window
```

đảm bảo UI không đọc combination state sai.

---

# 9. PHASE 5 — COMMAND / EVENT ARCHITECTURE

## Mục tiêu

Tách control plane khỏi data plane.

## Control plane

```text
UI
 |
 +-- WindowCommandQueue
 |
 +-- PlayerCommandQueue
```

## Data plane

```text
Audio
Video
PCM
VideoFrame
```

## PlayerCommand

```cpp
enum class PlayerCommandType {
    Load,
    Play,
    Pause,
    Stop,
    Seek,
    SetTrack,
    Shutdown
};
```

Command nên mang:

```text
PlayerId
generation nếu cần
payload
sequence
```

## MPV callback

Không:

```text
MPV callback
    -> UI
    -> WindowManager
    -> PlayerManager
```

Nên:

```text
MPV callback
    -> translate
    -> EventQueue
    -> return
```

## Quy tắc

Không giữ mutex trong:

```text
MPV callback
OpenGL callback
SDL audio callback
```

nếu có thể tránh.

---

# 10. PHASE 6 — VIDEO / OPENGL OWNERSHIP

## Mục tiêu

Đảm bảo mọi OpenGL resource có owner rõ.

## File/nhóm

```text
PlayBackRender.*
PlayBackRenderThread.*
IFrameBufferPool.*
OpenGLFrameBufferPool.*
OpenGLBackend.*
IGraphicsBackend.*
```

## Target ownership

```text
RenderThread
    |
    +-- GLContext
    +-- VideoTexture
    +-- FBO
    +-- Shader
    +-- FrameBufferPool
    +-- MPV video render context
```

## Không làm

```text
UI thread glDeleteTextures
UI thread glTexImage2D
Worker thread mutate same FBO
```

## Frame lifecycle

```text
Free
  |
  v
Writing
  |
  v
Ready
  |
  v
Rendering
  |
  v
GpuPending
  |
  v
Free
```

## Generation

```cpp
struct VideoFrameIdentity {
    PlayerId playerId;
    uint64_t playerGeneration;
    uint64_t frameSequence;
};
```

Frame cũ:

```text
frame.playerGeneration != currentGeneration
```

=> discard.

## Resize

```text
Window event
   |
   v
ResizeCommand
   |
   v
RenderThread
   |
   +-- recreate GPU resources
```

Không recreate GPU resource từ UI thread.

## Test

```text
Play
Pause
Seek
Resize while playing
Resize rapidly
Create second window
Render both
Close one window
Fullscreen
```

---

# 11. PHASE 7 — AUDIO OWNERSHIP

## Mục tiêu

Tách Audio resource và thread ownership.

## File/nhóm

```text
AudioCaptureManager.*
AudioProcessor.*
AudioOutputWorker.*
SdlAudioDevice.*
SpscRingBuffer.*
AudioFilterManager.*
```

## Target

```text
MPV
 |
 v
Raw Audio
 |
 v
Capture
 |
 v
RawRing
 |
 v
Processor
 |
 +------> PlaybackRing
 |
 +------> AnalysisRing
```

## SPSC rule

Chỉ dùng SPSC khi:

```text
exactly one producer
exactly one consumer
```

Nếu có nhiều consumer:

```text
không dùng một SPSC ring cho nhiều reader.
```

## AudioBlock

```cpp
struct AudioBlock {
    PlayerId playerId;
    uint64_t generation;
    uint64_t sequence;

    int sampleRate;
    int channels;
    int frames;

    double pts;
};
```

## Named Pipe

Không giả định:

```text
ReadFile == one AudioBlock
```

Cần:

```text
Pipe bytes
   |
   v
PCM accumulator
   |
   v
Complete AudioBlock
```

## SDL Audio Device

Một owner:

```text
AudioOutputWorker
    |
    +-- SDL audio device
```

UI không trực tiếp điều khiển device.

---

# 12. PHASE 8 — UNIFIED SEEK / TRACK TRANSACTION

## Mục tiêu

Seek là một logical transaction của Player.

Không để Audio/Video tự xử lý seek độc lập.

## Flow

```text
UI
 |
 v
SeekCommand
 |
 v
PlayerSession
 |
 +-- generation++
 |
 +-- MPV seek
 |
 +-- invalidate Audio
 |
 +-- invalidate Video
 |
 +-- invalidate Subtitle
 |
 +-- invalidate Analysis
```

## Generation

Ví dụ:

```text
generation = 20

Seek
  |
  v
generation = 21
```

Dữ liệu mang:

```text
generation = 20
```

=> discard.

## Track change

```text
SetTrack
 |
 v
PlayerSession
 |
 +-- generation++
 |
 +-- MPV track switch
 |
 +-- Audio reconfigure
 |
 +-- Subtitle reconfigure
```

---

# 13. PHASE 9 — SHUTDOWN TRANSACTION

## Mục tiêu

Không còn:

```text
destructor recursion
use-after-free
join deadlock
callback after destruction
```

## Shutdown sequence

```text
Application
 |
 v
SessionCoordinator::beginShutdown()
 |
 +-- stop accepting commands
 |
 +-- detach Window <-> Player
 |
 +-- stop Players
 |
 +-- stop MPV workers
 |
 +-- join MPV
 |
 +-- stop Audio
 |
 +-- join Audio
 |
 +-- stop Render
 |
 +-- join Render
 |
 +-- destroy GPU resources
 |
 +-- destroy ImGui contexts
 |
 +-- destroy GL contexts
 |
 +-- destroy SDL windows
 |
 +-- destroy Managers
```

## Quy tắc

Không:

```text
destroy resource
while worker can still access it
```

Không:

```text
join thread while holding manager mutex
```

Không:

```text
callback -> shutdown -> join itself
```

---

# 14. PHASE 10 — MULTI-WINDOW

Chỉ thực hiện khi:

```text
single Window
single Player
```

đã ổn định.

## Test matrix

### Case A

```text
Window A -> Player 1
```

### Case B

```text
Window A -> Player 1
Window B -> Player 1
```

### Case C

```text
Window A -> Player 1
Window B -> Player 2
```

### Case D

```text
A -> Player1
A switch -> Player2
```

### Case E

```text
A/B -> Player1
A closes
B continues
```

### Case F

```text
A/B -> Player1
Player1 closes
A/B remain alive
```

### Case G

```text
A resizing
B playing
```

### Case H

```text
A fullscreen
B playing
```

---

# 15. PHASE 11 — PERFORMANCE

Chỉ tối ưu sau correctness.

## Metrics

```text
CPU usage
GPU usage
FPS
frame drop
frame repeat
frame queue depth
audio queue depth
A/V drift
render latency
audio latency
allocation count
```

## Tối ưu theo thứ tự

```text
1. Allocation
2. Queue depth
3. Frame dropping
4. Buffer reuse
5. GPU synchronization
6. VSync
7. Frame pacing
8. Zero-copy
```

Không dùng:

```cpp
glFinish();
```

làm giải pháp mặc định cho synchronization.

---

# 16. PHASE 12 — FINAL HARDENING

## ThreadSanitizer / sanitizer nếu môi trường hỗ trợ

Kiểm tra:

```text
data race
use-after-free
double free
deadlock
```

Trên Windows/MSVC có thể kết hợp:

```text
Application Verifier
PageHeap
AddressSanitizer
Visual Studio Diagnostics
```

## Stress tests

```text
100x create/destroy Window
100x load/unload Player
100x seek
rapid resize
rapid player switch
multi-window
close during playback
close during seek
shutdown during load
shutdown during resize
```

---

# 17. FILE GROUPING

## GROUP A — WINDOW

```text
WindowManager.*
WindowRuntime.*
WindowFactory.*
WindowTemplate.*
WindowTemplateRegistry.*
WindowSharedGroup.*
```

Chỉ refactor cùng nhóm.

---

## GROUP B — PLAYER

```text
PlayerManager.*
PlayerSession.*
MPVManager.*
MPVSession.*
MPVPlayer.*
MPVObserver.*
MPVCommandDispatcher.*
```

Không trộn với low-level Audio/Render.

---

## GROUP C — VIDEO

```text
PlayBackRender.*
PlayBackRenderThread.*
IFrameBufferPool.*
OpenGLFrameBufferPool.*
OpenGLBackend.*
IGraphicsBackend.*
```

---

## GROUP D — AUDIO

```text
AudioCaptureManager.*
AudioProcessor.*
AudioOutputWorker.*
SpscRingBuffer.*
SdlAudioDevice.*
AudioFilterManager.*
```

---

## GROUP E — RUNTIME

```text
SessionCoordinator.*
WindowSnapshot.*
PlayerSnapshot.*
RenderSnapshot.*
CommandQueue.*
EventQueue.*
```

---

# 18. FILES KHÔNG NÊN SỬA CÙNG LÚC

## Không đồng thời:

```text
WindowManager + AudioCaptureManager
```

## Không đồng thời:

```text
WindowRuntime + SpscRingBuffer
```

## Không đồng thời:

```text
PlayerSession + OpenGLFrameBufferPool
```

trừ khi thay đổi là bắt buộc để compile.

## Không đồng thời:

```text
PlayBackRenderThread + AudioOutputWorker
```

## Không đồng thời:

```text
FrameBufferPool + Shutdown
```

## Không đồng thời:

```text
SPSC semantics + MPV architecture
```

---

# 19. COMMIT STRATEGY

Không tạo:

```text
refactor everything
```

Nên:

```text
01 docs: establish ownership model
02 window: add WindowId
03 window: add WindowState
04 window: centralize WindowManager ownership

05 player: add PlayerId
06 player: add PlayerState
07 player: centralize PlayerManager ownership

08 runtime: add SessionCoordinator
09 runtime: add WindowPlayerBinding

10 runtime: add WindowSnapshot
11 runtime: add PlayerSnapshot

12 runtime: add PlayerCommand
13 runtime: add EventQueue

14 render: isolate GL ownership
15 render: isolate FrameBufferPool
16 render: add frame generation

17 audio: isolate capture ownership
18 audio: fix SPSC ownership
19 audio: add audio generation

20 player: unify seek transaction
21 player: unify track transaction

22 runtime: deterministic shutdown
23 runtime: multi-window lifecycle

24 perf: frame latency
25 perf: audio latency
26 perf: allocation
```

Mỗi commit phải:

```text
Build
Run
Basic playback test
```

---

# 20. GIT BRANCH STRATEGY

```text
main
 |
 +-- refactor/window-ownership
 |
 +-- refactor/player-ownership
 |
 +-- refactor/session-coordinator
 |
 +-- refactor/snapshot-command
 |
 +-- refactor/video-render
 |
 +-- refactor/audio
 |
 +-- refactor/seek-lifecycle
 |
 +-- refactor/shutdown
```

Không merge branch nếu chưa pass regression tests.

---

# 21. DEFINITION OF DONE

Một Phase chỉ được coi là hoàn thành khi:

```text
[ ] Build Debug
[ ] Build Release
[ ] No new warnings nghiêm trọng
[ ] Play
[ ] Pause
[ ] Stop
[ ] Seek
[ ] Resize
[ ] Close
[ ] Shutdown
[ ] Lifecycle logs đúng
[ ] Không leak resource mới
[ ] Không thread mới không có owner
```

Render phase thêm:

```text
[ ] no stale frame
[ ] no frame flicker regression
[ ] no GPU resource race
[ ] resize safe
[ ] multi-window render safe
```

Audio phase thêm:

```text
[ ] no audio data race
[ ] no deadlock
[ ] no buffer corruption
[ ] no stale audio after seek
[ ] stable queue depth
```

---

# 22. CHECKPOINT SAU MỖI PHASE

Mỗi phase phải tạo checkpoint:

```text
Phase N
   |
   +-- source
   +-- build
   +-- runtime test
   +-- logs
   +-- regression result
```

Nếu Phase N lỗi:

```text
rollback Phase N
```

Không tiếp tục sửa Phase N+1 để che lỗi.

---

# 23. PHASE DEPENDENCY

```text
Phase 0
   |
   v
Phase 1 Window
   |
   v
Phase 2 Player
   |
   v
Phase 3 Coordinator
   |
   v
Phase 4 Snapshot
   |
   v
Phase 5 Command/Event
   |
   +--------------+
   |              |
   v              v
Phase 6 Video   Phase 7 Audio
   |              |
   +-------+------+
           |
           v
     Phase 8 Seek
           |
           v
     Phase 9 Shutdown
           |
           v
    Phase 10 MultiWindow
           |
           v
    Phase 11 Performance
           |
           v
    Phase 12 Hardening
```

---

# 24. PHASE CHECKLIST TÓM TẮT

```text
P0  Baseline
    ↓
P1  Window ownership
    ↓
P2  Player ownership
    ↓
P3  Coordinator
    ↓
P4  Snapshot
    ↓
P5  Command/Event
    ↓
P6  Video/OpenGL
    ↓
P7  Audio
    ↓
P8  Seek/Track/Generation
    ↓
P9  Shutdown
    ↓
P10 Multi-window
    ↓
P11 Performance
    ↓
P12 Hardening
```

---

# 25. THỨ TỰ FILE NÊN ĐỤNG VÀO

Nếu bắt đầu triển khai từ source hiện tại, thứ tự ưu tiên:

```text
1. WindowManager.*
2. WindowRuntime.*
3. WindowFactory.*
4. WindowTemplate.*
5. WindowTemplateRegistry.*
6. WindowSharedGroup.*

7. PlayerManager.*
8. PlayerSession.*
9. MPVManager.*
10. MPVSession.*
11. MPVPlayer.*
12. MPVObserver.*
13. MPVCommandDispatcher.*

14. SessionCoordinator.*

15. WindowSnapshot.*
16. PlayerSnapshot.*
17. RenderSnapshot.*

18. CommandQueue.*
19. EventQueue.*

20. OpenGLBackend.*
21. PlayBackRender.*
22. PlayBackRenderThread.*
23. IFrameBufferPool.*
24. OpenGLFrameBufferPool.*

25. AudioCaptureManager.*
26. AudioProcessor.*
27. AudioOutputWorker.*
28. SdlAudioDevice.*
29. SpscRingBuffer.*

30. Unified Seek
31. Unified Track
32. Shutdown
33. Multi-window
34. Performance
```

---

# 26. CÁC THỨ CHƯA NÊN LÀM

Trong toàn bộ refactor, tạm thời không ưu tiên:

```text
FFmpeg backend
Advanced plugin architecture
TTS
STT
Zero-copy hoàn chỉnh
HDR pipeline
Advanced subtitle renderer
Cloud provider
Remote playback
```

cho đến khi:

```text
Window
Player
Render
Audio
Seek
Shutdown
```

đã ổn định.

---

# 27. KIẾN TRÚC ĐÍCH

Cuối cùng source nên tiến gần:

```text
ApplicationRuntime
│
├── EventPump
│
├── WindowManager
│   └── WindowRuntime
│       └── RenderRuntime
│
├── PlayerManager
│   └── PlayerSession
│       └── MPVSession
│           ├── AudioRuntime
│           └── VideoRuntime
│
└── SessionCoordinator
    ├── Window ↔ Player Binding
    ├── Command Routing
    ├── Lifecycle
    └── Shutdown
```

Cross-thread:

```text
IDs
Snapshots
Commands
Events
AudioBlocks
VideoFrames
```

Không cross-thread:

```text
mutable WindowRuntime
mutable PlayerSession
ImGuiContext
GLContext
SDL_Window
MPV raw handle
SDL Audio Device
```

---

# 28. TIÊU CHUẨN KIẾN TRÚC SAU REFACTOR

Có thể coi refactor thành công khi:

```text
Window có thể sống mà không cần Player.

Player có thể sống mà không cần Window.

Player có thể đổi Window.

Một Player có thể phục vụ nhiều Window.

Window có thể đổi Player.

RenderThread có thể stop độc lập.

AudioThread có thể stop độc lập.

MPV callback không gọi UI trực tiếp.

UI không gọi GL trực tiếp ngoài owner context.

UI không gọi MPV blocking trực tiếp.

Audio không đọc state mutable của Window.

Video không đọc state mutable của Player xuyên thread.

Shutdown không phụ thuộc thứ tự destructor ngẫu nhiên.
```

---

# 29. KẾT LUẬN

Refactor `Im_player` không nên bắt đầu bằng việc "thêm mutex".

Trình tự đúng là:

```text
OWNERSHIP
    ↓
LIFETIME
    ↓
DEPENDENCY
    ↓
THREAD BOUNDARY
    ↓
SNAPSHOT
    ↓
COMMAND
    ↓
RENDER/AUDIO
    ↓
GENERATION
    ↓
SHUTDOWN
    ↓
PERFORMANCE
```

Nếu đảo thứ tự, đặc biệt nếu tối ưu Render/Audio trước khi giải quyết Window/Player ownership, rất dễ tạo thêm race condition mới.

**Mốc quan trọng nhất là Phase 3 — SessionCoordinator.**

Sau Phase 3, quan hệ Window/Player không còn là ownership trực tiếp. Từ đó Audio và Video mới có nền tảng để tách thread/resource ownership đúng cách.

---

# 30. NEXT STEP

Không nên bắt đầu sửa code ngay.

Bước kế tiếp nên là:

```text
PHASE 0
    ↓
đọc source hiện tại
    ↓
lập bảng:
Class
Owner
Thread
Resource
Mutex
Callback
Dependency
Lifetime
    ↓
xác định các ownership violation
    ↓
sau đó mới bắt đầu Phase 1
```

Tài liệu này là **master plan**, còn mỗi Phase sau đó nên có một tài liệu/commit riêng để tránh refactor quá lớn và khó rollback.
