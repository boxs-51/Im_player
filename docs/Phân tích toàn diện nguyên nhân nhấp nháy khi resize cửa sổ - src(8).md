# PHÂN TÍCH TOÀN DIỆN HIỆN TƯỢNG NHẤP NHÁY KHI RESIZE CỬA SỔ

## 1. Mục tiêu

Phân tích mã nguồn `src(8).zip` với mục tiêu chính:

- Xác định nguyên nhân hiện tượng **nhấp nháy/flicker** khi kéo resize cửa sổ.
- Xác định các race condition giữa:
  - Win32
  - SDL2
  - UI Render Thread
  - OpenGL
  - ImGui
  - MPV Render Thread
  - FBO/Triple Buffer
- Đánh giá các lỗi kiến trúc liên quan đến resize.
- Xác định nguyên nhân theo mức độ nghiêm trọng.
- Đề xuất kiến trúc resize ổn định hơn.
- Đưa ra thứ tự sửa lỗi ưu tiên.

---

# 2. Kết luận tổng quan

Hiện tượng nhấp nháy **không có vẻ xuất phát chủ yếu từ `WM_PAINT` hoặc `WM_ERASEBKGND`**.

Phần Win32 đã có một số xử lý chống flicker tương đối đúng:

```cpp
case WM_ERASEBKGND:
    return 1;
```

và:

```cpp
SetClassLongPtr(
    hwnd,
    GCLP_HBRBACKGROUND,
    (LONG_PTR)GetStockObject(NULL_BRUSH)
);
```

Nguyên nhân chính nằm ở **render pipeline bất đồng bộ**.

Pipeline hiện tại có nhiều thành phần cùng phản ứng với resize:

```text
Windows
   ↓
SDL
   ↓
SDL Event Watcher
   ↓
SDL Window Event Handler
   ↓
Window State
   ↓
UI Render Thread
   ↓
OpenGL
   ↓
MPV Render Thread
   ↓
FBO
   ↓
ImGui
   ↓
Swap
```

Trong đó có nhiều đường xử lý resize đồng thời.

Các vấn đề nghiêm trọng nhất:

1. `SDL_AddEventWatch()` và `HandleWindowRuntimeEvent()` cùng xử lý resize.
2. `SDL_AddEventWatch()` có thể được đăng ký nhiều lần.
3. UI render chạy thread riêng.
4. MPV render chạy thread riêng.
5. UI và MPV sử dụng shared OpenGL resources/context.
6. FBO triple-buffer synchronization chưa đủ chặt.
7. Resize không có generation/snapshot.
8. VSync đang bị tắt.
9. `stateMutex` được giữ trong quá trình GPU rendering.
10. FBO có thể bị resize trong khi frame cũ vẫn đang được sử dụng.
11. Lifecycle của MPV render context có khả năng race với render thread.

---

# 3. Luồng resize hiện tại

Luồng hiện tại có thể mô hình hóa như sau:

```text
Windows
    │
    ├── WM_SIZING
    ├── WM_SIZE
    │
    ▼
SDL
    │
    ├── SDL_WINDOWEVENT_RESIZED
    └── SDL_WINDOWEVENT_SIZE_CHANGED
    │
    ├─────────────────────────────┐
    │                             │
    ▼                             ▼
WindowEventWatcher        HandleWindowRuntimeEvent
    │                             │
    ▼                             ▼
RouteWindowStateUpdate()   RouteWindowStateUpdate()
    │                             │
    └──────────────┬──────────────┘
                   ▼
           UpdateWindowState()
                   │
          ┌────────┴────────┐
          ▼                 ▼
       UI Resize         MPV Resize
          │                 │
          ▼                 ▼
    OpenGL Viewport       FBO Resize
          │                 │
          └────────┬────────┘
                   ▼
              Render Frame
                   │
                   ▼
              ImGui::Image
                   │
                   ▼
                 Swap
```

Vấn đề cốt lõi:

> Một resize event có thể đi qua nhiều pipeline khác nhau trước khi hoàn thành.

---

# 4. NGUYÊN NHÂN #1 - Resize event bị xử lý nhiều lần

## 4.1. `SDL_AddEventWatch()`

Trong `WindowInitializer.cpp` có:

```cpp
SDL_AddEventWatch(WindowEventWatcher, nullptr);
```

Trong callback:

```cpp
static int SDLCALL WindowEventWatcher(
    void* userdata,
    SDL_Event* event
)
```

callback tiếp tục gọi:

```cpp
RouteWindowStateUpdate(runtime);
```

và:

```cpp
runtime->resource.uiRenderThread->RequestRender();
```

---

## 4.2. Nhưng SDL event loop cũng xử lý resize

Trong `HandleWindowRuntimeEvent()`:

```cpp
case SDL_WINDOWEVENT_RESIZED:
case SDL_WINDOWEVENT_MOVED:
case SDL_WINDOWEVENT_MAXIMIZED:
case SDL_WINDOWEVENT_RESTORED:
case SDL_WINDOWEVENT_MINIMIZED:
case SDL_WINDOWEVENT_HIDDEN:
case SDL_WINDOWEVENT_SHOWN:
case SDL_WINDOWEVENT_SIZE_CHANGED:
{
    runtime->state.runtime.is_dirty =
        RouteWindowStateUpdate(runtime);
    break;
}
```

Như vậy resize có hai đường:

```text
SDL Event
   │
   ├── WindowEventWatcher
   │       └── RouteWindowStateUpdate()
   │
   └── HandleWindowRuntimeEvent()
           └── RouteWindowStateUpdate()
```

Điều này không nên tồn tại.

---

# 5. Tại sao duplicate resize gây flicker?

Khi kéo cửa sổ:

```text
1200x700
1201x700
1202x700
1203x700
1204x700
...
```

Mỗi event có thể gây:

```text
RequestResize()
RequestRender()
MPV Resize
FBO Resize
Viewport Resize
```

Nếu hai pipeline cùng xử lý:

```text
Event N
    ↓
resize 1201

Event N
    ↓
resize 1201

Event N+1
    ↓
resize 1202

Event N+1
    ↓
resize 1202
```

thì UI/MPV/FBO có thể không còn cùng một frame generation.

---

# 6. Đánh giá

**Mức độ: HIGH**

Đây là một trong những lỗi cần sửa đầu tiên.

## Khuyến nghị

Không dùng:

```cpp
SDL_AddEventWatch(WindowEventWatcher, nullptr);
```

cho resize.

Chỉ giữ một đường:

```text
SDL_PollEvent()
    ↓
HandleWindowRuntimeEvent()
    ↓
ResizeController
```

---

# 7. NGUYÊN NHÂN #2 - `SDL_AddEventWatch()` có thể bị đăng ký nhiều lần

Nếu `WindowInitializer::Initialize()` được gọi cho nhiều cửa sổ:

```text
Window 1
    ↓
SDL_AddEventWatch()

Window 2
    ↓
SDL_AddEventWatch()

Window 3
    ↓
SDL_AddEventWatch()
```

thì callback được đăng ký nhiều lần.

Trong khi không thấy lifecycle tương ứng:

```cpp
SDL_DelEventWatch(...)
```

Điều này đặc biệt nguy hiểm với kiến trúc multi-window.

---

# 8. NGUYÊN NHÂN #3 - `RequestResize()` chỉ có mutex nhưng chưa có resize transaction

Code:

```cpp
void UIRenderThread::RequestResize(
    int newWidth,
    int newHeight
)
{
    {
        std::lock_guard lock(m_mutex);

        m_needsResize = true;
        m_newWidth = newWidth;
        m_newHeight = newHeight;
    }

    m_cv.notify_one();
}
```

Điểm tốt:

- Có mutex.
- Kích thước mới nhất được ghi lại.
- Condition variable được notify.

Nhưng vẫn thiếu một khái niệm quan trọng:

```text
Resize Generation
```

Ví dụ:

```text
Resize #100
width 1200
height 700

Resize #101
width 1201
height 700

Resize #102
width 1202
height 700
```

UI và MPV cần biết frame nào thuộc generation nào.

---

# 9. Vấn đề resize snapshot

Hiện tại có thể xảy ra:

```text
Main Thread:
    width = 1200

UI Thread:
    render 1200

Main Thread:
    width = 1201

MPV Thread:
    FBO = 1201

UI Thread:
    vẫn đang render frame 1200
```

hoặc:

```text
UI Viewport = 1201
MPV FBO = 1200
ImGui Image = 1201
```

Đây là frame mismatch.

---

# 10. Kiến trúc nên dùng

Nên tạo:

```cpp
struct ResizeSnapshot {
    uint64_t generation;
    int width;
    int height;
};
```

Mọi hệ thống nhận cùng một snapshot:

```text
ResizeGeneration = 100

UI:
    generation 100

MPV:
    generation 100

FBO:
    generation 100

Frame:
    generation 100
```

Không nên để:

```text
UI = 100
MPV = 99
FBO = 98
```

---

# 11. NGUYÊN NHÂN #4 - VSync đang bị tắt

Trong OpenGL backend:

```cpp
SDL_GL_SetSwapInterval(0);
```

Điều này tắt VSync.

Khi resize:

```text
Window Resize
     ↓
OpenGL Render
     ↓
Swap
```

không chờ VBlank.

Có thể dẫn tới:

- tearing
- frame mismatch
- visual flashing
- horizontal tearing
- flickering cảm nhận bằng mắt

---

# 12. Test quan trọng nhất

Đổi tạm:

```cpp
SDL_GL_SetSwapInterval(0);
```

thành:

```cpp
SDL_GL_SetSwapInterval(1);
```

Nếu hiện tượng giảm đáng kể:

```text
VSync OFF
    ↓
Swap không đồng bộ
    ↓
Resize
    ↓
Tearing/Flicker
```

thì VSync là một phần nguyên nhân.

Không nên xem đây là giải pháp duy nhất.

---

# 13. NGUYÊN NHÂN #5 - UI Render Thread và MPV Render Thread hoạt động song song

Kiến trúc hiện tại có dạng:

```text
UI Render Thread
    │
    ├── ImGui
    ├── OpenGL
    └── Swap

MPV Render Thread
    │
    ├── MPV
    ├── OpenGL
    └── FBO
```

Hai thread dùng shared OpenGL resources.

Pipeline:

```text
MPV Thread
    ↓
FBO
    ↓
Fence
    ↓
UI Thread
    ↓
ImGui::Image
```

Đây là kiến trúc có thể chạy tốt nhưng synchronization phải rất chặt.

---

# 14. Resize trong shared OpenGL pipeline

Khi resize:

```text
Windows
    ↓
UI Resize
    ↓
Viewport mới

đồng thời:

MPV
    ↓
FBO Resize
```

Nếu hai thread không dùng cùng generation:

```text
UI = size N+1
MPV = size N
```

hoặc:

```text
UI = size N
MPV = size N+1
```

có thể tạo:

- black frame
- texture mismatch
- frame jump
- flicker
- scaling jump

---

# 15. NGUYÊN NHÂN #6 - FBO triple buffer có state transition không an toàn

Trong:

```cpp
OpenGLFrameBufferPool::AcquireFreeBuffer()
```

phần chính dùng:

```cpp
BufferState expected = BufferState::FREE;

if (m_frames[i].state.compare_exchange_strong(
        expected,
        BufferState::RENDERING,
        std::memory_order_acq_rel))
{
    return i;
}
```

Phần này đúng hướng.

Nhưng fallback có dạng:

```cpp
for (int i = 0; i < 3; ++i) {
    if (i != m_currentDisplayIndex) {
        m_frames[i].state.store(
            BufferState::RENDERING,
            std::memory_order_release
        );

        return i;
    }
}
```

Vấn đề:

Không kiểm tra state hiện tại.

Ví dụ:

```text
Frame 0 = DISPLAYING
Frame 1 = RENDERING
Frame 2 = READY
```

Nếu:

```text
currentDisplayIndex = 0
```

fallback có thể chọn Frame 1.

Nhưng Frame 1 đang:

```text
RENDERING
```

và bị ép lại thành:

```text
RENDERING
```

Điều này phá vỡ state machine.

---

# 16. FBO state machine đúng nên là

```text
FREE
 │
 ▼
RENDERING
 │
 ▼
READY
 │
 ▼
DISPLAYING
 │
 ▼
FREE
```

Chỉ cho phép:

```text
FREE       -> RENDERING
RENDERING  -> READY
READY      -> DISPLAYING
DISPLAYING -> FREE
```

Không được tùy tiện:

```text
RENDERING -> RENDERING
DISPLAYING -> RENDERING
```

---

# 17. NGUYÊN NHÂN #7 - FBO resize có thể tạo nhiều allocation size

Cơ chế allocation:

```cpp
if (targetW > frame.allocatedW ||
    targetH > frame.allocatedH)
```

sau đó tăng khoảng:

```cpp
newW = frame.allocatedW * 1.5f;
newH = frame.allocatedH * 1.5f;
```

Kết quả trong resize có thể:

```text
Frame 0:
3840x2160

Frame 1:
3840x2160

Frame 2:
5760x3240
```

Khi frame được display thay đổi:

```text
texture allocation
```

cũng thay đổi.

Có thể gây:

- scaling jump
- blur
- UV change
- black frame
- visual instability

Không nhất thiết là nguyên nhân gốc nhưng làm flicker dễ thấy hơn.

---

# 18. NGUYÊN NHÂN #8 - OpenGL synchronization giữa hai context

Bạn có:

```cpp
glFenceSync(...)
```

và:

```cpp
glClientWaitSync(...)
```

Đây là hướng đúng.

Nhưng kiến trúc:

```text
Context A
    ↓
MPV
    ↓
FBO
    ↓
Fence
    ↓
Context B
    ↓
UI
```

yêu cầu synchronization rất chính xác.

Resize làm pipeline phức tạp hơn:

```text
MPV render
      ↓
FBO resize
      ↓
Fence

trong khi:

UI
      ↓
GetStableFrame()
      ↓
ImGui::Image()
```

Nếu frame cũ và frame mới không được phân biệt rõ bằng generation/state thì có thể xuất hiện frame không nhất quán.

---

# 19. NGUYÊN NHÂN #9 - MPV Render Context lifecycle có race

Trong shutdown có logic tương tự:

```cpp
mpv_render_context_free(m_render_ctx);
```

trước khi:

```cpp
m_renderThread->Stop();
```

Nếu MPV render thread vẫn đang chạy:

```text
MPV Render Thread
       │
       │ đang dùng m_render_ctx
       ▼
Main Thread
       │
       └── mpv_render_context_free()
```

có thể xảy ra use-after-free/race.

Thứ tự đúng:

```text
Disable callback
      ↓
Stop render thread
      ↓
Join render thread
      ↓
Free render context
```

Đây không nhất thiết là nguyên nhân flicker chính nhưng là bug nghiêm trọng cần sửa.

---

# 20. NGUYÊN NHÂN #10 - `stateMutex` giữ quá lâu trong render

UI render thread có logic tương tự:

```cpp
std::lock_guard<std::mutex> stateLock(
    m_ownerRuntime->stateMutex
);
```

sau đó thực hiện:

```text
BeginFrame()
RenderUI()
EndFrame()
SwapWindow()
```

Tức là mutex state bị giữ trong cả quá trình GPU rendering.

Điều này không tốt.

Main thread cần mutex để:

```text
SDL event
Window state
Resize
Layout
```

nhưng có thể phải chờ UI thread:

```text
ImGui
OpenGL
Swap
```

Trong resize timing sẽ trở nên khó đoán.

---

# 21. Cách đúng

Không giữ `stateMutex` trong toàn bộ render.

Thay vào đó:

```text
lock
   ↓
copy state snapshot
   ↓
unlock
   ↓
render snapshot
```

Ví dụ:

```cpp
WindowRenderSnapshot snapshot;

{
    std::lock_guard lock(runtime->stateMutex);
    snapshot = BuildRenderSnapshot(*runtime);
}

Render(snapshot);
```

---

# 22. NGUYÊN NHÂN #11 - `UpdateWindowState()` làm quá nhiều nhiệm vụ

`UpdateWindowState()` hiện xử lý nhiều thứ:

```text
GetWindowSize
    ↓
Layout
    ↓
ClientArea
    ↓
Detect Resize
    ↓
Request UI Resize
    ↓
Request MPV Resize
    ↓
Update Render State
```

Một hàm state update đang trực tiếp điều khiển:

- Window
- UI
- OpenGL
- MPV
- FBO

Điều này tạo coupling rất mạnh.

---

# 23. Kiến trúc nên tách

Nên chia:

```text
WindowState
    ↓
ResizeController
    ↓
RenderSnapshot
```

sau đó:

```text
RenderSnapshot
   ├── UI Render Thread
   └── MPV Render Thread
```

Không nên:

```text
UpdateWindowState()
    ├── RequestResize()
    ├── SetVideoSize()
    ├── ResizeFBO()
    └── Render()
```

---

# 24. NGUYÊN NHÂN #12 - `WM_SIZING` chưa có resize transaction hoàn chỉnh

Có:

```cpp
case WM_SIZING:
    runtime->state.input.resizing = true;
    break;
```

nhưng chưa có một transaction hoàn chỉnh kiểu:

```text
WM_ENTERSIZEMOVE
    ↓
resizing = true

WM_SIZING
    ↓
update latest size

WM_EXITSIZEMOVE
    ↓
resizing = false
    ↓
commit final resize
```

---

# 25. `WM_TIMER` hiện tại gần như vô dụng

Có:

```cpp
SetTimer(hwnd, IDT_RENDER_TIMER, 16, NULL);
```

nhưng phần:

```cpp
WM_TIMER
```

liên quan đến render đã bị comment.

Do đó:

```text
WM_ENTERSIZEMOVE
    ↓
SetTimer()
    ↓
WM_TIMER
    ↓
không có resize/render logic
```

Timer này hiện không đóng góp đáng kể.

---

# 26. `WM_ERASEBKGND` không phải nguyên nhân chính

Bạn đã có:

```cpp
case WM_ERASEBKGND:
    return 1;
```

và:

```cpp
NULL_BRUSH
```

Đây là hướng xử lý đúng để tránh Windows xóa background trước khi OpenGL render.

Vì vậy không nên tập trung sửa `WM_ERASEBKGND` trước.

---

# 27. `WM_PAINT` cũng không phải nghi phạm chính

Bạn có:

```cpp
case WM_PAINT:
{
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}
```

Điều này cũng không cho thấy nguyên nhân chính của flicker.

---

# 28. Bug khác - `WindowController::Resize()`

Có đoạn:

```cpp
runtime->state.geometry.width = w;
runtime->state.geometry.minWidth = h;
```

Dòng:

```cpp
minWidth = h
```

rất đáng ngờ.

Nhiều khả năng phải là:

```cpp
runtime->state.geometry.height = h;
```

hoặc một field khác liên quan đến height.

Đây là bug state độc lập với flicker.

---

# 29. Tổng hợp mức độ

| Vấn đề | Khả năng gây flicker | Mức độ |
|---|---:|---:|
| Duplicate resize event path | Rất cao | CRITICAL/HIGH |
| SDL_AddEventWatch nhiều lần | Cao | HIGH |
| VSync = 0 | Rất cao | HIGH |
| Async UI + MPV render | Rất cao | HIGH |
| FBO state machine | Cao | HIGH |
| Shared OpenGL context | Cao | HIGH |
| Không có resize generation | Cao | HIGH |
| FBO dynamic allocation | Trung bình | MEDIUM |
| Giữ stateMutex trong render | Trung bình/Cao | HIGH |
| WM_TIMER | Thấp | LOW |
| WM_PAINT | Thấp | LOW |
| WM_ERASEBKGND | Rất thấp | LOW |
| Geometry minWidth/minHeight bug | Thấp/gián tiếp | MEDIUM |

---

# 30. Ba nguyên nhân cần kiểm tra đầu tiên

## #1. Duplicate resize handling

Loại bỏ:

```cpp
SDL_AddEventWatch(WindowEventWatcher, nullptr);
```

và chỉ xử lý:

```text
SDL_PollEvent()
    ↓
HandleWindowRuntimeEvent()
```

---

## #2. VSync

Test:

```cpp
SDL_GL_SetSwapInterval(1);
```

Nếu flicker giảm rõ rệt thì VSync là một phần nguyên nhân.

---

## #3. Không resize FBO/frame liên tục khi đang drag

Trong lúc:

```text
resizing == true
```

giữ frame video hiện tại.

Chỉ commit frame mới khi resize hoàn thành:

```text
Resize Start
    ↓
Freeze displayed frame
    ↓
Window continuously changes
    ↓
Keep latest size only
    ↓
Resize End
    ↓
Resize FBO
    ↓
Render new frame
    ↓
Atomic publish
```

---

# 31. Kiến trúc ResizeController đề xuất

```text
                    Windows / SDL
                         │
                         ▼
                ┌─────────────────┐
                │ ResizeController│
                │                 │
                │ width           │
                │ height          │
                │ generation      │
                │ resizing        │
                └────────┬────────┘
                         │
                         ▼
                  RenderSnapshot
                         │
              ┌──────────┴──────────┐
              ▼                     ▼
       UI Render Thread       MPV Render Thread
              │                     │
              ▼                     ▼
        glViewport()            FBO Resize
              │                     │
              └──────────┬──────────┘
                         ▼
                  Frame Generation
                         │
                         ▼
                  Atomic Publish
                         │
                         ▼
                    ImGui Image
                         │
                         ▼
                       Swap
```

---

# 32. Resize Generation

Nên có:

```cpp
struct ResizeSnapshot {
    uint64_t generation;
    int width;
    int height;
};
```

Mỗi lần resize:

```text
generation++
```

Ví dụ:

```text
Generation 100
    width = 1200
    height = 700

Generation 101
    width = 1300
    height = 700
```

UI và MPV chỉ commit frame khi generation phù hợp.

---

# 33. Frame generation

FBO frame cũng nên có:

```cpp
struct FrameMetadata {
    uint64_t resizeGeneration;
    int width;
    int height;
};
```

Khi UI nhận frame:

```text
frame.generation == currentResize.generation
```

thì mới ưu tiên display.

Nếu:

```text
frame.generation < currentResize.generation
```

thì đó là frame cũ.

Trong lúc resize có thể tiếp tục giữ frame cũ ổn định thay vì render một frame không đồng bộ.

---

# 34. FBO state machine đề xuất

```text
                ┌──────────────┐
                │     FREE     │
                └──────┬───────┘
                       │
                       ▼
                ┌──────────────┐
                │  RENDERING   │
                └──────┬───────┘
                       │
                       ▼
                ┌──────────────┐
                │    READY     │
                └──────┬───────┘
                       │
                       ▼
                ┌──────────────┐
                │  DISPLAYING  │
                └──────┬───────┘
                       │
                       ▼
                     FREE
```

Mọi transition phải được kiểm soát bằng CAS/atomic state transition.

---

# 35. Resize strategy tốt nhất cho media player

Không nên resize video texture theo từng pixel trong khi người dùng kéo cửa sổ.

Thay vào đó:

```text
User starts dragging
        ↓
resizing = true
        ↓
UI window resize
        ↓
keep current video frame
        ↓
store latest width/height
        ↓
User releases mouse
        ↓
WM_EXITSIZEMOVE
        ↓
commit final resize
        ↓
resize FBO
        ↓
render
        ↓
publish
```

Ưu điểm:

- Không flicker.
- Không tạo hàng trăm FBO resize.
- Không tạo hàng trăm MPV resize.
- Không làm GPU realloc liên tục.
- Giảm CPU/GPU contention.
- Video ổn định trong lúc drag.

---

# 36. Thứ tự sửa đề xuất

## Phase 1 - Fix ngay

```text
[1] Remove SDL_AddEventWatch resize path
[2] Chỉ còn một resize event handler
[3] Test SDL_GL_SetSwapInterval(1)
[4] Fix WindowController::Resize()
```

---

## Phase 2 - Fix synchronization

```text
[5] Thêm ResizeGeneration
[6] Thêm ResizeSnapshot
[7] Thêm FrameGeneration
[8] Sửa FBO state machine
[9] Không overwrite RENDERING buffer
```

---

## Phase 3 - Fix rendering architecture

```text
[10] Không giữ stateMutex trong toàn bộ render
[11] Render từ immutable snapshot
[12] Tách WindowState khỏi RenderState
[13] Tách ResizeController
```

---

## Phase 4 - Fix resize UX

```text
[14] WM_ENTERSIZEMOVE -> resizing=true
[15] WM_SIZING -> update latest size
[16] WM_EXITSIZEMOVE -> final resize
[17] Freeze displayed frame trong quá trình drag
[18] Commit frame mới bằng atomic swap
```

---

## Phase 5 - Fix MPV lifecycle

```text
[19] Stop MPV render thread
[20] Join thread
[21] Free mpv_render_context
[22] Destroy GL resources
```

Không làm ngược thứ tự.

---

# 37. Kiến trúc cuối cùng nên đạt được

```text
                         ┌─────────────────────┐
                         │      Windows        │
                         └──────────┬──────────┘
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │     SDL Events      │
                         └──────────┬──────────┘
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │  ResizeController   │
                         │                     │
                         │ width               │
                         │ height              │
                         │ resizing            │
                         │ generation          │
                         └──────────┬──────────┘
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │  Render Snapshot    │
                         └──────────┬──────────┘
                                    │
                   ┌────────────────┴────────────────┐
                   │                                 │
                   ▼                                 ▼
        ┌─────────────────────┐          ┌─────────────────────┐
        │   UI Render Thread  │          │  MPV Render Thread  │
        │                     │          │                     │
        │ OpenGL              │          │ MPV                 │
        │ ImGui               │          │ FBO                 │
        │ Viewport            │          │ Fence               │
        └──────────┬──────────┘          └──────────┬──────────┘
                   │                                 │
                   │                                 │
                   └────────────────┬────────────────┘
                                    ▼
                         ┌─────────────────────┐
                         │   Frame Generation  │
                         │                     │
                         │ generation          │
                         │ width               │
                         │ height              │
                         └──────────┬──────────┘
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │   Atomic Publish    │
                         └──────────┬──────────┘
                                    │
                                    ▼
                              ImGui::Image
                                    │
                                    ▼
                               SwapBuffers
                                    │
                                    ▼
                              VSync / Present
```

---

# 38. Kết luận cuối cùng

Hiện tượng:

```text
Resize cửa sổ
      ↓
nhấp nháy
```

không nên được xử lý bằng cách chỉ sửa:

```cpp
WM_PAINT
WM_ERASEBKGND
glViewport
```

Nguyên nhân thực sự nằm ở **sự phối hợp giữa Window Resize và Render Pipeline**.

Ba vấn đề quan trọng nhất hiện tại là:

```text
1. Duplicate resize event handling
2. Async UI/MPV/FBO resize không có generation
3. VSync đang tắt
```

và vấn đề nền tảng:

```text
FBO state machine + shared OpenGL context
chưa có synchronization protocol đủ chặt.
```

Mục tiêu cuối cùng phải là:

```text
ONE RESIZE EVENT
        ↓
ONE RESIZE SNAPSHOT
        ↓
ONE GENERATION
        ↓
UI + MPV + FBO cùng generation
        ↓
ONE ATOMIC FRAME PUBLISH
        ↓
STABLE PRESENT
```

Nếu triển khai theo hướng này, resize sẽ không còn là quá trình để **UI, MPV và FBO tự resize độc lập**, mà trở thành một transaction có kiểm soát.

---

# 39. Checklist triển khai

- [ X ] Xóa `SDL_AddEventWatch()` cho resize.
- [ ] Chỉ xử lý resize tại một nơi.
- [ X ] Không đăng ký Event Watch nhiều lần.
- [ ] Thêm `ResizeSnapshot`.
- [ ] Thêm `resizeGeneration`.
- [ ] Thêm generation cho frame.
- [ ] Sửa FBO `AcquireFreeBuffer()`.
- [ ] Không overwrite buffer đang `RENDERING`.
- [ ] Không overwrite `DISPLAYING`.
- [ ] Test `SDL_GL_SetSwapInterval(1)`.
- [ ] Không giữ `stateMutex` trong GPU rendering.
- [ ] Render bằng immutable snapshot.
- [ ] Tách `WindowState` và `RenderState`.
- [ ] Tạo `ResizeController`.
- [ ] Dùng `WM_ENTERSIZEMOVE`.
- [ ] Dùng `WM_EXITSIZEMOVE`.
- [ ] Giữ frame hiện tại trong lúc drag resize.
- [ ] Chỉ commit final resize sau khi resize ổn định.
- [ ] Đồng bộ MPV FBO với resize generation.
- [ ] Stop MPV render thread trước khi free `mpv_render_context`.
- [ X ] Sửa bug `minWidth = h`.
- [ ] Xem xét loại bỏ `WM_TIMER` hiện tại nếu không dùng.

---

# 40. Mức độ ưu tiên

```text
CRITICAL
├── Duplicate resize handling
├── FBO state transition không an toàn
└── MPV render context lifecycle

HIGH
├── VSync OFF
├── UI/MPV async resize
├── Không có resize generation
├── Shared OpenGL synchronization
└── stateMutex giữ quá lâu

MEDIUM
├── FBO dynamic allocation
├── WindowController geometry bug
└── Resize transaction chưa hoàn chỉnh

LOW
├── WM_TIMER hiện tại
├── WM_PAINT
└── WM_ERASEBKGND
```

**Kết luận kỹ thuật:** Đừng tiếp tục vá flicker ở tầng Win32. Hãy sửa **Resize → RenderSnapshot → MPV/FBO → Frame Publish → Present**. Đây mới là điểm gốc của vấn đề trong kiến trúc hiện tại.