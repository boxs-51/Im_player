# Phân tích toàn diện hệ thống WindowRuntime, đồng bộ State và thiết kế WindowRenderSnapshot

## 1. Kết luận tổng quan

Sau khi phân tích source `src(9).zip`, vấn đề nhấp nháy hiện tại **không còn đơn thuần là thiếu `mutex`**.

Kiến trúc hiện tại đã có `stateMutex`, nhưng mutex đang được dùng theo kiểu:

```text
lock state
    đọc / ghi một vài field
unlock
    tiếp tục render
    đọc state/property lần nữa
    render
    đọc state/property lần nữa
unlock
```

Điều này **không tạo ra một frame nhất quán**.

Ví dụ:

```text
Frame N bắt đầu
│
├── đọc width  = 1280
├── đọc height = 720
│
├──────────── WM_SIZE ───────────────┐
│                                    │
│                         width = 1366
│                         height = 768
│                                    │
├── đọc layout mới                   │
│
├── render video với 1366x768        │
│
├──────────── WM_SIZE ───────────────┐
│                                    │
│                         width = 1440
│                         height = 900
│
├── render UI với 1440x900           │
│
└── EndFrame
```

Do đó:

> **Mutex hiện tại bảo vệ từng lần truy cập state, nhưng không bảo vệ tính nhất quán của toàn bộ frame.**

Đây chính là nguyên nhân cốt lõi giải thích hiện tượng:

- một frame bắt đầu bằng kích thước A;
- giữa frame state chuyển sang B;
- một phần renderer sử dụng A;
- phần khác sử dụng B;
- backend/FBO/video sử dụng C;
- frame tiếp theo lại sử dụng state khác.

Khi resize liên tục, vấn đề biểu hiện thành **flickering / tearing / jumping / viewport không ổn định**.

Giải pháp kiến trúc đúng là:

```text
                ┌──────────────────────────┐
                │     Native / Main Thread │
                │                          │
                │ WindowProc / SDL Events  │
                └────────────┬─────────────┘
                             │
                             │ mutate
                             ▼
                ┌──────────────────────────┐
                │   Authoritative State    │
                │      WindowState         │
                │     + stateMutex         │
                └────────────┬─────────────┘
                             │
                       capture once
                             │
                             ▼
                ┌──────────────────────────┐
                │   WindowRenderSnapshot   │
                │                          │
                │ immutable for one frame  │
                └────────────┬─────────────┘
                             │
                             ▼
                ┌──────────────────────────┐
                │      UIRenderThread      │
                │                          │
                │ BeginFrame               │
                │ Render(snapshot)         │
                │ EndFrame                 │
                └──────────────────────────┘
```

Đây là thay đổi quan trọng nhất cần thực hiện.

---

# 2. Kiến trúc WindowRuntime hiện tại

`WindowRuntime` hiện đang chứa:

```cpp
class WindowRuntime {
public:
    mutable std::mutex stateMutex;
    mutable std::mutex frameSyncMutex;

    WindowInfo info;
    WindowState state;
    WindowStyle style;
    PropertyBag properties;

    WindowRelation relation;
    WindowResource resource;

    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;

    std::unique_ptr<FrameTimer> windowloop;
};
```

Về mặt ownership, cấu trúc này khá tốt:

```text
WindowRuntime
│
├── WindowInfo
├── WindowState
├── WindowStyle
├── PropertyBag
│
├── WindowRelation
├── WindowResource
│   ├── SDL_Window
│   ├── HWND
│   ├── ImGuiContext
│   ├── GraphicsBackend
│   └── UIRenderThread
│
├── WindowRenderer
├── WindowController
└── FrameTimer
```

Nhưng vấn đề là `WindowRuntime` hiện đang đóng vai trò quá nhiều thứ cùng lúc:

```text
WindowRuntime
│
├── authoritative state
├── render state
├── event state
├── derived layout
├── command state
├── resource ownership
├── inter-thread synchronization
└── renderer coordination
```

Điều này khiến các thread có xu hướng truy cập trực tiếp vào object.

Đặc biệt:

```cpp
runtime->state
runtime->properties
runtime->resource
```

được truy cập ở nhiều nơi.

---

# 3. Các thread hiện tại

Kiến trúc hiện tại thực tế có ít nhất các execution context sau:

```text
1. Main Thread
   │
   ├── SDL_PollEvent
   ├── HandleWindowRuntimeEvent
   ├── WindowManager
   ├── Controller
   └── RequestRender

2. Windows message thread
   │
   └── MultiWindowWndProc
       ├── WM_SIZE
       ├── WM_MOVE
       ├── WM_SIZING
       ├── WM_MOVING
       ├── WM_ACTIVATE
       ├── WM_SETFOCUS
       └── ...

3. UIRenderThread
   │
   └── ImGui + UI rendering

4. PlayBackRenderThread
   │
   └── MPV → FBO rendering

5. MPV / observer / player-related threads
   │
   └── PlaybackStateSystem
```

Điểm nguy hiểm là `WindowState` không có một thread owner rõ ràng.

Nó hiện đang được thay đổi bởi:

```text
WindowProcHook
WindowController
UpdateWindowState
main1.cpp
các UI logic
```

Trong khi render thread lại đọc trực tiếp nó.

---

# 4. WindowProcHook hiện đang là State Writer

`MultiWindowWndProc()` hiện cập nhật trực tiếp:

```cpp
runtime->state.geometry
runtime->state.display
runtime->state.input
runtime->state.runtime
```

Ví dụ:

```cpp
case WM_SIZE:
{
    std::lock_guard<std::mutex> lock(runtime->stateMutex);

    runtime->state.geometry.width = newWidth;
    runtime->state.geometry.height = newHeight;
}
```

Điều này bản thân nó **không sai**.

Sai ở tầng sau.

Sau khi unlock:

```cpp
UpdateWindowState(runtime);
```

và `UpdateWindowState()` lại tạo ra:

```cpp
WindowLayout localLayout;
```

rồi:

```cpp
runtime->properties.Set<WindowLayout>("Layout", localLayout);
```

Renderer sau đó lấy:

```cpp
const auto* layout =
    runtime->properties.GetPtr<WindowLayout>("Layout");
```

và giữ pointer đó trong quá trình render.

Đây là một vấn đề nghiêm trọng.

---

# 5. Lỗi lớn trong PropertyBag

`PropertyBag` có mutex:

```cpp
mutable std::recursive_mutex m_mutex;
```

nhưng:

```cpp
template<typename T>
T* GetPtr(std::string_view key)
{
    auto it = properties.find(std::string(key));
    ...
}
```

**không lock mutex.**

Tương tự:

```cpp
const T* GetPtr(...)
```

cũng không lock.

Quan trọng hơn nữa, kể cả sửa thành:

```cpp
std::lock_guard lock(m_mutex);
```

thì vẫn chưa đủ.

Ví dụ:

```cpp
const auto* layout =
    runtime->properties.GetPtr<WindowLayout>("Layout");
```

Mutex chỉ tồn tại trong `GetPtr()`.

Sau khi hàm return:

```text
GetPtr()
   │
   ├── lock
   ├── lấy pointer
   └── unlock
          │
          ▼
     pointer vẫn được giữ
```

Trong lúc renderer đang dùng pointer:

```cpp
layout->ClientSize
layout->ClientArea
layout->WinW
layout->WinH
```

thread khác có thể:

```cpp
properties.Set<WindowLayout>("Layout", newLayout);
```

Do đó pointer/reference có thể không còn đại diện cho một snapshot ổn định.

### Kết luận

Không nên dùng:

```cpp
PropertyBag::GetPtr()
PropertyBag::GetRef()
```

cho dữ liệu render cross-thread.

Render phải lấy **copy**.

---

# 6. Lỗi lớn nhất trong UIRenderThread

Hiện tại:

```cpp
{
    std::lock_guard<std::mutex> stateLock(
        m_ownerRuntime->stateMutex
    );

    if (!m_ownerRuntime->state.display.isVisible)
        continue;
}
```

Đây là điểm rất quan trọng.

Mutex được lock ở đây nhưng ngay sau đó unlock.

Sau đó:

```cpp
renderer->RenderUI(currentWindow);
```

`RenderUI()` lại đọc:

```cpp
runtime->state
runtime->properties
```

rất nhiều lần.

Ví dụ:

```cpp
const auto* layout =
    runtime->properties.GetPtr<WindowLayout>("Layout");
```

sau đó:

```cpp
bool isFullscreen = false;
bool isMaximized = false;

{
    std::lock_guard<std::mutex> lock(runtime->stateMutex);

    isFullscreen =
        runtime->state.display.isFullscreen;

    isMaximized =
        runtime->state.display.isMaximized;
}
```

rồi:

```cpp
PlaybackState state =
    player_state->GetPlaybackState();
```

rồi:

```cpp
runtime->properties.GetValue<bool>(
    "RenderVideoFlag",
    true
);
```

rồi:

```cpp
runtime->properties.Set<bool>(
    "RenderVideoFlag",
    false
);
```

Tức là một `RenderUI()` không có một state duy nhất.

Nó đang tạo ra:

```text
RenderUI()
│
├── state snapshot A
│
├── layout snapshot B
│
├── property snapshot C
│
├── playback snapshot D
│
├── state snapshot E
│
└── property snapshot F
```

Đây chính xác là mô hình gây lỗi.

---

# 7. Frame hiện tại không có transaction boundary

Một frame render đúng phải có semantic:

```text
Frame N
│
├── Capture State
│
├── BeginFrame
│
├── Render using SAME State
│
├── EndFrame
│
└── Present
```

Trong source hiện tại:

```text
Frame N
│
├── đọc state
├── đọc property
├── đọc state
├── đọc layout
├── đọc playback
├── đọc property
├── render video
├── đọc state
├── render controls
└── present
```

Không có:

```cpp
FrameState snapshot;
```

Do đó frame không có tính atomic.

---

# 8. Tại sao thêm lock từ FrameStart → FrameEnd chỉ giảm flicker nhưng không giải quyết triệt để?

Nếu làm:

```cpp
std::lock_guard lock(runtime->stateMutex);

BeginFrame();

renderer->RenderUI(runtime);

EndFrame();
```

thì chỉ giải quyết một phần:

```text
WindowState
   │
   ├── locked
   ├── Render
   └── unlock
```

Nhưng vẫn còn các vấn đề:

1. `PropertyBag` có mutex riêng.
2. `GetPtr()` trả pointer ra ngoài lock.
3. PlaybackState có synchronization riêng.
4. MPV render thread có state riêng.
5. backend có state riêng.
6. `RequestResize()` có mutex riêng.
7. các callback có thể cập nhật resource/layout khác.
8. giữ `stateMutex` xuyên suốt GPU rendering là thiết kế không tốt.

Đặc biệt:

```text
WM thread
     │
     │ muốn đổi state
     ▼
stateMutex
     │
     X bị block trong toàn bộ GPU render
```

Khi resize liên tục, điều này tạo contention rất lớn.

Vì vậy:

> Không nên giữ mutex của authoritative state trong toàn bộ RenderFrame.

Thay vào đó:

> Lock chỉ để COPY state → snapshot, sau đó unlock.

---

# 9. Thiết kế WindowRenderSnapshot

Nên tạo:

```cpp
struct WindowRenderSnapshot
{
    uint64_t version = 0;

    WindowId windowId = 0;

    WindowGeometryState geometry;
    WindowDisplayState display;
    WindowInputState input;

    WindowLayout layout;

    WindowStyle style;

    bool visible = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;

    bool renderingEnabled = true;
    bool closedPending = false;

    bool isDirty = false;

    bool renderVideo = false;
};
```

Tuy nhiên không nên copy toàn bộ `WindowInputState` nếu renderer không cần tất cả.

Tốt hơn:

```cpp
struct WindowRenderSnapshot
{
    uint64_t version = 0;

    WindowId windowId = 0;

    struct Geometry {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    } geometry;

    struct Display {
        bool visible = false;
        bool shown = false;
        bool minimized = false;
        bool maximized = false;
        bool fullscreen = false;

        UINT dpiX = 96;
        UINT dpiY = 96;
        float dpiScale = 1.0f;
    } display;

    struct Input {
        bool focused = false;
        bool active = false;
        bool moving = false;
        bool resizing = false;
        bool dragging = false;
    } input;

    WindowLayout layout;

    bool renderingEnabled = true;
    bool renderVideo = false;
    bool dirty = false;
};
```

Đây mới là dữ liệu mà UI renderer thực sự cần.

---

# 10. Snapshot phải là immutable trong frame

Nguyên tắc:

```cpp
WindowRenderSnapshot snapshot;
```

được tạo tại đầu frame.

Sau đó:

```cpp
renderer->RenderUI(snapshot);
```

Không được truyền:

```cpp
renderer->RenderUI(runtime);
```

nếu renderer có khả năng tiếp tục đọc state từ runtime.

Kiến trúc đúng:

```cpp
void UIRenderThread::Run()
{
    ...

    WindowRenderSnapshot snapshot =
        m_ownerRuntime->CaptureRenderSnapshot();

    BeginFrame();

    renderer->RenderUI(
        m_ownerRuntime,
        snapshot
    );

    EndFrame();
}
```

Hoặc tốt hơn nữa:

```cpp
renderer->RenderUI(snapshot);
```

Renderer không cần biết `WindowRuntime`.

---

# 11. CaptureRenderSnapshot()

`WindowRuntime` nên có:

```cpp
WindowRenderSnapshot CaptureRenderSnapshot() const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    WindowRenderSnapshot snapshot;

    snapshot.windowId = info.id;

    snapshot.geometry.x =
        state.geometry.x;

    snapshot.geometry.y =
        state.geometry.y;

    snapshot.geometry.width =
        state.geometry.width;

    snapshot.geometry.height =
        state.geometry.height;

    snapshot.display.visible =
        state.display.isVisible;

    snapshot.display.shown =
        state.display.isShown;

    snapshot.display.minimized =
        state.display.isMinimized;

    snapshot.display.maximized =
        state.display.isMaximized;

    snapshot.display.fullscreen =
        state.display.isFullscreen;

    snapshot.display.dpiX =
        state.display.dpiX;

    snapshot.display.dpiY =
        state.display.dpiY;

    snapshot.display.dpiScale =
        state.display.dpiScale;

    snapshot.input.focused =
        state.input.hasFocus;

    snapshot.input.active =
        state.input.isActive;

    snapshot.input.moving =
        state.input.moving;

    snapshot.input.resizing =
        state.input.resizing;

    snapshot.input.dragging =
        state.input.dragging;

    snapshot.renderingEnabled =
        state.runtime.renderingEnabled;

    snapshot.dirty =
        state.runtime.is_dirty;

    return snapshot;
}
```

Nhưng `layout` không nên lấy từ `PropertyBag`.

Nó phải được tạo hoặc lưu trực tiếp trong render state.

---

# 12. Không nên dùng PropertyBag làm Render State

Hiện tại:

```cpp
runtime->properties.Set<WindowLayout>(
    "Layout",
    localLayout
);
```

nên bỏ khỏi render path.

Thay bằng:

```cpp
WindowLayout layout;
```

trong một state chuyên dụng.

Ví dụ:

```cpp
struct WindowDerivedState
{
    WindowLayout layout;
};
```

và:

```cpp
class WindowRuntime
{
public:
    mutable std::mutex stateMutex;

    WindowState state;
    WindowDerivedState derived;
};
```

Sau đó:

```cpp
{
    std::lock_guard lock(runtime->stateMutex);

    runtime->derived.layout = localLayout;
}
```

Khi capture:

```cpp
snapshot.layout = derived.layout;
```

Như vậy:

```text
WindowState
+
WindowDerivedState
        │
        │ single lock
        ▼
WindowRenderSnapshot
```

---

# 13. PropertyBag nên trở thành Command/Metadata Store

`PropertyBag` hiện đang được dùng cho nhiều mục đích:

```text
OldWndProc
Layout
RenderVideoFlag
ShowUiVideo
TriggerToggleFullscreen
IsSecondaryMpvOutput
...
```

Đây là một dấu hiệu kiến trúc không tốt.

Nên phân loại:

### Static metadata

```text
OldWndProc
IsSecondaryMpvOutput
```

có thể giữ trong PropertyBag.

### Runtime state

```text
Layout
ShowUiVideo
RenderVideoFlag
```

nên đưa vào typed state.

### Commands

```text
TriggerToggleFullscreen
```

nên trở thành command/event:

```cpp
WindowCommand::ToggleFullscreen
```

Không nên dùng:

```cpp
properties.Set<bool>(
    "TriggerToggleFullscreen",
    true
);
```

---

# 14. RenderVideoFlag cũng đang có vấn đề

Hiện tại event:

```cpp
runtime->properties.Set<bool>(
    "RenderVideoFlag",
    true
);
```

Renderer:

```cpp
bool flagRenderVideo =
    runtime->properties.GetValue<bool>(
        "RenderVideoFlag",
        true
    );
```

sau đó:

```cpp
runtime->properties.Set<bool>(
    "RenderVideoFlag",
    false
);
```

Đây thực chất là một mailbox.

Nó nên được thay bằng:

```cpp
struct WindowRenderRequests
{
    bool renderVideo = false;
};
```

hoặc atomic:

```cpp
std::atomic<bool> renderVideoRequested{false};
```

Nếu chỉ cần cờ coalescing:

```cpp
renderVideoRequested.store(
    true,
    std::memory_order_release
);
```

render thread:

```cpp
bool renderVideo =
    renderVideoRequested.exchange(
        false,
        std::memory_order_acq_rel
    );
```

Nhưng nếu muốn **snapshot hoàn toàn deterministic**, tốt hơn là capture request vào render snapshot.

---

# 15. WindowState nên có version

Đây là phần rất quan trọng để debug.

Thêm:

```cpp
uint64_t stateVersion = 0;
```

Mỗi mutation:

```cpp
++stateVersion;
```

Ví dụ:

```cpp
{
    std::lock_guard lock(runtime->stateMutex);

    runtime->state.geometry.width = newWidth;
    runtime->state.geometry.height = newHeight;

    ++runtime->stateVersion;
}
```

Snapshot:

```cpp
snapshot.version =
    runtime->stateVersion;
```

Log:

```text
[UIRender]
frame=10542
snapshotVersion=883
size=1280x720
```

Nếu Windows resize:

```text
[WndProc]
stateVersion=884
size=1366x768
```

Frame hiện tại vẫn:

```text
frame=10542
snapshotVersion=883
size=1280x720
```

Frame tiếp theo:

```text
frame=10543
snapshotVersion=884
size=1366x768
```

Đây chính là behavior mong muốn.

---

# 16. Quan trọng: State có thể thay đổi giữa frame

Điều này **không phải bug**.

Ví dụ:

```text
Frame 100
snapshot = 1280x720

       WM_SIZE
       ↓
live state = 1366x768

Frame 100 vẫn render:
1280x720

Frame 101:
snapshot = 1366x768
```

Đây là hoàn toàn đúng.

Không cần cố làm cho frame đang render "nhìn thấy" state mới.

Nguyên tắc:

> Live State có thể thay đổi bất cứ lúc nào. Render Snapshot thì không.

---

# 17. Resize phải được xử lý theo cùng snapshot

Hiện tại:

```cpp
RequestResize(
    localLayout.ClientArea.w,
    localLayout.ClientArea.h
);
```

sau đó render thread có:

```cpp
m_needsResize
m_newWidth
m_newHeight
```

Điều này tạo ra một state pipeline khác với `WindowState`.

Hiện tại có:

```text
WindowState width
        │
        ▼
UpdateWindowState
        │
        ├── PropertyBag Layout
        │
        └── RequestResize
                │
                ▼
         UIRenderThread state
```

Trong khi render:

```text
WindowState
PropertyBag
UIRenderThread resize state
GraphicsBackend
MPV render state
```

có thể lệch nhau.

Nên chuyển thành:

```text
Live WindowState
       │
       │ Capture
       ▼
WindowRenderSnapshot
       │
       ├── layout.ClientSize
       │
       ├── visible
       │
       ├── fullscreen
       │
       └── version
       │
       ▼
UIRenderThread
       │
       ├── Resize backend theo snapshot
       │
       └── Render theo chính snapshot
```

---

# 18. UIRenderThread nên được thiết kế lại

Hiện tại:

```cpp
RequestRender()
```

chỉ set:

```cpp
m_needsRender = true;
```

Điều này vẫn có thể giữ.

Nhưng vòng render nên thành:

```cpp
while (m_running)
{
    WaitForRenderRequest();

    WindowRenderSnapshot snapshot =
        m_ownerRuntime->CaptureRenderSnapshot();

    if (!snapshot.display.visible)
        continue;

    BeginFrame(snapshot);

    Render(snapshot);

    EndFrame(snapshot);
}
```

Không:

```cpp
lock(stateMutex)
render
unlock
```

---

# 19. Render loop mới

Pseudo-code:

```cpp
void UIRenderThread::Run()
{
    MakeCurrent(...);

    while (m_running)
    {
        WaitForRenderRequest();

        if (!m_running)
            break;

        WindowRenderSnapshot snapshot =
            m_ownerRuntime->CaptureRenderSnapshot();

        if (!snapshot.display.visible)
            continue;

        if (!snapshot.display.minimized &&
            snapshot.layout.ClientArea.w > 0 &&
            snapshot.layout.ClientArea.h > 0)
        {
            m_graphicsBackend->Resize(
                snapshot.layout.ClientArea.w,
                snapshot.layout.ClientArea.h
            );
        }

        BeginFrame();

        if (m_ownerRuntime->renderer)
        {
            m_ownerRuntime->renderer->RenderUI(snapshot);
        }

        EndFrame();

        SwapWindow();
    }
}
```

Quan trọng:

```text
Capture
   ↓
snapshot
   ↓
unlock
   ↓
GPU rendering
```

---

# 20. WindowRenderer interface nên đổi

Hiện tại:

```cpp
virtual void RenderUI(WindowRuntime* runtime) = 0;
```

nên chuyển thành:

```cpp
virtual void RenderUI(
    const WindowRenderSnapshot& snapshot
) = 0;
```

Nếu renderer cần resource:

```cpp
virtual void RenderUI(
    const WindowRenderSnapshot& snapshot,
    WindowRenderResources& resources
) = 0;
```

Không nên cho renderer tùy ý quay lại:

```cpp
runtime->state
runtime->properties
```

vì như vậy snapshot architecture sẽ bị phá.

---

# 21. MainWindowRenderer mới

Hiện tại:

```cpp
void MainWindowRenderer::RenderUI(
    WindowRuntime* runtime
)
```

nên trở thành:

```cpp
void MainWindowRenderer::RenderUI(
    const WindowRenderSnapshot& snapshot
)
```

Sau đó:

```cpp
const auto& layout =
    snapshot.layout;
```

và:

```cpp
bool isFullscreen =
    snapshot.display.fullscreen;

bool isMaximized =
    snapshot.display.maximized;
```

Thay vì:

```cpp
runtime->state...
```

---

# 22. Những dữ liệu không được phép đọc trực tiếp trong RenderUI

Sau khi áp dụng snapshot architecture, cấm:

```cpp
runtime->state
```

trong renderer.

Cấm:

```cpp
runtime->properties.GetPtr(...)
```

Cấm:

```cpp
runtime->properties.GetValue(...)
```

cho render state.

Cấm:

```cpp
runtime->stateMutex
```

trong renderer.

Renderer chỉ được nhận:

```text
WindowRenderSnapshot
```

và các immutable resources.

---

# 23. PlaybackState cũng cần snapshot boundary

Hiện tại:

```cpp
PlaybackState state =
    player_state->GetPlaybackState();
```

Nếu `PlaybackStateSystem` đã có cơ chế `ReadPlayback()`, nên dùng một API snapshot.

Ví dụ:

```cpp
PlaybackRenderSnapshot playback =
    player_state->CaptureRenderSnapshot();
```

Sau đó:

```cpp
WindowRenderSnapshot
        +
PlaybackRenderSnapshot
        │
        ▼
        Render
```

Không nên:

```text
Window snapshot
       +
Playback state read #1

       ...

Playback state read #2
```

Vì playback cũng có thể thay đổi giữa frame.

---

# 24. MPV Render Thread hiện tại làm đúng một phần

`PlayBackRenderThread` đã có một pattern tốt:

```cpp
{
    std::unique_lock lock(state.mtx);

    ...

    localDrawW =
        state.surface.drawW;

    localDrawH =
        state.surface.drawH;
}
```

Sau đó unlock:

```cpp
// render bằng localDrawW/localDrawH
```

Đây chính là pattern cần đưa sang `UIRenderThread`.

Tức là:

```text
PlayBackRenderThread
    │
    └── Capture primitive state
             ↓
         local state
             ↓
          unlock
             ↓
           render
```

`UIRenderThread` hiện tại chưa làm đầy đủ điều tương tự với `WindowRuntime`.

---

# 25. Tuy nhiên PlayBackRenderThread vẫn còn vấn đề

Ví dụ:

```cpp
if (!this->state.fboPool)
    continue;
```

sau khi lock đã được giải phóng.

Nếu ownership của `fboPool` có thể thay đổi từ thread khác thì vẫn có race.

Ngoài ra:

```cpp
state.fboPool->ResizeFrame(...)
```

được gọi trực tiếp.

Cần xác định rõ:

```text
fboPool owner = RenderThread
```

Nếu đúng thì tốt nhất `fboPool` chỉ được truy cập từ render thread.

Nguyên tắc:

> GPU resource nên có một owner thread rõ ràng.

---

# 26. Một lỗi khác: WindowManager trả raw pointer

`WindowManager` lưu:

```cpp
std::unordered_map<
    WindowId,
    std::unique_ptr<WindowRuntime>
> windows;
```

nhưng API trả:

```cpp
WindowRuntime*
```

Trong khi một thread khác có thể:

```cpp
DestroyWindow()
```

và:

```cpp
windows.erase(it);
```

Sau đó render thread vẫn giữ:

```cpp
WindowRuntime*
```

Đây là lifetime hazard.

Ví dụ:

```text
RenderThread
   │
   └── runtime*

MainThread
   │
   └── DestroyWindow(runtime)
             │
             └── delete runtime

RenderThread
   │
   └── runtime->...
             ↑
           dangling
```

`stateMutex` không giải quyết được lifetime.

---

# 27. Cần tách State Lifetime khỏi WindowRuntime Lifetime

Có hai hướng.

## Phương án A — shared_ptr

WindowRuntime:

```cpp
std::shared_ptr<WindowRuntime>
```

WindowManager:

```cpp
std::unordered_map<
    WindowId,
    std::shared_ptr<WindowRuntime>
>
```

Render thread giữ:

```cpp
std::shared_ptr<WindowRuntime>
```

Đây là phương án an toàn hơn.

## Phương án B — RenderThread không giữ Runtime

Tốt hơn về kiến trúc:

```text
UIRenderThread
    │
    ├── WindowRenderSnapshot
    ├── Graphics resources
    └── Renderer
```

WindowRuntime chỉ tồn tại ở orchestration layer.

Nhưng đây là refactor lớn hơn.

---

# 28. WindowRelation cũng có race

Hiện tại:

```cpp
std::vector<WindowRuntime*> children;
```

`AddChild()`:

```cpp
children.push_back(child);
```

`RemoveChild()`:

```cpp
children.erase(...);
```

nhưng không có mutex.

Trong khi:

```cpp
GetChildren()
```

có thể được gọi từ thread khác.

Nếu window tree chỉ được thao tác từ main thread thì nên ghi rõ invariant:

```text
WindowRelation:
Main-thread only
```

Nếu không đảm bảo được thì phải thêm synchronization.

---

# 29. HasVisibleChildren() cũng đọc state không lock

Hiện tại:

```cpp
if (child &&
    (child->state.display.isVisible ||
     checkRecursively(...)))
```

đọc:

```cpp
child->state.display.isVisible
```

không lock.

Đây là race.

Sau khi snapshot architecture:

```cpp
auto snapshot =
    child->CaptureRenderSnapshot();

if (snapshot.display.visible)
```

hoặc tạo API:

```cpp
bool WindowRuntime::IsVisible() const
{
    std::lock_guard lock(stateMutex);
    return state.display.isVisible;
}
```

---

# 30. UpdateWindowState hiện đang làm quá nhiều việc

Hàm hiện tại:

```cpp
UpdateWindowState()
```

đồng thời:

1. đọc WindowState;
2. tính layout;
3. phát hiện resize;
4. request UI resize;
5. cập nhật dirty;
6. cập nhật MPV render state;
7. cập nhật PropertyBag.

Đây là quá nhiều responsibility.

Nên chia:

```text
WindowStateMutation
        │
        ▼
WindowState
        │
        ▼
WindowLayoutCalculator
        │
        ▼
WindowDerivedState
        │
        ▼
RenderScheduler
        │
        ▼
RequestRender
```

---

# 31. Kiến trúc UpdateWindowState mới

Nên hướng tới:

```cpp
void WindowRuntime::OnNativeWindowChanged(...)
{
    {
        std::lock_guard lock(stateMutex);

        // mutate authoritative state
    }

    RebuildDerivedState();

    RequestRender();
}
```

Trong đó:

```cpp
void WindowRuntime::RebuildDerivedState()
{
    std::lock_guard lock(stateMutex);

    derived.layout =
        CalculateLayout(
            state,
            style
        );

    ++stateVersion;
}
```

Sau đó render thread tự capture snapshot.

Không cần:

```cpp
UpdateWindowState()
```

đẩy dữ liệu trực tiếp vào mọi subsystem.

---

# 32. Nên có một State Mutation API

Thay vì mọi nơi:

```cpp
{
    std::lock_guard lock(runtime->stateMutex);
    runtime->state.xxx = value;
}
```

nên có:

```cpp
runtime->MutateState(
    [](WindowState& state)
    {
        state.geometry.width = 1366;
        state.geometry.height = 768;
    }
);
```

Implementation:

```cpp
template<typename Fn>
void MutateState(Fn&& fn)
{
    {
        std::lock_guard lock(stateMutex);
        fn(state);
        ++stateVersion;
    }

    NotifyStateChanged();
}
```

Điều này tạo ra một invariant rất rõ:

> Mọi thay đổi authoritative WindowState đều đi qua một mutation boundary.

---

# 33. Capture API

Tương ứng:

```cpp
WindowRenderSnapshot
WindowRuntime::CaptureRenderSnapshot() const
{
    std::lock_guard lock(stateMutex);

    WindowRenderSnapshot snapshot;

    snapshot.version = stateVersion;

    snapshot.geometry = ...;
    snapshot.display = ...;
    snapshot.input = ...;

    snapshot.layout =
        derived.layout;

    snapshot.renderingEnabled =
        state.runtime.renderingEnabled;

    return snapshot;
}
```

Đây là read boundary.

Do đó có hai API:

```text
MutateState()
       │
       ▼
Authoritative State

CaptureRenderSnapshot()
       │
       ▼
Immutable Frame State
```

---

# 34. RenderFrame phải có semantic rõ ràng

Nên định nghĩa:

```text
Frame lifecycle

1. Wait
2. Capture
3. Validate
4. Prepare GPU
5. BeginFrame
6. Render
7. EndFrame
8. Present
```

Trong đó:

```text
Capture
```

là transaction boundary của state.

Không phải:

```text
BeginFrame
```

mới là state boundary.

---

# 35. Cần tránh cập nhật State trong RenderUI

Hiện tại:

```cpp
if (uiState.isAnyAction)
    runtime->state.runtime.is_dirty = true;
```

Đây là vấn đề kiến trúc.

Renderer đang:

```text
READ state
+
WRITE state
```

Trong cùng frame.

Nên thay bằng:

```cpp
WindowRenderResult result;
```

Ví dụ:

```cpp
struct WindowRenderResult
{
    bool requestRenderAgain = false;
    bool dirty = false;
    bool toggleFullscreen = false;
};
```

Renderer:

```cpp
WindowRenderResult
MainWindowRenderer::RenderUI(
    const WindowRenderSnapshot& snapshot
)
```

sau frame:

```cpp
runtime->ApplyRenderResult(result);
```

Hoặc tốt hơn:

```text
UI interaction
    ↓
Command
    ↓
Main/Main event layer
    ↓
State mutation
    ↓
next frame snapshot
```

Không nên:

```text
Render
  ↓
mutate authoritative state
  ↓
same frame
  ↓
render tiếp
```

---

# 36. `is_dirty` hiện tại cũng có semantic không rõ

`UpdateWindowState()`:

```cpp
runtime->state.runtime.is_dirty =
    hasSizeChanged;
```

Renderer:

```cpp
if (uiState.isAnyAction)
    runtime->state.runtime.is_dirty = true;
```

`AdjustWindowFrameRates()`:

```cpp
if (isDirty)
{
    setTargetFPS(60);
    isDirty = false;
}
```

Sau đó:

```cpp
runtime->state.runtime.is_dirty = isDirty;
```

Như vậy `is_dirty` vừa là:

```text
state changed
```

vừa là:

```text
request high FPS
```

vừa là:

```text
has UI interaction
```

Đây là ba khái niệm khác nhau.

Nên tách:

```cpp
stateChanged
renderRequested
interactionActive
needsHighFrequency
```

---

# 37. Frame scheduling cũng nên tách khỏi State

Hiện tại:

```text
WindowState
    ↓
AdjustWindowFrameRates
    ↓
FrameTimer
```

Nên có:

```cpp
WindowRenderScheduler
```

chịu trách nhiệm:

```text
RenderReason
-----------------
StateChanged
Resize
MouseInteraction
PlaybackUpdate
MPVFrameReady
Animation
ExplicitRequest
```

Ví dụ:

```cpp
enum class RenderReason
{
    None,
    WindowStateChanged,
    Resize,
    Interaction,
    Playback,
    VideoFrame,
    Animation,
    Explicit
};
```

Request:

```cpp
RequestRender(RenderReason::Resize);
```

---

# 38. Có thể coalescing request

Không cần render 20 frame nếu Windows gửi 20 resize messages liên tục.

Có thể:

```text
WM_SIZE 1280
WM_SIZE 1290
WM_SIZE 1300
WM_SIZE 1310
WM_SIZE 1320
       │
       ▼
   coalesce
       │
       ▼
Render latest snapshot = 1320
```

Điều này rất phù hợp với media player.

Render không cần replay mọi state transition.

Nó cần:

> latest coherent state.

---

# 39. Resize pipeline đề xuất

Khi Windows resize:

```text
WM_SIZE
   │
   ▼
Mutate WindowState
   │
   ├── width
   ├── height
   └── version++
   │
   ▼
RebuildDerivedState
   │
   └── ClientSize
   │
   ▼
RequestRender(Resize)
```

Render thread:

```text
Wake
 │
 ▼
Capture snapshot version=100
 │
 ▼
snapshot.ClientSize=1320x740
 │
 ▼
Resize graphics backend
 │
 ▼
Render UI
 │
 ▼
Render video
 │
 ▼
Present
```

Nếu trong lúc đó có:

```text
WM_SIZE = 1400
```

thì:

```text
LiveState = 1400
```

nhưng frame hiện tại vẫn:

```text
Snapshot = 1320
```

Frame tiếp theo:

```text
Snapshot = 1400
```

Đây chính là consistency mà hệ thống hiện tại đang thiếu.

---

# 40. MPV render state nên nhận cùng geometry generation

Hiện tại `UpdateWindowState()` trực tiếp:

```cpp
renderState.surface.newW
renderState.surface.newH
renderState.surface.needResize
```

Nên thêm:

```cpp
uint64_t windowStateVersion;
```

vào MPV render request:

```cpp
struct RenderSurfaceRequest
{
    uint64_t version = 0;

    int width = 0;
    int height = 0;
};
```

Ví dụ:

```text
WindowSnapshot v100
    1280x720
        │
        ▼
MPV Render Request v100

WindowSnapshot v101
    1366x768
        │
        ▼
MPV Render Request v101
```

Render thread luôn biết mình đang render version nào.

---

# 41. Một invariant cực kỳ quan trọng

Sau refactor phải đảm bảo:

```text
For every rendered frame:

snapshot.version
    == layout.version
    == viewport.version
    == render-request.version
```

Nếu không cần version cho tất cả object thì ít nhất:

```text
frame.version
```

phải xác định rõ state generation mà frame đại diện.

---

# 42. State ownership nên được xác định

Đề xuất:

```text
WindowState
    Owner:
        Native/Main/Event side

WindowRenderSnapshot
    Owner:
        UIRenderThread

WindowRenderResources
    Owner:
        UIRenderThread

MPV Render State
    Owner:
        PlayBackRenderThread

PropertyBag
    Owner:
        Application/control layer
```

Không:

```text
mọi thread đều có quyền đọc/ghi mọi thứ.
```

---

# 43. State flow hoàn chỉnh

Kiến trúc cuối cùng nên là:

```text
                    ┌──────────────────────┐
                    │ Windows / SDL Events │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │ WindowState Mutation │
                    │     + stateMutex     │
                    └──────────┬───────────┘
                               │
                         stateVersion++
                               │
                               ▼
                    ┌──────────────────────┐
                    │ Derived State/Layout │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │ Render Scheduler     │
                    └──────────┬───────────┘
                               │
                         RequestRender
                               │
                               ▼
                    ┌──────────────────────┐
                    │    UIRenderThread    │
                    └──────────┬───────────┘
                               │
                         Capture once
                               │
                               ▼
                    ┌──────────────────────┐
                    │ WindowRenderSnapshot │
                    │      immutable       │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │      BeginFrame      │
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │    MainWindowRender  │
                    │      Render(snapshot)│
                    └──────────┬───────────┘
                               │
                               ▼
                    ┌──────────────────────┐
                    │      EndFrame        │
                    │       Present        │
                    └──────────────────────┘
```

---

# 44. Các lỗi/rủi ro chính được phát hiện

## Critical

### C1 — Render không sử dụng snapshot

Renderer đọc live state nhiều lần trong một frame.

**Mức độ: Critical**

---

### C2 — `PropertyBag::GetPtr()` không lock

```cpp
GetPtr()
```

truy cập `unordered_map` trực tiếp.

**Mức độ: Critical**

---

### C3 — Pointer từ PropertyBag sống sau khi lock kết thúc

Ngay cả khi thêm lock vào `GetPtr()`, pointer vẫn không bảo vệ lifetime/data consistency.

**Mức độ: Critical**

---

### C4 — Layout không atomic với WindowState

```text
WindowState
PropertyBag Layout
UIRenderThread resize state
```

là ba state representation khác nhau.

**Mức độ: Critical**

---

### C5 — `stateMutex` không tạo frame transaction

Lock chỉ được dùng ở các đoạn nhỏ.

**Mức độ: Critical**

---

## High

### H1 — Renderer vừa đọc vừa mutate runtime state

```cpp
runtime->state.runtime.is_dirty = true;
```

**Mức độ: High**

---

### H2 — Render thread truy cập Runtime trực tiếp

```cpp
renderer->RenderUI(runtime);
```

tạo khả năng bypass snapshot.

**Mức độ: High**

---

### H3 — WindowRuntime lifetime dùng raw pointer

```cpp
WindowRuntime*
```

trong nhiều subsystem.

**Mức độ: High**

---

### H4 — WindowRelation không synchronized

```cpp
children
```

có thể race nếu lifecycle xảy ra ngoài main thread.

**Mức độ: High**

---

### H5 — `is_dirty` có quá nhiều semantic

Nó vừa là state changed, render request, interaction/high FPS.

**Mức độ: High**

---

## Medium

### M1 — PropertyBag đang đóng vai trò State Store

Key string:

```text
Layout
RenderVideoFlag
ShowUiVideo
TriggerToggleFullscreen
```

làm giảm type safety.

**Mức độ: Medium**

---

### M2 — UpdateWindowState có quá nhiều responsibility

Nó vừa derive layout, vừa resize, vừa MPV sync, vừa PropertyBag update.

**Mức độ: Medium**

---

### M3 — RequestRender chỉ biểu diễn bool

Không biết render vì:

```text
resize
interaction
playback
video frame
animation
```

**Mức độ: Medium**

---

# 45. Refactor theo từng phase

Không nên refactor toàn bộ một lần.

## Phase 1 — Snapshot

Thêm:

```cpp
WindowRenderSnapshot
```

và:

```cpp
CaptureRenderSnapshot()
```

Sau đó:

```cpp
RenderUI(snapshot)
```

Đây là thay đổi quan trọng nhất.

---

## Phase 2 — Remove Layout khỏi PropertyBag

Chuyển:

```cpp
"Layout"
```

sang:

```cpp
WindowDerivedState
```

---

## Phase 3 — Remove render state khỏi PropertyBag

Chuyển:

```text
RenderVideoFlag
ShowUiVideo
```

sang typed state.

---

## Phase 4 — State versioning

Thêm:

```cpp
uint64_t stateVersion;
```

---

## Phase 5 — Render scheduler

Thêm:

```cpp
RenderReason
```

và:

```cpp
RequestRender(RenderReason)
```

---

## Phase 6 — Playback snapshot

Thêm:

```cpp
PlaybackRenderSnapshot
```

để UI không đọc playback state nhiều lần.

---

## Phase 7 — Lifetime

Xử lý:

```cpp
WindowRuntime*
```

và lifecycle của `WindowManager`.

---

# 46. Minimal implementation nên làm ngay

Nếu chưa muốn refactor lớn, chỉ cần thực hiện 4 thay đổi sau trước.

### 1. Thêm snapshot

```cpp
struct WindowRenderSnapshot
{
    uint64_t version = 0;

    WindowLayout layout;

    bool visible = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;

    bool renderingEnabled = true;
    bool dirty = false;
    bool renderVideo = false;
};
```

### 2. Capture dưới lock

```cpp
WindowRenderSnapshot WindowRuntime::CaptureRenderSnapshot() const
{
    std::lock_guard lock(stateMutex);

    WindowRenderSnapshot s;

    s.version = stateVersion;

    s.visible =
        state.display.isVisible;

    s.minimized =
        state.display.isMinimized;

    s.maximized =
        state.display.isMaximized;

    s.fullscreen =
        state.display.isFullscreen;

    s.renderingEnabled =
        state.runtime.renderingEnabled;

    s.dirty =
        state.runtime.is_dirty;

    s.layout =
        derived.layout;

    s.renderVideo =
        derived.renderVideo;

    return s;
}
```

### 3. Render thread capture một lần

```cpp
WindowRenderSnapshot snapshot =
    m_ownerRuntime->CaptureRenderSnapshot();
```

### 4. Renderer chỉ dùng snapshot

```cpp
renderer->RenderUI(snapshot);
```

Không đọc:

```cpp
runtime->state
runtime->properties
```

trong render path nữa.

---

# 47. Behavior sau khi sửa

Trước:

```text
Frame 100
 ├── width = 1280
 ├── resize
 ├── width = 1366
 ├── layout = 1366
 ├── resize
 ├── width = 1440
 └── present
```

Sau:

```text
Frame 100
 ├── Capture v500
 │      width = 1280
 │      height = 720
 │
 ├── Render v500
 │      width = 1280
 │      height = 720
 │
 └── Present v500

             ↓ WM_SIZE

Live State v501
width = 1366
height = 768

             ↓

Frame 101
 ├── Capture v501
 │      width = 1366
 │      height = 768
 │
 ├── Render v501
 │      width = 1366
 │      height = 768
 │
 └── Present v501
```

Nếu có 20 `WM_SIZE` trong lúc resize:

```text
v501 1280
v502 1290
v503 1300
v504 1310
...
v520 1480
```

renderer có thể bỏ qua các version trung gian và render:

```text
v520 = 1480
```

Đây là behavior tốt cho media player.

---

# 48. Kết luận cuối cùng

Vấn đề hiện tại **không nên tiếp tục giải quyết bằng cách tăng số lượng mutex hoặc mở rộng phạm vi lock**.

Kiến trúc hiện tại đang thiếu một abstraction quan trọng:

```text
LIVE STATE
    ↓
FRAME SNAPSHOT
    ↓
RENDER
```

`WindowRuntime::state` phải là:

> authoritative mutable state.

`WindowRenderSnapshot` phải là:

> immutable state của một frame.

`UIRenderThread` phải là:

> consumer của snapshot.

`WindowRenderer` phải là:

> pure render consumer, không đọc trực tiếp authoritative state.

Và `PropertyBag` không nên tiếp tục đóng vai trò render-state database.

Kiến trúc mục tiêu:

```text
             EVENT / NATIVE THREAD
                     │
                     ▼
             ┌───────────────┐
             │  WindowState  │
             │  authoritative│
             └───────┬───────┘
                     │
                 stateMutex
                     │
                     ▼
             ┌───────────────┐
             │ Capture()     │
             └───────┬───────┘
                     │
                     ▼
             ┌─────────────────────┐
             │ WindowRenderSnapshot│
             │ immutable            │
             │ version = N          │
             └──────────┬──────────┘
                        │
                        ▼
                  UIRenderThread
                        │
                        ▼
                 BeginFrame
                        │
                        ▼
                  Render(snapshot)
                        │
                        ▼
                   EndFrame
                        │
                        ▼
                     Present
```

**Đây là kiến trúc nên dùng cho hệ thống WindowRuntime hiện tại.**

Đặc biệt, nếu mục tiêu tiếp theo là xử lý triệt để hiện tượng nhấp nháy khi resize, tôi đánh giá thứ tự ưu tiên là:

```text
1. WindowRenderSnapshot
2. bỏ GetPtr() khỏi render path
3. đưa Layout ra khỏi PropertyBag
4. snapshot PlaybackState
5. đồng bộ resize với snapshot version
6. tách render request khỏi is_dirty
7. xử lý lifetime WindowRuntime
```

Chỉ riêng **1 → 3** đã loại bỏ phần lớn race/temporal inconsistency hiện tại mà không cần thay đổi toàn bộ kiến trúc WindowManager.