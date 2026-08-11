# Phân tích toàn diện mã nguồn `src(6).zip`

> **Phạm vi:** phân tích tĩnh toàn bộ mã nguồn C++/C/Python có trong `src(6).zip`.
>
> **Mục tiêu:** đánh giá kiến trúc hiện tại, ownership/lifecycle, đa luồng, rendering, MPV, ImGui/SDL, backend Python/YouTube, API provider, các lỗi hiện hữu, lỗi nguy hiểm, khả năng phát triển và lộ trình refactor.
>
> **Lưu ý:** ZIP chỉ chứa thư mục `src/`, không có CMake/build configuration, dependency lockfile, test suite đầy đủ hoặc binary runtime. Vì vậy báo cáo này là **static source audit**, không phải xác nhận build/runtime 100%. Các lỗi được đánh dấu **[XÁC NHẬN TỪ SOURCE]** là những vấn đề có thể suy ra trực tiếp từ mã nguồn.

---

# 1. Executive Summary

## 1.1 Đánh giá tổng thể

Mã nguồn hiện tại có một nền tảng kiến trúc **khá tốt về ý tưởng**, đặc biệt ở các điểm:

- Đã bắt đầu tách `WindowRuntime`, `WindowManager`, `WindowFactory`, `WindowTemplate`.
- Đã có `PlayerSession` thay vì để toàn bộ MPV nằm trực tiếp trong UI.
- Đã tách `Player`, `PlaybackCommandDispatcher`, `PlayBackProperty`, `PlaybackObserver`.
- Đã xây dựng `PlayerStateSystem` với snapshot/read/write và lock riêng cho từng nhóm state.
- Đã có hướng `IGraphicsBackend` để mở đường cho OpenGL/D3D11.
- Đã có `PlayBackRenderThread` và FBO pool cho pipeline render bất đồng bộ.
- Backend Python đã được tách khỏi client C++ thông qua HTTP/FastAPI.
- Hệ thống ranking đã có plugin model (`FreshnessPlugin`, `UserHistoryPlugin`, `TabAffinityPlugin`).
- API provider đã có abstraction `IAPIProvider` và queue worker.

Tuy nhiên, **kiến trúc triển khai hiện tại vẫn đang ở trạng thái prototype/transition**, chưa phải production architecture.

Nguyên nhân chính không phải do thiếu module mà là:

1. Ownership giữa `WindowRuntime` ↔ `PlayerSession` chưa hoàn chỉnh.
2. Threading model đang trộn nhiều mô hình khác nhau.
3. Raw pointer và lifetime của object cross-thread còn nhiều.
4. OpenGL/ImGui context ownership chưa được định nghĩa đủ chặt.
5. FBO pool có race/lifecycle issue.
6. Có lỗi deadlock trực tiếp trong `PlayBackRenderThread::SetVideoSize()`.
7. Có bug ID session nghiêm trọng trong `PlayerManager::CreateSession()`.
8. Có lỗi `std::any_cast` không phù hợp trong `OpenGLFrameBufferPool`.
9. `WindowManager` expose iterator trực tiếp lên `unordered_map` nội bộ.
10. Python backend có shared global state và cache chưa có concurrency model rõ ràng.
11. API key YouTube đang hard-code trong source.
12. `/thumbnail` có khả năng trở thành SSRF proxy.
13. `tls-verify=no` làm giảm an toàn TLS.
14. D3D11 backend mới chỉ là skeleton, chưa thực sự tương thích với kiến trúc render thread.
15. API provider còn mock và có vấn đề về state synchronization.
16. Có nhiều phần legacy/global state cùng tồn tại với state model mới.

## 1.2 Mức độ rủi ro

| Khu vực | Mức rủi ro | Nhận xét |
|---|---|---|
| Player/MPV lifecycle | 🔴 Critical | Có khả năng UAF/leak/lifecycle sai |
| Render thread | 🔴 Critical | Deadlock + race + context ownership |
| FBO pool | 🔴 Critical | Cross-thread GPU resource race |
| WindowManager | 🔴 Critical | Iterator và raw pointer lifetime |
| ImGui multi-thread | 🔴 Critical | Backend/context model chưa an toàn |
| Security backend | 🔴 Critical | Hard-coded key + SSRF + TLS verify disabled |
| Python cache | 🟠 High | Global mutable state, thread-safety chưa rõ |
| API Manager | 🟠 High | Provider/state/worker lifecycle chưa hoàn chỉnh |
| State model | 🟡 Medium | Hướng đúng nhưng còn duplicate legacy state |
| Architecture modularity | 🟡 Medium | Tiềm năng tốt, nhưng coupling vẫn cao |
| UI code size | 🟠 High | `gui_widgets.cpp` ~130 KB, khó maintain |
| Testing | 🔴 Critical | Chưa thấy test infrastructure trong ZIP |

---

# 2. Cấu trúc hiện tại

## 2.1 Các subsystem chính

```text
Application
│
├── Main Thread
│   ├── SDL event loop
│   ├── WindowManager
│   ├── Window creation/destruction
│   ├── state update
│   └── command dispatch
│
├── Window System
│   ├── WindowFactory
│   ├── WindowTemplate
│   ├── WindowRuntime
│   ├── WindowController
│   ├── WindowRelation
│   ├── WindowResource
│   ├── WindowSharedGroup
│   └── UIRenderThread
│
├── Graphics
│   ├── IGraphicsBackend
│   ├── OpenGLBackend
│   ├── D3D11Backend
│   └── FrameBufferPool
│
├── Player
│   ├── Player
│   ├── PlayerSession
│   ├── PlayerManager
│   ├── PlaybackCommandDispatcher
│   ├── PlayBackProperty
│   ├── PlaybackObserver
│   ├── PlayerStateSystem
│   ├── PlayBackRender
│   └── PlayBackRenderThread
│
├── UI
│   ├── ImGui
│   ├── MainWindowRenderer
│   ├── controls
│   ├── overlays
│   ├── settings
│   └── popup system
│
├── API
│   ├── APIManager
│   ├── IAPIProvider
│   └── GeminiProvider
│
└── Python Video Service
    ├── FastAPI
    ├── YouTube fetcher
    ├── caches
    ├── keyword manager
    ├── ranking
    └── thumbnail proxy/cache
```

---

# 3. Kiến trúc hiện tại: điểm mạnh

## 3.1 `PlayerSession` là hướng kiến trúc đúng

`PlayerSession` gom:

```text
Player
Renderer
Observer
Commander
Property
State
RenderThread
```

Đây là một bước tiến lớn so với mô hình global MPV.

Mục tiêu đúng nên là:

```text
Window
   │
   ▼
PlayerSessionHandle
   │
   ├── Player
   ├── State
   ├── Command
   ├── Observer
   ├── Renderer
   └── RenderThread
```

Thay vì:

```text
Window
 ├── global Player
 ├── global PlaybackState
 ├── global VideoInfo
 ├── global playlist
 └── global render state
```

## 3.2 `PlayerStateSystem` là phần tốt nhất trong refactor hiện tại

`PlayerStateSystem` đã chia state thành:

```text
Playback
Media
Video
Audio
Subtitle
Track
Playlist
Network
```

và có:

```cpp
ReadX(...)
WriteX(...)
GetXModel()
GetFullState()
```

Đây là abstraction có khả năng phát triển tốt.

Định hướng này phù hợp để sau này xây:

```text
MPV Event
   ↓
PlaybackObserver
   ↓
State Reducer / State Writer
   ↓
PlayerStateSystem
   ↓
UI / Analytics / AI / Automation
```

## 3.3 `IGraphicsBackend` có giá trị dài hạn

Interface:

```cpp
IGraphicsBackend
```

cho phép hướng tới:

```text
OpenGL
D3D11
D3D12
Vulkan
Metal
```

Tuy nhiên implementation hiện tại chưa đủ abstraction để đạt mục tiêu đó.

## 3.4 Python service tách khỏi C++

Việc C++ gọi:

```text
127.0.0.1:8000
```

thay vì nhúng toàn bộ YouTube logic vào C++ là lựa chọn hợp lý.

Điều này cho phép:

- update ranking độc lập;
- thay đổi API client;
- cache riêng;
- AI recommendation;
- telemetry;
- Python ecosystem;
- async HTTP.

---

# 4. Vấn đề kiến trúc lớn nhất

## 4.1 Hiện tại đang tồn tại 3 state model cùng lúc

Có:

```text
A. PlayerStateSystem
B. globals.cpp / mpv_data.cpp
C. Window PropertyBag
```

Ví dụ:

```cpp
GetMPVPlaybackStatus()
GetVideoInfo()
```

vẫn dùng global mirror.

Trong khi:

```cpp
PlayerSession
 └── PlayerStateSystem
```

đã là nguồn state mới.

Điều này tạo ra:

```text
MPV
 ├── StateSystem
 ├── g_playbackStatus
 └── g_videoInfo
```

và dễ dẫn tới:

```text
UI đọc A
logic đọc B
observer ghi A
một phần code ghi B
```

Kết quả là state drift.

### Khuyến nghị

Chọn duy nhất:

```text
PlayerStateSystem = authoritative state
```

Sau đó legacy:

```cpp
GetMPVPlaybackStatus()
GetVideoInfo()
```

chỉ nên là adapter tạm thời:

```text
PlayerStateSystem
      ↓
LegacyAdapter
      ↓
old API
```

và đánh dấu deprecated.

---

# 5. LỖI NGUY HIỂM P0/P1

# P0-01 — Deadlock trực tiếp trong `SetVideoSize`

File:

```text
src/player/render/PlayBackRenderThread.cpp
```

Khoảng dòng 56-62.

Code:

```cpp
void PlayBackRenderThread::SetVideoSize(int w, int h) {
    std::lock_guard lock(state.mtx);

    if (w != state.surface.drawW || h != state.surface.drawH) {
        state.surface.newW = w;
        state.surface.newH = h;
        state.surface.needResize = true;
        RequestRender();
    }
}
```

Trong khi `RequestRender()` lại:

```cpp
void PlayBackRenderThread::RequestRender() {
    {
        std::lock_guard lock(state.mtx);
        state.needRender = true;
    }
    Notify();
}
```

Do đó:

```text
SetVideoSize()
  └── lock(state.mtx)
       └── RequestRender()
            └── lock(state.mtx)  ← DEADLOCK
```

`std::mutex` không recursive.

### Hậu quả

Khi resize xảy ra:

```text
thread bị treo
render không tiếp tục
UI có thể đứng/chớp/đứng video
Shutdown có thể join thread đang deadlock
```

### Cách sửa

Không gọi `RequestRender()` khi đang giữ mutex:

```cpp
void PlayBackRenderThread::SetVideoSize(int w, int h) {
    bool notify = false;

    {
        std::lock_guard lock(state.mtx);

        if (w != state.surface.drawW || h != state.surface.drawH) {
            state.surface.newW = w;
            state.surface.newH = h;
            state.surface.needResize = true;
            state.needRender = true;
            notify = true;
        }
    }

    if (notify) {
        Notify();
    }
}
```

**Ưu tiên sửa ngay.**

---

# P0-02 — Bug Session ID trong `PlayerManager::CreateSession`

File:

```text
src/player/session/PlayerManager.cpp
```

Code hiện tại:

```cpp
std::string sessionId = id.empty() ? GenerateUniqueSessionId() : id;

if (m_sessions.count(id))
{
    return nullptr;
}

auto session = std::make_unique<PlayerSession>(id);

...

m_defaultSessionId = id;

m_sessions[id] = std::move(session);
```

Đây là bug logic rõ ràng.

Bạn đã tạo:

```cpp
sessionId
```

nhưng sau đó vẫn dùng:

```cpp
id
```

Nếu gọi:

```cpp
CreateSession(runtime)
```

thì:

```cpp
id == ""
sessionId == "mpv_session_..."
```

nhưng object vẫn được tạo bằng:

```cpp
PlayerSession("")
```

và map bằng:

```cpp
m_sessions[""]
```

### Hậu quả

- tất cả auto-generated session có thể va vào key `""`;
- `GetDefaultSession()` có thể trả session sai;
- session ID trong `WindowResource` không khớp;
- multi-player/multi-window bị phá.

### Sửa

```cpp
const std::string sessionId =
    id.empty() ? GenerateUniqueSessionId() : id;

if (m_sessions.contains(sessionId)) {
    return nullptr;
}

auto session = std::make_unique<PlayerSession>(sessionId);

if (session->Init(runtime)) {
    if (m_defaultSessionId.empty()) {
        m_defaultSessionId = sessionId;
    }

    auto* ptr = session.get();
    m_sessions.emplace(sessionId, std::move(session));
    return ptr;
}
```

**Ưu tiên sửa ngay.**

---

# P0-03 — `OpenGLFrameBufferPool.cpp` sử dụng `std::any_cast` sai abstraction

File:

```text
src/player/render/OpenGLFrameBufferPool.cpp
```

`FrameNode` hiện tại:

```cpp
struct FrameNode {
    GraphicsFboHandle fbo;
    GraphicsTextureHandle texture;
};
```

Trong đó:

```cpp
using GraphicsFboHandle = uint32_t;
using GraphicsTextureHandle = uint32_t;
```

Nhưng `ResizeFrame()` lại:

```cpp
GLuint texture = std::any_cast<GLuint>(frame.texture);
GLuint fbo = std::any_cast<GLuint>(frame.fbo);
```

`frame.texture` không phải `std::any`.

### Đây là dấu hiệu code cũ và code mới đang bị trộn.

Interface đã được refactor từ:

```text
std::any handle
```

sang:

```text
typed handle
```

nhưng implementation chưa được sửa hoàn toàn.

### Sửa

Chỉ cần:

```cpp
GLuint texture = static_cast<GLuint>(frame.texture);
GLuint fbo = static_cast<GLuint>(frame.fbo);
```

Hoặc tốt hơn:

```cpp
const auto texture = frame.texture;
const auto fbo = frame.fbo;
```

và chỉ conversion ở boundary OpenGL.

**Đây là lỗi cần kiểm tra build ngay.**

---

# P0-04 — Callback MPV render dùng heap userdata nhưng không giải phóng

File:

```text
src/player/render/PlayBackRender.cpp
```

Code:

```cpp
mpv_render_context_set_update_callback(
    m_render_ctx,
    [](void* userdata) {
        auto* renderThreadPtr =
            static_cast<std::weak_ptr<PlayBackRenderThread>*>(userdata);

        if (renderThreadPtr) {
            if (auto thread = renderThreadPtr->lock()) {
                thread->RequestRender();
            }
        }
    },
    new std::weak_ptr<PlayBackRenderThread>(m_renderThread)
);
```

Bạn tạo:

```cpp
new std::weak_ptr<PlayBackRenderThread>
```

nhưng `Shutdown()` không:

```cpp
delete userdata;
```

### Hậu quả

Mỗi `PlayBackRender::Init()` làm leak một object.

Nếu session/window tạo-hủy nhiều lần:

```text
Create
 → new weak_ptr
Destroy
 → leak
Create
 → new weak_ptr
Destroy
 → leak
...
```

### Sửa tốt nhất

Không dùng raw heap userdata.

Có thể lưu callback context thành member:

```cpp
std::shared_ptr<RenderCallbackContext> m_callbackContext;
```

hoặc:

```cpp
std::unique_ptr<std::weak_ptr<PlayBackRenderThread>> m_callbackUserdata;
```

và bảo đảm:

```text
callback disabled
→ render context free
→ userdata destroyed
```

thứ tự lifecycle phải rõ ràng.

---

# P0-05 — FBO pool bị truy cập từ UI thread và render thread mà không có ownership protocol đầy đủ

`PlayBackRenderThreadState` chứa:

```cpp
std::unique_ptr<IFrameBufferPool> fboPool;
```

Render thread:

```cpp
state.fboPool->ResizeFrame(...)
state.fboPool->MarkAsReady(...)
state.fboPool->Shutdown()
```

UI thread:

```cpp
thread->state.fboPool->GetStableFrame()
```

Đây là cùng một object.

Có atomic state trong từng frame nhưng **không có lifetime synchronization cho chính `fboPool`**.

Shutdown:

```text
UI có thể đang GetStableFrame()
RenderThread Shutdown()
→ fboPool->Shutdown()
→ PlayerSession reset render thread
```

Nếu lifecycle không được bảo đảm tuyệt đối có thể xảy ra UAF.

### Khuyến nghị

Không expose:

```cpp
state.fboPool
```

ra UI.

Thay bằng API:

```cpp
FrameHandle AcquireDisplayFrame();
```

và:

```text
RenderThread owns FBO pool
UI receives immutable FrameHandle
```

hoặc:

```text
FrameExchange
    ├── producer: render thread
    └── consumer: UI thread
```

---

# P0-06 — `WindowManager` expose iterator trực tiếp vào `unordered_map`

File:

```text
src/windows/WindowManager.h
```

Có:

```cpp
auto begin() { return windows.begin(); }
auto end() { return windows.end(); }
```

Trong khi `windows` được quản lý bằng:

```cpp
m_windowsMutex
```

nhưng iterator không giữ lock.

Điều này tạo:

```text
Thread A:
for (auto& [id, window] : winManager)

Thread B:
CreateWindow()
DestroyWindow()
```

→ `unordered_map` có thể rehash/erase trong khi iterator đang chạy.

### Hậu quả

Undefined behavior.

Có thể:

- crash;
- access violation;
- iterator invalidation;
- corrupted state.

### Sửa

Không expose iterator nội bộ.

Dùng:

```cpp
std::vector<WindowSnapshot> GetWindowsSnapshot();
```

hoặc:

```cpp
std::vector<WindowRuntime*> GetAllWindows();
```

đã copy pointer dưới lock.

Nếu cần iteration:

```cpp
template<typename F>
void ForEachWindow(F&& fn) {
    std::vector<WindowRuntime*> snapshot;
    {
        std::lock_guard lock(m_windowsMutex);
        ...
    }

    for (auto* window : snapshot) {
        fn(window);
    }
}
```

---

# P0-07 — Raw `WindowRuntime*` không có lifetime guarantee

Các object như:

```cpp
WindowResource::playersession
WindowResource::GetPlayerSession()
WindowRelation::parent
WindowRelation::children
UIRenderThread::m_ownerRuntime
```

đều dùng raw pointer.

Ví dụ:

```cpp
PlayerSession* playersession = nullptr;
```

và:

```cpp
WindowRuntime* m_ownerRuntime;
```

Trong khi:

```cpp
WindowManager::DestroyWindow()
```

có thể:

```cpp
windows.erase(it);
```

→ object bị delete.

Nếu một thread/callback vẫn giữ pointer:

```text
WindowManager
   ↓ delete WindowRuntime
        ↑
UIRenderThread vẫn dùng pointer
```

→ Use-after-free.

### Kiến trúc nên chuyển sang

Window lifecycle:

```text
WindowHandle
WindowId
weak_ptr / shared ownership
```

Không truyền raw pointer xuyên subsystem nếu object có thể bị destroy bất đồng bộ.

---

# P0-08 — ImGui backend multi-thread/multi-context chưa có ownership model an toàn

Mỗi `WindowRuntime` tạo:

```cpp
ImGuiContext
```

và mỗi window có:

```cpp
UIRenderThread
```

Đây là kiến trúc phức tạp.

`OpenGLBackend` lại giữ:

```cpp
std::mutex m_imguiBackendMutex;
```

và gọi:

```cpp
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplSDL2_NewFrame();
ImGui::NewFrame();
```

trên render thread.

Trong khi event processing xảy ra ở main thread:

```cpp
ImGui_ImplSDL2_ProcessEvent(e);
```

### Vấn đề

Mutex backend không đồng nghĩa với thread safety của:

```text
ImGuiContext
ImGui backend global state
OpenGL current context
SDL window
platform backend
```

Đặc biệt:

```text
Main Thread
 └── ProcessEvent()

UIRenderThread
 └── NewFrame()
 └── Render()
 └── Swap()
```

Đang dùng chung backend implementation.

### Khuyến nghị

Chọn một trong hai mô hình:

### Mô hình A — tốt nhất cho hiện tại

```text
Main Thread
 ├── SDL event
 ├── state update
 └── render request

Render Thread
 ├── ImGuiContext
 ├── ImGui backend
 ├── GL context
 └── Present
```

Input phải được chuyển thành một `InputSnapshot` thread-safe.

Không gọi ImGui backend trực tiếp từ main thread.

### Mô hình B

Một UI thread duy nhất render tất cả window.

Đơn giản hơn nhiều.

---

# 6. OpenGL context architecture

## 6.1 `CreateSubContext()` đang phụ thuộc vào SDL current context

Trong:

```cpp
OpenGLBackend::CreateSubContext()
```

có:

```cpp
SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
SDL_GL_CreateContext(ownerWindow);
```

Điều này có nghĩa context sharing phụ thuộc vào context hiện tại của calling thread.

Đây là coupling rất nguy hiểm.

### Nên có

```text
GraphicsDevice
    ├── MainContext
    ├── SharedResourceContext
    └── WindowContext[]
```

và context creation phải được quản lý tập trung.

---

# 7. D3D11 backend chưa thực sự là backend tương đương

`D3D11Backend` trả:

```cpp
CreateSubContext() -> std::any()
MakeCurrent() -> true
CreateFrameBufferPool() -> nullptr
GetPlayBackRenderParams() -> {}
```

Trong khi `UIRenderThread` yêu cầu:

```text
CreateSubContext
MakeCurrent
BeginFrame
EndFrame
Swap
```

### Vấn đề

D3D11 không có concept OpenGL context tương đương.

Do đó:

```cpp
MakeCurrent() -> true
```

không có nghĩa là thread-safe.

Cùng một:

```cpp
ID3D11DeviceContext*
```

không thể được coi như context độc lập cho mỗi thread.

### Kết luận

`D3D11Backend` hiện tại là:

```text
experimental skeleton
```

chứ chưa phải production backend.

### Kiến trúc đúng

D3D11 nên có:

```text
ID3D11Device
      │
      ├── immediate context
      │
      └── deferred contexts / command lists
```

hoặc render UI chỉ trên một thread.

---

# 8. `WindowInitializer` có cleanup không nhất quán

File:

```text
src/windows/WindowInitializer.cpp
```

Có nhiều đoạn:

```cpp
SDL_DestroyWindow(runtime->resource.sdlWindow);
```

nhưng không reset:

```cpp
runtime->resource.sdlWindow = nullptr;
```

Tương tự:

```cpp
ImGui::DestroyContext(runtime->resource.imguiCtx);
```

nhưng pointer vẫn có thể còn giá trị.

Sau đó destructor `WindowRuntime` tiếp tục:

```cpp
SDL_DestroyWindow(...)
ImGui::DestroyContext(...)
```

### Nguy cơ

Double destroy.

### Sửa

Sau cleanup:

```cpp
SDL_DestroyWindow(runtime->resource.sdlWindow);
runtime->resource.sdlWindow = nullptr;
```

và:

```cpp
ImGui::DestroyContext(runtime->resource.imguiCtx);
runtime->resource.imguiCtx = nullptr;
```

Tốt hơn nữa: exception-safe RAII.

---

# 9. `WindowRuntime` destructor chưa xử lý PlayerSession lifecycle

Destructor hiện xử lý:

```text
UIRenderThread
GraphicsBackend
ImGui
SDL_Window
```

nhưng không:

```cpp
PlayerManager::DestroySession(...)
```

Trong khi root window có thể sở hữu player session.

### Hậu quả

Đóng window:

```text
Window destroyed
   ↓
PlayerSession vẫn nằm trong PlayerManager
   ↓
MPV vẫn sống
   ↓
session leak về lifecycle
```

Nếu tạo/hủy window nhiều lần:

```text
Session 1
Session 2
Session 3
...
```

có thể tích tụ.

### Cần quyết định ownership rõ:

```text
Window owns Session
```

hoặc:

```text
PlayerManager owns Session
```

Nếu `PlayerManager` owns session thì phải có lifecycle event:

```text
WindowClosed
    ↓
Detach session
    ↓
Destroy session nếu không còn consumer
```

---

# 10. `WindowResource` có hai nguồn session pointer

Có:

```cpp
std::string playersessionid;
PlayerSession* playersession;
```

và:

```cpp
GetPlayerSession()
```

trả:

```cpp
PlayerManager::GetInstance().GetSession(playersessionid)
```

hoặc fallback:

```cpp
return playersession;
```

Đây là design nguy hiểm vì tồn tại hai source of truth.

### Nên chỉ giữ:

```cpp
PlayerSessionId sessionId;
```

và:

```cpp
PlayerManager::GetSession(sessionId)
```

Raw pointer chỉ được tạo tạm thời trong scope.

---

# 11. `PropertyBag` đang quá mạnh và quá nguy hiểm

`PropertyBag` có:

```cpp
std::unordered_map<std::string, std::any>
```

Ưu điểm:

- linh hoạt;
- dễ prototype;
- không cần schema.

Nhược điểm:

- stringly typed;
- runtime type mismatch;
- khó refactor;
- khó tìm usage;
- khó serialize;
- khó validate;
- khó IDE tooling.

## 11.1 Các API nguy hiểm

Ví dụ:

```cpp
T* GetPtr(...)
T& GetRef(...)
std::any* GetAny(...)
```

đều trả pointer/reference vào object nằm trong map sau khi mutex đã được unlock.

Ví dụ:

```cpp
auto& value = bag.GetRef<T>("x");
```

sau khi return:

```text
lock đã mất
```

Nếu thread khác:

```cpp
bag.Remove("x");
```

thì reference có thể dangling.

### Đánh giá

`PropertyBag` phù hợp:

```text
configuration / metadata / optional extension
```

nhưng không nên là:

```text
core runtime state synchronization
```

---

# 12. `PropertyBag::Merge()` có vấn đề synchronization

Code:

```cpp
std::lock_guard<std::recursive_mutex> lock(m_mutex);

for (const auto& [k, v] : other.properties) {
    properties[k] = v;
}
```

Không lock `other.m_mutex`.

Nếu `other` được mutate từ thread khác:

```text
Thread A: Merge(other)
Thread B: other.Set(...)
```

→ data race.

Tương tự `CopyIf()` sử dụng `GetAny()` trả pointer sau khi lock kết thúc.

### Sửa

Dùng snapshot:

```cpp
auto snapshot = other.Snapshot();
Merge(snapshot);
```

Không trả raw pointer/reference từ concurrent container.

---

# 13. MPV command layer cần kiểm tra error

`PlaybackCommandDispatcher`:

```cpp
mpv_command(...)
mpv_command_string(...)
mpv_set_property(...)
```

hầu như không kiểm tra return code.

Ví dụ:

```cpp
void Exec(const char** cmd) {
    if (m_player.GetHandle()) {
        mpv_command(m_player.GetHandle(), cmd);
    }
}
```

### Vấn đề

UI gọi:

```text
LoadFile
Seek
Play
Pause
```

nhưng không biết:

```text
success
invalid parameter
not initialized
command error
```

### Nên đổi thành

```cpp
Result<void> Exec(...)
```

hoặc:

```cpp
MpvResult Exec(...)
```

và log/telemetry tập trung.

---

# 14. MPV error handling đang phụ thuộc `DefaultSession`

`PlaybackObserver` gọi:

```cpp
PlayerManager::GetInstance().GetDefaultSession()
```

trong xử lý lỗi.

Điều này phá multi-session.

Nếu:

```text
Session A gặp lỗi
Session B là default
```

thì observer của A có thể command vào B.

### Đây là lỗi kiến trúc quan trọng.

Observer đã có:

```cpp
Player& m_player;
PlayerStateSystem& m_state;
```

nên mọi command phải đi qua chính session/player owner.

Không được lookup global default session.

---

# 15. `PlaybackObserver` vẫn chứa global state

Có:

```cpp
static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
static VideoInfo& g_videoInfo = GetVideoInfo();
```

Điều này làm observer của nhiều session ghi/đọc chung global model.

### Hậu quả

Multi-player không thể thực sự độc lập.

Kiến trúc hiện tại nói:

```text
PlayerSession = independent
```

nhưng implementation lại:

```text
PlaybackObserver
    ↓
global state
```

### Mục tiêu

Mỗi session phải có:

```text
SessionState
```

và UI truy cập:

```cpp
session->GetState()
```

---

# 16. `GetMPVPlaybackStatus()` là snapshot nhưng tên API gây hiểu nhầm

Function trả:

```cpp
MPVPlaybackStatus&
```

nhưng bên trong lại:

```text
đọc PlayerStateSystem
copy sang global object
return reference
```

Đây không phải state reference thật sự của session.

Nó là:

```text
global compatibility cache
```

Nên đổi tên kiểu:

```cpp
GetLegacyPlaybackSnapshot()
```

hoặc bỏ hoàn toàn sau migration.

---

# 17. FBO pool có race dữ liệu

`FrameNode`:

```cpp
int allocatedW;
int allocatedH;
int contentW;
int contentH;
```

không atomic.

Render thread ghi:

```cpp
frame.contentW = ...
frame.contentH = ...
```

UI thread đọc:

```cpp
info.u = frame.contentW / frame.allocatedW;
```

Nếu hai thread thực hiện đồng thời:

```text
Render thread write
UI thread read
```

thì có data race.

Atomic `state` không bảo vệ tự động các field khác.

### Sửa

Dùng immutable frame metadata:

```cpp
FrameSnapshot
```

hoặc bảo vệ metadata bằng mutex/sequence counter.

---

# 18. FBO resize có nguy cơ GPU memory explosion

Mặc dù có:

```cpp
MAX_SAFE_TEXTURE_SIZE = 4096;
```

nhưng code:

```cpp
frame.allocatedW = std::max(targetW, newW);
frame.allocatedH = std::max(targetH, newH);
```

Nếu:

```text
targetW = 7680
```

thì kết quả vẫn là:

```text
7680
```

`MAX_SAFE_TEXTURE_SIZE` không thực sự giới hạn target.

### Sửa

```cpp
targetW = std::min(targetW, MAX_SAFE_TEXTURE_SIZE);
targetH = std::min(targetH, MAX_SAFE_TEXTURE_SIZE);
```

và reject invalid size.

---

# 19. FBO fallback có thể ghi đè state

Trong `AcquireFreeBuffer()`:

```cpp
if (i != m_currentDisplayIndex) {
    m_frames[i].state.store(BufferState::RENDERING);
    return i;
}
```

Không CAS.

Trong architecture hiện tại chỉ một render producer nên rủi ro giảm, nhưng nếu sau này:

```text
multiple render producers
```

thì không còn đúng.

Nên transition state phải là explicit state machine:

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

và mọi transition dùng CAS.

---

# 20. `PlayBackRenderThread::Stop()` chưa có shutdown protocol hoàn chỉnh

Hiện tại:

```cpp
m_running = false;
state.cv.notify_all();
m_thread.join();
```

Điều này tốt ở mức cơ bản.

Nhưng thread có thể đang ở:

```cpp
mpv_render_context_render(...)
```

hoặc:

```cpp
glClientWaitSync(...)
```

hoặc driver call.

Do đó `Stop()` không có cancellation protocol cho GPU/MPV.

### Kiến trúc tốt hơn

```text
Stop requested
    ↓
stop token
    ↓
render loop stops accepting work
    ↓
flush GPU
    ↓
destroy FBO
    ↓
destroy render context
    ↓
destroy GL subcontext
```

---

# 21. `hasExited` không được set khi exception

Trong `Run()`:

```cpp
catch (...) {
    SDL_Log(...);
}
```

nhưng không:

```cpp
state.hasExited = true;
```

Nếu thread chết vì exception:

```text
hasExited vẫn false
```

các subsystem khác có thể nghĩ thread còn sống.

### Sửa

RAII hoặc scope guard:

```cpp
auto exitGuard = finally([&] {
    state.hasExited.store(true);
});
```

---

# 22. `ThreadManager` đang dùng hai mô hình thread

Có:

```text
Run() → detached
Register() → manual join
```

Đây là complexity không cần thiết.

Detached thread làm shutdown khó.

Ví dụ:

```cpp
StartRuntimeServices()
```

dùng:

```cpp
std::thread(...).detach();
```

Nếu process shutdown trong khi task chưa kết thúc:

```text
static manager/service state
```

có thể bị hủy trước thread.

### Khuyến nghị

Dùng:

```cpp
std::jthread
```

hoặc:

```cpp
ThreadHandle
```

có stop/join.

---

# 23. YouTube service có startup race

`StartRuntimeServices()`:

```cpp
std::thread([]() {
    EnsureYouTubeServiceRunning(services);
}).detach();
```

Sau đó application có thể gọi HTTP trước khi service ready.

Mặc dù có:

```cpp
WaitForServicesReady()
```

nhưng không thấy flow bắt buộc toàn hệ thống phải chờ.

### Ngoài ra

Nếu launch Python thất bại, code vẫn có thể:

```cpp
g_YouTubeServiceStarted.store(true);
```

sau timeout.

Điều này khiến:

```text
ServiceStarted == true
```

mặc dù process không chạy.

### Sửa

Có trạng thái:

```text
Starting
Ready
Failed
Stopping
Stopped
```

không chỉ boolean.

---

# 24. `TerminateProcess()` là shutdown rất mạnh

`StopYouTubeService()`:

```cpp
TerminateProcess(...)
```

đây là kill process ngay lập tức.

Không cho Python:

- flush cache;
- close file;
- cleanup;
- release resources.

### Nên

Ưu tiên:

```text
POST /shutdown
```

hoặc IPC signal.

`TerminateProcess` chỉ là fallback.

---

# 25. Python backend — lỗi security nghiêm trọng: hard-coded API key

File:

```text
src/backends/vid/config/settings.py
```

có:

```python
LOCAL_KEYS = ["AIzaSy..."]
```

Đây là secret trong source.

### Rủi ro

- commit Git;
- leak ZIP;
- leak binary;
- reverse engineering;
- key bị quota abuse.

### Phải làm

```python
LOCAL_KEYS = os.getenv("YOUTUBE_API_KEYS", "").split(",")
```

hoặc secret manager.

Nếu key đã từng commit/public:

**phải revoke/rotate key**, không chỉ xóa dòng source.

---

# 26. `/thumbnail` có khả năng SSRF

File:

```text
src/backends/vid/app.py
```

Endpoint:

```python
@app.get("/thumbnail")
async def thumbnail(url: str):
```

sau đó:

```python
content = await fetch_thumbnail(url)
```

và:

```python
session.get(url)
```

Đây là server-side URL fetch.

Nếu endpoint bị expose, attacker có thể thử:

```text
http://127.0.0.1:...
http://localhost:...
http://169.254.169.254/...
http://internal-service/...
```

### Sửa

Whitelist domain:

```text
i.ytimg.com
ytimg.com
googleusercontent.com
```

và chặn:

```text
private IP
loopback
link-local
RFC1918
IPv6 local
redirect tới internal IP
```

---

# 27. CORS cấu hình không an toàn

Có:

```python
allow_origins=["*"]
allow_credentials=True
```

Nếu service được expose ngoài localhost thì đây là cấu hình không phù hợp.

### Production

Chỉ whitelist frontend origin:

```text
http://localhost:5173
http://127.0.0.1:5173
```

hoặc origin thực tế.

---

# 28. TLS verification bị tắt trong MPV

File:

```text
src/utils.cpp
```

có:

```cpp
{"tls-verify", "no"}
```

Điều này làm HTTPS certificate verification bị vô hiệu.

### Rủi ro

- MITM;
- stream URL giả;
- download nội dung không đáng tin;
- network security giảm.

### Khuyến nghị

Không disable TLS verify mặc định.

Nếu cần workaround cho stream lỗi:

```text
explicit per-source override
```

thay vì global config.

---

# 29. Python `OFFLINE_MODE` có bug do import value

Trong:

```python
utils/network.py
```

có:

```python
OFFLINE_MODE = True
```

nhưng các module khác:

```python
from config.settings import OFFLINE_MODE
```

Khi `network.py` làm:

```python
OFFLINE_MODE = True
```

nó chỉ thay đổi variable trong module `network`.

Các module đã import:

```python
OFFLINE_MODE
```

vẫn giữ value cũ.

### Đây là lỗi logic.

### Sửa

Dùng module:

```python
from config import settings

settings.OFFLINE_MODE
```

hoặc tốt hơn:

```python
NetworkState
```

---

# 30. Python cache concurrency chưa được thiết kế hoàn chỉnh

Các global:

```python
url_locks = {}
cache_locks = {}
memory_cache = TTLCache(...)
search_cache = TTLCache(...)
trending_cache = TTLCache(...)
GLOBAL_STORE = ...
```

được sử dụng từ:

- FastAPI;
- thread pool;
- executor;
- async tasks.

`TTLCache` không phải concurrent cache mặc định.

### Đặc biệt

`cache_locks` được tạo lazy:

```python
if key not in cache_locks:
    cache_locks[key] = threading.Lock()
```

Hai thread đồng thời có thể tạo hai lock khác nhau cho cùng key.

### Sửa

Có global lock cho lock registry:

```python
_lock_registry_mutex
```

hoặc dùng cache manager class.

---

# 31. `keyword_manager` có nguy cơ race/corrupt JSON

Mỗi request có thể:

```python
track_search_keyword()
    ↓
prune_keywords()
    ↓
save_search_keywords()
```

Nếu hai request đồng thời:

```text
Thread A write
Thread B write
```

có thể race.

### Sửa

Dùng:

```text
KeywordStore
    └── mutex
```

và atomic file replace:

```text
write temp
fsync
os.replace
```

---

# 32. `fetch_youtube_trending()` bị gọi với tham số không tồn tại

Trong:

```text
src/backends/vid/app.py
```

có:

```python
fetch_youtube_trending(
    tab=tab,
    page_token=page_token
)
```

Nhưng function:

```python
def fetch_youtube_trending(
    page_token=None,
    total_results=50,
    key_ratio=1,
    max_keys=5,
    include_trending=True,
    include_keywords=True,
    region="VN"
):
```

không có:

```python
tab
```

### Kết quả

Khi `/trending?tab=...` chạy:

```text
TypeError
```

và flow ranking tab có thể fail.

### Sửa

Hoặc thêm:

```python
tab: str | None = None
```

và thực sự dùng nó,

hoặc bỏ `tab=` khỏi caller.

---

# 33. Ranking system đang tồn tại hai implementation

Có:

```text
services/ranking_engine.py
```

và:

```text
ranking/engine.py
```

`app.py` import:

```python
from services.ranking_engine import RankingEngine
```

sau đó lại:

```python
from ranking.engine import RankingEngine
```

Import thứ hai overwrite tên.

### Đây là dấu hiệu migration chưa hoàn thành.

### Khuyến nghị

Chọn:

```text
ranking/
    engine.py
    resolver.py
    plugins/
```

và xóa implementation cũ.

---

# 34. `services/ranking_engine.py` và plugin engine có semantics khác nhau

Implementation cũ:

```text
decay_factor
recovery_factor
priority
```

implementation mới:

```text
plugins
score()
on_served()
```

Hai engine có behavior khác nhau.

Nếu một phần code dùng engine A, phần khác dùng B:

```text
recommendation inconsistency
```

### Cần có một ranking contract duy nhất.

---

# 35. `weighted_random()` làm recommendation khó deterministic

Trong:

```python
utils/video_utils.py
```

dùng:

```python
random.choices(...)
```

và lưu:

```python
recent_videos
```

global.

### Vấn đề

- test không deterministic;
- user A ảnh hưởng user B;
- request A ảnh hưởng request B;
- recommendation behavior phụ thuộc thứ tự request.

### Nên

Ranking context phải chứa:

```text
user_id
session_id
seed
timestamp
```

và state phải per-user/per-session.

---

# 36. Global recommendation state không scale

Các biến:

```python
recent_videos
ranking_engine
GLOBAL_STORE
```

đều là process-global.

Nếu chạy:

```text
uvicorn workers=4
```

mỗi worker có state riêng.

Kết quả:

```text
user behavior không nhất quán
cache phân mảnh
ranking phân mảnh
```

### Production nên dùng

```text
Redis
PostgreSQL
SQLite
```

tùy quy mô.

---

# 37. APIManager — worker state không thread-safe hoàn toàn

`m_isRunning` là:

```cpp
bool
```

nhưng được đọc ở worker:

```cpp
while (m_isRunning)
```

và ghi trong `Shutdown()` dưới mutex.

Worker predicate cũng đọc:

```cpp
!m_isRunning
```

Có lock trong wait, nhưng điều kiện `while (m_isRunning)` nằm ngoài lock.

### Nên

```cpp
std::atomic<bool> m_isRunning;
```

hoặc bảo vệ toàn bộ bằng mutex.

---

# 38. APIManager dùng cùng mutex cho providers và task queue

`m_queueMutex` bảo vệ:

```text
providers
taskQueue
isRunning
```

Điều này làm coupling không cần thiết.

Nên tách:

```text
providerMutex
queueMutex
stateMutex
```

---

# 39. APIManager không có response channel

Task:

```cpp
APITask {
    providerName
    payload
}
```

Worker chỉ:

```cpp
std::cout << response;
```

Không có:

```text
callback
future
promise
event
request id
correlation id
```

Do đó API subsystem chưa thể trở thành infrastructure thực sự.

### Nên:

```cpp
struct APITask {
    RequestId id;
    ProviderId provider;
    Payload payload;
    CompletionHandler completion;
};
```

hoặc:

```cpp
std::future<APIResponse>
```

---

# 40. Gemini provider đang là mock

`GeminiProvider`:

```cpp
std::this_thread::sleep_for(std::chrono::seconds(2));
return "Gemini AI Analysis: ...";
```

Không gọi API thật.

Đây là technical debt đã được ghi chú trong source docs.

---

# 41. API key không nên được truyền/lưu plaintext trong nhiều tầng

Hiện tại:

```text
config file
↓
unordered_map<string,string>
↓
provider
↓
m_key
```

Không có secret abstraction.

Nên có:

```text
SecretProvider
```

ví dụ:

```cpp
SecretRef
SecretStore
```

và provider chỉ nhận credential handle.

---

# 42. PlayerManager nên là service, không phải global singleton lâu dài

Hiện tại:

```cpp
static PlayerManager instance;
```

Singleton làm:

- test khó;
- dependency injection khó;
- multi-runtime khó;
- lifecycle khó;
- shutdown order khó.

### Tương lai

```text
ApplicationRuntime
    ├── WindowManager
    ├── PlayerManager
    ├── APIManager
    ├── ServiceManager
    └── ThreadRuntime
```

Inject dependency thay vì global lookup.

---

# 43. `WindowFactory` / `WindowTemplate` là nền tảng tốt nhưng cần immutable definition

Hiện tại template chứa:

```cpp
WindowStyle
WindowState
PropertyBag
factory functions
```

Nên tách:

```text
WindowDefinition
```

thành immutable:

```text
WindowDefinition
    ├── id/name
    ├── style config
    ├── initial state
    ├── backend factory
    ├── renderer factory
    └── loop config
```

Sau đó:

```text
WindowRuntime
```

mới chứa mutable runtime state.

---

# 44. `WindowState` và `PropertyBag` đang bị dùng chồng chéo

Ví dụ:

```text
state.display.isVisible
state.runtime.isClosedPending
properties["RenderVideoFlag"]
properties["TriggerToggleFullscreen"]
properties["IsSecondaryMpvOutput"]
```

Một số property thực chất là state.

### Nên phân loại

```text
State
    persistent/runtime semantic state

Command/Event
    one-shot request

Property
    extension metadata
```

Ví dụ:

```text
TriggerToggleFullscreen
```

nên là:

```cpp
WindowCommand::ToggleFullscreen
```

không phải property.

---

# 45. Event architecture nên được nâng cấp

Hiện tại:

```text
SDL Event
 ↓
HandleWindowRuntimeEvent
 ↓
trực tiếp thay đổi runtime
```

Trong docs đã ghi nhận ý tưởng đưa event vào queue.

Định hướng đúng:

```text
SDL
 ↓
Input/Event Adapter
 ↓
EventQueue
 ↓
WindowRuntime
 ↓
Controller / State
```

Điều này sẽ giảm coupling main thread với runtime internals.

---

# 46. Kiến trúc đề xuất V2

## 46.1 Runtime Kernel

Đề xuất:

```text
ApplicationRuntime
│
├── EventRuntime
│
├── WindowRuntimeManager
│
├── PlayerRuntime
│
├── GraphicsRuntime
│
├── ServiceRuntime
│
├── APIManager
│
└── ShutdownCoordinator
```

---

# 47. Player architecture V2

```text
PlayerRuntime
│
├── SessionRegistry
│
└── PlayerSession
     │
     ├── PlayerCore
     │    └── mpv_handle
     │
     ├── PlayerCommandBus
     │
     ├── PlayerEventBus
     │
     ├── PlayerState
     │
     ├── PlayerRenderer
     │
     └── AudioPipeline
```

## 47.1 Không cho UI gọi MPV trực tiếp

Sai:

```text
UI → mpv_command()
```

Đúng:

```text
UI
 ↓
PlayerCommand
 ↓
CommandBus
 ↓
PlayerCore
 ↓
MPV
```

---

# 48. Render architecture V2

```text
MPV
 │
 │ update callback
 ▼
RenderScheduler
 │
 ▼
RenderThread
 │
 ├── GraphicsContext
 ├── MPV RenderContext
 ├── FramePool
 └── FrameExchange
          │
          ▼
       UI Thread
```

UI chỉ nhận:

```cpp
FrameSnapshot
```

không chạm trực tiếp:

```cpp
fboPool
```

---

# 49. FrameExchange nên là abstraction riêng

Đề xuất:

```cpp
class IFrameExchange {
public:
    virtual FrameHandle acquireProducerFrame() = 0;
    virtual void publish(FrameHandle frame) = 0;
    virtual std::optional<FrameHandle> acquireLatest() = 0;
};
```

Điều này cho phép:

```text
OpenGLFrameExchange
D3D11FrameExchange
VulkanFrameExchange
```

mà không ép `WindowRuntime` biết FBO.

---

# 50. Graphics abstraction nên tách Device / Context / Swapchain

Hiện tại `IGraphicsBackend` làm quá nhiều việc.

Nên:

```text
GraphicsDevice
GraphicsContext
Swapchain
ImGuiRenderer
VideoRenderer
FrameAllocator
```

Ví dụ:

```text
IGraphicsDevice
    ├── createWindowContext()
    ├── createRenderContext()
    ├── createTexture()
    └── createFrameBuffer()

ISwapchain
    ├── resize()
    ├── acquire()
    └── present()

IImGuiRenderer
    ├── begin()
    └── render()

IVideoRenderer
    └── renderFrame()
```

---

# 51. State architecture V2

Nên giữ:

```text
PlayerState
```

làm source of truth.

Có thể chuyển sang:

```text
StateStore
    ├── snapshot()
    ├── update()
    └── subscribe()
```

Ví dụ:

```cpp
auto snapshot = session.state().snapshot();
```

UI không giữ reference mutable.

---

# 52. Event Bus

Nên có:

```text
PlayerEvent
WindowEvent
ApplicationEvent
NetworkEvent
APIEvent
```

Ví dụ:

```cpp
struct PlaybackStarted {
    SessionId session;
};

struct PlaybackEnded {
    SessionId session;
    EndReason reason;
};

struct WindowClosed {
    WindowId window;
};
```

---

# 53. Command Bus

Tách:

```text
Command
```

khỏi:

```text
Property
```

Ví dụ:

```text
Play
Pause
Seek
LoadMedia
SetVolume
SetSpeed
ToggleFullscreen
CloseWindow
CreateWindow
```

Mọi command có:

```text
source
timestamp
requestId
target
```

---

# 54. Shutdown architecture

Hiện tại shutdown phụ thuộc destructor.

Nên có:

```text
ShutdownCoordinator
```

thứ tự:

```text
1. Stop accepting commands
2. Stop network requests
3. Stop API workers
4. Stop video service
5. Stop render scheduling
6. Stop UI render threads
7. Destroy player render contexts
8. Destroy PlayerSession
9. Destroy windows
10. Destroy graphics
11. SDL_Quit
```

Không nên dựa vào static destructor order.

---

# 55. Python service architecture V2

Đề xuất:

```text
FastAPI
│
├── API Router
│
├── VideoService
│   ├── YouTubeClient
│   ├── CacheService
│   └── ThumbnailService
│
├── RecommendationService
│   ├── RankingEngine
│   └── RankingPlugin[]
│
├── KeywordService
│
└── Storage
    ├── SQLite
    └── Redis (optional)
```

Không để global dict làm database.

---

# 56. YouTube client V2

Nên có:

```python
class YouTubeClient:
    async def search(...)
    async def trending(...)
    async def video_details(...)
```

và:

```python
class APIKeyPool:
    async def acquire()
    async def release()
    async def report_quota()
```

Không để credential management lẫn với HTTP client.

---

# 57. Cache architecture V2

```text
CacheService
│
├── MemoryCache
├── DiskCache
└── PersistentCache
```

Mỗi cache có:

```text
get
set
delete
invalidate
stats
```

Không truy cập `TTLCache` trực tiếp từ route.

---

# 58. Recommendation V2

```text
VideoCandidate
      ↓
FeatureExtractor
      ↓
RankingContext
      ↓
Plugin[]
      ↓
Score
      ↓
Diversity filter
      ↓
Final selection
```

Context:

```python
{
    "user_id": ...,
    "session_id": ...,
    "tab": ...,
    "timestamp": ...,
    "history": ...,
}
```

Không dùng `recent_videos` global.

---

# 59. Testing strategy bắt buộc

## 59.1 C++ unit tests

Cần test:

```text
PlayerManager
PlayerSession
PlayerStateSystem
PropertyBag
WindowRelation
WindowPropertyBag
CommandDispatcher
```

## 59.2 Render tests

Không dễ unit test GPU, nhưng cần:

```text
FramePool state transition tests
Resize tests
Acquire/Publish tests
Shutdown while rendering
```

## 59.3 Concurrency tests

Bắt buộc:

```text
Create/Destroy Window concurrently
Create/Destroy Session concurrently
Render + Resize
Render + Shutdown
UI + FBO exchange
Command + Player shutdown
```

## 59.4 Python tests

Cần:

```text
YouTube client
API key rotation
quota handling
cache refill
keyword store
ranking
thumbnail validation
SSRF protection
```

---

# 60. Sanitizer / diagnostic tools

C++ nên bật:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
ThreadSanitizer
```

MSVC/Visual Studio có thể dùng các cơ chế tương ứng tùy toolchain.

Đặc biệt:

```text
ASan
```

sẽ rất hữu ích cho:

```text
WindowRuntime
PlayerSession
FBO
callback userdata
```

---

# 61. Logging architecture

Hiện tại có:

```text
std::cout
std::cerr
SDL_Log
RATE_LIMITED_COUT
Python logging
```

Quá nhiều logging mechanism.

Nên thống nhất:

```text
Logger
 ├── debug
 ├── info
 ├── warning
 ├── error
 └── critical
```

và mỗi record có:

```text
timestamp
thread
subsystem
session_id
window_id
request_id
```

---

# 62. Observability

Sau này nên đo:

```text
render FPS
frame latency
MPV event latency
command latency
GPU frame time
cache hit rate
YouTube API quota
thumbnail hit rate
ranking latency
queue depth
thread count
```

---

# 63. Performance

Các điểm có khả năng gây overhead:

### C++

- `std::any` trong PropertyBag.
- string key lookup.
- global state synchronization.
- nhiều mutex.
- nhiều thread cho mỗi window.
- tạo `std::vector<mpv_render_param>`.
- `std::this_thread::sleep_for(1ms)` trong UI render path.
- texture allocation khi resize.

### Python

- tạo `aiohttp.ClientSession()` cho mỗi thumbnail request.
- JSON file save mỗi keyword interaction.
- `GLOBAL_STORE` tuyến tính.
- ranking sort toàn bộ candidate mỗi request.
- random selection global.

---

# 64. Thumbnail service nên reuse HTTP session

Hiện tại:

```python
async with aiohttp.ClientSession() as session:
```

mỗi request.

Nên có:

```text
Application lifespan
    ↓
shared aiohttp.ClientSession
```

giảm connection setup overhead.

---

# 65. Cache thumbnail cần giới hạn dung lượng

Có:

```text
MEMORY_CACHE_SIZE = 500
```

nhưng file cache:

```text
thumbnail_cache/
```

không có eviction rõ ràng.

Theo thời gian:

```text
disk usage → tăng vô hạn
```

Nên có:

```text
max disk size
TTL
LRU
periodic cleanup
```

---

# 66. API `/batch-videos` hiện chưa có nghiệp vụ

Endpoint chỉ:

```python
for v in videos:
    print(...)
```

và trả:

```json
{"received": N}
```

Nó chưa tạo integration thật.

---

# 67. `/watched` hiện chưa có persistence user history thực sự

Endpoint track keyword từ title nhưng chưa có:

```text
user identity
watch timestamp
watch duration
completion rate
skip
like
dislike
```

Do đó recommendation engine hiện mới ở mức prototype.

---

# 68. Tiềm năng phát triển

Nếu sửa các lỗi lifecycle/concurrency trên, project có tiềm năng phát triển thành:

## 68.1 Media Runtime

```text
Multiple PlayerSession
Multiple Window
Multiple Output
```

## 68.2 AI Media Pipeline

```text
Audio
 ↓
ASR
 ↓
Translation
 ↓
LLM
 ↓
TTS
 ↓
Audio Mixer
```

## 68.3 Intelligent recommendation

```text
YouTube data
 ↓
Feature extraction
 ↓
User history
 ↓
Ranking plugins
 ↓
Personalized feed
```

## 68.4 Plugin ecosystem

```text
Provider
Plugin
Script
Renderer
Filter
Ranking plugin
AI service
```

---

# 69. Kiến trúc mục tiêu

```text
                    ApplicationRuntime
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
 WindowRuntime       PlayerRuntime       ServiceRuntime
        │                  │                  │
        │                  │                  ├── YouTube
        │                  │                  ├── AI API
        │                  │                  └── Key Store
        │                  │
        │                  ▼
        │             SessionRegistry
        │                  │
        │                  ▼
        │             PlayerSession
        │                  │
        │      ┌───────────┼────────────┐
        │      │           │            │
        │      ▼           ▼            ▼
        │   StateStore  CommandBus  EventBus
        │      │
        │      ▼
        │   MPV Core
        │
        ▼
 GraphicsRuntime
        │
   ┌────┼───────────────┐
   ▼    ▼               ▼
 OpenGL D3D11         Future Vulkan
```

---

# 70. Roadmap sửa lỗi đề xuất

## Phase 0 — Không thêm feature

**Mục tiêu: ổn định**

1. Sửa `PlayerManager::CreateSession()`.
2. Sửa deadlock `SetVideoSize()`.
3. Sửa `OpenGLFrameBufferPool::any_cast`.
4. Sửa callback userdata lifecycle.
5. Tắt hard-coded API key.
6. Bật TLS verification.
7. Chặn SSRF `/thumbnail`.
8. Sửa `fetch_youtube_trending(tab=...)`.
9. Sửa `OFFLINE_MODE`.
10. Xóa iterator trực tiếp của `WindowManager`.

---

# 71. Phase 1 — Ownership

Chuẩn hóa:

```text
WindowManager owns WindowRuntime
PlayerManager owns PlayerSession
WindowRuntime only stores SessionId
```

Không còn:

```cpp
PlayerSession* playersession;
```

trong resource.

---

# 72. Phase 2 — Thread model

Chọn:

```text
Main Thread
    SDL events + orchestration

UI Render Thread
    ImGui + graphics

Player Render Thread
    MPV + video frame production

Worker Threads
    network/API
```

Mỗi thread có:

```text
ownership
queue
shutdown protocol
```

---

# 73. Phase 3 — Event/Command bus

Tách:

```text
Event
Command
State
Property
```

Không dùng PropertyBag để truyền mọi thứ.

---

# 74. Phase 4 — Graphics abstraction

Tách:

```text
GraphicsDevice
GraphicsContext
FrameExchange
Swapchain
ImGuiRenderer
VideoRenderer
```

sau đó mới triển khai:

```text
OpenGL
D3D11
```

và chỉ khi abstraction thực sự đúng mới thêm:

```text
Vulkan
D3D12
```

---

# 75. Phase 5 — Python service productionization

Tách:

```text
API
Service
Repository
Cache
Provider
Ranking
```

và bỏ global mutable state.

---

# 76. Phase 6 — Test + sanitizer

Ưu tiên:

```text
ASan
TSan
unit tests
lifecycle tests
render tests
Python pytest
integration tests
```

---

# 77. Phase 7 — Feature development

Chỉ sau Phase 0-6 mới nên mở rộng:

```text
AI
TTS
translation
audio analysis
recommendation
multi-player
plugin system
remote control
```

Nếu thêm feature trước khi sửa lifecycle/concurrency, technical debt sẽ tăng rất nhanh.

---

# 78. Danh sách lỗi theo mức độ

## 🔴 P0 — phải sửa ngay

1. `PlayBackRenderThread::SetVideoSize()` deadlock.
2. `PlayerManager::CreateSession()` dùng sai `id` thay vì `sessionId`.
3. `OpenGLFrameBufferPool.cpp` dùng `std::any_cast` trên typed handle.
4. FBO pool lifetime cross-thread chưa an toàn.
5. WindowManager iterator không thread-safe.
6. Raw `WindowRuntime*`/`PlayerSession*` cross-thread lifetime nguy hiểm.
7. ImGui backend multi-thread/context ownership chưa được bảo đảm.
8. Hard-coded YouTube API key.
9. `/thumbnail` SSRF.
10. `tls-verify=no`.
11. Python cache/global state concurrency.
12. Window/session destruction lifecycle chưa hoàn chỉnh.

## 🟠 P1 — sửa trước khi mở rộng lớn

1. Hai ranking engine.
2. Hai state model.
3. Global MPV playback state.
4. APIManager response channel thiếu.
5. APIManager state synchronization.
6. D3D11 backend chưa thực sự usable.
7. ThreadManager detached model.
8. Shutdown order.
9. PropertyBag raw references.
10. Keyword JSON concurrent writes.
11. Thumbnail cache không có disk eviction.
12. YouTube quota management global.
13. `weighted_random` global state.
14. Render metadata race.

## 🟡 P2 — technical debt

1. `gui_widgets.cpp` quá lớn.
2. Legacy comments/code.
3. duplicated include.
4. inconsistent naming.
5. logging mechanism phân tán.
6. magic strings.
7. magic constants.
8. API provider mock.
9. missing response/event architecture.
10. test coverage thấp.

---

# 79. Những thứ KHÔNG nên làm ngay

Không nên tiếp tục:

```text
+ thêm UI
+ thêm popup
+ thêm provider
+ thêm ranking algorithm
+ thêm thread
+ thêm backend graphics
```

khi chưa xử lý:

```text
ownership
threading
render lifecycle
session lifecycle
security
```

Nếu không:

```text
feature
 ↓
bug
 ↓
workaround
 ↓
mutex
 ↓
global
 ↓
callback
 ↓
race
 ↓
crash
```

sẽ trở thành vòng lặp khó thoát.

---

# 80. Kết luận

Project hiện tại **không phải kiến trúc tệ**.

Ngược lại, phần design đang đi đúng hướng:

```text
Window abstraction
PlayerSession
PlayerStateSystem
Graphics abstraction
Python service
Ranking plugin
API provider
```

là nền tảng tốt.

Vấn đề chính là implementation đang nằm giữa hai thế hệ:

```text
Legacy global architecture
            +
New session/runtime architecture
```

và giữa:

```text
single-thread UI assumptions
            +
multi-thread rendering
```

Đây là lý do xuất hiện nhiều:

```text
raw pointer
global state
PropertyBag
singleton
detached thread
shared FBO
ImGui context
default session
```

## Đánh giá cuối

```text
Architecture concept:       8/10
Current implementation:     5/10
Maintainability:            5/10
Thread safety:              3/10
Lifecycle safety:           3/10
Graphics abstraction:       5/10
Player abstraction:         7/10
State architecture:         7/10
Python backend concept:     7/10
Python production safety:   4/10
Security:                   3/10
Extensibility potential:    9/10
```

**Kết luận quan trọng nhất:**

> Không cần viết lại toàn bộ project.

Nên thực hiện **controlled refactor**, giữ lại:

```text
PlayerSession
PlayerStateSystem
WindowTemplate
WindowFactory
IGraphicsBackend
RankingPlugin
IAPIProvider
```

nhưng tái cấu trúc:

```text
ownership
lifetime
threading
event
command
frame exchange
security
cache
shutdown
```

Nếu thực hiện đúng, codebase này có thể tiến tới một kiến trúc media runtime khá mạnh, hỗ trợ:

```text
multi-window
multi-session
MPV
AI
TTS
ASR
translation
recommendation
plugin
multiple graphics backend
remote service
```

mà không cần phá bỏ toàn bộ nền tảng hiện tại.

---

# 81. Checklist hành động ngay

```text
[ ] Fix PlayerManager sessionId bug
[ ] Fix SetVideoSize deadlock
[ ] Fix OpenGLFrameBufferPool any_cast
[ ] Fix render callback userdata lifetime
[ ] Remove WindowManager begin/end
[ ] Audit WindowRuntime raw pointers
[ ] Define PlayerSession ownership
[ ] Remove default-session access from observer
[ ] Make FBO FrameExchange explicit
[ ] Audit ImGui multi-thread model
[ ] Disable hard-coded API key
[ ] Rotate leaked API key
[ ] Enable TLS verification
[ ] Protect /thumbnail from SSRF
[ ] Fix OFFLINE_MODE state
[ ] Fix trending(tab=...)
[ ] Merge duplicate ranking engines
[ ] Merge legacy/global player state
[ ] Add API response/callback model
[ ] Add shutdown coordinator
[ ] Add ASan/TSan
[ ] Add lifecycle/concurrency tests
```

---

# 82. Thứ tự sửa tối ưu

Nếu chỉ có thời gian sửa một số ít vấn đề, hãy làm đúng thứ tự này:

```text
1. SetVideoSize deadlock
2. PlayerManager sessionId
3. FBO any_cast
4. Render callback lifetime
5. WindowManager iterator
6. Window/Session ownership
7. FBO FrameExchange
8. ImGui threading model
9. Hard-coded API key
10. SSRF
11. TLS verification
12. Python cache synchronization
13. Duplicate ranking/state systems
14. Shutdown coordinator
15. Tests + sanitizers
```

Sau 15 bước trên mới nên tiếp tục mở rộng feature lớn.