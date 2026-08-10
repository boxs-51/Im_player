# Kiến trúc Hệ thống Quản lý Cửa sổ (Window System)

Tài liệu này giải thích kiến trúc và luồng hoạt động của các thành phần quản lý cửa sổ trong dự án. Mục tiêu là cung cấp một cái nhìn tổng quan để dễ dàng bảo trì và mở rộng sau này.

## Sơ đồ kiến trúc tổng quan

```
┌───────────────────┐       ┌───────────────────┐       ┌───────────────────┐
|   main loop       | ───>  |   WindowManager   | ───>  |   WindowRuntime   |
└───────────────────┘       └───────────────────┘       └───────────────────┘
        ▲                         (Singleton)                   │
        │                                                       │
        │ Event                   (Quản lý nhiều cửa sổ)        │
        │                                                       ├─> WindowState (Trạng thái)
┌───────────────────┐                                           ├─> WindowStyle (Phong cách)
|  WindowProcHook   | <─────────────────────────────────────────┤─> PropertyBag (Dữ liệu động)
└───────────────────┘                                           ├─> IGraphicsBackend (Đồ họa)
 (Xử lý message Win32)                                          ├─> WindowController (Điều khiển)
                                                                ├─> WindowRenderer (Vẽ UI)
                                                                └─> PlayerSession* (Link tới MPV)
```

---

## 1. `WindowRuntime.h` - Trái tim của mỗi Cửa sổ

Đây là lớp chứa toàn bộ dữ liệu, trạng thái và các đối tượng liên quan đến **một** cửa sổ duy nhất. Nó không chứa logic phức tạp mà đóng vai trò như một "struct" lớn để gom nhóm tài nguyên.

### Các thành phần chính:

- **Định danh & Handle:**
  - `id`: `WindowId` (uint32_t) định danh duy nhất do `WindowManager` cấp.
  - `sdlWindow`: Con trỏ `SDL_Window*` để tương tác với SDL.
  - `hwnd`: `HWND` của cửa sổ trên Windows, dùng cho các API Win32.

- **Đồ họa & Giao diện:**
  - `graphicsBackend`: `std::unique_ptr<IGraphicsBackend>` là interface đồ họa trừu tượng (ví dụ: `OpenGLBackend`), chịu trách nhiệm cho các lệnh vẽ cấp thấp.
  - `imguiCtx`: `ImGuiContext*` riêng cho mỗi cửa sổ, cho phép các cửa sổ có giao diện ImGui độc lập.
  - `renderer`: `std::unique_ptr<WindowRenderer>` là interface chịu trách nhiệm vẽ giao diện người dùng (UI) **cấp cao** cho cửa sổ (ví dụ: `MainWindowRenderer`).

- **Trạng thái & Dữ liệu:**
  - `state`: `WindowState` chứa các trạng thái vật lý như vị trí, kích thước, isMinimized, isMaximized, isFullscreen...
  - `style`: `WindowStyle` chứa các thuộc tính về phong cách như isMainWindow, borderless, titlebar...
  - `properties`: `PropertyBag` là một túi đồ "thần kỳ" cho phép lưu trữ và truy xuất dữ liệu động theo key (chuỗi), giúp các thành phần giao tiếp với nhau một cách linh hoạt.

- **Điều khiển & Tương tác:**
  - `controller`: `std::unique_ptr<WindowController>` cung cấp các API để thực hiện hành động trên cửa sổ (Move, Resize, Close...).
  - `PlayerSession`: `PlayerSession*` là con trỏ **không sở hữu** tới session MPV đang được liên kết với cửa sổ này. Quyền sở hữu thực sự nằm ở `PlayerManager`.

---

## 2. `WindowManager.h` - Nhạc trưởng của các Cửa sổ

`WindowManager` là một **Singleton** có vai trò trung tâm, chịu trách nhiệm tạo, hủy và truy xuất các `WindowRuntime`.

### API Công khai:

- `static WindowManager& GetInstance()`: Lấy về instance duy nhất của manager.
- `WindowRuntime* CreateNewWindow(templateName, backend)`: Tạo một cửa sổ mới dựa trên một `WindowTemplate` đã đăng ký và một `IGraphicsBackend` cụ thể. Đây là cách duy nhất để tạo ra một `WindowRuntime` hợp lệ.
- `void DestroyWindow(id)`: Hủy một cửa sổ và giải phóng toàn bộ tài nguyên liên quan.
- `WindowRuntime* GetWindowBySDLHandle(sdlWin)`: Tìm kiếm `WindowRuntime` dựa trên con trỏ `SDL_Window*`. Rất hữu ích trong vòng lặp xử lý sự kiện của SDL.
- `WindowRuntime* GetMainWindow()`: Tìm và trả về cửa sổ chính của ứng dụng.

---

## 3. `WindowController.h` & `.cpp` - Bộ điều khiển Hành vi

`WindowController` đóng gói các hành động có thể thực hiện trên một cửa sổ. Nó sử dụng `HWND` từ `WindowRuntime` để gọi các API của Win32, đảm bảo hành vi nhất quán.

### API Công khai:

- `void Move(x, y)`: Di chuyển cửa sổ đến vị trí mới.
- `void Resize(w, h)`: Thay đổi kích thước cửa sổ.
- `void ToggleFullscreen()`: Bật/tắt chế độ toàn màn hình giả (borderless fullscreen). Logic này lưu lại vị trí cũ để có thể khôi phục chính xác.
- `void Close()`: Gửi một sự kiện `SDL_WINDOWEVENT_CLOSE` vào hàng đợi của SDL, thay vì hủy cửa sổ trực tiếp. Đây là cách làm an toàn và chuẩn mực.
- `void Maximize()`, `Minimize()`, `Restore()`: Các hành động cơ bản khác.

---

## 4. `UpdateWindowState.cpp` & `MainWindowState.h` - Logic Cập nhật Trạng thái

Các hàm trong file này chịu trách nhiệm đồng bộ trạng thái của `WindowRuntime` với trạng thái thực tế của cửa sổ vật lý, và tính toán layout.

### Các hàm chính:

- `void UpdateWindowStateCommon(runtime)`: Cập nhật các trạng thái cơ bản nhất như `isVisible`, `isMinimized`... áp dụng cho **mọi loại cửa sổ**.
- `void UpdateMainWindowState(runtime)`: Hàm chuyên biệt **chỉ dành cho cửa sổ chính**. Ngoài việc gọi `UpdateWindowStateCommon`, nó còn thực hiện:
  1.  **Tính toán Layout:** Dựa vào trạng thái (fullscreen, windowed), nó tính toán vị trí và kích thước của vùng `TitleBar` và vùng `Video`.
  2.  **Lưu vào PropertyBag:** Dữ liệu layout (`MainWindowLayout`) được đẩy vào `PropertyBag` để `MainWindowRenderer` có thể lấy ra sử dụng khi vẽ.
  3.  **Đồng bộ với Luồng MPV:** Nếu đang dùng `RENDER_MPV_THREAD`, hàm này sẽ lấy `weak_ptr` tới luồng render một cách an toàn, cập nhật kích thước video mới và "đánh thức" luồng render dậy (`Notify()`) để vẽ lại với kích thước mới. Đây là mắt xích cực kỳ quan trọng trong kiến trúc render đa luồng.

- `void RouteWindowStateUpdate(runtime)`: Hàm định tuyến thông minh, tự động gọi `UpdateMainWindowState` nếu là cửa sổ chính, hoặc `UpdateWindowStateCommon` cho các cửa sổ phụ.

---

## 5. `WindowProcHook.cpp` - Can thiệp vào "Trái tim" của Windows

Đây là nơi chúng ta cài đặt một `WndProc` (Window Procedure) tùy chỉnh để bắt và xử lý các message của hệ điều hành Windows trước khi SDL xử lý chúng. Điều này cho phép chúng ta tùy biến sâu các hành vi của cửa sổ.

### Các chức năng chính:

- **`WM_NCHITTEST`**: "Bài toán khó nhất". Hàm này xác định con trỏ chuột đang ở vùng nào của cửa sổ (thân, viền, góc, hay các nút custom).
  - Trả về `HTCAPTION` để cho phép kéo thả cửa sổ từ vùng không phải title bar.
  - Trả về `HTLEFT`, `HTRIGHT`... để cho phép thay đổi kích thước cửa sổ không viền.
  - Trả về các giá trị custom (`HTCUSTOM_CLOSE`...) để nhận diện khi chuột ở trên các nút đóng/phóng to/thu nhỏ do ImGui vẽ.

- **`WM_NCLBUTTONDOWN` / `WM_NCLBUTTONUP`**: Bắt sự kiện click chuột lên các vùng "non-client" (bao gồm cả các nút custom của chúng ta) để thực hiện hành động tương ứng (đóng, phóng to...).

- **`WM_NCCALCSIZE`**: Xử lý khi Windows yêu cầu tính toán kích thước vùng client. Đây là chìa khóa để tạo ra cửa sổ không viền (borderless) đúng nghĩa.

- **`WM_TIMER`**: Bắt sự kiện timer khi người dùng đang kéo/thay đổi kích thước cửa sổ. Trong sự kiện này, chúng ta chủ động gọi `UpdateMainWindowState` và ra lệnh vẽ lại frame ngay lập tức. Điều này tạo ra trải nghiệm kéo thả và resize **mượt mà, real-time**, không bị giật lag.

---

## 6. `MainWindowRenderer.cpp` - Họa sĩ của Cửa sổ Chính

Lớp này kế thừa từ `WindowRenderer` và chịu trách nhiệm vẽ toàn bộ giao diện người dùng (UI) cho cửa sổ chính bằng ImGui.

### Luồng hoạt động trong hàm `RenderUI()`:

1.  **Lấy Layout:** Lấy dữ liệu `MainWindowLayout` từ `PropertyBag` của `runtime`.
2.  **Vẽ nền:** Bắt đầu một cửa sổ ImGui lớn bằng kích thước `runtime`.
3.  **Cập nhật trạng thái UI:** Gọi `UpdateUIState()` để xử lý logic ẩn/hiện con trỏ chuột và các control của player dựa trên tương tác của người dùng.
4.  **Vẽ Title Bar:** Gọi hàm `RenderTitleBarWindowObject()` để vẽ thanh tiêu đề tùy chỉnh.
5.  **Vẽ vùng Video:**
    - Bắt đầu một `ImGui::BeginChild` với kích thước của vùng video đã tính toán.
    - Nếu video đang phát, gọi `runtime->PlayerSession->GetRenderer()->Render()` để vẽ texture video do MPV cung cấp.
    - Vẽ các lớp phủ (overlay) khác như icon loading, icon pause, thanh điều khiển player (`RenderPlayerControls`)...

---

Hy vọng tài liệu này sẽ giúp bạn nắm rõ hơn về hệ thống và dễ dàng làm việc trong các giai đoạn tiếp theo!