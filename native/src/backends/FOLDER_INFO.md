# Metadata
- **Last Scan:** 2024-07-24
- **Source Files:** 9
- **Hash:** TBD
- **Depends On:** `SDL2`, `gl3w`, `d3d11`, `imgui`, `mpv`, `cpr`, `nlohmann/json`, `stb_image`
- **Scanned Files:** `IGraphicsBackend.h`, `OpenGLBackend.h`, `D3D11Backend.h`, `client_backend.cpp`, `client_backend.h`, `backend.cpp`, `backend.h`, `vid.h`, `vid/app.py`

# 📂 Thư Mục: `backends`

## 0. 🚨 LƯU Ý KIẾN TRÚC (ARCHITECTURAL NOTE)
Thư mục này vi phạm Nguyên tắc trách nhiệm đơn (Single Responsibility Principle). Nó chứa hai hệ thống con hoàn toàn không liên quan:
1.  **Graphics Backend Abstraction:** Lớp trừu tượng cho các API đồ họa (OpenGL, D3D11).
2.  **Data-Fetching Backend Client:** Client để giao tiếp với một service Python lấy dữ liệu YouTube.

**Khuyến nghị:** Tách toàn bộ logic của `client_backend`, `backend` và `vid` ra một module riêng, ví dụ `native/src/data_provider`. Thư mục `backends` chỉ nên chứa code liên quan đến đồ họa. Phân tích dưới đây sẽ tách biệt hai hệ thống con này.

---

## 1. Hệ Thống Con 1: Graphics Backend Abstraction

### 1.1. Architecture Decisions & Design Patterns
- **Patterns:** 
    - **Strategy:** `IGraphicsBackend` là một interface (chiến lược) định nghĩa các hoạt động đồ họa. `OpenGLBackend` và `D3D11Backend` là các chiến lược cụ thể.
- **Decisions:**
    - Toàn bộ ứng dụng được tách biệt khỏi một API đồ họa cụ thể, cho phép chuyển đổi giữa OpenGL và D3D11 khi khởi tạo.
    - Tích hợp sâu với `libmpv` thông qua `mpv_render_context` và `GetMpvRenderParams`, cho phép MPV render video trực tiếp vào texture của backend đồ họa.

### 1.2. Dependency & Ownership Graph
- **Dependency:** `WindowRuntime` → `IGraphicsBackend` ← `OpenGLBackend` / `D3D11Backend`
- **Ownership:** `WindowRuntime` **owns** a `std::unique_ptr<IGraphicsBackend>`. Backend sở hữu context đồ họa của nó (ví dụ: `SDL_GLContext`).

### 1.3. Thread Model & Data Flow
- **Render Thread:** Tất cả các phương thức trên một instance `IGraphicsBackend` phải được gọi từ `UIRenderThread` chuyên dụng của cửa sổ đó. Context đồ họa là thread-local.
- **Data Flow (Video Rendering):** Backend cung cấp `mpv_render_param` để MPV có thể render vào một FBO. FBO này sau đó được sử dụng như một `ImTextureID` để vẽ video trong ImGui.

### 1.4. Risk Matrix & Technical Debt
- **Risk (Rendering):** Rủi ro cao. Đây là nơi phát sinh các lỗi đồ họa cấp thấp (invalid context, unbound FBOs, driver issues).
- **Risk (Threading):** Việc gọi các hàm của backend từ sai luồng sẽ gây crash.
- **Debt:** `D3D11Backend` chưa hoàn thiện và thiếu phần tích hợp với MPV.

---

## 2. Hệ Thống Con 2: Data-Fetching Backend Client

### 2.1. Architecture Decisions & Design Patterns
- **Patterns:** 
    - **Client-Server:** Đây là client C++ cho một backend server Python (`vid/app.py`).
    - **Asynchronous Task:** Các yêu cầu mạng (`UpdateVideoData`, `LoadThumbnail`) được thực thi trên các luồng riêng (`std::thread`) để không block UI.
- **Decisions:**
    - Tách logic lấy và xử lý dữ liệu (tìm kiếm video, trending...) ra một service Python riêng.
    - Quản lý vòng đời của service Python từ bên trong ứng dụng C++ (`StartRuntimeServices`, `StopService`).
    - Giao tiếp giữa C++ và Python được thực hiện qua HTTP requests (sử dụng thư viện `cpr`).

### 2.2. Dependency & Ownership Graph
- **Dependency:** `client_backend` → `cpr` (HTTP lib), `nlohmann/json`, `stb_image`.
- **Ownership:** Logic này sử dụng nhiều biến toàn cục (`g_videoList`, `g_thumbnailCache`, `g_mutex`) không có ownership rõ ràng, gây rủi ro cao.

### 2.3. Thread Model & Data Flow
- **Main Thread (Render):** Kích hoạt `UpdateVideoData` hoặc `LoadThumbnail`. Đọc `g_videoList` và `g_thumbnailCache` để vẽ UI. Chạy `ProcessThumbnailQueue` để tạo texture OpenGL từ dữ liệu đã tải về.
- **Worker Threads (Network):** Các luồng được tạo bởi `std::thread` để thực hiện các HTTP request đến backend Python.
- **Synchronization:** Sử dụng một `g_mutex` toàn cục để bảo vệ `g_videoList`, `g_thumbnailCache`, và `g_thumbnailQueue`. `g_loading` (atomic) được dùng để chống gửi request trùng lặp.
- **Data Flow:** UI Trigger → `UpdateVideoData()` → `std::thread` → `cpr::Get` → `nlohmann::json::parse` → `(lock g_mutex)` → push to `g_videoList`.

### 2.4. Risk Matrix & Technical Debt
- **Risk (Global State):** **[CRITICAL]** Việc sử dụng các biến toàn cục (`g_videoList`, `g_thumbnailCache`, `g_mutex`) là một anti-pattern nghiêm trọng. Nó làm cho luồng dữ liệu khó theo dõi và là nguồn gốc tiềm tàng của các lỗi data race và deadlock.
- **Risk (Misplaced Module):** Toàn bộ hệ thống con này nằm sai vị trí, gây nhầm lẫn về kiến trúc của dự án.
- **Debt:** Cần phải tái cấu trúc (refactor) toàn bộ hệ thống con này vào một module riêng và loại bỏ các biến toàn cục. Dữ liệu nên được quản lý trong một lớp `DataProvider` với các phương thức thread-safe rõ ràng.
