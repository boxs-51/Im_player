# IM_PLAYER --- PHÂN TÍCH TOÀN DIỆN WINDOW MANAGEMENT & PLAYER MANAGEMENT

> Phạm vi: hệ thống quản lý Window, WindowRuntime,
> WindowManager/Factory/SharedGroup, PlayerSession/MPVSession và quan hệ
> giữa Window ↔ Player ↔ Render ↔ UI.
>
> Mục tiêu: tìm race condition, ownership sai, lifetime sai, coupling,
> deadlock, stale state, lỗi multi-window và đưa ra kiến trúc có thể mở
> rộng.

------------------------------------------------------------------------

# 1. Kết luận tổng thể

Hệ thống hiện tại có xu hướng gom quá nhiều trách nhiệm vào quan hệ:

``` text
Window
  ↓
WindowRuntime
  ↓
MPVManager / MPVSession
  ↓
Player / Render / Observer
```

Trong khi về mặt kiến trúc nên tách thành:

``` text
Application
    │
    ├── WindowManager
    │       └── WindowRuntime
    │
    ├── PlayerManager
    │       └── PlayerSession
    │
    └── SessionCoordinator
             │
             ├── Window ↔ Player binding
             ├── commands
             └── lifecycle
```

Điểm quan trọng:

> **Window không nên sở hữu toàn bộ Player. Player cũng không nên sở hữu
> Window.**

Hai hệ thống phải liên kết bằng một lớp orchestration/binding.

------------------------------------------------------------------------

# 2. Mô hình hiện tại nên được nhìn như 4 domain

``` text
WINDOW DOMAIN
    SDL_Window
    GLContext
    WindowRuntime
    ImGuiContext
    Window state

PLAYER DOMAIN
    PlayerSession
    MPVSession
    MPVHandle
    Playback state

MEDIA DOMAIN
    Audio
    Video
    Tracks
    Subtitle
    Timeline

RENDER DOMAIN
    RenderThread
    OpenGL resources
    FrameBufferPool
    VideoFrame
```

Không nên để một object trực tiếp điều khiển cả bốn domain.

------------------------------------------------------------------------

# 3. Vấn đề ownership lớn nhất

Mỗi resource phải có một owner rõ ràng.

## Window

``` text
WindowManager
    owns
      ↓
WindowRuntime
    owns
      ↓
SDL_Window
GLContext
ImGuiContext
```

## Player

``` text
PlayerManager
    owns
      ↓
PlayerSession
    owns
      ↓
MPVSession
MPVHandle
MPVObserver
MPVCommandDispatcher
```

## Render

``` text
WindowRuntime
    owns
      ↓
RenderThread
    owns
      ↓
OpenGL resources
```

## Không nên

``` text
WindowRuntime
 ├── owns MPV
 ├── owns Render
 ├── owns Audio
 ├── owns Observer
 ├── owns UI
 └── owns global state
```

vì lifetime trở nên quá khó kiểm soát.

------------------------------------------------------------------------

# 4. Window và Player phải là hai lifecycle độc lập

Một lỗi kiến trúc thường gặp:

``` text
CreateWindow()
    → createPlayer()

CloseWindow()
    → destroyPlayer()
```

Điều này chỉ đúng nếu:

> một window = một player và player không bao giờ được tái sử dụng.

Nhưng nếu tương lai cần:

``` text
1 player
  ↓
window A
window B
```

hoặc:

``` text
window A
  ↓
detach
  ↓
window B
```

thì coupling trên sẽ trở thành vấn đề.

------------------------------------------------------------------------

# 5. Đề xuất PlayerSession độc lập Window

``` text
PlayerSession
    │
    ├── Media identity
    ├── Playback state
    ├── Timeline
    ├── Audio pipeline
    ├── Video pipeline
    ├── Track state
    └── MPV
```

Window chỉ có:

``` text
WindowRuntime
    │
    ├── layout
    ├── UI
    ├── render target
    └── PlayerBinding
```

------------------------------------------------------------------------

# 6. PlayerBinding

Nên có abstraction:

``` cpp
struct PlayerBinding {
    PlayerId playerId;
    WindowId windowId;

    uint64_t generation;
    bool primary;
};
```

Hoặc:

``` text
WindowRuntime
    └── optional<PlayerBinding>
```

Khi đổi player:

``` text
Window
  ↓
unbind old Player
  ↓
bind new Player
```

không destroy Window.

------------------------------------------------------------------------

# 7. WindowManager không nên điều khiển playback trực tiếp

WindowManager nên quản lý:

``` text
create
destroy
find
activate
focus
resize
visibility
lifecycle
```

Không nên chứa:

``` text
mpv_command("seek ...")
audio processing
subtitle processing
video decoding
```

Các thao tác đó thuộc PlayerSession.

------------------------------------------------------------------------

# 8. PlayerManager

Nên có:

``` cpp
class PlayerManager {
public:
    PlayerId create();
    void destroy(PlayerId);
    PlayerSession* find(PlayerId);
};
```

PlayerManager quản lý:

``` text
PlayerId
PlayerSession lifetime
```

không quản lý:

``` text
SDL_Window
ImGuiContext
OpenGL texture
```

------------------------------------------------------------------------

# 9. WindowManager

Tương tự:

``` cpp
class WindowManager {
public:
    WindowId create(WindowConfig);
    void destroy(WindowId);
    WindowRuntime* find(WindowId);
};
```

WindowManager không giữ:

``` text
MPVHandle
AudioDevice
VideoFramePool
```

------------------------------------------------------------------------

# 10. SessionCoordinator

Đây là lớp nên bổ sung.

``` text
SessionCoordinator
      │
      ├── WindowManager
      └── PlayerManager
```

Nhiệm vụ:

``` text
attach(window, player)
detach(window)
switchPlayer(window, player)
closePlayer(player)
```

Ví dụ:

``` cpp
attach(WindowId w, PlayerId p);
detach(WindowId w);
```

------------------------------------------------------------------------

# 11. Vì sao cần Coordinator?

Nếu không có Coordinator:

``` text
WindowRuntime → PlayerSession
PlayerSession → WindowRuntime
WindowManager → PlayerManager
PlayerManager → WindowManager
```

sẽ tạo graph dependency:

``` text
A → B
B → C
C → A
```

và rất dễ dẫn đến:

``` text
circular ownership
deadlock
shutdown recursion
```

Coordinator phá vòng dependency.

------------------------------------------------------------------------

# 12. Dependency direction

Nên:

``` text
Application
    ↓
SessionCoordinator
    ↓
WindowManager      PlayerManager
    ↓                   ↓
WindowRuntime       PlayerSession
    ↓                   ↓
Render/UI           MPV/Audio/Video
```

Không:

``` text
WindowRuntime
    ↔
PlayerSession
```

------------------------------------------------------------------------

# 13. WindowRuntime state machine

Không nên chỉ có:

``` text
bool running;
bool initialized;
bool visible;
```

Nên:

``` cpp
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

Transition:

``` text
Created
  ↓
Initializing
  ↓
Ready
  ↓
Visible
  ↓
Closing
  ↓
Closed
```

------------------------------------------------------------------------

# 14. PlayerSession state machine

Tương tự:

``` cpp
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

Không để UI tự suy diễn state từ nhiều atomic bool.

------------------------------------------------------------------------

# 15. MPVSession không nên đồng nghĩa PlayerSession

Nên:

``` text
PlayerSession
    │
    └── MPVSession
```

PlayerSession là abstraction của player.

MPVSession là implementation/backend.

Sau này có thể:

``` text
PlayerSession
   ├── MPVBackend
   ├── FFmpegBackend
   └── RemoteBackend
```

------------------------------------------------------------------------

# 16. MPVManager

Nếu MPVManager đang quản lý nhiều trách nhiệm, nên giới hạn:

``` text
MPVManager
    ├── create MPVSession
    ├── destroy MPVSession
    └── backend-level services
```

Không để MPVManager trở thành:

``` text
global god object
```

chứa:

``` text
window
audio
video
UI
thread
subtitle
```

------------------------------------------------------------------------

# 17. Thread ownership

Đề xuất:

``` text
Main/UI Thread
    │
    ├── WindowManager commands
    └── Player commands
```

``` text
Player/MPV Thread
    │
    └── MPV event processing
```

``` text
RenderThread
    │
    └── OpenGL
```

``` text
AudioOutputThread
    │
    └── Audio device
```

Các thread giao tiếp qua:

``` text
command queue
snapshot
SPSC data queue
```

thay vì gọi trực tiếp object của thread khác.

------------------------------------------------------------------------

# 18. UI thread không nên gọi sâu vào Player

Anti-pattern:

``` text
ImGui button
 ↓
WindowRuntime
 ↓
PlayerSession
 ↓
MPV command
 ↓
wait mutex
```

Điều này có thể block UI.

Nên:

``` text
ImGui button
 ↓
PlayerCommandQueue.push(Seek)
 ↓
return
```

Player thread xử lý.

------------------------------------------------------------------------

# 19. Command plane và data plane

Tách hoàn toàn:

``` text
COMMAND PLANE
UI
 ↓
CommandQueue
 ↓
Player/Window thread
```

và:

``` text
DATA PLANE
Audio
Video
Frames
PCM
```

Không dùng data ring buffer để truyền command.

------------------------------------------------------------------------

# 20. Snapshot thay vì đọc state xuyên thread

UI không nên:

``` cpp
player->mpv->get_property(...)
```

liên tục từ render/UI thread.

Nên:

``` cpp
struct PlayerSnapshot {
    PlayerState state;

    double position;
    double duration;

    bool paused;
    bool buffering;

    int videoWidth;
    int videoHeight;

    uint64_t generation;
};
```

Player thread publish snapshot.

UI chỉ đọc snapshot.

------------------------------------------------------------------------

# 21. WindowSnapshot

Tương tự:

``` cpp
struct WindowSnapshot {
    WindowState state;

    int width;
    int height;

    bool visible;
    bool focused;
    bool fullscreen;

    WindowId id;
};
```

RenderThread không đọc mutable WindowRuntime trực tiếp.

------------------------------------------------------------------------

# 22. PlayerSnapshot + WindowSnapshot

UI:

``` text
WindowSnapshot
+
PlayerSnapshot
+
RenderSnapshot
```

là đủ để render UI.

Không cần lock sâu vào runtime objects.

------------------------------------------------------------------------

# 23. Multi-window

Kiến trúc nên hỗ trợ:

``` text
Player 1
 ├── Window A
 └── Window B

Player 2
 └── Window C
```

hoặc:

``` text
Window A → Player 1
Window B → Player 1
```

Nếu hiện tại mỗi Window sở hữu Player, đây là hạn chế kiến trúc cần loại
bỏ sớm.

------------------------------------------------------------------------

# 24. Một Player nhiều Window

Có thể dùng:

``` text
PlayerSession
      │
      ├── VideoSource
      │       ├── Window A
      │       └── Window B
      │
      └── AudioSource
```

Nhưng mỗi window phải có:

``` text
WindowRenderContext
```

riêng.

Không share:

``` text
window framebuffer
```

------------------------------------------------------------------------

# 25. Một Window đổi Player

Flow:

``` text
Window A
  ↓
Player 1
  ↓
detach
  ↓
Player 2
```

Không destroy:

``` text
Window A
```

Không destroy:

``` text
Player 1
```

nếu Player 1 còn được window khác sử dụng.

------------------------------------------------------------------------

# 26. Player close trong khi Window còn tồn tại

Phải xử lý:

``` text
Player destroyed
       ↓
Window binding invalidated
       ↓
Window remains alive
       ↓
UI shows "No media"
```

Không được:

``` text
destroy player
 ↓
window dereference dangling Player*
```

------------------------------------------------------------------------

# 27. Window close trong khi Player còn chạy

Flow:

``` text
Window A close
   ↓
detach binding
   ↓
stop RenderThread A
   ↓
destroy Window A
   ↓
Player continues
```

Nếu không còn window:

``` text
Player vẫn có thể chạy
```

hoặc:

``` text
PlayerManager policy:
auto-pause
```

Đây phải là policy rõ ràng.

------------------------------------------------------------------------

# 28. Shutdown toàn hệ thống

Thứ tự khuyến nghị:

``` text
Application::Shutdown
        ↓
stop accepting commands
        ↓
SessionCoordinator::shutdown
        ↓
detach windows
        ↓
stop PlayerSessions
        ↓
join MPV threads
        ↓
stop AudioOutputThreads
        ↓
stop RenderThreads
        ↓
destroy GPU resources
        ↓
destroy ImGui contexts
        ↓
destroy SDL GL contexts
        ↓
destroy SDL windows
        ↓
destroy managers
```

Không destroy manager trước worker threads.

------------------------------------------------------------------------

# 29. Shutdown dependency graph

``` text
Application
    ↓
Coordinator
    ↓
PlayerManager
    ↓
PlayerSession
    ├── MPV
    ├── Audio
    └── Video

WindowManager
    ↓
WindowRuntime
    └── RenderThread
```

Phải join thread trước resource destruction.

------------------------------------------------------------------------

# 30. Deadlock nguy hiểm

Một pattern cần tránh:

``` text
UI:
 lock Window
   ↓
 lock Player

Player:
 lock Player
   ↓
 lock Window
```

=\> deadlock.

Tốt hơn:

``` text
WindowManager
PlayerManager
```

không lock chéo.

Coordinator thực hiện orchestration theo một thứ tự cố định.

------------------------------------------------------------------------

# 31. Lock hierarchy

Nếu mutex vẫn cần:

``` text
GlobalManagerLock
       ↓
WindowLock
       ↓
PlayerLock
```

Không bao giờ:

``` text
PlayerLock → WindowLock
```

nếu chiều ngược lại đã tồn tại.

Tốt hơn nữa:

> tránh nested lock giữa Window và Player.

------------------------------------------------------------------------

# 32. Không giữ mutex trong callback MPV

MPV callback nên:

``` text
read event
 ↓
translate event
 ↓
push EventQueue
 ↓
return
```

Không:

``` text
callback
 ↓
lock WindowManager
 ↓
lock PlayerManager
 ↓
call UI
```

Callback càng ngắn càng tốt.

------------------------------------------------------------------------

# 33. EventBus

Nên có:

``` text
PlayerEvent
WindowEvent
RenderEvent
AudioEvent
```

Ví dụ:

``` cpp
struct PlayerEvent {
    PlayerId player;
    PlayerEventType type;
    uint64_t generation;
};
```

UI subscribe snapshot/event.

------------------------------------------------------------------------

# 34. Generation toàn hệ thống

Không chỉ Audio/Video.

Nên có:

``` text
SessionGeneration
```

Ví dụ:

``` text
Player 1
generation = 100

load new media
generation = 101
```

Mọi asynchronous work mang theo:

``` text
playerId
generation
```

Nếu mismatch:

``` text
discard
```

Đây là cơ chế cực mạnh để chống stale async result.

------------------------------------------------------------------------

# 35. Media identity

Generation chưa đủ.

Nên có:

``` cpp
struct MediaIdentity {
    PlayerId player;
    uint64_t generation;
};
```

Mọi:

``` text
AudioBlock
VideoFrame
SubtitleEvent
MetadataEvent
```

đều có:

``` text
MediaIdentity
```

------------------------------------------------------------------------

# 36. Window identity

Tương tự:

``` cpp
struct WindowIdentity {
    WindowId id;
    uint64_t generation;
};
```

Render event:

``` text
windowId
windowGeneration
playerId
playerGeneration
```

Ngăn event của window cũ tác động window mới.

------------------------------------------------------------------------

# 37. Window recreation

Một số platform có thể yêu cầu recreate:

``` text
SDL Window
GL context
```

Không được coi:

``` text
WindowId
```

là đủ.

Nên:

``` text
WindowId = logical identity
WindowGeneration = physical instance
```

------------------------------------------------------------------------

# 38. Player recreation

Tương tự:

``` text
PlayerId
PlayerGeneration
```

Một Player logical session có thể thay MPV backend instance.

Do đó:

``` text
PlayerId ≠ MPV pointer
```

Đây là distinction quan trọng.

------------------------------------------------------------------------

# 39. Raw pointer cần giảm mạnh

Tránh:

``` cpp
WindowRuntime* m_window;
PlayerSession* m_player;
```

lưu xuyên thread.

Ưu tiên:

``` text
WindowId
PlayerId
FrameHandle
Command
Snapshot
```

Object pointer chỉ dùng trong owner thread.

------------------------------------------------------------------------

# 40. Registry

WindowManager:

``` text
unordered_map<WindowId, unique_ptr<WindowRuntime>>
```

PlayerManager:

``` text
unordered_map<PlayerId, unique_ptr<PlayerSession>>
```

Chỉ manager sở hữu object.

Các subsystem khác dùng ID.

------------------------------------------------------------------------

# 41. WindowFactory

WindowFactory nên chỉ chịu trách nhiệm:

``` text
construct WindowRuntime
```

Không nên:

``` text
register global state
create Player
start playback
create audio
```

Factory nên deterministic.

------------------------------------------------------------------------

# 42. WindowTemplateRegistry

Template nên chứa configuration:

``` text
WindowTemplate
    ├── size
    ├── flags
    ├── title
    ├── role
    └── capabilities
```

Không nên chứa mutable runtime object.

------------------------------------------------------------------------

# 43. WindowSharedGroup

Nếu có shared GL context/group:

Phải xác định rõ:

``` text
shared immutable resource
```

và:

``` text
window-local resource
```

Không để shared group trở thành:

``` text
global mutable GPU state
```

------------------------------------------------------------------------

# 44. ImGui context

Mỗi WindowRuntime nên có:

``` text
ImGuiContext
```

riêng nếu render trên các thread/window độc lập.

Không nên:

``` text
Thread A
 └── ImGuiContext X

Thread B
 └── ImGuiContext X
```

trừ khi toàn bộ access được serialize.

------------------------------------------------------------------------

# 45. ImGui frame lifecycle

Mỗi context:

``` text
NewFrame
 ↓
UI
 ↓
Render
 ↓
RenderDrawData
```

phải được thực hiện nhất quán trên owner thread.

Không:

``` text
UI build draw data
 ↓
thread khác mutate same context
```

------------------------------------------------------------------------

# 46. Window event processing

SDL event:

``` text
SDL_PollEvent
```

nên có một owner rõ.

Không nên để nhiều WindowRuntime cùng:

``` text
SDL_PollEvent()
```

tùy ý.

Nên:

``` text
Application/EventPump
       ↓
WindowEventQueue
       ↓
WindowRuntime
```

------------------------------------------------------------------------

# 47. Event routing

``` text
SDL event
   ↓
WindowId
   ↓
WindowManager
   ↓
WindowRuntime
```

Nếu event liên quan player:

``` text
WindowRuntime
   ↓
PlayerCommand
```

Không gọi trực tiếp sâu vào MPV.

------------------------------------------------------------------------

# 48. Focus/active window

Nên tách:

``` text
focusedWindow
activeWindow
renderTargetWindow
```

Không giả định ba khái niệm này giống nhau.

Ví dụ:

``` text
Window A focused
Window B playing
Window C rendering fullscreen
```

------------------------------------------------------------------------

# 49. Player active window

Player nên không phụ thuộc:

``` text
focusedWindow
```

Ví dụ:

``` text
Player 1
 ├── playback
 └── Window A + B
```

Focus chỉ là UI concept.

------------------------------------------------------------------------

# 50. Command routing

Ví dụ Play:

``` text
UI Window A
 ↓
WindowCommand::Play
 ↓
Coordinator
 ↓
PlayerCommand::Play
 ↓
PlayerSession
```

Không:

``` text
Window A
 ↓
WindowRuntime
 ↓
mpv_command()
```

trực tiếp.

------------------------------------------------------------------------

# 51. Seek routing

``` text
Window A
 ↓
SeekCommand(120.0)
 ↓
PlayerSession
 ↓
generation++
 ↓
MPV seek
 ↓
Audio invalidation
 ↓
Video invalidation
 ↓
Subtitle invalidation
```

Một seek phải là một transaction logic.

------------------------------------------------------------------------

# 52. Track change

Tương tự:

``` text
TrackChange
 ↓
Player generation++
 ↓
Audio pipeline reconfigure
 ↓
Subtitle pipeline reconfigure
 ↓
Video unaffected nếu cần
```

Không để từng subsystem tự suy đoán track state.

------------------------------------------------------------------------

# 53. PlayerSnapshot consistency

Snapshot nên được publish atomically.

Có thể dùng:

``` text
mutex + immutable snapshot
```

hoặc:

``` text
double-buffered snapshot
```

Không để UI đọc:

``` text
position
duration
state
generation
```

từ nhiều atomic độc lập và có thể nhìn thấy combination không hợp lệ.

------------------------------------------------------------------------

# 54. Example inconsistent state

UI có thể thấy:

``` text
state = Playing
generation = 20
position = 0.2
duration = old media 5000
```

trong khi:

``` text
new media duration = 100
```

Một immutable PlayerSnapshot giải quyết vấn đề này.

------------------------------------------------------------------------

# 55. Player → Window notification

Không gọi:

``` cpp
player->window->updateUI();
```

Nên:

``` text
PlayerSnapshot publish
       ↓
UI next frame reads snapshot
```

Hoặc:

``` text
PlayerEvent
```

cho sự kiện discrete.

------------------------------------------------------------------------

# 56. Window → Player notification

Tương tự:

``` text
Window resize
```

không trực tiếp mutate Player.

Nếu player cần biết:

``` text
RenderTargetChanged
```

thì gửi command/event.

------------------------------------------------------------------------

# 57. Error isolation

Nếu:

``` text
Audio crash/failure
```

không được làm:

``` text
Window crash
```

Nếu:

``` text
Render error
```

không nên:

``` text
destroy Player
```

nếu player backend vẫn hoạt động.

Domain isolation giúp recovery.

------------------------------------------------------------------------

# 58. Recommended top-level architecture

``` text
ApplicationRuntime
│
├── EventPump
│
├── WindowManager
│    └── WindowRuntime*
│         ├── WindowSnapshot
│         ├── ImGuiContext
│         └── RenderRuntime
│
├── PlayerManager
│    └── PlayerSession*
│         ├── PlayerSnapshot
│         ├── MPVSession
│         ├── AudioRuntime
│         └── VideoRuntime
│
└── SessionCoordinator
     ├── Window ↔ Player binding
     ├── Command routing
     ├── lifecycle
     └── shutdown
```

------------------------------------------------------------------------

# 59. Detailed dependency graph

``` text
Application
   │
   ├──────────────► EventPump
   │
   ├──────────────► WindowManager
   │                   │
   │                   └── WindowRuntime
   │                         │
   │                         └── RenderRuntime
   │
   ├──────────────► PlayerManager
   │                   │
   │                   └── PlayerSession
   │                         │
   │                         ├── MPVSession
   │                         ├── AudioRuntime
   │                         └── VideoRuntime
   │
   └──────────────► SessionCoordinator
                         │
                         └── Binding Registry
```

Dependency direction không quay ngược.

------------------------------------------------------------------------

# 60. Binding registry

``` cpp
struct PlayerWindowBinding {
    PlayerId playerId;
    WindowId windowId;
};
```

Registry:

``` text
Window A → Player 1
Window B → Player 1
Window C → Player 2
```

Player không cần giữ raw pointer tới Window.

Window không cần giữ raw pointer tới Player.

------------------------------------------------------------------------

# 61. Lifetime rules

### Rule 1

``` text
Manager owns runtime object.
```

### Rule 2

``` text
Thread owns thread-affine resource.
```

### Rule 3

``` text
ID crosses thread, raw pointer does not.
```

### Rule 4

``` text
Commands cross thread, mutable state does not.
```

### Rule 5

``` text
Snapshot crosses thread, runtime object does not.
```

### Rule 6

``` text
Generation invalidates stale async work.
```

------------------------------------------------------------------------

# 62. Deadlock prevention rules

``` text
1. Không lock WindowManager rồi gọi PlayerManager.
2. Không lock PlayerManager rồi gọi WindowManager.
3. Không giữ mutex khi gọi MPV.
4. Không giữ mutex khi gọi OpenGL.
5. Không giữ mutex khi chờ thread.
6. Không join thread trong callback.
7. Không destroy object khi worker còn có thể truy cập.
```

------------------------------------------------------------------------

# 63. Shutdown rules

Tuyệt đối tránh:

``` cpp
WindowRuntime::~WindowRuntime() {
    renderThread.stop();
    player->stop();
}
```

nếu:

``` text
Player → Window
```

và:

``` text
Window → Player
```

cùng tồn tại.

Shutdown phải được orchestration bởi Coordinator.

------------------------------------------------------------------------

# 64. Recommended shutdown transaction

``` text
Application
 ↓
Coordinator::beginShutdown()
 ↓
freeze new commands
 ↓
stop Window commands
 ↓
detach Window ↔ Player
 ↓
stop Player
 ↓
join Player workers
 ↓
stop Render workers
 ↓
join Render workers
 ↓
destroy Window resources
 ↓
destroy Player resources
 ↓
destroy managers
```

Thứ tự thực tế có thể đảo một số bước tùy ownership, nhưng nguyên tắc
là:

> **detach dependency trước, stop worker sau, destroy resource cuối
> cùng.**

------------------------------------------------------------------------

# 65. P0 --- Những việc nên sửa trước

``` text
[ ] Tách WindowManager và PlayerManager
[ ] Không để WindowRuntime sở hữu Player trực tiếp nếu không bắt buộc
[ ] Thêm SessionCoordinator
[ ] Dùng WindowId / PlayerId
[ ] Xây WindowSnapshot
[ ] Xây PlayerSnapshot
[ ] Xây WindowState
[ ] Xây PlayerState
[ ] Tách command plane
[ ] Tách event plane
[ ] Thêm generation
[ ] Xóa dependency Window ↔ Player hai chiều
[ ] Xác định owner của từng thread
[ ] Xác định owner của SDL/GL/ImGui
```

------------------------------------------------------------------------

# 66. P1 --- Sau khi ownership ổn định

``` text
[ ] EventBus
[ ] Binding registry
[ ] Player multi-window
[ ] Window player switching
[ ] Player background playback
[ ] Snapshot versioning
[ ] Error recovery
[ ] explicit shutdown transaction
[ ] lifecycle diagnostics
```

------------------------------------------------------------------------

# 67. P2 --- Mở rộng tương lai

Kiến trúc này cho phép:

``` text
Playlist
Multiple players
Picture-in-picture
Preview window
Detached video window
Subtitle window
Waveform window
Visualizer window
Multi-monitor
Remote playback
Recording
Streaming
STT
Translation
TTS
```

mà không cần biến WindowManager thành một God Object.

------------------------------------------------------------------------

# 68. Diagnostic system nên bổ sung

Mỗi runtime object nên có:

``` text
id
generation
state
ownerThread
createdAt
destroyedAt
```

Ví dụ:

``` text
Window:
 id=3
 generation=8
 state=Visible
 ownerThread=RenderThread#2

Player:
 id=7
 generation=14
 state=Playing
 ownerThread=MPVThread#1
```

Khi crash/race sẽ dễ truy nguyên.

------------------------------------------------------------------------

# 69. Kiến trúc cuối cùng được khuyến nghị

``` text
                         APPLICATION
                              │
                ┌─────────────┴─────────────┐
                │                           │
          WINDOW DOMAIN                PLAYER DOMAIN
                │                           │
        WindowManager                 PlayerManager
                │                           │
        WindowRuntime                  PlayerSession
                │                           │
         RenderRuntime                  MPVSession
                │                      ┌────┴────┐
         OpenGL/ImGui                 Audio    Video
                │
                └───────────┐
                            │
                    SESSION COORDINATOR
                            │
                   Window ↔ Player
                        Binding
```

Đây là kiến trúc nên hướng tới.

------------------------------------------------------------------------

# 70. Kết luận cuối

Vấn đề lớn nhất của hệ thống Window/Player không phải là thiếu class.

Ngược lại:

> **Có nhiều abstraction nhưng ownership và dependency direction chưa đủ
> rõ.**

Ba thay đổi quan trọng nhất là:

``` text
1. Window ≠ Player

2. Runtime object ≠ cross-thread object

3. Command/Snapshot/ID phải thay cho
   direct mutable access xuyên thread
```

Sau khi thực hiện ba nguyên tắc này, các vấn đề:

``` text
deadlock
use-after-free
window ảnh hưởng window khác
player bị destroy ngoài ý muốn
shutdown crash
stale state
render race
ImGui context race
```

sẽ giảm mạnh và hệ thống có nền tảng tốt để tiếp tục phát triển Audio +
Video + Subtitle + STT.

------------------------------------------------------------------------

# 71. Thứ tự triển khai thực tế

``` text
Phase 0
Document ownership
        ↓
Phase 1
WindowManager / PlayerManager
        ↓
Phase 2
WindowId / PlayerId
        ↓
Phase 3
SessionCoordinator
        ↓
Phase 4
WindowSnapshot / PlayerSnapshot
        ↓
Phase 5
CommandQueue
        ↓
Phase 6
Generation
        ↓
Phase 7
Fix RenderThread ownership
        ↓
Phase 8
Fix AudioThread ownership
        ↓
Phase 9
Unified Seek/TrackChange transaction
        ↓
Phase 10
Shutdown transaction
```

**Không nên bắt đầu refactor bằng cách đổi class trước. Hãy lập
ownership/lifetime graph trước, sau đó mới di chuyển code.**
