# Kiến trúc Hệ thống MPV

Tài liệu này mô tả kiến trúc và luồng hoạt động của các thành phần tích hợp `libmpv` trong dự án.

## Sơ đồ kiến trúc tổng quan

```
┌──────────────────┐      ┌──────────────────┐      ┌──────────────────┐
|  WindowRuntime   |----->|    PlayerManager    |----->|    PlayerSession    |
└──────────────────┘      └──────────────────┘      └──────────────────┘
 (Giữ con trỏ thô)         (Singleton, sở hữu     (Sở hữu tất cả các
                          toàn bộ PlayerSession)      thành phần MPV)
                                                        │
           ┌────────────────────────────────────────────┴───────────────────────────────────────────┐
           │                                                                                        │
┌──────────▼──────────┐  ┌───────────▼──────────┐  ┌───────────▼──────────┐  ┌──────────▼───────────┐  ┌──────────▼──────────┐
|     Player       |  |      PlayBackRender       |  |      PlaybackObserver     |  | PlaybackCommandDispatcher |  |     PlayBackProperty      |
└─────────────────────┘  └──────────────────────┘  └──────────────────────┘  └──────────────────────┘  └──────────────────────┘
 (Sở hữu mpv_handle)     (Sở hữu mpv_render_ctx)   (Lắng nghe sự kiện)       (Gửi lệnh)                (Get/Set thuộc tính)
                                │
                                │ RENDER_MPV_THREAD
                                ▼
                      ┌──────────────────────┐
                      |   PlayBackRenderThread    | (std::shared_ptr)
                      └──────────────────────┘
                                │
           ┌────────────────────┴────────────────────┐
           │                                         │
┌──────────▼──────────┐                   ┌──────────▼──────────┐
|     MPVInstance     |                   |   FrameBufferPool   |
└─────────────────────┘                   └─────────────────────┘
 (Struct chứa state)                       (Quản lý các FBO)

```

---

## 1. Lớp Quản lý & Session

### `PlayerManager.h` & `.cpp`
- **Vai trò**: Là một **Singleton**, đóng vai trò là trình quản lý trung tâm cho tất cả các `PlayerSession`.
- **Chức năng chính**:
  - `CreateSession`: Tạo một session mới và khởi tạo nó với một `WindowRuntime`.
  - `RegisterSession`: Đăng ký một session được tạo từ bên ngoài (ví dụ: từ `main()`), nhận quyền sở hữu `std::unique_ptr`.
  - `GetDefaultSession`: Cung cấp truy cập nhanh đến session chính.
  - `DestroySession`: Hủy một session và giải phóng tài nguyên.
- **Quyền sở hữu**: Sở hữu toàn bộ các `PlayerSession` thông qua `std::unordered_map<std::string, std::unique_ptr<PlayerSession>>`.

### `PlayerSession.h` & `.cpp`
- **Vai trò**: Đóng gói tất cả các thành phần liên quan đến một thực thể player MPV. Đây là lớp giao tiếp chính giữa `WindowRuntime` và lõi MPV.
- **Chức năng chính**:
  - `Init()`: Khởi tạo tất cả các thành phần con (`Player`, `PlayBackRender`, `PlayBackRenderThread`...).
  - `Shutdown()`: Dọn dẹp và giải phóng tài nguyên theo đúng thứ tự.
  - Sở hữu các thành phần cốt lõi (`Player`, `PlayBackRender`, `PlaybackObserver`...) thông qua `std::unique_ptr`.
  - Sở hữu luồng render (`PlayBackRenderThread`) thông qua `std::shared_ptr` để cho phép truy cập an toàn từ các luồng khác (ví dụ: `UpdateMainWindowState`).

## 2. Các thành phần cốt lõi của `PlayerSession`

- **`property/PlayBackProperty`**: Cung cấp các API để get/set các thuộc tính của MPV một cách an toàn (ví dụ: `GetString()`, `SetString()`). Sử dụng template `mpv_get_prop<T>` để đơn giản hóa việc lấy dữ liệu với nhiều kiểu khác nhau.


## 3. Kiến trúc Render (Quan trọng)

Hệ thống hỗ trợ hai chế độ render thông qua macro `RENDER_MPV_THREAD`.

### Chế độ 1: Single-Thread (Mặc định)
- `render/PlayBackRender`: Khởi tạo `mpv_render_context`.
- `mpv_render_context_set_update_callback`: Đăng ký một callback để đẩy sự kiện `SDL_MPV_RENDER_UPDATE` vào hàng đợi của SDL mỗi khi có frame mới.
- Vòng lặp chính (`main.cpp`) bắt sự kiện này và gọi `PlayBackRender::Render()` để vẽ trực tiếp lên framebuffer của ImGui.

### Chế độ 2: Multi-Thread (`RENDER_MPV_THREAD` được định nghĩa)
Đây là kiến trúc phức tạp nhưng hiệu quả hơn, tách biệt việc giải mã/render của MPV ra khỏi luồng chính (UI).

#### `render/PlayBackRenderThread.h` & `.cpp`
- **Vai trò**: Một luồng chuyên dụng chỉ để render video từ MPV.
- **Vòng đời**: Được tạo và sở hữu bởi `PlayerSession` qua `std::shared_ptr`.
- **Luồng hoạt động (`Run()`):**
  1.  **Chờ đợi**: Luồng sẽ "ngủ" bằng `cv.wait_for()` cho đến khi được đánh thức. Nó sẽ tự thức dậy sau một khoảng thời gian ngắn (ví dụ: 100ms) để tránh bị treo.
  2.  **Đánh thức**: Được đánh thức bởi:
      - `RequestRender()`: Khi có frame mới từ MPV (`update_callback`) hoặc khi có yêu cầu vẽ lại từ luồng chính.
      - `Notify()`: Khi luồng chính thay đổi kích thước cửa sổ (`UpdateMainWindowState`).
  3.  **Render vào FBO**:
      - Yêu cầu một framebuffer trống từ `FrameBufferPool`.
      - Gọi `mpv_render_context_render()` để vẽ frame video vào FBO này.
      - Đánh dấu FBO là "sẵn sàng" (`MarkAsReady`).
  4.  **Thông báo cho luồng chính**: Đẩy sự kiện `SDL_MPV_RENDER_UPDATE` để báo cho luồng chính rằng có frame mới để vẽ lên UI.

#### `mpv_instance.h`
- **Vai trò**: Một struct dữ liệu (`MPVInstance`) chứa **toàn bộ trạng thái** cần thiết cho `PlayBackRenderThread`.
- **Thiết kế**: Việc gom tất cả state vào đây (bao gồm `mpv_handle`, `render_ctx`, `graphicsBackend`, mutex, cờ...) giúp luồng render trở nên độc lập, không cần truy cập vào các đối tượng không an toàn từ luồng khác như `WindowRuntime`.

#### `render/FrameBufferPool.h` & `.cpp`
- **Vai trò**: Quản lý một nhóm các Frame Buffer Objects (FBOs), thường là 3 (triple buffering).
- **Luồng hoạt động**:
  - `AcquireFreeBuffer()`: `PlayBackRenderThread` gọi để lấy một FBO đang rảnh.
  - `MarkAsReady(index)`: `PlayBackRenderThread` gọi sau khi đã render xong vào FBO.
  - `GetStableFrame()`: **Luồng chính (UI)** gọi hàm này. Nó sẽ tìm FBO mới nhất đã "sẵn sàng" và trả về texture ID của nó để ImGui vẽ. Đồng thời, nó đánh dấu FBO cũ (đang hiển thị) là "rảnh" để luồng render có thể tái sử dụng.
- **Lợi ích**: Cơ chế này cho phép luồng render và luồng UI hoạt động song song mà không khóa lẫn nhau, tạo ra trải nghiệm mượt mà, không bị xé hình (tearing).

#### `render/PlayBackRender.cpp` (trong chế độ thread)
- **Vai trò**: Thay vì tự render, hàm `Render()` giờ đây chỉ đơn giản là gọi `fboPool->GetStableFrame()` để lấy texture đã được luồng phụ vẽ sẵn và hiển thị nó bằng `ImGui::Image()`.


## 4. Các Hệ thống Mở rộng

### `mpv_ui.cpp` - Giao diện Player
- **Vai trò**: Chịu trách nhiệm vẽ toàn bộ giao diện người dùng (UI) tương tác trực tiếp với video (lớp phủ) bằng ImGui.
- **API chính**:
  - `RenderPlayerControls`: Vẽ thanh điều khiển chính (play/pause, seekbar, volume, fullscreen, settings...). Logic ẩn/hiện và animation được quản lý tại đây.
  - `RenderIdleBackground`: Hiển thị ảnh nền khi không có video nào đang phát.
  - `RenderLoading`: Hiển thị icon loading xoay tròn khi MPV đang tải/buffer.
  - `RenderSeekingOverlay`: Hiển thị hiệu ứng mũi tên tua tới/lui khi người dùng seek video.
  - `RenderGhostStatusOverlay`: Hiển thị icon Play/Pause lớn mờ dần ở giữa màn hình khi người dùng thay đổi trạng thái phát.

### `shaders/ShaderManager.cpp` - Quản lý Hiệu ứng Hình ảnh
- **Vai trò**: Là một **Singleton** quản lý toàn bộ vòng đời của các shader GLSL, cho phép người dùng thêm/bớt, tùy chỉnh và sắp xếp các hiệu ứng xử lý hậu kỳ (post-processing) cho video.
- **Luồng hoạt động**:
  1.  **Load**: Quét các thư mục được chỉ định (`searchPaths`) để tìm các file `.glsl` hoặc `.hook`.
  2.  **Parse**: Đọc metadata trong comment của shader (ví dụ: `//!DESC`, `//!HOOK`, `//!PARAM`) để nhận diện thông tin, các tham số tùy chỉnh (`ShaderParam`), và giai đoạn can thiệp (`HookStage`).
  3.  **Generate & Apply**: Khi pipeline thay đổi (bật/tắt, sắp xếp lại), `ShaderManager` sẽ:
      - Tạo ra một file shader tạm thời (`_active.glsl`) cho mỗi shader đang hoạt động.
      - "Nướng" (bake) các giá trị tham số (`p.value`) trực tiếp vào file tạm dưới dạng `#define`.
      - Nối chuỗi đường dẫn của các file tạm này lại với nhau.
      - Gửi một lệnh `set glsl-shaders` duy nhất cho MPV để áp dụng toàn bộ pipeline.
  4.  **State Management**: Lưu và tải trạng thái (danh sách shader đang bật, giá trị tham số, thứ tự) vào file JSON để giữ nguyên cấu hình giữa các lần chạy.
- **API chính**:
  - `AddFolder(path)` / `RemoveFolder(path)`: Quản lý các thư mục chứa shader.
  - `Enable(name)` / `Disable(name)` / `Toggle(name)`: Bật/tắt một shader.
  - `MoveUp(name)` / `MoveDown(name)`: Thay đổi thứ tự ưu tiên của shader trong cùng một `HookStage`.
  - `ApplyChanges(name)`: Lưu các giá trị `temp_value` (từ UI) vào `value` và áp dụng lại pipeline.
  - `ResetToDefault(name)`: Khôi phục tham số của một shader về giá trị mặc định.
  - `SaveState()` / `LoadState()`: Quản lý persistence.

### `audio/filter/` - Hệ thống Filter Âm thanh
- **Vai trò**: Cung cấp một kiến trúc để quản lý và áp dụng các bộ lọc âm thanh của `libavfilter` (được MPV hỗ trợ qua cờ `--af`).
- **Cấu trúc dữ liệu (`af_m_types.h`)**:
  - `AudioFilter`: Đại diện cho một filter (ví dụ: `acompressor`, `superequalizer`). Chứa ID, tên, trạng thái `enabled`, và danh sách các tham số (`FilterParam`).
  - `FilterParam`: Lưu trữ các giá trị `min`, `max`, `default`, và `current` cho một tham số của filter.
  - `AudioContext`: Một struct lớn chứa toàn bộ thông tin thời gian thực về luồng âm thanh, được lấy từ các thuộc tính của MPV (ví dụ: `loudness_momentary`, `true_peak`, `codec`...). Đây là đầu vào cho các thuật toán xử lý âm thanh thông minh.
  - `AdaptiveTargets`: Struct chứa các giá trị mục tiêu do AI hoặc các preset tính toán ra (ví dụ: các dải tần EQ, mức độ nén...).
- **Mục tiêu**: Kiến trúc này được thiết kế để làm nền tảng cho một hệ thống âm thanh "thích ứng", có khả năng tự động điều chỉnh các filter dựa trên nội dung đang phát (ví dụ: tự động tăng âm lượng lời thoại trong phim, hoặc kích bass cho nhạc EDM).

### `mpv_basic_formats.cpp`
  - `HandleYTDLLog`: Bắt và phân tích chuỗi JSON mà `yt-dlp` xuất ra trong log của MPV.
  - `ExtractAllFormats`: Trích xuất thông tin chi tiết từ JSON (độ phân giải, codec, bitrate...).
  - `BuildVideoOptions`, `BuildAudioOptions`: Tạo ra các danh sách định dạng thân thiện với người dùng để hiển thị trên UI.

### `utils.cpp` (phần liên quan đến MPV)


Tài liệu này cung cấp một cái nhìn sâu sắc về cách hệ thống MPV được cấu trúc, từ lõi render đa luồng cho đến các hệ thống mở rộng như quản lý shader và filter âm thanh, giúp việc bảo trì và phát triển các tính năng mới trở nên dễ dàng và có hệ thống hơn.

---

## Định hướng Kiến trúc Tương lai (Refactoring Plan)

Để giải quyết các vấn đề về sự phức tạp ngày càng tăng và tạo nền tảng vững chắc cho các tính năng trong tương lai, kiến trúc của hệ thống MPV sẽ được tái cấu trúc theo các định hướng sau:

### Sơ đồ kiến trúc mục tiêu

```
   PlayerManager
        │
        ▼
   PlayerSession (Coordinator)
        │
 ┌──────┴──────────────────────────────────┐
 │                                         │
 ▼                                         ▼
PlaybackRuntime                       RenderRuntime
 │                                         │
Player (mpv_handle)                   RenderContext (mpv_render_ctx)
Observer                              RenderThread (Job Queue)
Command                               TexturePool
Property (Cache)                      GraphicsBackend
Playlist                              ShaderPipeline

                │
                ▼
          EventDispatcher
                │
      ┌─────────┼─────────┐
      ▼         ▼         ▼
 Subtitle    Thumbnail   Audio
 Extension   Extension   Filter

                │
                ▼
           MPVStateCache
                │
                ▼
               UI (Read-only)
```

### Các Nguyên tắc Tái cấu trúc chính

1.  **Phân rã "God Object" `PlayerSession`**:
    - **Vấn đề**: `PlayerSession` hiện đang sở hữu quá nhiều trách nhiệm (Player, Render, Observer, Command, Property, Playlist, Subtitle...).
    - **Giải pháp**: Chia `PlayerSession` thành các `Subsystem` chuyên biệt (`PlaybackSubsystem`, `RenderSubsystem`, `UISubsystem`, `ExtensionSubsystem`). `PlayerSession` chỉ giữ `unique_ptr` đến các subsystem này và đóng vai trò điều phối.

2.  **Chuyển sang Kiến trúc Hướng sự kiện (Event-Driven)**:
    - **Vấn đề**: `Observer` sử dụng `switch(event)` lớn, khó mở rộng khi số lượng sự kiện tăng lên (FILE_LOADED, SEEK, PAUSE, PROPERTY_CHANGE...).
    - **Giải pháp**: Xây dựng một `EventDispatcher` trung tâm. Các module sẽ tự đăng ký (`Subscribe`) vào các sự kiện mà chúng quan tâm (`observer.Subscribe("pause", callback)`). Điều này giúp giảm sự phụ thuộc và làm cho các module độc lập hơn.

3.  **Cache thuộc tính MPV (`PropertyCache`)**:
    - **Vấn đề**: Các lệnh `GetProperty()` như `time-pos`, `duration` được gọi mỗi frame, gây ra nhiều cuộc gọi IPC không cần thiết giữa UI và lõi MPV.
    - **Giải pháp**: Tạo một `PropertyCache` (`unordered_map` với `dirty flag`). `Observer` sẽ lắng nghe sự kiện thay đổi thuộc tính và cập nhật cache. Luồng UI chỉ đọc từ cache này, giúp giảm đáng kể chi phí giao tiếp.

4.  **Biến `RenderThread` thành `Job Thread`**:
    - **Vấn đề**: Cơ chế `RequestRender()` hiện tại không đủ linh hoạt cho các tác vụ render phức tạp trong tương lai (Resize, Capture, Screenshot, Thumbnail...).
    - **Giải pháp**: Chuyển `RenderThread` sang mô hình `Job Queue`. Các tác vụ như `RenderJob`, `ResizeJob` sẽ được đẩy vào hàng đợi. Luồng render chỉ cần chạy một vòng lặp `while(true)` để lấy và thực thi các job.

5.  **Nâng cấp `FrameBufferPool` thành `TexturePool`**:
    - **Vấn đề**: `FrameBufferPool` hiện tại chỉ quản lý FBO cho video.
    - **Giải pháp**: Mở rộng thành một `TexturePool` tổng quát, có khả năng quản lý nhiều loại tài nguyên đồ họa (Texture, FBO, Depth, MSAA...). Điều này rất cần thiết cho một `ShaderPipeline` phức tạp, cho phép tái sử dụng tài nguyên giữa các bước render.

6.  **Tập trung hóa Trạng thái (`Runtime State`)**:
    - **Vấn đề**: Trạng thái của player (pause, speed, fullscreen, track...) nằm rải rác ở nhiều nơi.
    - **Giải pháp**: Gom tất cả trạng thái vào một cấu trúc duy nhất `MPVState`. Luồng UI sẽ chỉ đọc từ `MPVState` này, không truy cập trực tiếp vào MPV. Điều này tạo ra một luồng dữ liệu một chiều, dễ quản lý và gỡ lỗi.

7.  **Xây dựng Giao diện Mở rộng (`Extension Interface`)**:
    - **Vấn đề**: Việc thêm các tính năng mới như Thumbnail, Discord RPC, SponsorBlock, Anime4K... sẽ làm `PlayerSession` ngày càng phình to.
    - **Giải pháp**: Định nghĩa một interface `IMPVExtension` với các phương thức ảo như `OnInit()`, `OnShutdown()`, `OnEvent()`. `PlayerSession` chỉ cần quản lý một `vector<IMPVExtension*>` và gọi các phương thức tương ứng, cho phép cắm-rút tính năng một cách linh hoạt.

### Lợi ích của Kiến trúc Mới

- **Dễ bảo trì & mở rộng**: Các module được phân tách rõ ràng, giảm sự phụ thuộc lẫn nhau.
- **Hiệu năng cao**: Giảm thiểu giao tiếp liên luồng không cần thiết thông qua caching.
- **Luồng dữ liệu rõ ràng**: UI không đọc trực tiếp từ `libmpv` mà thông qua `MPVStateCache`, giúp hệ thống dễ dự đoán hơn.
- **Khả năng cắm-rút**: Dễ dàng thêm các tính năng mới dưới dạng `Extension` mà không cần sửa đổi lõi.