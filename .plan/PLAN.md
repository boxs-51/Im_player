# EVENT / RUNTIME / PLAYER / RENDER ARCHITECTURE REFACTOR PLAN

## 1. Mục tiêu

Mục tiêu chính của đợt refactor này là loại bỏ sự phụ thuộc không cần thiết giữa:

- Main Thread
- SDL Event Loop
- Win32 Native Event
- Event System
- Runtime
- WindowRuntime
- PlayerSession
- MPVSession
- RenderScheduler
- UIRenderThread

Đặc biệt phải giải quyết hiện tượng:

1. Video phát bình thường khi không resize/drag.
2. Khi resize/drag một window, các window khác bị ảnh hưởng.
3. Window khác chỉ render lại khi `RequestRender()` được gọi từ Main Thread.
4. `SDL_PollEvent()` nằm trên Main Thread khiến Event processing phụ thuộc vào Main Thread.
5. Windows native move/resize loop có thể làm Main Thread không chạy application loop theo cadence bình thường.
6. MPV render/update không được phép phụ thuộc vào Main Thread.
7. Event System không được trở thành một phần của Render Scheduler.

---

# 2. Nguyên tắc kiến trúc mới

## 2.1 Main Thread không phải Global Heartbeat

Main Thread KHÔNG được là nơi bắt buộc phải:

- update mọi Runtime
- update mọi PlayerSession
- RequestRender cho mọi Window
- dispatch toàn bộ Event
- kích hoạt MPV render
- duy trì FPS cho toàn application

Main Thread chỉ nên chịu trách nhiệm những công việc có tính platform/native:

```text
SDL_PollEvent()
SDL initialization
SDL window lifecycle
Win32 integration
Application lifecycle
Native API cần Main Thread
```

---

# 3. Kiến trúc tổng thể

```text
                              APPLICATION
                                  |
                    +-------------+-------------+
                    |                           |
                MAIN THREAD                 WORKERS
                    |                           |
          +---------+---------+          +------+------+
          |                   |          |             |
      SDL Producer       Win32 Producer MPV        Render
          |                   |          |             |
          +---------+---------+          |             |
                    |                    |             |
                    v                    v             v
              Event Queue          MPV Event       Render
                    |                Queue          Threads
                    v                    |             |
              Event Worker              v             |
                    |              MPV Worker         |
                    |                    |             |
          +---------+---------+          |             |
          |         |         |          |             |
          v         v         v          v             |
       Runtime  WindowRuntime PlayerSession             |
                    |          |                       |
                    |          v                       |
                    |       MPVSession                 |
                    |                                  |
                    +-------------> RenderScheduler <+
                                      |
                                      v
                                UIRenderThread
                                      |
                                      v
                                    OpenGL
                                      |
                                      v
                                  SwapWindow
```

---

# 4. Event Architecture

## 4.1 Event không còn được xử lý trực tiếp trên Main Thread

Kiến trúc cũ:

```text
SDL_PollEvent()
    |
    v
Main Thread
    |
    +--> xử lý event
    +--> Runtime
    +--> Player
    +--> RequestRender
```

Kiến trúc mới:

```text
SDL_PollEvent()
    |
    v
EventQueue
    |
    v
EventWorker
    |
    v
EventDispatcher
    |
    +--> Runtime
    +--> WindowRuntime
    +--> PlayerSession
```

Main Thread chỉ producer event.

---

# 5. Hai Event Producer

Không coi `SDL_PollEvent()` là nguồn Event duy nhất.

## 5.1 SDL Event Producer

```text
Main Thread
    |
    v
SDL_PollEvent()
    |
    v
SDL Event
    |
    v
EventQueue
```

Pseudo-code:

```cpp
while (running)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        eventQueue.push(
            Event::fromSDL(event)
        );
    }
}
```

Không dispatch event tại đây.

---

# 6. Win32 Native Event Producer

Trong `WndProc`:

```text
Windows
    |
    v
WndProc
    |
    v
Native Event
    |
    v
EventQueue
```

Ví dụ:

```cpp
LRESULT CALLBACK MultiWindowWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    NativeEvent event;

    if (translateWin32Message(
            hwnd,
            msg,
            wParam,
            lParam,
            event))
    {
        EventSystem::Post(
            std::move(event)
        );
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );
}
```

---

# 7. Tại sao cần Win32 Event Producer?

Đây là vấn đề quan trọng nhất.

Bình thường:

```text
Main Thread
    |
    v
SDL_PollEvent()
    |
    v
SDL Event
```

Nhưng khi Windows resize/drag:

```text
Main Thread
    |
    v
WM_ENTERSIZEMOVE
    |
    v
Windows Native Modal Loop
    |
    +--> WM_MOUSEMOVE
    +--> WM_SIZING
    +--> WM_SIZE
    +--> WM_PAINT
    +--> ...
```

Application loop không còn chạy theo cadence bình thường.

Nếu Event chỉ lấy từ:

```text
SDL_PollEvent()
```

thì EventWorker sẽ không nhận được event mới kịp thời.

Do đó:

```text
WndProc
   |
   +--> Native synchronous work
   |
   +--> EventQueue
```

phải tồn tại độc lập với SDL event pump.

---

# 8. WndProc phải cực kỳ nhẹ

Không được:

```text
WndProc
   |
   +--> Runtime
   +--> Player
   +--> MPV
   +--> Render
   +--> RequestRenderAll
```

Thay vào đó:

```text
WndProc
   |
   +--> synchronous native operation nếu bắt buộc
   |
   +--> push event
   |
   +--> return
```

---

# 9. Native Synchronous Work

Một số thao tác KHÔNG thể đưa sang EventWorker.

Ví dụ:

```text
WM_SIZING
```

Nếu cần chỉnh `RECT`:

```cpp
case WM_SIZING:
{
    RECT* rect =
        reinterpret_cast<RECT*>(lParam);

    adjustAspectRatio(*rect);

    EventSystem::Post(
        WindowSizingEvent(...)
    );

    return TRUE;
}
```

Lý do:

`WM_SIZING` cần kết quả ngay trong native message processing.

Do đó:

```text
WndProc
   |
   +--> synchronous native operation
   |
   +--> async EventQueue
```

---

# 10. EventQueue

EventQueue nên là thread-safe.

Ví dụ concept:

```cpp
class EventQueue
{
public:

    void push(Event event);

    bool waitPop(Event& event);

    void stop();

private:

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::queue<Event> m_queue;

    bool m_running = true;
};
```

EventQueue không chứa logic.

Nó chỉ:

```text
push
pop
wait
stop
```

---

# 11. EventWorker

EventWorker chịu trách nhiệm:

```text
EventQueue
    |
    v
waitPop()
    |
    v
EventDispatcher
```

Pseudo-code:

```cpp
void EventWorker::run()
{
    while (running)
    {
        Event event;

        if (!eventQueue.waitPop(event))
            break;

        dispatcher.dispatch(event);
    }
}
```

EventWorker không render.

EventWorker không điều khiển FPS.

EventWorker không gọi MPV render.

---

# 12. EventDispatcher

Dispatcher chỉ định tuyến Event.

Ví dụ:

```text
WindowResized
    |
    +--> WindowRuntime

PlaybackEnded
    |
    +--> PlayerSession

ApplicationQuit
    |
    +--> Runtime
```

Dispatcher không chứa business logic.

---

# 13. Event và Command phải tách biệt

## Event

Mô tả:

> Điều gì đã xảy ra?

Ví dụ:

```text
WindowCreated
WindowDestroyed
WindowResized
WindowMoved
MouseMoved
PlaybackStarted
PlaybackEnded
MPVPropertyChanged
MPVError
```

## Command

Mô tả:

> Hãy làm điều này.

Ví dụ:

```text
Play
Pause
Stop
Seek
SetVolume
CloseWindow
CreateWindow
ResizeWindow
```

Không gom Command và Event thành một abstraction không rõ nghĩa.

---

# 14. Event Classification

Không phải Event nào cũng nên đưa vào queue giống nhau.

Chia thành:

## 14.1 Queued Event

Phải xử lý theo thứ tự:

```text
Play
Pause
Stop
Seek
Close
FileLoaded
EndFile
Error
WindowCreated
WindowDestroyed
```

Pipeline:

```text
Producer
    |
    v
EventQueue
    |
    v
Worker
```

---

# 15. Coalesced Event / State

Một số Event chỉ cần trạng thái mới nhất:

```text
MouseMove
WindowResize
WindowMove
WindowExpose
```

Ví dụ:

```text
800x600
801x600
802x600
803x601
804x602
...
1500x900
```

Không cần xử lý tất cả.

Chỉ cần:

```text
WindowState.latestSize
    =
1500x900
```

---

# 16. Resize State

Ví dụ:

```cpp
struct WindowInteractionState
{
    std::atomic<int> width;
    std::atomic<int> height;

    std::atomic<bool> resizePending;

    std::atomic<bool> moving;
    std::atomic<bool> resizing;
};
```

WndProc:

```text
WM_SIZING
    |
    v
update latest size
    |
    v
resizePending = true
```

EventWorker/WindowRuntime:

```text
resizePending
    |
    v
consume latest size
```

---

# 17. Không spam EventQueue bằng WM_MOUSEMOVE

Không nên:

```text
WM_MOUSEMOVE
WM_MOUSEMOVE
WM_MOUSEMOVE
WM_MOUSEMOVE
...
```

đẩy hàng nghìn Event vào queue.

Thay vào đó:

```text
WindowInteractionState
    |
    +--> latestMouseX
    +--> latestMouseY
```

EventWorker chỉ cần quan tâm trạng thái mới nhất.

---

# 18. Runtime tự quản lý lifecycle

Runtime không nên phụ thuộc Main Thread để update từng frame.

Ví dụ:

```cpp
class Runtime
{
public:

    void start();
    void stop();

    void post(Event event);

private:

    EventQueue m_eventQueue;
    EventWorker m_eventWorker;

    RuntimeState m_state;
};
```

Runtime tự quản:

```text
start
stop
state
events
subsystems
shutdown
```

---

# 19. WindowRuntime tự quản lý

```cpp
class WindowRuntime
{
public:

    void start();
    void stop();

    void handleEvent(
        const WindowEvent& event);

private:

    WindowState m_state;

    RenderScheduler m_renderScheduler;

    UIRenderThread m_renderThread;
};
```

WindowRuntime không cần Main Thread gọi:

```cpp
update();
```

liên tục.

---

# 20. PlayerSession tự quản lý

```cpp
class PlayerSession
{
public:

    void start();
    void stop();

    void play();
    void pause();
    void stopPlayback();
    void seek(double position);

    void post(PlayerCommand command);

private:

    PlaybackState m_state;

    MPVSession m_mpv;
};
```

PlayerSession tự chịu trách nhiệm:

```text
Playback lifecycle
Playback state
MPV interaction
Playback events
```

---

# 21. MPVSession

MPVSession nên có ownership rõ ràng:

```text
MPVSession
    |
    +--> mpv_handle
    +--> mpv_render_context
    +--> MPV command interface
    +--> MPV event processing
    +--> MPV state
```

Không để Main Thread trực tiếp điều khiển mọi thao tác MPV.

---

# 22. MPV Event Pipeline

MPV có event source riêng:

```text
MPV
 |
 | mpv_wait_event()
 v
MPVEventQueue
 |
 v
MPVEventWorker
 |
 v
PlayerSession
```

Không:

```text
MPV
 |
 v
Main Thread
 |
 v
PlayerSession
```

---

# 23. MPV Frame Update phải tách khỏi Logic Event

Đây là điểm rất quan trọng.

Có hai loại:

## Logic Event

```text
MPV_EVENT_START_FILE
MPV_EVENT_END_FILE
MPV_EVENT_PROPERTY_CHANGE
MPV_EVENT_VIDEO_RECONFIG
MPV_EVENT_LOG_MESSAGE
...
```

Đi vào:

```text
MPVEventQueue
    |
    v
MPVEventWorker
    |
    v
PlayerSession
```

## Frame Update

Ví dụ:

```text
mpv_render_context_set_update_callback()
```

Không nhất thiết phải đi qua EventQueue.

Có thể:

```text
MPV render callback
    |
    v
frameAvailable = true
    |
    v
RenderScheduler
```

---

# 24. Không render OpenGL từ MPV callback

MPV callback chỉ được:

```text
signal
state update
notify
```

Không:

```text
mpv callback
    |
    +--> OpenGL
    +--> ImGui
    +--> SwapWindow
```

OpenGL phải được render trên thread sở hữu GL context.

---

# 25. Render Architecture

Render phải độc lập với Main Thread.

```text
MPV Frame
    |
    v
RenderScheduler
    |
    v
UIRenderThread
    |
    +--> MPV render
    +--> ImGui
    +--> OpenGL
    +--> Swap
```

---

# 26. RenderReason

Không dùng duy nhất:

```cpp
RequestRender();
```

Nên có lý do:

```cpp
enum class RenderReason : uint32_t
{
    None        = 0,
    UI          = 1 << 0,
    Video       = 1 << 1,
    Resize      = 1 << 2,
    Animation   = 1 << 3,
    Window      = 1 << 4
};
```

Ví dụ:

```text
MPV frame
    -> Video

Window resize
    -> Resize

ImGui state changed
    -> UI

Animation
    -> Animation
```

---

# 27. RenderScheduler

Mỗi Window nên có scheduler riêng.

```text
WindowRuntime #1
    |
    +--> RenderScheduler #1
    |
    +--> UIRenderThread #1


WindowRuntime #2
    |
    +--> RenderScheduler #2
    |
    +--> UIRenderThread #2
```

Không dùng:

```text
Global RequestRenderAll()
```

làm cơ chế chính.

---

# 28. RenderThread

RenderThread tự quyết định khi nào render.

Ví dụ:

```text
RenderReason != None
        |
        v
RenderFrame()
```

Trong trường hợp video:

```text
frameAvailable
        |
        v
RenderFrame()
```

Trong trường hợp resize:

```text
resizePending
        |
        v
RenderFrame()
```

---

# 29. Continuous Render

Trong native resize:

```text
WM_ENTERSIZEMOVE
    |
    v
WindowState.resizing = true
    |
    v
RenderScheduler
    |
    v
Continuous Render
```

Khi:

```text
WM_EXITSIZEMOVE
```

thì:

```text
resizing = false
```

và quay lại render policy bình thường.

---

# 30. Không dùng WM_TIMER làm kiến trúc chính

`WM_TIMER -> RequestRenderAll()` có thể giữ lại tạm thời để diagnostic.

Nhưng không nên là architecture cuối.

Lý do:

```text
Window 2
    |
    v
WM_TIMER
    |
    v
RequestRenderAll()
    |
    +--> Window 1
    +--> Window 2
    +--> Window 3
```

Window 2 trở thành global render scheduler.

Đây là coupling không tốt.

---

# 31. Multi-window isolation

Khi Window 2 resize:

```text
Window 2
    |
    v
WM_SIZING
    |
    v
WindowRuntime #2
    |
    v
RenderScheduler #2
```

Window 1:

```text
WindowRuntime #1
    |
    v
RenderScheduler #1
    |
    v
UIRenderThread #1
```

Không phụ thuộc Window 2.

---

# 32. Main Thread bị native modal loop vẫn không được làm chết Render

Mục tiêu cuối:

```text
Main Thread
    |
    X
native resize modal loop
```

nhưng:

```text
EventWorker
    |
    +--> Runtime

MPVWorker
    |
    +--> PlayerSession

RenderThread #1
    |
    +--> Window 1

RenderThread #2
    |
    +--> Window 2
```

vẫn chạy độc lập.

---

# 33. Ownership Model

## Application

Sở hữu:

```text
ApplicationRuntime
EventSystem
Global lifecycle
```

## Runtime

Sở hữu:

```text
WindowRuntime
PlayerSession
Application state
```

## WindowRuntime

Sở hữu:

```text
WindowState
RenderScheduler
UIRenderThread
```

## PlayerSession

Sở hữu:

```text
PlaybackState
MPVSession
```

## MPVSession

Sở hữu:

```text
mpv_handle
mpv_render_context
MPV state
MPV event processing
```

---

# 34. Thread Model đề xuất

Không tạo thread cho mọi object.

## Main Thread

```text
SDL_PollEvent
Win32 lifecycle
Native synchronous operations
Application bootstrap/shutdown
```

## EventWorker

```text
EventQueue
EventDispatcher
```

## MPV Event Worker

```text
mpv_wait_event
MPV event processing
```

## UIRenderThread

```text
OpenGL
ImGui
MPV rendering
SwapWindow
```

## Player Worker

Chỉ thêm nếu PlayerSession cần serialized command processing độc lập.

Không tạo nếu MPV command handling hiện tại đã đáp ứng tốt.

---

# 35. Thread Ownership

Phải xác định rõ:

```text
Main Thread
    |
    +--> SDL / Window lifecycle

EventWorker
    |
    +--> Event dispatch

MPV Worker
    |
    +--> MPV event processing

UIRenderThread
    |
    +--> OpenGL context
    +--> ImGui
    +--> mpv_render_context_render()
```

Không để nhiều thread cùng thao tác trực tiếp lên:

```text
SDL_Window
OpenGL context
ImGui context
mpv_render_context
```

nếu không có synchronization/ownership rõ ràng.

---

# 36. Data Ownership

State nên phân loại:

```text
WindowState
PlaybackState
RenderState
InputState
```

Không dùng một object khổng lồ:

```cpp
RuntimeState
```

cho mọi thread đọc/ghi tùy ý.

---

# 37. Data Flow chuẩn

## Window Resize

```text
Windows
    |
    v
WndProc
    |
    +--> synchronous native work
    |
    +--> ResizeEvent
             |
             v
        EventQueue
             |
             v
        EventWorker
             |
             v
       WindowRuntime
             |
             v
        WindowState
             |
             v
       RenderScheduler
             |
             v
        RenderThread
```

---

# 38. Video Playback

```text
PlayerSession
    |
    v
MPVSession
    |
    v
MPV
    |
    +----------------------+
    |                      |
    v                      v
MPV Logic Event        Frame Update
    |                      |
    v                      v
MPVEventQueue         RenderScheduler
    |                      |
    v                      v
PlayerSession         UIRenderThread
                           |
                           v
                      mpv_render()
```

---

# 39. Input

```text
SDL / Win32
    |
    v
EventQueue
    |
    v
EventWorker
    |
    v
InputState
    |
    v
UI / Player
```

Không:

```text
InputEvent
    |
    v
RequestRenderAll()
```

Chỉ render nếu state/UI thực sự thay đổi.

---

# 40. Shutdown

Shutdown phải có thứ tự rõ ràng.

```text
Application
    |
    v
Stop accepting new events
    |
    v
Stop Event Producers
    |
    v
Stop EventWorker
    |
    v
Stop PlayerSessions
    |
    v
Stop MPV
    |
    v
Stop RenderThreads
    |
    v
Destroy Windows / GL
    |
    v
Shutdown SDL
```

Không được:

```text
SDL_Quit()
    |
    v
EventWorker vẫn chạy
```

hoặc:

```text
OpenGL context destroyed
    |
    v
RenderThread vẫn render
```

---

# 41. Error Handling

EventWorker không được chết toàn bộ thread chỉ vì một Handler lỗi.

Ví dụ:

```cpp
try
{
    dispatcher.dispatch(event);
}
catch (...)
{
    logException();
}
```

Sau đó worker tiếp tục.

Tương tự MPV EventWorker.

---

# 42. Logging / Diagnostic

Nên có logging theo pipeline:

```text
[SDLProducer]
[Win32Producer]
[EventQueue]
[EventWorker]
[Dispatcher]
[WindowRuntime]
[PlayerSession]
[MPVWorker]
[RenderScheduler]
[RenderThread]
```

Đặc biệt log:

```text
WM_ENTERSIZEMOVE
WM_SIZING
WM_SIZE
WM_EXITSIZEMOVE
```

và:

```text
MPV_EVENT_RENDER_UPDATE
```

để xác định chính xác thread nào đang phát sinh event.

---

# 43. Diagnostic cần kiểm tra

Khi resize Window 2:

```text
Expected:

MainThread
    -> WM_ENTERSIZEMOVE

Win32Producer
    -> ResizeEvent

EventWorker
    -> WindowRuntime #2

RenderThread #2
    -> render

RenderThread #1
    -> render
```

Không được xuất hiện:

```text
Window 2
    |
    v
MainThread
    |
    v
RequestRenderAll
    |
    v
Window 1
```

---

# 44. Migration Plan

Không refactor toàn bộ cùng lúc.

## Phase 0 — Baseline

Giữ code hiện tại.

Xác nhận:

```text
Resize Window 2
    |
    v
Window 1 bị ảnh hưởng
```

và:

```text
RequestRenderAll()
```

có thể làm Window 1 render lại.

---

# 45. Phase 1 — Tách Event Producer

Tạo:

```text
EventSystem
EventQueue
EventDispatcher
EventWorker
```

Chưa thay đổi MPV.

SDL:

```text
SDL_PollEvent
    |
    v
EventQueue
```

---

# 46. Phase 2 — Native Win32 Producer

Đưa các event quan trọng từ WndProc vào EventQueue:

```text
WM_ENTERSIZEMOVE
WM_EXITSIZEMOVE
WM_SIZING
WM_SIZE
WM_MOVE
```

Giữ synchronous native handling trong WndProc.

---

# 47. Phase 3 — WindowRuntime tự quản lý

WindowRuntime nhận:

```text
WindowEvent
```

và tự cập nhật:

```text
WindowState
ResizeState
InteractionState
```

Không để Main Thread gọi:

```text
WindowRuntime.update()
```

liên tục.

---

# 48. Phase 4 — Tách RenderScheduler

Mỗi Window:

```text
WindowRuntime
    |
    +--> RenderScheduler
    |
    +--> UIRenderThread
```

Loại bỏ dần:

```text
Main Thread
    |
    v
RequestRenderAll()
```

---

# 49. Phase 5 — MPV Event Isolation

Tách:

```text
MPVEventQueue
MPVEventWorker
```

MPV logic event không còn cần Main Thread.

---

# 50. Phase 6 — MPV Frame Update Isolation

`MPV_EVENT_RENDER_UPDATE` hoặc update callback:

```text
MPV
    |
    v
frameAvailable
    |
    v
RenderScheduler
```

Không:

```text
MPV
    |
    v
Main Thread
    |
    v
RequestRender
```

---

# 51. Phase 7 — RenderReason

Thay:

```cpp
RequestRender();
```

bằng:

```cpp
RequestRender(RenderReason::Video);
RequestRender(RenderReason::Resize);
RequestRender(RenderReason::UI);
```

---

# 52. Phase 8 — Continuous Render During Resize

```text
WM_ENTERSIZEMOVE
    |
    v
resizing = true
    |
    v
Continuous Render
```

và:

```text
WM_EXITSIZEMOVE
    |
    v
resizing = false
    |
    v
Normal Render Policy
```

---

# 53. Phase 9 — Remove WM_TIMER Dependency

Sau khi RenderScheduler ổn định:

```text
WM_TIMER
    |
    X
RequestRenderAll()
```

có thể loại bỏ.

Chỉ giữ nếu có lý do cụ thể.

---

# 54. Phase 10 — Performance Optimization

Sau khi correctness ổn định:

```text
Static Window
    -> OnDemand

Video Window
    -> Frame Driven

Resize
    -> Continuous

Animation
    -> Requested FPS
```

Không render tất cả window ở 60 FPS.

---

# 55. Acceptance Criteria

Refactor chỉ được coi là thành công khi:

## Test 1

Window 1 phát video bình thường.

Window 2 resize liên tục.

Kết quả:

```text
Window 1 vẫn render
Window 2 vẫn resize/render
```

Không flicker bất thường.

---

## Test 2

Window 2 drag.

Window 1 vẫn:

```text
video playback
audio playback
UI update
render
```

---

## Test 3

Window 2 resize cực nhanh.

Không:

```text
EventQueue explosion
```

---

## Test 4

Window 2 resize nhưng Window 1 static.

Window 1 không bị render 60 FPS vô lý.

---

## Test 5

MPV frame update khi Main Thread đang native resize.

Render Window 1 vẫn hoạt động.

---

## Test 6

Close Window 2 trong lúc Window 1 đang phát.

Window 1 không crash.

---

## Test 7

Stop MPV trong lúc resize.

Không deadlock.

---

## Test 8

Shutdown application trong lúc:

```text
resize
drag
MPV playback
render
EventWorker
```

Không:

```text
use-after-free
deadlock
crash
render vào context đã destroy
```

---

# 56. Các vấn đề cần đặc biệt audit

## Critical

### C1

Main Thread đang là dependency của RenderThread.

### C2

MPV callback đang gọi trực tiếp Main Thread.

### C3

MPV callback đang gọi OpenGL từ sai thread.

### C4

WndProc đang thực hiện business logic.

### C5

`RequestRenderAll()` đang được dùng làm global heartbeat.

### C6

Event queue không có shutdown protocol.

### C7

Window destroy trong khi RenderThread vẫn chạy.

### C8

MPV destroy trong khi MPV callback vẫn có thể chạy.

---

# 57. Các vấn đề concurrency cần kiểm tra

Audit:

```text
WindowRuntime lifetime
PlayerSession lifetime
MPVSession lifetime
RenderThread lifetime
EventWorker lifetime
EventQueue lifetime
OpenGL context lifetime
SDL_Window lifetime
```

Đặc biệt:

```text
EventWorker
    |
    v
WindowRuntime*
```

phải đảm bảo pointer vẫn hợp lệ.

Không được:

```text
Window destroyed
    |
    v
EventWorker dispatch event
    |
    v
dangling pointer
```

---

# 58. Khuyến nghị về Event Target

Event nên chứa stable identifier:

```cpp
struct Event
{
    EventType type;

    RuntimeId runtimeId;
    WindowId windowId;
    PlayerId playerId;

    EventPayload payload;
};
```

Không nên Event giữ raw pointer:

```cpp
WindowRuntime* window;
```

vì lifetime có thể thay đổi.

Dispatcher tìm object thông qua manager/registry.

---

# 59. Khuyến nghị về Render Request

Render request cũng nên dùng stable ID:

```cpp
RenderRequest
{
    WindowId windowId;
    RenderReason reason;
};
```

Scheduler tìm WindowRuntime tương ứng.

---

# 60. Kiến trúc mục tiêu cuối cùng

```text
                                      APPLICATION
                                           |
                    +----------------------+----------------------+
                    |                                             |
               MAIN THREAD                                    SERVICES
                    |                                             |
          +---------+---------+                         +---------+---------+
          |                   |                         |                   |
     SDL Producer       Win32 Producer             MPV Worker        Render System
          |                   |                         |                   |
          +---------+---------+                         |                   |
                    |                                   |                   |
                    v                                   v                   |
             Concurrent Event Queue               PlayerSession             |
                    |                                   |                   |
                    v                                   v                   |
              Event Worker                            MPV                   |
                    |                                   |                   |
                    v                                   +--------->----------+
              Dispatcher                                        Frame Signal
                    |
          +---------+---------+
          |         |         |
          v         v         v
       Runtime   Window    Player
                    |         |
                    |         v
                    |      MPVSession
                    |
                    v
              RenderScheduler
                    |
             +------+------+
             |             |
             v             v
           Window 1      Window 2
             |             |
             v             v
        UIRenderThread UIRenderThread
             |             |
             v             v
           OpenGL        OpenGL
             |             |
             v             v
           Swap          Swap
```

---

# 61. Nguyên tắc cuối cùng

Kiến trúc mới phải tuân thủ các nguyên tắc:

```text
1. Main Thread != Global Heartbeat

2. SDL_PollEvent() chỉ là Event Producer.

3. WndProc là Native Event Producer.

4. WndProc chỉ làm synchronous native work khi bắt buộc.

5. EventWorker chịu trách nhiệm dispatch Event.

6. Runtime tự quản lý lifecycle của Runtime.

7. WindowRuntime tự quản lý lifecycle của Window.

8. PlayerSession tự quản lý playback lifecycle.

9. MPVSession tự quản lý MPV lifecycle.

10. MPV Logic Event và MPV Frame Update phải tách.

11. MPV callback không render OpenGL trực tiếp.

12. RenderThread sở hữu OpenGL rendering.

13. RenderScheduler không phụ thuộc Main Thread.

14. Mỗi Window có render scheduling độc lập.

15. Resize/MouseMove cần coalescing.

16. Event và Command phải tách.

17. Stable ID được ưu tiên hơn raw pointer trong async Event.

18. Shutdown phải có ownership/lifetime rõ ràng.

19. WM_TIMER không phải global render architecture.

20. Không tối ưu performance trước khi giải quyết ownership và correctness.
```

---

# 62. Kết luận

Vấn đề hiện tại không nên được giải quyết bằng cách tiếp tục thêm:

```text
WM_TIMER
RequestRender()
RequestRenderAll()
SDL_AddEventWatch()
```

vì những cách đó chỉ **bù đắp cho việc Main Thread đang đóng quá nhiều vai trò**.

Hướng refactor nên là:

```text
              SDL
               |
               v
        SDL Event Producer
               |
               |
Win32 -------> Event Queue <------- MPV
   |               |                 |
   |               v                 |
   |         Event Worker            |
   |               |                 |
   |        +------+------+          |
   |        |             |          |
   |        v             v          v
   |     Runtime       PlayerSession
   |                      |
   |                      v
   |                   MPVSession
   |                      |
   |                      |
   +----------------------+----------+
                          |
                          v
                   RenderScheduler
                          |
                    +-----+-----+
                    |           |
                   W1           W2
                    |           |
                   RT1         RT2
                    |           |
                   GL          GL
```

Điểm quan trọng nhất là **EventWorker không phải giải pháp để thay thế `SDL_PollEvent()`**.

`SDL_PollEvent()` vẫn có thể nằm trên Main Thread.

Giải pháp là:

```text
SDL_PollEvent() = SDL Event Producer

WndProc = Native Event Producer

                ↓

          Common EventQueue

                ↓

           EventWorker
```

Nhờ vậy, khi Windows vào native resize/drag loop và Main Thread bị giữ trong đó, **Win32 vẫn có thể đưa Event vào hệ thống**, trong khi các RenderThread/MPV subsystem không cần chờ Main Thread.

Đây là nền tảng tôi khuyên nên chốt trước khi bắt đầu sửa code MPV và Render.