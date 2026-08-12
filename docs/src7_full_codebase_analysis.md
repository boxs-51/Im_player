# Phân tích toàn diện mã nguồn `src(7).zip`

> **Ngày phân tích:** 2026-08-12  
> **Phạm vi:** toàn bộ `src/` trong `src(7).zip`  
> **Phương pháp:** static analysis trực tiếp trên source/header/Python/Markdown; kiểm tra ownership, lifecycle, threading, rendering, MPV, event flow, API/cache và các điểm compile/link/runtime.  
> **Giới hạn:** archive chỉ chứa `src/`, không chứa đầy đủ build system/dependency tree, nên chưa thể kết luận build/release thành công.

---

## 1. Executive summary

Đây là media player C++/SDL2/ImGui/libmpv có kiến trúc tương đối lớn, đồng thời nhúng backend Python/FastAPI để lấy, cache và rank dữ liệu video.

### Quy mô

- **220 entries** trong ZIP.
- **161 file thực** sau khi bỏ directory và `__pycache__`.
- Khoảng **26.515 dòng code/header/Python**.
- C++ tập trung vào Window, SDL2, ImGui, OpenGL, libmpv, PlayerSession, audio filter, popup/UI, thread manager và API provider.
- Python tập trung vào YouTube fetching, cache, keyword history, thumbnail proxy và ranking plugin system.

### Đánh giá tổng thể

| Hạng mục | Đánh giá |
|---|---|
| Module separation | Khá tốt về ý tưởng |
| Window architecture | Khá tốt |
| PlayerSession architecture | Tốt |
| Player state model | Tốt |
| Ownership | Trung bình/khá |
| Threading | Rủi ro cao |
| MPV/render synchronization | Rủi ro rất cao |
| Multi-window/multi-session | Chưa an toàn |
| Event routing | Có lỗi kiến trúc quan trọng |
| Error handling | Không đồng nhất |
| Security | Có điểm cần xử lý ngay |
| Testability | Thấp |
| Extensibility | Cao nếu ổn định lifecycle/concurrency trước |

### Kết luận

**Không nên tiếp tục mở rộng feature lớn trước khi xử lý lifecycle, threading, MPV event routing và FrameBufferPool.**

Thứ tự ưu tiên:

1. Compile/link correctness.
2. Null safety.
3. MPV event routing theo session/window.
4. Window/PlayerSession lifetime.
5. FrameBufferPool synchronization.
6. Thread shutdown.
7. ImGui/OpenGL context ownership.
8. Python cache/security.
9. Tests.
10. Feature expansion.

---

# 2. Kiến trúc hiện tại

```text
Application
└── main1.cpp
    ├── SDL
    ├── WindowManager
    │   ├── WindowTemplateRegistry
    │   ├── WindowFactory
    │   └── WindowRuntime
    │       ├── WindowState
    │       ├── PropertyBag
    │       ├── WindowRelation
    │       ├── WindowResource
    │       │   ├── SDL_Window
    │       │   ├── ImGuiContext
    │       │   ├── IGraphicsBackend
    │       │   └── UIRenderThread
    │       └── WindowRenderer
    │
    ├── PlayerManager
    │   └── PlayerSession
    │       ├── Player -> mpv_handle
    │       ├── PlayBackRender -> mpv_render_context
    │       ├── PlaybackObserver
    │       ├── PlaybackCommandDispatcher
    │       ├── PlayBackProperty
    │       ├── PlayerStateSystem
    │       ├── AudioFilterManager
    │       ├── VideoFilterManager
    │       └── PlayBackRenderThread
    │
    ├── IGraphicsBackend
    │   └── OpenGLBackend
    │       └── OpenGLFrameBufferPool
    │
    ├── ThreadManager
    ├── ConfigManager
    ├── FontManager
    ├── Popup/UI
    └── APIManager
        └── IAPIProvider
            └── GeminiProvider

Python service
└── FastAPI
    ├── YouTube fetch
    ├── Cache
    ├── Keyword manager
    ├── Thumbnail proxy
    └── Ranking plugins
```

---

# 3. Module assessment

## 3.1 `src/windows`

Đây là một trong các module quan trọng nhất.

### Thành phần

- `WindowManager`
- `WindowFactory`
- `WindowTemplate`
- `WindowTemplateBuilder`
- `WindowInitializer`
- `WindowRuntime`
- `WindowResource`
- `WindowRelation`
- `WindowController`
- `WindowProcHook`
- `UIRenderThread`
- `WindowSharedGroup`

### Điểm tốt

- Template-based window creation.
- Registry/factory.
- `WindowRuntime` là aggregate khá rõ.
- Resource dùng `unique_ptr`.
- Parent/children relation.
- Shared group cho resource dùng chung.
- `ForEachWindow()` snapshot trước callback.

### CRITICAL — `WindowRuntime` constructor/destructor không có implementation trong archive

`src/windows/WindowRuntime.h` khai báo:

```cpp
WindowRuntime(WindowId id = 0, SDL_Window* sdlWindow = nullptr, HWND hwnd = nullptr);
~WindowRuntime();
```

Không tìm thấy implementation trong archive.

Nếu chúng không tồn tại ở source ngoài ZIP, đây là **linker blocker**.

**P0.**

### CRITICAL — qualified member definition trong `WindowResource.h`

Code:

```cpp
PlayerSession* WindowResource::GetPlayerSession() const {
```

đang nằm ngay bên trong `struct WindowResource`.

Phải viết:

```cpp
PlayerSession* GetPlayerSession() const {
```

hoặc đưa definition ra ngoài class.

Đây là lỗi compiler có cơ sở trực tiếp từ source.

**P0.**

### HIGH — destroy window trong khi giữ `m_windowsMutex`

Luồng hiện tại:

```text
DestroyWindow
 -> lock m_windowsMutex
 -> DestroyWindowInternal
 -> windows.erase
 -> WindowRuntime destructor
 -> UIRenderThread Stop
 -> join
```

Nếu render thread trong lúc đó cần resource/mutex liên quan, có thể deadlock.

**Khuyến nghị:** remove ownership khỏi registry dưới lock, unlock, sau đó destroy object.

### HIGH — raw pointer snapshot không có lifetime guarantee

`ForEachWindow()` snapshot `WindowRuntime*`, unlock rồi mới callback.

Nếu thread khác destroy window giữa hai bước, pointer có thể dangling.

---

## 3.2 `WindowFactory`

Ý tưởng builder:

```cpp
.WithBackend<OpenGLBackend>()
.WithRenderer<MainWindowRenderer>()
.WithLoop(60)
```

rất tốt.

Nhưng `WindowFactory::Create()` vẫn:

```cpp
auto* runtime = new WindowRuntime();
```

nên chuyển thành:

```cpp
std::unique_ptr<WindowRuntime> Create(...);
```

để failure/exception-safe.

---

# 4. PropertyBag

`PropertyBag` là abstraction tiện dụng nhưng đang làm quá nhiều việc.

### Điểm tốt

- `std::any`.
- copy constructor riêng.
- recursive mutex.
- `Set`, `GetValue`, `TryGet`, `Merge`, `Emplace`.

### HIGH — `GetPtr()` không lock

```cpp
auto it = properties.find(std::string(key));
```

không có lock.

Trong khi `Set`, `Remove`, `GetValue` đều lock.

Có thể xảy ra data race.

### HIGH — `GetRef()` trả reference ra ngoài lock

```cpp
T& GetRef(...)
```

Lock hết khi function return.

Reference có thể trở nên invalid khi thread khác thay đổi property.

### Khuyến nghị

Ưu tiên:

```cpp
std::optional<T> Get<T>(key);
bool TryGet<T>(key, T& out);
void Update<T>(key, callback);
```

Hạn chế public pointer/reference vào dữ liệu nội bộ.

---

# 5. PlayerManager / PlayerSession

## PlayerManager

Thiết kế:

```text
PlayerManager
└── unordered_map<string, unique_ptr<PlayerSession>>
```

là hợp lý.

### HIGH — raw `PlayerSession*` cross-thread

`GetSession()` trả raw pointer.

Thread khác có thể gọi `DestroySession()` ngay sau đó.

Do đó pointer có thể dangling.

Nên dùng `shared_ptr<PlayerSession>` hoặc SessionId + lookup có lifetime scope rõ ràng.

---

## PlayerSession

Đây là một abstraction tốt:

```text
PlayerSession
├── Player
├── PlayBackRender
├── PlaybackObserver
├── PlaybackCommandDispatcher
├── PlayBackProperty
├── PlayerStateSystem
├── AudioFilterManager
├── VideoFilterManager
└── PlayBackRenderThread
```

### Điểm tốt

- ownership rõ bằng smart pointer.
- renderer shutdown trước player.
- render thread được stop trước renderer/player.
- Session có ID.

### Cần cải thiện

`Init()` có nhiều side effect xuyên subsystem. Nên chuyển thành explicit lifecycle:

```text
Created
 -> PlayerReady
 -> RendererReady
 -> ObserverReady
 -> AudioReady
 -> Running
 -> Stopping
 -> Destroyed
```

---

# 6. MPV event routing — lỗi kiến trúc quan trọng nhất

`Player::Init()`:

```cpp
mpv_set_wakeup_callback(m_mpv, [](void*) {
    SDL_Event ev;
    ev.type = SDL_MPV_EVENT;
    SDLUtils::SDLX_PushUniqueEvent(ev);
}, nullptr);
```

Event không chứa SessionId/WindowId.

Trong `main1.cpp`:

```cpp
WindowRuntime *targetWin = winManager.GetWindowFromEvent(&e);
```

nhưng `SDL_MPV_EVENT` không có window ID.

Sau đó:

```cpp
if (!targetWin) {
    targetWin = mainWin;
}
```

### Hậu quả

Với:

```text
Session A -> Window A
Session B -> Window B
```

event của Session B có thể bị xử lý bởi main window/Session A.

Đây phá vỡ multi-session.

### Thiết kế đúng

```cpp
struct MpvEventContext {
    SessionId sessionId;
    WindowId windowId;
};
```

Wakeup callback phải gắn context.

Sau đó:

```text
SDL_MPV_EVENT
 -> EventRouter
 -> SessionId
 -> PlayerSession
```

Không fallback MPV event về main window.

**P0/P1.**

---

# 7. Null dereference trong `main1.cpp`

Có:

```cpp
if (session->GetObserver())
    session->GetObserver()->ProcessEvents();
```

phải check:

```cpp
if (session && session->GetObserver()) {
    session->GetObserver()->ProcessEvents();
}
```

### CRITICAL — all-window loop

Có:

```cpp
window->resource.GetPlayerSession()->GetCommander()
```

và:

```cpp
window->resource.GetPlayerSession()->GetAudioFilterManager()
```

Nếu window không có PlayerSession => crash.

Đây đặc biệt nguy hiểm với các window phụ không có MPV.

Phải lấy session một lần:

```cpp
auto* session = window->resource.GetPlayerSession();
if (!session)
    return;
```

sau đó dùng `session`.

---

# 8. MainWindowRenderer

Ngay đầu `RenderUI()`:

```cpp
PlaybackState state =
    runtime->resource.GetPlayerSession()
        ->GetState()
        ->GetPlaybackState();
```

Không check null.

Renderer nên:

```cpp
auto* session = runtime->resource.GetPlayerSession();
if (!session) {
    RenderNonPlayerUI(runtime);
    return;
}
```

Không gọi `GetPlayerSession()` lặp lại nhiều lần.

---

# 9. Graphics backend

`IGraphicsBackend` là abstraction đúng hướng:

```text
IGraphicsBackend
├── OpenGLBackend
└── D3D11Backend
```

Nhưng interface chưa thật sự backend-neutral vì chứa:

```cpp
mpv_render_param
GetGLInternalFormat()
GetGLFormat()
GetGLType()
```

Nếu D3D11 được hoàn thiện, interface này sẽ ép D3D11 biết concept OpenGL.

### Đề xuất

Tách:

```text
IGraphicsBackend
IMpvRenderBackend
IFrameSurfacePool
IGraphicsContext
```

---

# 10. OpenGL context

`CreateSubContext()` là ý tưởng đúng cho:

```text
Main context
├── UI context
└── MPV render context
```

Nhưng SDL GL attributes là global state.

Ví dụ:

```cpp
SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
```

được gọi nhiều lần.

Nên cấu hình graphics system một lần ở bootstrap.

---

# 11. ImGui threading

Mỗi window có:

```text
ImGuiContext
UIRenderThread
```

có thể hoạt động nhưng cần invariant cực chặt.

Mutex hiện tại bảo vệ backend calls, nhưng không tự động bảo vệ mọi ImGui core state.

Đặc biệt:

```cpp
ImGui::UpdatePlatformWindows();
ImGui::RenderPlatformWindowsDefault();
```

trong `SwapWindow()` có rủi ro nếu multi-viewport được bật.

Vì WindowManager đã tự quản lý OS windows, nên tốt nhất backend không quản lý ImGui platform viewports.

---

# 12. UIRenderThread

Điểm tốt:

- condition variable.
- stop/join.
- context riêng.
- request coalescing.

### HIGH — giữ `stateMutex` quá lâu

```cpp
std::lock_guard<std::mutex> stateLock(m_ownerRuntime->stateMutex);
m_ownerRuntime->renderer->RenderUI(currentWindow);
```

Renderer có thể làm rất nhiều việc.

Nên snapshot state:

```text
lock
 -> copy render state
unlock
 -> render(snapshot)
```

Không giữ state mutex xuyên GPU/UI rendering.

---

# 13. MPV RenderThread

Pipeline:

```text
MPV callback
 -> RequestRender
 -> condition_variable
 -> PlayBackRenderThread
 -> shared OpenGL context
 -> FBO pool
 -> mpv_render_context_render
 -> READY frame
 -> UI thread
 -> GetStableFrame
 -> ImGui::Image
```

Ý tưởng tốt.

Implementation hiện có rủi ro race rất cao.

---

# 14. CRITICAL — FrameBufferPool state machine

Các trạng thái:

```text
FREE
RENDERING
READY
DISPLAYING
```

là đúng hướng.

Nhưng fallback:

```cpp
return (m_currentDisplayIndex + 1) % 3;
```

có thể trả buffer không FREE/READY.

Đặc biệt:

```cpp
m_frames[i].state.store(BufferState::RENDERING);
```

có thể overwrite state của buffer đang được sử dụng.

### Hậu quả

- tearing.
- corruption.
- GPU synchronization bug.
- random crash.

### Invariant bắt buộc

Producer chỉ được:

```text
FREE -> RENDERING
READY -> RENDERING
```

Không bao giờ:

```text
RENDERING -> RENDERING
DISPLAYING -> RENDERING
```

Nếu không có buffer:

```text
drop frame / reuse old frame
```

không được overwrite.

---

# 15. CRITICAL — FrameBufferPool data race

Các field:

```cpp
contentW
contentH
allocatedW
fence
```

được đọc/ghi từ nhiều thread.

Atomic `state` không bảo vệ tự động toàn bộ object.

Cần:

- metadata mutex,
- atomic immutable snapshot,
- hoặc single-producer/single-consumer queue.

### Fence ownership

`GLsync` được tạo render thread nhưng có thể bị clear UI thread.

Nên có ownership protocol rõ ràng thay vì cả hai thread thao tác cùng pointer.

---

# 16. `PlayBackRender::Render()`

Đang retry:

```cpp
for (int i = 0; i < 3; ++i) {
    ...
    sleep_for(1ms);
}
```

Không nên sleep trong UI rendering path.

Tốt hơn:

```text
GetLatestReadyFrame()
```

Nếu chưa có frame mới thì giữ frame cũ.

---

# 17. AudioFilterManager

Architecture có tiềm năng lớn:

- adaptive mode.
- presets.
- safety.
- bypass.
- EQ.
- track.
- logging.

Nhưng các field:

```cpp
bool m_autoMode;
bool m_enableOuterStabilizer;
bool m_enableOuterBooster;
```

đang public.

UI có thể truy cập trực tiếp, phá encapsulation/thread safety.

Nên chỉ expose setter/getter synchronized.

---

# 18. PlayerStateSystem

Đây là phần nên giữ.

State chia thành:

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

với `shared_mutex`.

Điểm tốt:

- domain isolation.
- read/write API.
- full-state snapshot.

Về lâu dài có thể chuyển sang immutable snapshot/versioning để giảm contention.

---

# 19. PlaybackCommandDispatcher

API khá rõ.

Nhưng:

```cpp
m_isSeekPending
m_seekTargetTime
m_lastSeekRequestTime
```

không an toàn nếu command được gọi từ nhiều thread.

Nên quy định:

> Mọi MPV command phải đi qua một command queue của PlayerSession.

Không cho subsystem bất kỳ gọi `mpv_*` tùy ý.

---

# 20. MPV ownership rule

Hiện nhiều subsystem dùng MPV:

```text
PlaybackCommandDispatcher
PlaybackObserver
PlayBackProperty
AudioFilterManager
ShaderManager
ScriptManager
PlayBackRender
PlayBackRenderThread
```

Nên formalize:

```text
PlayerSession
└── exclusively owns mpv_handle
```

Shutdown order:

```text
stop events
↓
stop render threads
↓
detach MPV callbacks
↓
destroy render context
↓
shutdown filters/scripts/shaders
↓
terminate mpv
↓
destroy session
```

---

# 21. ThreadManager

Có hai mô hình:

1. detached task.
2. registered thread.

Đây là source complexity không cần thiết.

### CRITICAL — detached thread

```cpp
t.detach();
```

Core runtime thread không nên detached.

Nếu task capture object đã destroyed => use-after-free.

### HIGH — JoinAllRegistered giữ mutex khi join

Hiện có pattern:

```cpp
lock(mutex_)
thread->join()
```

Thread đang join có thể cần chính mutex này.

Nên snapshot threads, unlock rồi join.

### Đề xuất

Dùng C++20:

```cpp
std::jthread
std::stop_token
```

cho core threads.

---

# 22. APIManager

Kiến trúc queue/worker/provider hợp lý.

### CRITICAL — `m_isRunning` data race

Header:

```cpp
bool m_isRunning;
```

Worker:

```cpp
while (m_isRunning)
```

Shutdown ghi dưới mutex nhưng worker đọc ngoài mutex.

Sửa thành:

```cpp
std::atomic<bool> m_isRunning;
```

hoặc bảo vệ mọi access bằng cùng mutex.

### HIGH — paused task bị drop

```cpp
Paused -> Task dropped
```

Nên có policy:

```text
DROP
REQUEUE
CANCEL
EXPIRE
```

---

# 23. API secrets

Key được load từ file plain text.

Cần:

```text
SecretStore
CredentialProvider
RedactedLogger
```

Không log secret.

---

# 24. Python backend

Cấu trúc:

```text
vid/
├── app.py
├── config
├── models
├── services
├── ranking
└── utils
```

khá tốt về modularity.

Nhưng có technical debt rõ.

---

# 25. CRITICAL — duplicate RankingEngine

Có implementation cũ:

```text
services/ranking_engine.py
```

và implementation mới:

```text
ranking/engine.py
ranking/plugins/*
```

`app.py` import cả hai, trong đó import mới ghi đè tên cũ.

Đây là dấu hiệu migration chưa hoàn tất.

Nên xóa implementation cũ hoặc biến thành compatibility wrapper.

---

# 26. Ranking state

Các plugin:

```text
UserHistoryPlugin.seen
TabAffinityPlugin.affinity
```

lưu state process-local.

Nếu FastAPI chạy nhiều worker:

```text
worker A != worker B
```

state ranking sẽ không nhất quán.

Nếu cần personalization thực sự, dùng shared storage:

```text
SQLite/PostgreSQL/Redis
```

hoặc ghi rõ ranking chỉ là process-local cache.

---

# 27. Python cache concurrency

`TTLCache` không tự thread-safe.

Có:

```python
cache_locks = {}
```

nhưng creation của lock registry không được bảo vệ.

Hai request cùng key có thể tạo hai lock khác nhau.

Nên có lock cho lock registry hoặc cache abstraction duy nhất.

---

# 28. Unbounded memory

Các cấu trúc:

```python
url_locks = {}
cache_locks = {}
GLOBAL_STORE["videos"]
GLOBAL_STORE["index"]
```

không có eviction đầy đủ.

URL/video mới liên tục => memory tăng.

Cần:

```text
TTL
LRU
max_items
periodic cleanup
```

---

# 29. Thumbnail SSRF

Endpoint:

```python
/thumbnail?url=...
```

server fetch arbitrary URL.

Hiện bind localhost nên rủi ro thấp hơn production public, nhưng nếu service được expose ra LAN/public thì đây là SSRF vector.

Phải có:

- HTTP/HTTPS allowlist.
- block private IP.
- block localhost.
- DNS rebinding protection.
- redirect validation.
- timeout.
- response size limit.
- content-type validation.

---

# 30. CRITICAL — API key leak

`key_manager.py` có log:

```python
print(f"[release] Key server unavailable, released key locally: {k}")
```

và:

```python
print(f"[report] Key server unavailable, quota exceeded for key: {k}")
```

API key có thể xuất hiện trong log.

**Phải sửa ngay.**

Chỉ log:

```text
key_id
last4
hash
```

không log secret.

---

# 31. CORS

Hiện:

```python
allow_origins=["*"]
allow_credentials=True
```

không phù hợp production.

Dùng allowlist cụ thể.

---

# 32. Blocking file I/O

`keyword_manager.py` ghi JSON thường xuyên.

`track_search_keyword()` có thể:

```text
request
 -> update
 -> prune
 -> save file
```

Mỗi interaction có thể ghi disk.

Nên:

- debounce.
- batch save.
- background writer.
- database nếu cần scale.

---

# 33. Cache architecture

Ý tưởng:

```text
request
 -> memory cache
 -> background refill
 -> YouTube
 -> global store
 -> ranking
```

là tốt.

Nhưng có quá nhiều state store:

```text
search_cache
trending_cache
GLOBAL_STORE
ranking tabs
```

Nên có một `VideoRepository` quản lý source/cache/ranking boundary.

---

# 34. SettingsManager

`shared_mutex` là hướng tốt.

Nhưng save file trực tiếp có thể tạo file hỏng khi crash giữa write.

Nên:

```text
write .tmp
flush
atomic rename
```

---

# 35. GUI/Payload size

Một số file rất lớn:

```text
gui_widgets.cpp
popup_test.cpp
popup_setting.cpp
popup_url.cpp
sidebar_popup.cpp
```

`gui_widgets.cpp` đặc biệt lớn.

Nên tách thành:

```text
widgets/
├── buttons
├── sliders
├── timeline
├── media
├── layout
├── style
└── icons
```

---

# 36. Empty/legacy files

Archive có:

```text
src/windows/main/mpv_ui.cpp
src/windows/main/mpv_ui.h
src/windows/main/mpv_ui_settings.cpp
```

rỗng.

Nếu đã deprecated thì xóa.

---

# 37. Naming consistency

Có sự pha trộn:

```text
PlayBack
Playback
Player
Render
PlayBackRender
PlayBackProperty
```

Nên chuẩn hóa thành một convention, ví dụ:

```text
PlaybackRender
PlaybackProperty
PlaybackCommandDispatcher
PlaybackSession
```

---

# 38. Error handling

Hiện có:

```text
return false
return nullptr
return -1
throw
SDL_ShowSimpleMessageBox
SDL_Log
std::cout
std::cerr
Python print
```

Nên thống nhất:

```text
Core -> Error/expected
UI -> Notification
Logger -> central logging
```

Core không nên tự popup message box.

---

# 39. Logging

Nên gom:

```text
Trace
Debug
Info
Warn
Error
Fatal
```

và category:

```text
WINDOW
PLAYER
MPV
RENDER
AUDIO
API
NETWORK
CACHE
```

---

# 40. Build/dependency risk

Archive không chứa đầy đủ build files.

Native dependencies được suy ra từ include/source:

- SDL2
- ImGui
- libmpv
- OpenGL/gl3w
- cpr
- nlohmann/json
- stb_image
- freetype
- WinToast
- Windows SDK

Python:

- FastAPI
- Uvicorn
- requests
- aiohttp
- cachetools
- Pydantic/FastAPI dependencies

Nên quản lý bằng:

```text
CMake
vcpkg manifest
CMakePresets.json
pyproject.toml
lock file
```

---

# 41. Testability

Hiện code phụ thuộc mạnh vào:

```text
SDL global
OpenGL context
ImGui current context
mpv_handle
HWND
filesystem
network
YouTube
```

Nên tạo interfaces:

```text
IMpvClient
IGraphicsBackend
IWindowSystem
IClock
INetworkClient
IFileSystem
ISecretStore
```

Sau đó test:

```text
PlayerSession
FrameBufferPool
Ranking
Cache
WindowLifecycle
```

---

# 42. P0 — phải sửa trước

1. `WindowResource::GetPlayerSession()` qualified definition.
2. `WindowRuntime` ctor/dtor nếu không tồn tại ngoài archive.
3. MPV event routing không có SessionId.
4. `session->GetObserver()` null dereference.
5. `GetPlayerSession()->GetCommander()` null dereference.
6. `GetPlayerSession()->GetAudioFilterManager()` null dereference.
7. `MainWindowRenderer` null dereference.
8. FrameBufferPool có thể lấy buffer đang render.
9. FrameBufferPool metadata data race.

---

# 43. P1

1. `APIManager::m_isRunning` data race.
2. `ThreadManager::JoinAllRegistered()` lock-while-join.
3. detached core threads.
4. WindowManager destroy-under-lock.
5. raw WindowRuntime pointers cross-thread.
6. raw PlayerSession pointers cross-thread.
7. PropertyBag `GetPtr()` không lock.
8. PropertyBag reference lifetime.
9. GL context lifecycle.
10. ImGui context/thread contract.
11. duplicate ranking engine.
12. cache lock registry race.
13. unbounded Python global state.
14. API key leak.
15. CORS wildcard.
16. thumbnail SSRF.

---

# 44. P2

1. `main1.cpp` quá lớn.
2. `gui_widgets.cpp` quá lớn.
3. popup files quá lớn.
4. logging không thống nhất.
5. error model không thống nhất.
6. naming inconsistency.
7. duplicate/legacy code.
8. excessive `std::any`.
9. direct UI access subsystem state.
10. settings save per interaction.
11. process-local ranking nếu cần persistence.
12. thiếu event bus.

---

# 45. Kiến trúc V2 đề xuất

```text
Application
├── Runtime
│   ├── WindowRuntimeManager
│   ├── SessionRuntime
│   ├── EventBus
│   └── TaskScheduler
│
├── Window
│   ├── WindowManager
│   ├── WindowFactory
│   ├── WindowRuntime
│   ├── WindowRenderer
│   └── WindowGraphics
│
├── Playback
│   ├── PlaybackSession
│   ├── PlaybackController
│   ├── PlaybackStateStore
│   ├── PlaybackEventRouter
│   ├── PlaybackRenderer
│   └── PlaybackCommandQueue
│
├── Rendering
│   ├── GraphicsDevice
│   ├── GraphicsContext
│   ├── FrameSurface
│   └── FrameQueue
│
├── Audio
│   ├── AudioPipeline
│   ├── AudioFilter
│   ├── AdaptiveEngine
│   └── SafetyEngine
│
├── Services
│   ├── YouTubeService
│   ├── APIService
│   ├── CacheService
│   └── SecretService
│
└── UI
    ├── MainWindow
    ├── Controls
    ├── Popups
    └── Widgets
```

---

# 46. Event architecture V2

Thay vì:

```text
mpv -> SDL user event -> main -> đoán window
```

dùng:

```text
MPV Instance
    |
    v
PlaybackEventSource
    |
    v
EventBus
    |
    +--> Session A
    +--> Session B
    +--> UI
```

Event:

```cpp
struct PlaybackEvent {
    SessionId session;
    PlaybackEventType type;
    Payload payload;
};
```

Đây là thay đổi có giá trị kiến trúc rất lớn.

---

# 47. Render architecture V2

```text
             Producer
                |
                v
        +----------------+
        | FrameQueue     |
        |                |
        | FREE           |
        | WRITING        |
        | READY          |
        | DISPLAYING     |
        +----------------+
                |
                v
             Consumer
```

Invariant:

```text
Producer never writes READY/DISPLAYING.
Consumer never reads WRITING.
```

Nếu hết buffer:

```text
drop frame
```

không overwrite.

---

# 48. Ownership rules V2

## Window

```text
WindowManager owns WindowRuntime.
WindowRuntime owns resources.
```

## Session

```text
PlayerManager owns PlayerSession.
Window stores SessionId.
```

## MPV

```text
PlayerSession exclusively owns mpv_handle.
```

## Render

```text
RenderSubsystem owns render context/FBO queue.
```

## Threads

```text
Owner object owns std::jthread.
```

Không detached core runtime thread.

---

# 49. Roadmap

## Phase 0 — Stabilization

1. Fix compile/link blockers.
2. Fix null dereference.
3. Fix MPV event routing.
4. Fix FrameBufferPool.
5. Fix APIManager race.
6. Fix ThreadManager join.
7. Remove secret logging.
8. Fix CORS/thumbnail restrictions.

## Phase 1 — Lifecycle

1. Window lifecycle.
2. PlayerSession lifecycle.
3. ownership.
4. deterministic thread shutdown.
5. resource shutdown ordering.

## Phase 2 — Event Bus

1. PlaybackEvent.
2. WindowEvent.
3. AppEvent.
4. session-aware routing.

## Phase 3 — Rendering

1. FrameQueue.
2. GPU fence ownership.
3. render context ownership.
4. immutable frame metadata.

## Phase 4 — UI

1. split giant widgets file.
2. state snapshots.
3. remove direct subsystem access.
4. notification/error layer.

## Phase 5 — Python

1. remove duplicate ranking.
2. cache abstraction.
3. persistent ranking if required.
4. SSRF protection.
5. secret handling.
6. bounded memory.

## Phase 6 — Testing

1. unit tests.
2. integration tests.
3. lifecycle tests.
4. stress tests.
5. multi-window tests.
6. multi-session tests.
7. renderer stress tests.

---

# 50. Test matrix bắt buộc

## Window

- create main.
- create child.
- destroy child.
- destroy parent.
- rapid create/destroy.
- resize while rendering.
- minimize/restore.
- fullscreen.

## MPV

- local file.
- YouTube URL.
- live stream.
- invalid URL.
- load/unload rapidly.
- rapid seek.
- playlist.
- multiple sessions.
- destroy session while event pending.

## Rendering

- rapid resize.
- minimize during rendering.
- close during MPV render.
- context failure.
- FBO allocation failure.
- frame queue exhaustion.

## Audio

- filter toggle.
- adaptive toggle.
- reset.
- shutdown while update active.

## Python

- concurrent search.
- concurrent trending.
- concurrent thumbnail.
- duplicate thumbnail requests.
- invalid thumbnail URL.
- oversized response.
- key server unavailable.
- quota exhausted.
- offline mode.

---

# 51. Static quality score

| Area | Score |
|---|---:|
| Module separation | 7/10 |
| Naming/API consistency | 5/10 |
| Ownership | 6/10 |
| Threading | 4/10 |
| Rendering safety | 4/10 |
| MPV integration | 5/10 |
| Window architecture | 7/10 |
| State model | 8/10 |
| Error handling | 4/10 |
| Security | 4/10 |
| Python backend | 6/10 |
| Testability | 3/10 |
| Extensibility | 7/10 |

**Overall static architecture: khoảng 5.5–6/10.**

Đây không phải codebase tệ. Vấn đề chính là **ý tưởng kiến trúc đang đi trước mức độ formalization của lifecycle/thread contract**.

---

# 52. Điểm mạnh nên giữ

1. `PlayerSession`.
2. `PlayerManager`.
3. `PlayerStateSystem`.
4. `WindowTemplateBuilder`.
5. `WindowTemplateRegistry`.
6. `IGraphicsBackend`.
7. `OpenGLFrameBufferPool` nhưng cần viết lại state machine.
8. `AudioFilterManager`.
9. Python ranking plugin architecture.
10. Cache refill concept.
11. WindowSharedGroup.
12. ConfigManager locking model.

---

# 53. Nên loại bỏ/refactor

1. `main1.cpp` logic khổng lồ.
2. detached core threads.
3. raw pointer cross-thread.
4. PropertyBag pointer/reference API.
5. duplicate ranking engine.
6. API key logging.
7. wildcard CORS.
8. arbitrary thumbnail fetch.
9. global mutable state không cần thiết.
10. excessive `std::any` ở boundary.
11. repeated `GetPlayerSession()`.
12. giant GUI files.
13. empty legacy MPV UI files.

---

# 54. Migration an toàn

Không rewrite big-bang.

### Bước 1

Tạo:

```text
SessionId
WindowId
PlaybackEvent
```

### Bước 2

Gắn SessionId vào MPV callback.

### Bước 3

Sửa main event router.

### Bước 4

Sửa toàn bộ:

```cpp
GetPlayerSession()->...
```

thành checked local pointer.

### Bước 5

Fix FrameBufferPool.

### Bước 6

Fix WindowRuntime lifecycle.

### Bước 7

Chuyển core threads sang `jthread`.

### Bước 8

Tách application orchestrator.

### Bước 9

Tách Python ranking/cache.

### Bước 10

Viết integration tests.

---

# 55. Production readiness checklist

```text
[ ] Build clean
[ ] No linker errors
[ ] No known null dereference
[ ] No known data race
[ ] MPV events session-aware
[ ] Multi-session verified
[ ] Frame queue state machine verified
[ ] GL fence ownership verified
[ ] Window destruction race tested
[ ] Thread shutdown deterministic
[ ] No detached core threads
[ ] API secrets never logged
[ ] Thumbnail SSRF protection
[ ] CORS allowlist
[ ] Cache bounded
[ ] Ranking persistence policy defined
[ ] Settings atomic write
[ ] Central logging
[ ] Central error handling
[ ] Unit tests
[ ] Integration tests
[ ] Stress tests
[ ] Sanitizer/static analysis
[ ] Release build tested
```

---

# 56. Kết luận

Codebase có nền tảng tốt và có khả năng phát triển thành media player architecture lớn, đặc biệt ở:

```text
WindowTemplate
WindowRuntime
PlayerSession
PlayerStateSystem
IGraphicsBackend
AudioFilterManager
RankingPlugin
```

Nhưng phần khó nhất:

```text
lifecycle
threading
GPU synchronization
MPV event routing
multi-session
```

chưa được khóa bằng invariant rõ ràng.

Nếu tiếp tục thêm feature ngay, nguy cơ tăng các lỗi:

```text
random crash
black frame
flicker
deadlock
use-after-free
wrong-session event
GPU resource corruption
```

### Thứ tự ưu tiên tuyệt đối

```text
1. Compile/link correctness
2. Null safety
3. MPV session-aware event routing
4. Window/Session lifetime
5. FrameBufferPool synchronization
6. Thread shutdown
7. ImGui/OpenGL context ownership
8. Python cache/security
9. Tests
10. Feature expansion
```

**Không nên rewrite toàn bộ.** Hướng đúng là **stabilization + incremental refactor** trên các abstraction hiện có.

---

# 57. File inventory

- Tổng số entries thực: **161**
- Tổng số dòng code/header/Python: **26,515**

## src/FOLDER_INFO.md

- `src/FOLDER_INFO.md` — 6,343 bytes

## src/FontManager.cpp

- `src/FontManager.cpp` — 29,321 bytes, 735 lines

## src/api

- `src/api/FOLDER_INFO.md` — 4,658 bytes
- `src/api/api_log.h` — 114 bytes, 4 lines
- `src/api/api_manager.cpp` — 6,250 bytes, 152 lines
- `src/api/api_manager.h` — 1,542 bytes, 54 lines
- `src/api/api_types.h` — 1,004 bytes, 29 lines
- `src/api/provider/gemini.h` — 766 bytes, 17 lines

## src/backends

- `src/backends/D3D11Backend.h` — 4,414 bytes, 105 lines
- `src/backends/FOLDER_INFO.md` — 5,568 bytes
- `src/backends/IGraphicsBackend.h` — 1,564 bytes, 40 lines
- `src/backends/OpenGLBackend.h` — 6,320 bytes, 165 lines
- `src/backends/backend.cpp` — 480 bytes, 19 lines
- `src/backends/backend.h` — 517 bytes, 21 lines
- `src/backends/client_backend.cpp` — 10,123 bytes, 275 lines
- `src/backends/client_backend.h` — 1,245 bytes, 60 lines
- `src/backends/vid.h` — 3,802 bytes, 110 lines
- `src/backends/vid/__init__.py` — 0 bytes, 0 lines
- `src/backends/vid/app.py` — 6,556 bytes, 228 lines
- `src/backends/vid/config/logger_config.py` — 2,697 bytes, 89 lines
- `src/backends/vid/config/settings.py` — 1,567 bytes, 52 lines
- `src/backends/vid/models/manager_key.py` — 83 bytes, 4 lines
- `src/backends/vid/models/video_item.py` — 109 bytes, 7 lines
- `src/backends/vid/ranking/__init__.py` — 0 bytes, 0 lines
- `src/backends/vid/ranking/engine.py` — 1,750 bytes, 60 lines
- `src/backends/vid/ranking/plugins/__init__.py` — 0 bytes, 0 lines
- `src/backends/vid/ranking/plugins/base.py` — 399 bytes, 17 lines
- `src/backends/vid/ranking/plugins/freshness.py` — 500 bytes, 18 lines
- `src/backends/vid/ranking/plugins/tab_affinity.py` — 639 bytes, 20 lines
- `src/backends/vid/ranking/plugins/user_history.py` — 593 bytes, 22 lines
- `src/backends/vid/ranking/resolver.py` — 388 bytes, 14 lines
- `src/backends/vid/ranking/store.py` — 357 bytes, 15 lines
- `src/backends/vid/services/caches.py` — 8,972 bytes, 271 lines
- `src/backends/vid/services/key_manager.py` — 1,047 bytes, 29 lines
- `src/backends/vid/services/keyword_manager.py` — 3,263 bytes, 113 lines
- `src/backends/vid/services/ranking_engine.py` — 2,658 bytes, 93 lines
- `src/backends/vid/services/wrapper_service.py` — 3,600 bytes, 101 lines
- `src/backends/vid/services/youtube_fetch.py` — 9,434 bytes, 272 lines
- `src/backends/vid/utils/network.py` — 292 bytes, 12 lines
- `src/backends/vid/utils/text_utils.py` — 477 bytes, 15 lines
- `src/backends/vid/utils/video_utils.py` — 2,352 bytes, 83 lines

## src/common

- `src/common/Exception.h` — 920 bytes, 31 lines
- `src/common/FOLDER_INFO.md` — 1,819 bytes

## src/gl3w.c

- `src/gl3w.c` — 22,074 bytes, 926 lines

## src/globals.cpp

- `src/globals.cpp` — 367 bytes, 19 lines

## src/gui

- `src/gui/FOLDER_INFO.md` — 4,435 bytes
- `src/gui/gui.cpp` — 11,383 bytes, 238 lines
- `src/gui/gui.h` — 27,274 bytes, 664 lines
- `src/gui/gui_widgets.cpp` — 137,713 bytes, 3,549 lines
- `src/gui/gui_widgets.h` — 3,806 bytes, 127 lines

## src/hotkey_handler.cpp

- `src/hotkey_handler.cpp` — 11,152 bytes, 394 lines

## src/main1.cpp

- `src/main1.cpp` — 14,792 bytes, 414 lines

## src/notification.cpp

- `src/notification.cpp` — 5,455 bytes, 187 lines

## src/player

- `src/player/PlayerDataModels.h` — 7,361 bytes, 294 lines
- `src/player/PlayerStateSystem.h` — 6,522 bytes, 226 lines
- `src/player/PlayerUtils.h` — 9,387 bytes, 237 lines
- `src/player/README.md` — 19,567 bytes
- `src/player/audio/filter/af_m.h` — 4,856 bytes, 135 lines
- `src/player/audio/filter/af_m_ai.cpp` — 28,985 bytes, 546 lines
- `src/player/audio/filter/af_m_core.cpp` — 8,998 bytes, 236 lines
- `src/player/audio/filter/af_m_io.cpp` — 4,480 bytes, 100 lines
- `src/player/audio/filter/af_m_log.h` — 114 bytes, 4 lines
- `src/player/audio/filter/af_m_safety.cpp` — 4,365 bytes, 107 lines
- `src/player/audio/filter/af_m_sync.cpp` — 7,475 bytes, 190 lines
- `src/player/audio/filter/af_m_types.h` — 6,502 bytes, 151 lines
- `src/player/command/PlaybackCommand.cpp` — 4,546 bytes, 127 lines
- `src/player/command/PlaybackCommand.h` — 1,328 bytes, 48 lines
- `src/player/event/BuildFormatYTDLP.cpp` — 13,617 bytes, 321 lines
- `src/player/event/PlaybackObserver.cpp` — 46,332 bytes, 827 lines
- `src/player/event/PlaybackObserver.h` — 2,231 bytes, 60 lines
- `src/player/player/Player.cpp` — 1,224 bytes, 55 lines
- `src/player/player/Player.h` — 413 bytes, 20 lines
- `src/player/property/PlayBackProperty.cpp` — 1,428 bytes, 40 lines
- `src/player/property/PlayBackProperty.h` — 608 bytes, 21 lines
- `src/player/render/IFrameBufferPool.h` — 1,410 bytes, 41 lines
- `src/player/render/OpenGLFrameBufferPool.cpp` — 7,850 bytes, 198 lines
- `src/player/render/OpenGLFrameBufferPool.h` — 992 bytes, 32 lines
- `src/player/render/PlayBackRender.cpp` — 3,725 bytes, 105 lines
- `src/player/render/PlayBackRender.h` — 1,453 bytes, 45 lines
- `src/player/render/PlayBackRenderThread.cpp` — 6,657 bytes, 190 lines
- `src/player/render/PlayBackRenderThread.h` — 834 bytes, 35 lines
- `src/player/render/PlayBackRenderThreadState.h` — 1,521 bytes, 52 lines
- `src/player/scripts/script_manager.cpp` — 4,691 bytes, 125 lines
- `src/player/scripts/script_manager.h` — 1,209 bytes, 45 lines
- `src/player/session/FOLDER_INFO.md` — 3,356 bytes
- `src/player/session/PlayerManager.cpp` — 2,302 bytes, 83 lines
- `src/player/session/PlayerManager.h` — 1,253 bytes, 43 lines
- `src/player/session/PlayerSession.cpp` — 3,128 bytes, 81 lines
- `src/player/session/PlayerSession.h` — 1,939 bytes, 52 lines
- `src/player/shaders/shader_manager.cpp` — 24,719 bytes, 735 lines
- `src/player/shaders/shaders_manager.h` — 4,351 bytes, 141 lines
- `src/player/video/filter/video_filter_manager.cpp` — 4,547 bytes, 137 lines
- `src/player/video/filter/video_filter_manager.h` — 1,978 bytes, 60 lines

## src/popup

- `src/popup/popup.cpp` — 1,291 bytes, 58 lines
- `src/popup/popup.h` — 490 bytes, 18 lines
- `src/popup/popup_about_video.cpp` — 24,634 bytes, 637 lines
- `src/popup/popup_about_video.h` — 172 bytes, 5 lines
- `src/popup/popup_setting.cpp` — 26,271 bytes, 565 lines
- `src/popup/popup_setting.h` — 252 bytes, 7 lines
- `src/popup/popup_test.cpp` — 41,049 bytes, 816 lines
- `src/popup/popup_test.h` — 158 bytes, 4 lines
- `src/popup/popup_url.cpp` — 25,113 bytes, 620 lines
- `src/popup/popup_url.h` — 757 bytes, 29 lines
- `src/popup/reusable_popup.cpp` — 2,049 bytes, 69 lines
- `src/popup/reusable_popup.h` — 746 bytes, 34 lines
- `src/popup/sidebar_popup.cpp` — 32,818 bytes, 932 lines
- `src/popup/sidebar_popup.h` — 242 bytes, 9 lines

## src/settings_manager.cpp

- `src/settings_manager.cpp` — 3,816 bytes, 84 lines

## src/settings_manager.h

- `src/settings_manager.h` — 4,035 bytes, 123 lines

## src/stb_impl.cpp

- `src/stb_impl.cpp` — 56 bytes, 2 lines

## src/threads

- `src/threads/thread_id.h` — 1,483 bytes, 44 lines
- `src/threads/thread_manager.cpp` — 3,844 bytes, 119 lines
- `src/threads/thread_manager.h` — 3,618 bytes, 96 lines

## src/utils.cpp

- `src/utils.cpp` — 3,755 bytes, 127 lines

## src/windows

- `src/windows/EventQueue.h` — 1,030 bytes, 40 lines
- `src/windows/FOLDER_INFO.md` — 3,274 bytes
- `src/windows/NativeWindowRegistrar.cpp` — 1,453 bytes, 32 lines
- `src/windows/NativeWindowRegistrar.h` — 161 bytes, 9 lines
- `src/windows/README.md` — 10,532 bytes
- `src/windows/UIRenderThread.cpp` — 6,087 bytes, 175 lines
- `src/windows/UIRenderThread.h` — 2,185 bytes, 70 lines
- `src/windows/UpdateWindowState.h` — 7,860 bytes, 177 lines
- `src/windows/WindowController.cpp` — 6,086 bytes, 168 lines
- `src/windows/WindowController.h` — 481 bytes, 23 lines
- `src/windows/WindowDefs.h` — 2,818 bytes, 118 lines
- `src/windows/WindowFactory.h` — 2,573 bytes, 85 lines
- `src/windows/WindowFlagBuilder.h` — 1,572 bytes, 71 lines
- `src/windows/WindowInfo.h` — 178 bytes, 11 lines
- `src/windows/WindowInitializer.cpp` — 6,019 bytes, 162 lines
- `src/windows/WindowInitializer.h` — 874 bytes, 26 lines
- `src/windows/WindowManager.h` — 11,742 bytes, 403 lines
- `src/windows/WindowProcHook.cpp` — 10,904 bytes, 280 lines
- `src/windows/WindowPropertyBag.h` — 21,381 bytes, 581 lines
- `src/windows/WindowRelation.cpp` — 704 bytes, 28 lines
- `src/windows/WindowRelation.h` — 516 bytes, 23 lines
- `src/windows/WindowRenderer.h` — 379 bytes, 14 lines
- `src/windows/WindowResource.h` — 1,264 bytes, 38 lines
- `src/windows/WindowRuntime.h` — 1,304 bytes, 53 lines
- `src/windows/WindowSharedGroup.cpp` — 161 bytes, 6 lines
- `src/windows/WindowSharedGroup.h` — 200 bytes, 12 lines
- `src/windows/WindowTemplate.h` — 725 bytes, 29 lines
- `src/windows/WindowTemplateBuilder.h` — 3,059 bytes, 89 lines
- `src/windows/WindowUtils.h` — 747 bytes, 23 lines
- `src/windows/main/MainWindowRenderer.cpp` — 15,805 bytes, 353 lines
- `src/windows/main/MainWindowRenderer.h` — 692 bytes, 26 lines
- `src/windows/main/MainWindowState.h` — 537 bytes, 21 lines
- `src/windows/main/mpv_ui.cpp` — 0 bytes, 0 lines
- `src/windows/main/mpv_ui.h` — 0 bytes, 0 lines
- `src/windows/main/mpv_ui_settings.cpp` — 0 bytes, 0 lines
- `src/windows/main/ui/ui.h` — 163 bytes, 9 lines
- `src/windows/main/ui/ui_controls.cpp` — 27,266 bytes, 643 lines
- `src/windows/main/ui/ui_controls.h` — 320 bytes, 11 lines
- `src/windows/main/ui/ui_overlays.cpp` — 8,617 bytes, 250 lines
- `src/windows/main/ui/ui_overlays.h` — 484 bytes, 13 lines
- `src/windows/main/ui/ui_settings.cpp` — 19,250 bytes, 447 lines
- `src/windows/main/ui/ui_settings.h` — 315 bytes, 12 lines
- `src/windows/main/ui/ui_widgets.cpp` — 7,080 bytes, 185 lines
- `src/windows/main/ui/ui_widgets.h` — 628 bytes, 13 lines
- `src/windows/sub/MockSubWindowRenderer.cpp` — 1,802 bytes, 45 lines
- `src/windows/sub/MockSubWindowRenderer.h` — 428 bytes, 14 lines

## src/wintoastlib.cpp

- `src/wintoastlib.cpp` — 64,754 bytes, 1,514 lines


---

# 58. File rỗng

- `src/backends/vid/ranking/plugins/__init__.py`
- `src/backends/vid/ranking/__init__.py`
- `src/backends/vid/__init__.py`
- `src/windows/main/mpv_ui.cpp`
- `src/windows/main/mpv_ui.h`
- `src/windows/main/mpv_ui_settings.cpp`

---

# 59. Độ tin cậy của báo cáo

Đây là **static source review**.

Archive không chứa đầy đủ:

- CMake/build files.
- dependency source.
- installed package tree.
- compiler/linker flags.
- runtime config/data.
- external libraries.

Vì vậy các lỗi có bằng chứng trực tiếp từ source có độ tin cậy cao; các race/lifetime issue cần được xác nhận thêm bằng Debug build, sanitizer và stress test.

---

# 60. Prompts tiếp theo

### Prompt 1 — sửa P0

```text
Dựa trên báo cáo src(7), hãy lập kế hoạch sửa toàn bộ lỗi P0 theo thứ tự dependency, chỉ rõ file cần sửa, API cần thay đổi và patch code cụ thể.
```

### Prompt 2 — MPV event routing

```text
Thiết kế lại toàn bộ MPV event routing để hỗ trợ nhiều PlayerSession và WindowRuntime, không fallback về main window. Đưa ra kiến trúc, data structure và code C++ cụ thể.
```

### Prompt 3 — FrameBufferPool

```text
Phân tích và viết lại OpenGLFrameBufferPool thành triple-buffer state machine thread-safe, đảm bảo không bao giờ ghi vào buffer đang RENDERING hoặc DISPLAYING và quản lý GL fence đúng ownership.
```

### Prompt 4 — lifecycle

```text
Thiết kế lại lifecycle WindowRuntime -> PlayerSession -> MPV -> RenderThread -> GraphicsContext theo RAII và std::jthread, bảo đảm shutdown không deadlock/use-after-free.
```

### Prompt 5 — Python backend

```text
Phân tích riêng backend Python trong src/backends/vid và viết kế hoạch refactor cache, ranking, thumbnail proxy, API key handling và concurrency thành kiến trúc production-ready.
```

### Prompt 6 — test plan

```text
Từ toàn bộ lỗi trong báo cáo, tạo bộ test C++/Python theo P0/P1/P2, ưu tiên race condition, deadlock, use-after-free, multi-session MPV và FBO corruption.
```
