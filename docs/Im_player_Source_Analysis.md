
# Im_player — Phân tích toàn diện mã nguồn, lỗi hiện tại, cấu trúc và hướng phát triển

> **Nguồn phân tích:** `src(5).zip` được cung cấp trong cuộc trò chuyện.  
> **Ngày phân tích:** 2026-08-10  
> **Phạm vi:** mã nguồn trong archive, kiến trúc, dependency, lifecycle, ownership, threading, rendering, MPV, window system, API/Python backend, test strategy và roadmap.
>
> **Lưu ý quan trọng:** archive hiện tại chỉ chứa thư mục `src/` và tài liệu nội bộ; **không thấy `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, solution/project file hoặc cấu hình build ở root**. Vì vậy không thể xác nhận một build sạch chỉ từ archive này. Các lỗi bên dưới được phân loại theo mức độ chắc chắn dựa trên code hiện có.

---

# 1. Executive Summary

Project hiện tại không còn là một media player đơn giản. Nó đã phát triển thành một hệ thống gồm nhiều subsystem:

```text
Application
├── Window System
│   ├── WindowManager
│   ├── WindowRuntime
│   ├── WindowController
│   ├── WindowProcHook
│   ├── UIRenderThread
│   └── WindowRenderer
│
├── Graphics
│   ├── IGraphicsBackend
│   ├── OpenGLBackend
│   └── D3D11Backend
│
├── MPV
│   ├── PlayerManager
│   ├── PlayerSession
│   ├── Player
│   ├── PlayBackRender
│   ├── PlayBackRenderThread
│   ├── PlaybackObserver
│   ├── PlayBackProperty
│   ├── PlaybackCommandDispatcher
│   ├── ShaderManager
│   └── AudioFilterManager
│
├── UI
│   ├── MainWindowRenderer
│   ├── GUI
│   ├── Popup
│   └── ImGui
│
├── Services
│   ├── API Manager
│   ├── Gemini Provider
│   └── Python YouTube backend
│
├── Threading
│   └── ThreadManager
│
└── Global/Application State
    ├── globals.cpp
    ├── settings_manager
    ├── utils
    └── miscellaneous globals
```

## Đánh giá tổng quát

| Khu vực | Đánh giá | Nhận xét |
|---|---:|---|
| Kiến trúc tổng thể | 🟡 | Có nhiều abstraction tốt nhưng ownership/lifecycle chưa ổn định |
| Window system | 🟡🔴 | Ý tưởng tốt nhưng đang phụ thuộc mạnh vào raw pointer, global manager và thread |
| MPV integration | 🟡🔴 | Phân lớp khá tốt nhưng lifecycle/render-thread còn nhiều rủi ro |
| OpenGL | 🔴 | Context/thread/ImGui synchronization là điểm rủi ro lớn nhất |
| Threading | 🔴 | Có race/lifecycle/thread identity issues |
| FrameBufferPool | 🔴 | State machine chưa đủ chặt và có vấn đề đồng bộ GPU/CPU |
| Error handling | 🟡🔴 | Có exception nhưng nhiều API trả bool/null và cleanup phân tán |
| Build/dependency | 🔴 | Archive thiếu build system; dependency đang phụ thuộc include path ngoài source |
| Testability | 🔴 | Nhiều singleton/global/hardware dependency khiến unit test khó |
| Python backend | 🟡🔴 | Chức năng phong phú nhưng state global, network và key management chưa production-safe |
| Khả năng mở rộng | 🟡 | Có nền tảng tốt nếu refactor ownership và runtime boundary |
| Technical debt | 🔴 | Đang ở mức cần refactor trước khi tiếp tục thêm feature lớn |

**Kết luận chính:** hiện tại **không nên tiếp tục mở rộng feature MPV/UI/threading ngay**. Ưu tiên đúng nên là:

```text
Build reproducible
→ Ownership/Lifecycle
→ Thread model
→ Rendering model
→ Remove globals
→ Tests
→ MPV integration tests
→ Feature development
```

---

# 2. Inventory của archive

Archive chứa khoảng:

```text
70 .h
52 .cpp
1 .c
24 .py
9 .md
----------------
156 source/document files
```

Ngoài ra còn các thư mục:

```text
api/
backends/
common/
gui/
mpv/
popup/
threads/
windows/
```

Trong `backends/vid/` có một Python subsystem tương đối lớn.

---

# 3. Cấu trúc hiện tại

## 3.1 Application layer

File chính:

```text
src/main1.cpp
```

`main()` hiện đang làm quá nhiều việc:

```text
Process startup
    ↓
Single-instance mutex
    ↓
Runtime services
    ↓
SDL_Init
    ↓
Config load
    ↓
WindowManager initialization
    ↓
Window template registration
    ↓
Window creation
    ↓
Font loading
    ↓
PlayerSession creation
    ↓
MPV event loop
    ↓
Hotkey
    ↓
Window event routing
    ↓
MPV command update
    ↓
Audio filter update
    ↓
FPS management
    ↓
Window rendering requests
    ↓
Window destruction
    ↓
Shutdown
```

Đây là **God Function**.

### Hướng sửa

Tách thành:

```text
Application
├── Application::Initialize()
├── Application::Run()
├── Application::ProcessEvents()
├── Application::Update()
├── Application::Render()
└── Application::Shutdown()
```

Và:

```text
RuntimeServices
WindowRuntimeSystem
PlaybackRuntime
InputSystem
```

---

# 4. Lỗi nghiêm trọng nhất hiện tại

## P0-01 — `main()` có dereference nullptr khi tạo main window thất bại

Trong `src/main1.cpp`:

```cpp
WindowRuntime* mainWin = winManager.GetMainWindow();

if (!mainWin) {
    winManager.DestroyWindow(mainWin->info.id);
    ...
}
```

Nếu `mainWin == nullptr` thì:

```cpp
mainWin->info.id
```

là dereference nullptr.

### Đúng phải là

```cpp
if (!mainWin) {
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return 1;
}
```

### Mức độ

**P0 — crash chắc chắn trong failure path.**

---

# 5. P0-02 — `WindowManager::QueueCreateWindow()` trả về ID sai và capture dangling reference

Trong `WindowManager`:

```cpp
WindowId QueueCreateWindow(
    const std::string& templateName,
    WindowRuntime* parent = nullptr
) {
    WindowId newId = 0;

    m_creationQueue.emplace_back(
        [this, templateName, parent, &newId]() {
            auto* runtime = factory->Create(templateName, parent);

            if (runtime) {
                newId = runtime->info.id;
                ...
            }
        }
    );

    return newId;
}
```

Đây là lỗi rất nghiêm trọng.

`newId` là local variable.

Lambda được chạy **sau khi function return**, nhưng lambda giữ:

```cpp
&newId
```

tức là reference tới biến stack đã hết lifetime.

Đây là:

```text
dangling reference
→ undefined behavior
```

Ngoài ra:

```cpp
return newId;
```

được thực hiện ngay trước khi queued task chạy, nên gần như luôn trả:

```text
0
```

### Thiết kế đúng

Không nên trả `WindowId` từ một queue asynchronous kiểu này.

Có thể dùng:

```cpp
WindowId CreateWindow(...)
```

nếu creation chạy synchronous.

Hoặc:

```cpp
std::future<WindowId> QueueCreateWindow(...)
```

hoặc một request object:

```cpp
struct WindowCreateRequest {
    std::string templateName;
    WindowRuntime* parent;
    std::promise<WindowId> result;
};
```

### Khuyến nghị

Nếu mọi window creation đều được xử lý trên main thread:

```cpp
WindowId QueueCreateWindow(...)
```

nên trả về một `WindowCreationHandle`, không phải ID thật.

---

# 6. P0-03 — Main window có nguy cơ bị destroy hai lần

Trong event loop:

```cpp
if (winToClose->style.isMainWindow) {
    running = false;
    winManager.DestroyWindow(idToClose);
}
```

Sau loop lại:

```cpp
if (mainWin) {
    winManager.DestroyWindow(mainWin->info.id);
}
```

Nhưng `mainWin` là raw pointer.

Sau:

```cpp
winManager.DestroyWindow(...)
```

nó trở thành dangling pointer.

Sau đó code vẫn dùng:

```cpp
mainWin->info.id
```

Đây là **use-after-free**.

### Đúng

Ownership phải được centralized:

```cpp
WindowManager::DestroyWindow(id);
```

và sau khi destroy:

```cpp
mainWin = nullptr;
```

hoặc tốt hơn:

```cpp
WindowId mainWindowId;
```

Không giữ raw pointer lâu hơn lifecycle của object.

---

# 7. P0-04 — ImGui multi-threading hiện tại rất nguy hiểm

Project đang có:

```text
UIRenderThread per window
```

và mỗi thread thực hiện:

```cpp
ImGui::SetCurrentContext(...)
ImGui::NewFrame()
renderer->RenderUI()
ImGui::Render()
ImGui_ImplOpenGL3_RenderDrawData(...)
```

Trong khi main thread cũng gọi:

```cpp
ImGui::SetCurrentContext(...)
ImGui_ImplSDL2_ProcessEvent(...)
```

Đáng chú ý là code TLS custom cho ImGui đang bị comment:

```cpp
/*
#define GImGui MyImGuiTLS
...
*/
thread_local ImGuiContext* MyImGuiTLS = NULL;
```

Nhưng chỉ khai báo:

```cpp
thread_local ImGuiContext* MyImGuiTLS
```

**không biến ImGui thành TLS.**

Nếu ImGui được build theo cấu hình mặc định, `GImGui` không tự nhiên dùng `MyImGuiTLS`.

### Hệ quả

Hai thread có thể cùng truy cập:

```text
GImGui
ImGuiContext
ImGui backend state
Font atlas
Draw data
```

→ data race / corruption / crash / black frame.

### Đây là một trong những nguyên nhân rất có khả năng liên quan đến:

```text
black flash
resize flicker
crash khi đóng window
UI không ổn định
```

---

# 8. P0-05 — ImGui backend event processing và rendering khác thread

Main thread:

```cpp
runtime->resource.graphicsBackend->ProcessEvent(e);
```

và OpenGL backend:

```cpp
return ImGui_ImplSDL2_ProcessEvent(e);
```

Trong khi render thread:

```cpp
ImGui_ImplSDL2_NewFrame();
ImGui_ImplOpenGL3_NewFrame();
```

Nếu backend state được truy cập đồng thời, synchronization hiện tại chưa đủ.

`stateMutex` không giải quyết toàn bộ vấn đề vì ImGui backend có state riêng.

### Khuyến nghị

Chọn **một trong hai kiến trúc**:

### Phương án A — Khuyến nghị

```text
Main Thread
    ├── SDL events
    ├── ImGui
    ├── UI rendering
    └── Swap

MPV Render Thread
    ├── libmpv rendering
    └── FBO
```

Chỉ MPV rendering chạy background.

### Phương án B

Mỗi window có một ImGui context + backend state hoàn toàn độc lập và đảm bảo mọi access tới context/backend chỉ từ đúng thread.

Phương án này khó hơn nhiều.

**Không nên tiếp tục architecture hiện tại mà không khóa rõ ownership/thread affinity.**

---

# 9. P0-06 — `OpenGLBackend.h` có extra qualification trong class definition

Trong class:

```cpp
class OpenGLBackend : public IGraphicsBackend {
public:

    std::vector<mpv_render_param>
    OpenGLBackend::GetPlayBackRenderParams(...)
```

và:

```cpp
const char* OpenGLBackend::GetMpvApiType() const override
```

Khi định nghĩa function ngay bên trong class, không cần:

```cpp
OpenGLBackend::
```

### Nên viết

```cpp
std::vector<mpv_render_param>
GetPlayBackRenderParams(const ImVec2& size) override
```

và:

```cpp
const char* GetMpvApiType() const override
```

### Khuyến nghị tốt hơn

Đưa toàn bộ implementation ra:

```text
OpenGLBackend.h
OpenGLBackend.cpp
```

Header chỉ chứa declaration.

---

# 10. P0-07 — archive thiếu các header nội bộ được include

Static scan cho thấy nhiều include nội bộ không tồn tại trong archive.

Ví dụ:

```text
globals.h
utils.h
FontManager.h
hotkey_handler.h
notification.h
WindowManager.h
MainWindowRenderer.h
D3D11Backend.h
OpenGLBackend.h
WindowRuntime.h
```

Một số header thực sự có trong archive nhưng đường dẫn include phụ thuộc include directories; tuy nhiên nhiều file như:

```text
globals.h
utils.h
FontManager.h
hotkey_handler.h
```

không xuất hiện ở archive.

### Kết luận

Có hai khả năng:

1. Archive thiếu file.
2. Project hiện tại đang phụ thuộc vào file nằm ngoài source tree.

Nếu là (2), build không reproducible.

### Việc cần làm

Đảm bảo:

```text
repo/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── include/
├── src/
├── tests/
└── third_party/
```

---

# 11. P0-08 — Build system không nằm trong archive

Không thấy:

```text
CMakeLists.txt
CMakePresets.json
vcpkg.json
.vcxproj
.sln
Makefile
```

Điều này cực kỳ quan trọng vì project đang sử dụng nhiều dependency:

```text
SDL2
ImGui
OpenGL
gl3w
libmpv
FreeType
cpr
OpenSSL
D3D11
Win32
nlohmann/json
stb
WinToast
```

Không có build definition thì không thể xác định:

```text
include path
library path
compile definitions
link order
runtime DLLs
architecture
Debug/Release differences
```

### Ưu tiên

Đưa build system vào Git.

---

# 12. P1 — `WindowInitializer::InitializeSDL()` có bug dùng `isMaximized` cho flag Minimized

Code:

```cpp
.Minimized(runtime->state.display.isMaximized)
```

Đây gần như chắc chắn là typo.

Phải là:

```cpp
.Minimized(runtime->state.display.isMinimized)
```

Nếu không:

```text
isMaximized = true
→ SDL tạo window với SDL_WINDOW_MINIMIZED
```

---

# 13. P1 — `WindowManager` Singleton + mutable state không được synchronize đầy đủ

`WindowManager` có:

```cpp
std::unordered_map<WindowId, std::unique_ptr<WindowRuntime>> windows;
```

Nhưng:

```cpp
GetWindowById()
GetMainWindow()
GetAllWindows()
DestroyWindow()
GetWindowBySDLHandle()
```

không lock.

Trong khi project có nhiều thread.

Nếu một thread:

```cpp
DestroyWindow()
```

và thread khác:

```cpp
GetWindowById()
```

→ iterator/pointer lifetime race.

### Khuyến nghị

WindowManager nên có thread affinity rõ:

```text
WindowManager
    ↓
MAIN THREAD ONLY
```

Các worker thread không được trực tiếp mutate `windows`.

Worker gửi command:

```text
CreateWindowCommand
DestroyWindowCommand
ShowWindowCommand
```

vào queue.

---

# 14. P1 — `WindowRuntime` chứa quá nhiều trách nhiệm

Hiện tại `WindowRuntime` là kiểu:

```text
identity
SDL
Win32
graphics
ImGui
renderer
controller
state
property bag
MPV
relation
thread
resource
```

Nó trở thành "god object".

### Nên tách

```text
WindowRuntime
├── WindowIdentity
├── WindowState
├── WindowResources
├── WindowRelation
├── WindowInputState
├── WindowGraphicsContext
└── WindowServices
```

---

# 15. P1 — Raw pointer giữa Window và MPV

Hiện tại:

```cpp
PlayerSession* PlayerSession;
```

Window không sở hữu session.

Ownership:

```text
PlayerManager
    ↓ owns
PlayerSession
    ↑ raw pointer
WindowRuntime
```

Điều này có thể chạy được nhưng lifecycle rất dễ sai.

### Tốt hơn

```cpp
using PlayerSessionId = std::string;
```

Window lưu:

```cpp
PlayerSessionId sessionId;
```

và runtime service resolve:

```cpp
PlayerManager::GetSession(sessionId)
```

hoặc dùng một non-owning handle abstraction.

---

# 16. P1 — Child window dùng chung PlayerSession

Trong:

```cpp
WindowInitializer::AttachMPV()
```

```cpp
if (parent && parent->resource.PlayerSession) {
    runtime->resource.PlayerSession =
        parent->resource.PlayerSession;
}
```

Điều này có nghĩa:

```text
Parent Window
     │
     └── PlayerSession
           ↑
     Child Window
```

Nếu child và parent cùng gọi:

```cpp
GetRenderer()->Render()
```

thì cùng một `mpv_render_context` có thể được dùng từ nhiều window/context.

Đây là vùng cực kỳ nhạy cảm.

### Phải xác định rõ semantic

Child window là:

1. mirror cùng video?
2. controller?
3. detached player?
4. overlay?
5. independent player?

Nếu là independent player:

```text
1 Window = 1 Session
```

Nếu là mirror:

```text
1 Session
→ multiple presentation surfaces
```

nhưng cần một abstraction:

```text
PlaybackSession
    ↓
VideoOutput[]
```

Không nên đơn giản share raw pointer.

---

# 17. P1 — `PlayBackRenderThread::SetVideoSize()` lấy DefaultSession

Code:

```cpp
auto* session =
    PlayerManager::GetInstance().GetDefaultSession();
```

Trong class render thread của một session.

Đây là coupling sai.

Một render thread của session A không nên tự động thao tác:

```text
default session
```

### Đúng

`PlayBackRenderThread` phải sở hữu/nhận context:

```cpp
PlayerSessionId
```

hoặc trực tiếp reference tới state owner.

---

# 18. P1 — tất cả PlayBackRenderThread dùng cùng ThreadID

Code:

```cpp
GetThreadManager().Register("PlayBackRenderThread", &m_thread);
```

Nếu có nhiều session:

```text
Session A → PlayBackRenderThread
Session B → PlayBackRenderThread
Session C → PlayBackRenderThread
```

tất cả dùng:

```text
"PlayBackRenderThread"
```

Nếu `ThreadManager` không cho duplicate:

```text
Session B/C
→ collision
```

### Đúng

```cpp
"PlayBackRenderThread_" + sessionId
```

---

# 19. P1 — `PlayBackRender` tạo heap `weak_ptr` userdata nhưng không free

Code:

```cpp
new std::weak_ptr<PlayBackRenderThread>(m_renderThread)
```

Sau đó callback được đăng ký.

Comment cũng thừa nhận:

```cpp
// Cần giải phóng trong Shutdown()
```

Nhưng `Shutdown()` hiện tại chỉ:

```cpp
mpv_render_context_set_update_callback(
    m_render_ctx, nullptr, nullptr
);
```

Không:

```cpp
delete userdata;
```

### Hệ quả

Memory leak.

### Thiết kế tốt hơn

Lưu userdata:

```cpp
std::unique_ptr<std::weak_ptr<PlayBackRenderThread>>
    m_renderThreadCallbackData;
```

và cleanup trong destructor/shutdown.

---

# 20. P1 — callback lifecycle của MPV phải được xử lý trước thread destruction

Đúng thứ tự phải là:

```text
stop render requests
→ unregister callback
→ stop render thread
→ free render context
→ destroy player
```

Hiện tại lifecycle tương đối gần đúng nhưng callback userdata và thread ownership chưa được formalize.

Nên tạo explicit shutdown state machine:

```text
Running
  ↓
Stopping
  ↓
CallbacksDisabled
  ↓
RenderThreadStopped
  ↓
RenderContextDestroyed
  ↓
PlayerDestroyed
  ↓
Stopped
```

---

# 21. P1 — `OpenGLFrameBufferPool` có race ở `m_currentDisplayIndex`

`m_frames[i].state` là atomic.

Nhưng:

```cpp
int m_currentDisplayIndex = -1;
```

không atomic và không được lock.

Nếu:

```text
RenderThread
    → MarkAsReady()

UIThread
    → GetStableFrame()
```

thì `m_currentDisplayIndex` và các `FrameNode` field khác có thể bị truy cập đồng thời.

### Ngoài ra

Các field:

```cpp
contentW
contentH
allocatedW
allocatedH
fence
```

không atomic.

Chỉ atomic `state` không làm cả `FrameNode` thread-safe.

---

# 22. P1 — OpenGL fence đang bị dùng không đúng thread/context model

`MarkAsReady()`:

```cpp
glFenceSync(...)
glFlush()
```

được gọi từ render thread.

`GetStableFrame()`:

```cpp
glClientWaitSync(...)
```

được gọi từ UI thread.

Điều này có thể hợp lệ trong một số shared-context design, nhưng phải đảm bảo:

```text
shared GL object
+
proper sync visibility
+
context lifetime
+
fence ownership
```

được thiết kế rõ.

Hiện tại code chưa thể hiện invariant đó.

### Nên thiết kế

```text
Render Thread
    GL context R
    ↓
    render FBO
    ↓
    fence
    ↓
    publish FrameHandle atomically

UI Thread
    GL context U
    ↓
    wait fence
    ↓
    display texture
```

và frame state phải là một state machine thực sự.

---

# 23. P1 — `AcquireFreeBuffer()` fallback lấy READY buffer

Code:

```cpp
for (...) {
    if (state == READY) {
        state = RENDERING;
        return i;
    }
}
```

Điều này có nghĩa render thread có thể lấy một buffer mà UI vẫn có thể đang cần display.

Trong mô hình:

```text
READY
→ DISPLAYING
```

thì READY chưa chắc đã an toàn để overwrite nếu consumer chưa claim.

### Cần định nghĩa state transition

Ví dụ:

```text
FREE
 ↓
RENDERING
 ↓
READY
 ↓
DISPLAYING
 ↓
FREE
```

Không được:

```text
READY
 ↓
RENDERING
```

trừ khi có guarantee rằng consumer không còn dùng.

---

# 24. P1 — `AcquireFreeBuffer()` trả index 0 nếu hết buffer

Code:

```cpp
return 0;
```

Đây là behavior nguy hiểm.

Nếu không có buffer:

```text
buffer 0 có thể đang DISPLAYING
```

nhưng render thread vẫn lấy.

### Đúng

```cpp
return -1;
```

và caller:

```cpp
if (index < 0)
    continue;
```

Hoặc block/wait.

---

# 25. P1 — `UIRenderThread` bỏ qua return value của `MakeCurrent`

Code:

```cpp
m_graphicsBackend->MakeCurrent(
    currentWindow->resource.sdlWindow,
    m_graphicsContext
);
```

không kiểm tra kết quả.

Nếu context binding fail:

```text
Render tiếp
→ OpenGL calls vào context sai
→ undefined behavior / black screen
```

Nên:

```cpp
if (!m_graphicsBackend->MakeCurrent(...)) {
    SDL_Log(...);
    break;
}
```

---

# 26. P1 — `UIRenderThread` dùng owner runtime raw pointer xuyên suốt thread

Constructor nhận:

```cpp
WindowRuntime* owner
```

Thread chạy lâu.

Nếu WindowManager destroy runtime trước khi thread stop:

```text
UIRenderThread
    ↓
m_ownerRuntime
    ↓
freed WindowRuntime
```

→ use-after-free.

### Ownership rule bắt buộc

```text
WindowRuntime
    owns
UIRenderThread

UIRenderThread
    may reference WindowRuntime
```

và destructor:

```text
WindowRuntime destructor
    ↓
UIRenderThread::Stop()
    ↓
join
    ↓
destroy resources
```

phải được đảm bảo tuyệt đối.

---

# 27. P1 — `WindowRuntime` destructor không thể hiện rõ trong archive

Do nhiều dependency/header không nằm trong archive, không thể xác nhận cleanup đầy đủ.

Đây là một vấn đề cần kiểm tra trực tiếp trong bản working tree.

Đặc biệt phải đảm bảo:

```text
Stop UIRenderThread
Stop PlayBackRenderThread
Shutdown ImGui backend
Delete GL context
Destroy ImGui context
Destroy SDL window
```

đúng thứ tự.

---

# 28. P1 — ImGui backend shutdown chỉ được thực hiện khi final shutdown

`OpenGLBackend::Shutdown(bool isFinalShutdown)`:

```cpp
if (isFinalShutdown) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
}
```

Trong khi mỗi window có context/backend riêng.

Nếu nhiều window có backend instance riêng thì việc chỉ shutdown backend ở final shutdown có thể khiến lifecycle backend/window không đối xứng.

Cần xác định:

```text
backend lifetime
=
window lifetime?
```

hay:

```text
backend lifetime
=
application lifetime?
```

Không nên để hai khái niệm trộn nhau.

---

# 29. P1 — `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`

Code:

```cpp
SDL_GL_SetAttribute(
    SDL_GL_SHARE_WITH_CURRENT_CONTEXT,
    1
);
```

được đặt trước context creation.

Nhưng việc share context phụ thuộc context hiện tại.

Cần đảm bảo:

```text
Current context = expected main context
```

trước khi tạo subcontext.

Nếu không:

```text
share với context không mong muốn
```

hoặc không share.

### Nên encapsulate

```cpp
SDL_GLContext CreateSharedContext(
    SDL_Window* window,
    SDL_GLContext shareWith
);
```

---

# 30. P1 — `SDL_GL_MakeCurrent` global per-thread state

OpenGL context là thread-local.

Điều này nghĩa:

```text
Thread A → Context A
Thread B → Context B
```

là hợp lệ.

Nhưng code hiện tại phải đảm bảo không có hai subsystem cùng thao tác một SDL window/context ngoài owner thread.

Đây là lý do nên ghi rõ:

```text
Window X
    UI thread = T1
    GL context = C1

MPV render
    thread = T2
    GL context = C2
```

---

# 31. P1 — `OpenGLBackend` đang gánh quá nhiều trách nhiệm

Hiện tại backend làm:

```text
SDL GL context
ImGui backend
OpenGL state
MPV render params
FBO pool factory
swap
event processing
```

Nên tách:

```text
IGraphicsDevice
    ↓
OpenGLDevice

ImGuiRendererBackend
    ↓
OpenGLImGuiRenderer

MPVGraphicsBridge
    ↓
OpenGLMpvBridge
```

---

# 32. P1 — D3D11 backend đang ở trạng thái chưa hoàn thiện

Có:

```text
D3D11Backend.h
```

nhưng cần kiểm tra implementation và CMake linkage.

Nếu interface yêu cầu:

```cpp
GetGLInternalFormat()
GetGLFormat()
GetGLType()
```

thì abstraction này đã bị thiết kế theo OpenGL.

Đây là dấu hiệu:

> `IGraphicsBackend` hiện chưa thật sự graphics-API agnostic.

D3D11 không nên bị ép vào API:

```text
GLInternalFormat
GLFormat
GLType
```

### Kiến trúc tốt hơn

```cpp
struct VideoSurfaceFormat {
    ...
};

struct PlayBackRenderTarget {
    ...
};
```

và backend tự map sang GL/D3D11.

---

# 33. P1 — `std::any` được sử dụng quá rộng

Ví dụ:

```cpp
std::any CreateSubContext(...)
```

và:

```cpp
std::any fbo;
std::any texture;
std::any fence;
std::any texID;
```

### Vấn đề

Compile-time type safety bị mất.

Ví dụ:

```cpp
std::any_cast<GLuint>(...)
```

có thể throw:

```text
std::bad_any_cast
```

### Khuyến nghị

Dùng type abstraction:

```cpp
struct GraphicsContextHandle {
    void* native = nullptr;
};
```

hoặc backend-specific opaque handle.

Cho framebuffer:

```cpp
struct FrameHandle {
    uint64_t id;
};
```

Backend giữ mapping nội bộ.

---

# 34. P1 — `WindowPropertyBag` quá dynamic

`PropertyBag` tiện nhưng:

```text
string key
→ std::any
```

làm mất compile-time contract.

Ví dụ:

```cpp
properties.Set<bool>("RenderVideoFlag", true);
```

và ở nơi khác:

```cpp
GetValue<WindowLayout>("Layout")
```

Không có compiler đảm bảo key tồn tại.

### Nên giữ PropertyBag cho extension/plugin.

Nhưng core state nên dùng:

```cpp
WindowState
WindowLayout
PlaybackState
RenderState
```

typed.

---

# 35. P1 — Global state là technical debt lớn

`globals.cpp` chứa:

```cpp
int main_loop_rate;
UiWindowsState uiState;
std::vector<std::string> g_keywords;
std::mutex g_mutex;
std::string g_nextPageToken;
std::string g_searchQuery;
bool g_WindowVisible;
bool Disabehotkey;
bool playImmediately;
bool was_ui_video;
double pendingSeekTime;
std::vector<std::wstring> playlist;
```

Đây là state của nhiều domain khác nhau nhưng đặt chung một chỗ.

### Nên tách

```text
PlaybackState
SearchState
UIState
PlaylistState
HotkeyState
ApplicationState
```

---

# 36. P1 — `utils.cpp` là God Utility

Tài liệu nội bộ cũng đã ghi nhận vấn đề này.

Nếu `utils.cpp` chứa:

```text
MPV config
string utils
UI animation
path handling
network helper
application helper
```

thì dependency graph sẽ ngày càng rối.

### Tách thành

```text
utils/
├── PathUtils
├── StringUtils
├── ProcessUtils
├── SDLUtils
├── MPVConfig
├── TimeUtils
└── UIAnimation
```

---

# 37. P1 — API layer hiện chưa hoàn chỉnh

`api/` có:

```text
api_manager
api_types
GeminiProvider
```

Nhưng tài liệu nội bộ ghi:

```text
GeminiProvider::ExecuteRequest
```

vẫn là mock.

Ngoài ra:

```text
GPTProvider
YouTubeProvider
```

chưa hoàn thiện.

### Hướng đúng

```text
IAIProvider
├── GeminiProvider
├── OpenAIProvider
└── ...
```

và:

```text
ApiManager
    ↓
ProviderRegistry
    ↓
IAIProvider
```

Request/response phải typed.

---

# 38. P1 — API async contract chưa rõ

Nếu API call mất:

```text
100ms
1s
10s
```

thì không nên block UI thread.

Nên có:

```cpp
std::future<ApiResponse>
```

hoặc:

```cpp
Task<ApiResponse>
```

hoặc callback/event.

---

# 39. P1 — Python backend có state global quá nhiều

Ví dụ:

```python
search_keywords = {}
recent_videos = []
ranking_engine = RankingEngine()
YOUTUBE_QUOTA_EXCEEDED = False
QUOTA_RESET_AT = None
```

Các global này gây khó:

```text
test
concurrency
reset
multiple users
multiple sessions
```

### Nên chuyển thành service object

```text
RecommendationService
    ├── KeywordStore
    ├── RankingEngine
    ├── VideoCache
    └── YouTubeClient
```

---

# 40. P1 — Python key management có nguy cơ leak secret

`key_manager.py` có:

```python
print(
    f"[release] Key server unavailable, released key locally: {k}"
)
```

Điều này có thể ghi API key ra log.

### Không được làm

Nên:

```python
log.warning("Key server unavailable")
```

không in secret.

---

# 41. P1 — Python dùng `except:` quá rộng

Ví dụ:

```python
except:
```

Điều này bắt cả:

```text
KeyboardInterrupt
SystemExit
...
```

và làm mất root cause.

### Nên

```python
except requests.RequestException as e:
```

hoặc exception cụ thể.

---

# 42. P1 — `video_utils.weighted_random()` dùng `random.choices`

Code:

```python
random.choices(..., k=min(k, len(videos)))
```

cho phép cùng một item được chọn nhiều lần.

Nếu mục tiêu là chọn `k` video **không trùng**, nên dùng:

```python
random.choices(...)
```

không phù hợp.

Cần:

```python
random.sample(...)
```

hoặc weighted sampling không replacement.

---

# 43. P1 — `RankingEngine` là singleton global

```python
ranking_engine = RankingEngine()
```

Điều này khiến state ranking sống suốt process.

Nếu có:

```text
tab/session/user
```

thì dễ trộn state.

Nên inject instance.

---

# 44. P1 — `fetch_youtube_search()` làm nhiều request

Luồng:

```text
search request
    ↓
fetch IDs
    ↓
fetch details
```

Có thể rất tốn quota.

Cần có:

```text
quota accounting
cache
batch policy
rate limit
retry policy
```

và test riêng.

---

# 45. P1 — Network code cần centralized policy

Hiện tại network logic nằm rải rác.

Nên có:

```text
HttpClient
├── timeout
├── retry
├── TLS
├── logging
├── error mapping
└── metrics
```

Sau đó:

```text
YouTubeClient
GeminiClient
```

đều dùng `HttpClient`.

---

# 46. P2 — Naming inconsistency

Ví dụ:

```text
Disabehotkey
g_WindowVisible
Audio_visualizers
framerender
m_mpv_fbo
GetMpvApiType
```

Có nhiều convention.

Nên thống nhất:

```text
camelCase
PascalCase
m_member
```

Ví dụ:

```cpp
bool disableHotkey;
bool windowVisible;
bool audioVisualizers;
FrameTimer frameTimer;
```

---

# 47. P2 — Comment tiếng Việt + tiếng Anh trộn

Không phải lỗi runtime nhưng làm codebase khó maintain.

Nên chọn:

```text
Code identifiers = English
Comments = English
Architecture docs = Vietnamese hoặc English thống nhất
```

---

# 48. P2 — `main1.cpp` nên đổi thành `main.cpp`

Nếu đây là entry point thật:

```text
main1.cpp
```

gây ambiguity.

Nên:

```text
src/app/main.cpp
```

---

# 49. P2 — `FontManager.cpp` quá lớn

Khoảng vài trăm dòng và có nhiều trách nhiệm.

Nên tách:

```text
FontManager
├── FontDiscovery
├── FontLoader
├── FontRegistry
└── FontAtlasBuilder
```

---

# 50. P2 — UI files quá lớn

Các file như:

```text
gui_widgets.cpp
mpv_ui.cpp
mpv_ui_settings.cpp
shader_manager.cpp
sidebar_popup.cpp
popup_test.cpp
```

rất lớn.

Đây là dấu hiệu UI logic chưa được module hóa.

### Ví dụ

`mpv_ui.cpp`:

```text
PlaybackControls
SeekBar
VolumeControl
SubtitleControl
FullscreenControl
LoadingOverlay
GhostStatusOverlay
```

nên tách.

---

# 51. P2 — `popup_test.cpp` rất lớn

Nếu file này thực sự là test/demo popup nhưng chứa hàng trăm dòng UI production logic thì nên phân biệt:

```text
tests/
examples/
src/
```

Không nên để test/demo logic lẫn production UI.

---

# 52. Kiến trúc đề xuất

Kiến trúc mới nên hướng tới:

```text
                    Application
                         │
             ┌───────────┴───────────┐
             │                       │
        Runtime Kernel           UI Runtime
             │                       │
       ┌─────┼─────┐          ┌──────┼──────┐
       │     │     │          │      │      │
    Window Playback Media    ImGui  Popup  Input
    System Session Runtime
             │
        PlaybackSession
             │
      ┌──────┼─────────┐
      │      │         │
   Player  Renderer  Observer
      │      │
    libmpv  VideoOutput
             │
       GraphicsBackend
        ┌────┴────┐
      OpenGL    D3D11
```

---

# 53. Tách PlaybackSession khỏi Window

Đây là refactor quan trọng nhất.

Hiện tại:

```text
WindowRuntime
    └── PlayerSession*
```

Nên thành:

```text
PlaybackSession
    ├── Player
    ├── PlaybackState
    ├── CommandDispatcher
    ├── PropertySystem
    └── Observer
```

và:

```text
WindowRuntime
    └── VideoOutputHandle
```

Quan hệ:

```text
PlaybackSession
      ↓
VideoOutput
      ↓
WindowRuntime
```

Như vậy:

```text
1 playback
→ multiple windows/output
```

có thể hỗ trợ sạch.

---

# 54. Tách RenderService khỏi UI thread

Đề xuất:

```text
Main Thread
├── SDL events
├── Window state
├── ImGui
└── presentation

MPV Render Thread
├── mpv_render_context
├── GL sub-context
├── FBO
└── frame synchronization
```

Không để MPV thread render ImGui.

Đây là điểm quan trọng.

---

# 55. Frame pipeline đề xuất

```text
libmpv
   │
   ▼
MPV Render Thread
   │
   ▼
RenderTargetPool
   │
   ▼
FrameReady
   │
   ▼
UI/Main Thread
   │
   ▼
ImGui::Image(texture)
   │
   ▼
Swap
```

Frame metadata:

```cpp
struct VideoFrame {
    uint64_t sequence;
    uint32_t width;
    uint32_t height;
    VideoTextureHandle texture;
    FrameSyncHandle sync;
};
```

---

# 56. State machine cho FrameBuffer

Không nên chỉ dùng `atomic<BufferState>`.

Định nghĩa:

```text
FREE
 ↓
RENDERING
 ↓
GPU_PENDING
 ↓
READY
 ↓
DISPLAYING
 ↓
FREE
```

Transitions:

```text
RenderThread:
FREE → RENDERING
RENDERING → GPU_PENDING
GPU_PENDING → READY

UIThread:
READY → DISPLAYING
DISPLAYING → FREE
```

Không được:

```text
READY → RENDERING
```

trừ khi consumer đã release.

---

# 57. Thread model đề xuất

Mỗi thread phải có trách nhiệm rõ:

```text
Main Thread
    SDL event
    Window state
    ImGui

MPV Thread
    libmpv
    decode/render request
    GPU FBO

Network Thread
    API
    YouTube
    Gemini

Worker Threads
    CPU tasks
```

Không nên có:

```text
random thread
→ ImGui
→ WindowManager
→ MPV
```

---

# 58. Thread affinity rule

Tài liệu nên ghi rõ:

```text
SDL Window API:
    MAIN THREAD

WindowManager:
    MAIN THREAD

ImGui Context:
    OWNER THREAD ONLY

ImGui Backend:
    OWNER THREAD ONLY

MPV Render Context:
    MPV RENDER THREAD ONLY

OpenGL Context:
    ONE OWNER THREAD AT A TIME

FrameBufferPool:
    shared through explicit synchronization
```

---

# 59. Ownership rule

Quy tắc mới:

```text
Application
 ├── WindowManager
 │    └── WindowRuntime
 │         ├── Renderer
 │         ├── GraphicsContext
 │         └── UIThread
 │
 └── PlaybackManager
      └── PlaybackSession
           ├── Player
           ├── PlayBackRender
           └── PlayBackRenderThread
```

Không:

```text
raw pointer ownership
```

Nếu non-owning pointer bắt buộc phải ghi rõ:

```cpp
// non-owning; lifetime owned by PlaybackManager
PlaybackSession* session;
```

---

# 60. Error handling

Hiện tại có:

```text
bool
nullptr
exception
SDL_Log
SDL_ShowSimpleMessageBox
```

quá nhiều kiểu.

Nên thống nhất.

Ví dụ:

```cpp
enum class ErrorCode {
    None,
    InvalidArgument,
    SDLInitFailed,
    GraphicsInitFailed,
    MpvInitFailed,
    ResourceUnavailable
};

template<class T>
using Result = Expected<T, ErrorCode>;
```

Không phải mọi function đều cần exception.

---

# 61. Logging

Nên có:

```text
Logger
├── Debug
├── Info
├── Warning
├── Error
└── Critical
```

và category:

```text
[APP]
[WINDOW]
[MPV]
[GL]
[THREAD]
[NETWORK]
[UI]
```

Ví dụ:

```text
[ERROR][MPV][RenderThread]
Failed to create render context
```

---

# 62. Build system đề xuất

Root:

```text
CMakeLists.txt
CMakePresets.json
vcpkg.json
```

`vcpkg.json` tối thiểu:

```json
{
  "name": "im-player",
  "version-string": "1.0.0",
  "dependencies": [
    "sdl2",
    "cpr",
    "openssl",
    "freetype",
    "gtest"
  ]
}
```

Nếu libmpv không dùng vcpkg:

```text
third_party/
└── mpv/
    ├── include/
    ├── lib/
    └── bin/
```

---

# 63. CMake target structure

Không nên có một target khổng lồ:

```text
Im_player
```

nối tất cả.

Nên:

```text
im_core
im_window
im_graphics
im_playback
im_ui
im_network
im_app
```

Dependency:

```text
im_core
   ↑
im_graphics
   ↑
im_playback
   ↑
im_window
   ↑
im_ui
   ↑
im_app
```

Không để:

```text
utils.cpp
```

làm dependency của gần như mọi module.

---

# 64. Test architecture

Đây là phần cần ưu tiên nếu bạn quay lại project để viết test.

## Unit

```text
tests/unit/
├── core/
├── window/
├── playback/
├── command/
├── property/
├── frame/
└── ranking/
```

Test:

```text
WindowPropertyBag
PlaybackState
CommandDispatcher
FrameStateMachine
RankingEngine
TextUtils
```

Không cần GPU.

---

# 65. Component tests

```text
tests/component/
├── window_manager/
├── playback_session/
├── mpv_observer/
└── render_pipeline/
```

Ví dụ:

```text
PlaybackSession
    ↓
Command
    ↓
State
    ↓
Observer
```

---

# 66. Integration tests

```text
tests/integration/
├── sdl/
├── opengl/
├── mpv/
├── youtube/
└── api/
```

Đây mới test:

```text
real SDL
real OpenGL
real libmpv
real network
```

Không đưa những thứ này vào unit test.

---

# 67. Test các bug hiện tại trước

Các test đầu tiên nên bắt những lỗi đã phát hiện.

## Test 1 — Window creation

```cpp
TEST(WindowManagerTest, FailedCreationDoesNotCrash);
```

## Test 2 — Queue creation

```cpp
TEST(WindowManagerTest, QueuedCreationProducesValidWindowId);
```

## Test 3 — Destroy

```cpp
TEST(WindowManagerTest, DestroyedWindowIsNoLongerAccessible);
```

## Test 4 — Parent-child

```cpp
TEST(WindowRelationTest, DestroyParentDestroysChildren);
```

## Test 5 — Frame pool

```cpp
TEST(FrameBufferPoolTest, DoesNotReuseDisplayingFrame);
```

## Test 6 — Frame exhaustion

```cpp
TEST(FrameBufferPoolTest, ReturnsNoBufferWhenAllBuffersAreBusy);
```

## Test 7 — Playback lifecycle

```cpp
TEST(PlaybackSessionTest, ShutdownIsIdempotent);
```

## Test 8 — Thread lifecycle

```cpp
TEST(ThreadLifecycleTest, StopJoinsThreadBeforeOwnerDestruction);
```

---

# 68. Không viết test theo implementation

Sai:

```cpp
EXPECT_EQ(session.internalQueue.size(), 1);
```

nếu queue là implementation detail.

Đúng:

```cpp
session.play();

EXPECT_EQ(
    session.state().playback,
    PlaybackState::Playing
);
```

Test:

```text
contract
```

không phải:

```text
private implementation
```

---

# 69. Roadmap phát triển

## Phase 0 — Freeze feature

Tạm dừng:

```text
new UI
new shader features
new audio AI
new provider
new recommendation logic
```

Cho tới khi build/test ổn.

---

## Phase 1 — Reproducible Build

Làm:

```text
CMake
CMakePresets
vcpkg
dependency lock/version
Debug build
Release build
```

Mục tiêu:

```text
clone repo
→ configure
→ build
```

trên máy mới.

---

## Phase 2 — Fix P0

Bắt buộc xử lý:

```text
QueueCreateWindow dangling reference
mainWin nullptr
double destroy
OpenGLBackend extra qualification
missing headers/build files
```

---

## Phase 3 — Thread model

Thiết kế chính thức:

```text
Main thread
UI thread
MPV render thread
Network workers
```

và thread affinity.

---

## Phase 4 — Ownership

Loại bỏ:

```text
global ownership
raw owning pointers
ambiguous singleton lifecycle
```

Tập trung ownership vào:

```text
Application
WindowManager
PlaybackManager
```

---

## Phase 5 — Rendering

Ổn định:

```text
OpenGL context
ImGui context
FBO pool
fence
frame publication
resize
```

Chỉ sau khi ổn định mới tối ưu FPS.

---

## Phase 6 — Tests

Mục tiêu ban đầu:

```text
Unit tests: 70%+ core
Component tests: critical paths
Integration: MPV/SDL/GL
E2E: startup/play/seek/pause/close
```

Không nhất thiết đạt coverage cao bằng mọi giá.

---

# 70. Phase 7 — Feature development

Sau đó mới phát triển:

```text
Audio AI
Subtitle
Translation
TTS
Shader pipeline
Recommendation
Cloud API
Plugin system
```

---

# 71. Kiến trúc tương lai đề xuất

```text
                    ┌───────────────────┐
                    │    Application    │
                    └─────────┬─────────┘
                              │
             ┌────────────────┼────────────────┐
             │                │                │
             ▼                ▼                ▼
      WindowRuntime      PlaybackRuntime   ServiceRuntime
             │                │                │
             ▼                ▼                ▼
        WindowManager    PlaybackManager    API Manager
             │                │                │
             ▼                ▼                ├── Gemini
        WindowRuntime    PlaybackSession      ├── YouTube
             │                │               └── HTTP
             │                │
             ▼                ▼
         UI Renderer       MPV Player
             │                │
          ImGui              MPV
             │                │
             ▼                ▼
      GraphicsDevice      PlayBackRenderer
             │                │
       ┌─────┴─────┐          ▼
       │           │      RenderTarget
      OpenGL      D3D11       Pool
```

---

# 72. Nguyên tắc thiết kế cần giữ

## Rule 1

```text
Window != Playback
```

## Rule 2

```text
UI != Rendering engine
```

## Rule 3

```text
PlaybackSession owns MPV
```

## Rule 4

```text
WindowManager owns WindowRuntime
```

## Rule 5

```text
One thread owns one ImGui context
```

## Rule 6

```text
One thread owns one GL context at a time
```

## Rule 7

```text
No raw pointer as ownership
```

## Rule 8

```text
Core state must be typed
```

## Rule 9

```text
PropertyBag only for dynamic extension
```

## Rule 10

```text
Tests verify behavior
```

---

# 73. Danh sách lỗi ưu tiên

| ID | Vấn đề | Severity | Hành động |
|---|---|---:|---|
| P0-01 | nullptr dereference khi main window creation fail | P0 | Fix ngay |
| P0-02 | dangling reference trong QueueCreateWindow | P0 | Thiết kế lại API |
| P0-03 | main window double destroy/use-after-free | P0 | Fix lifecycle |
| P0-04 | ImGui multi-thread access | P0 | Thiết kế lại thread model |
| P0-05 | ImGui backend event/render cross-thread | P0 | Enforce thread affinity |
| P0-06 | Extra qualification trong OpenGLBackend | P0 | Fix compile |
| P0-07 | Missing internal headers trong archive | P0 | Khôi phục source/build |
| P0-08 | Missing CMake/vcpkg/build definition | P0 | Rebuild build system |
| P1-01 | Minimized dùng isMaximized | P1 | Fix typo |
| P1-02 | WindowManager không có thread affinity | P1 | Main-thread ownership |
| P1-03 | WindowRuntime god object | P1 | Refactor |
| P1-04 | raw PlayerSession pointer | P1 | Handle/reference model |
| P1-05 | Child share PlayerSession chưa rõ semantic | P1 | Playback/Output split |
| P1-06 | SetVideoSize dùng default session | P1 | Inject owner |
| P1-07 | PlayBackRenderThread ID collision | P1 | Unique thread ID |
| P1-08 | weak_ptr userdata leak | P1 | RAII callback data |
| P1-09 | FrameBufferPool race | P1 | Formal state machine |
| P1-10 | AcquireFreeBuffer fallback unsafe | P1 | Return -1/block |
| P1-11 | UIRenderThread MakeCurrent result ignored | P1 | Check failure |
| P1-12 | UIRenderThread raw owner lifetime | P1 | Stop-before-destroy |
| P1-13 | GL fence cross-thread lifecycle | P1 | Explicit synchronization |
| P1-14 | std::any used too broadly | P1 | Typed handles |
| P1-15 | PropertyBag core state | P1 | Typed state |
| P1-16 | Global state | P1 | Domain state objects |
| P1-17 | Python secret logging | P1 | Remove key output |
| P1-18 | Python broad except | P1 | Specific exceptions |
| P1-19 | Python global ranking state | P1 | Dependency injection |
| P2-01 | Naming inconsistency | P2 | Style pass |
| P2-02 | Huge UI files | P2 | Module split |
| P2-03 | main1.cpp | P2 | Rename/split |
| P2-04 | utils.cpp God utility | P2 | Split |
| P2-05 | API mock provider | P2 | Provider architecture |

---

# 74. Thứ tự sửa code cụ thể

Nếu bắt đầu lại ngay hôm nay, thứ tự nên là:

```text
01. Khôi phục CMakeLists.txt
02. Khôi phục vcpkg.json
03. Xác định toàn bộ dependency
04. Build Debug sạch
05. Fix compile blockers
06. Fix QueueCreateWindow
07. Fix main window lifecycle
08. Fix WindowManager ownership
09. Tắt UIRenderThread multi-window tạm thời
10. Đưa ImGui về một owner thread
11. Làm MPV render thread độc lập
12. Thiết kế FrameBuffer state machine
13. Test FrameBuffer
14. Test WindowManager
15. Test PlaybackSession
16. Test MPV integration
17. Refactor globals
18. Refactor utils
19. Tách Playback khỏi Window
20. Sau đó mới thêm feature
```

---

# 75. Kiến trúc migration an toàn

Không nên rewrite toàn bộ project một lần.

## Bước 1

Giữ code hiện tại:

```text
Legacy
```

và tạo:

```text
runtime/
```

## Bước 2

Đưa state mới vào:

```text
PlaybackSession
```

nhưng MPV implementation cũ vẫn chạy.

## Bước 3

Window chỉ giữ:

```text
VideoOutputHandle
```

## Bước 4

Di chuyển PlayerManager thành:

```text
PlaybackManager
```

## Bước 5

Xóa dần:

```text
globals.cpp
```

## Bước 6

Cuối cùng xóa các compatibility layer.

---

# 76. Đánh giá điểm mạnh của code hiện tại

Không nên chỉ nhìn vào lỗi. Project có một số nền tảng tốt:

### 1. Đã có abstraction graphics

```text
IGraphicsBackend
```

Đây là hướng đúng.

### 2. MPV đã được chia thành component

```text
Player
Render
Observer
Property
Command
Session
Manager
```

Đây là cấu trúc tốt hơn một MPV wrapper khổng lồ.

### 3. Có ý tưởng render thread

Tách:

```text
MPV render
```

khỏi:

```text
UI
```

là hướng đúng.

Vấn đề là synchronization chưa hoàn thiện.

### 4. Có FrameBufferPool

Đây là nền tảng tốt để xây dựng:

```text
triple buffering
```

### 5. Có WindowTemplate

```text
WindowTemplateBuilder
WindowFactory
WindowRuntime
```

là hướng tốt cho multi-window.

### 6. Có vạch rõ các technical debt

Các `FOLDER_INFO.md` cho thấy project đã tự nhận diện nhiều vấn đề.

Điều này rất tốt cho refactoring.

---

# 77. Những thứ KHÔNG nên làm tiếp lúc này

Không nên tiếp tục ngay:

```text
❌ thêm AI audio
❌ thêm shader framework mới
❌ thêm nhiều window type
❌ thêm provider mới
❌ tối ưu FPS
❌ thêm network features
❌ thêm recommendation algorithm
```

trước khi xử lý:

```text
❗ build
❗ lifecycle
❗ thread
❗ rendering
❗ ownership
❗ tests
```

---

# 78. Definition of Done cho giai đoạn stabilization

Project chỉ nên chuyển sang feature development khi:

```text
[ ] Clone repo trên máy mới
[ ] vcpkg install
[ ] CMake configure
[ ] Debug build
[ ] Release build
[ ] Unit tests pass
[ ] Application starts
[ ] Main window opens
[ ] MPV session initializes
[ ] Play local video
[ ] Pause
[ ] Seek
[ ] Resize
[ ] Fullscreen
[ ] Open/close child window
[ ] Close application
[ ] No crash
[ ] No known use-after-free
[ ] No known race in render pipeline
[ ] Clean shutdown
```

---

# 79. Kết luận

Project hiện tại **không phải là codebase cần bỏ đi**.

Nó đang ở trạng thái:

```text
Prototype
   ↓
Feature-rich prototype
   ↓
Architecture emerging
   ↓
Technical debt accumulation
   ↓
Cần stabilization/refactor
   ↓
Production architecture
```

Điểm nguy hiểm nhất không phải là một bug đơn lẻ.

Điểm nguy hiểm là:

```text
Window
+ ImGui
+ OpenGL
+ MPV
+ multiple threads
+ global state
+ singleton
+ raw pointers
+ dynamic any
```

đang giao thoa với nhau.

Nếu tiếp tục thêm feature trước khi xử lý những boundary này, mỗi feature mới sẽ làm dependency graph phức tạp hơn.

### Chiến lược đúng

```text
              CURRENT
                 │
                 ▼
       ┌───────────────────┐
       │ Stabilize Build   │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Fix Ownership     │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Fix Thread Model  │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Fix Render Model  │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Add Unit Tests    │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Integration Tests │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Refactor Globals  │
       └─────────┬─────────┘
                 ▼
       ┌───────────────────┐
       │ Feature Development│
       └───────────────────┘
```

**Ưu tiên tuyệt đối:** `WindowManager` lifecycle → ImGui thread ownership → MPV render thread → FrameBufferPool → build system → tests.

---

# 80. Phụ lục — Các file nên kiểm tra đầu tiên

```text
src/main1.cpp
src/globals.cpp

src/windows/WindowManager.h
src/windows/WindowRuntime.h
src/windows/WindowInitializer.cpp
src/windows/WindowProcHook.cpp
src/windows/UIRenderThread.cpp
src/windows/WindowController.cpp

src/backends/IGraphicsBackend.h
src/backends/OpenGLBackend.h
src/backends/D3D11Backend.h

src/mpv/session/PlayerSession.cpp
src/mpv/session/PlayerManager.cpp
src/mpv/player/Player.cpp
src/mpv/render/PlayBackRender.cpp
src/mpv/render/PlayBackRenderThread.cpp
src/mpv/render/OpenGLFrameBufferPool.cpp
src/mpv/render/IFrameBufferPool.h

src/mpv/event/PlaybackObserver.cpp
src/mpv/property/PlayBackProperty.cpp
src/mpv/command/PlaybackCommand.cpp

src/api/api_manager.cpp
src/api/provider/gemini.h

src/backends/vid/services/youtube_fetch.py
src/backends/vid/services/key_manager.py
src/backends/vid/services/ranking_engine.py
src/backends/vid/utils/video_utils.py
```

---

# 81. Ghi chú về độ tin cậy của phân tích

Các lỗi được chia thành:

- **Definite:** có thể thấy trực tiếp từ source, ví dụ dangling reference trong `QueueCreateWindow`.
- **High confidence:** code cho thấy lifecycle/thread contract không an toàn, ví dụ ImGui multi-thread.
- **Needs build confirmation:** cần CMake/compiler/dependency thật để xác nhận, ví dụ một số include/ABI/linking issue.
- **Architectural concern:** chưa chắc là crash nhưng sẽ gây khó mở rộng, ví dụ `std::any`, global state, God object.

Archive hiện tại **không chứa build configuration**, vì vậy bước tiếp theo để xác nhận toàn bộ compile/link errors là lấy bản project có:

```text
CMakeLists.txt
CMakePresets.json
vcpkg.json
third_party/
libs/
include/
```

Nếu thiếu các file đó thì cần phục hồi chúng trước khi chạy một vòng build/test đáng tin cậy.
