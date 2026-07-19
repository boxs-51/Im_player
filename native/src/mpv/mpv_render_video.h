#pragma once
#include <utils.h>
#include <mpv/client.h>
#include <GL/gl3w.h>
#include <SDL.h>
#include <any>
#include <WindowRuntime.h>

#define SDL_MPV_EVENT (SDL_USEREVENT + 1)
#define SDL_MPV_RENDER_UPDATE (SDL_USEREVENT + 2)

#ifdef RENDER_MPV_THREAD
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <SDL.h>
#include <mpv/render_gl.h>

// Định nghĩa trạng thái của Buffer
enum class BufferState {
    FREE = 0,    // Trống, sẵn sàng để vẽ
    RENDERING,   // Render Thread đang vẽ
    READY,       // Đã vẽ xong, chờ luồng UI lấy
    DISPLAYING   // Luồng UI đang sử dụng để hiển thị
};

struct FrameNode {
    GLuint fbo = 0;
    GLuint texture = 0;
    GLsync fence = nullptr;
    std::atomic<BufferState> state{BufferState::FREE};
    
    int allocatedW = 0; // Kích thước VRAM thực tế đã cấp phát (Capacity)
    int allocatedH = 0;
    int contentW = 0;   // Kích thước khung hình mpv thực sự vẽ vào (Size)
    int contentH = 0;
};

// Quản lý trạng thái vùng vẽ chung
struct SurfaceState {
    int drawW = 1280;
    int drawH = 720;
    std::atomic<int> newW{0};
    std::atomic<int> newH{0};
    std::atomic<bool> needResize{false};
};

// Struct trả về cho ImGui để tính UV
struct FrameTextureInfo {
    GLuint texID = 0;
    float u = 1.0f;
    float v = 1.0f;
};

struct MPVRenderThread {
    mpv_render_context* ctx = nullptr;

    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    std::any graphicsContext = nullptr;
    // Gộp mảng rời rạc thành mảng đối tượng
    FrameNode frames[3];
    SurfaceState surface;

    const int MAX_SAFE_TEXTURE_SIZE = 8192; 

    const int MAX_W = 3840; // Mặc định khởi tạo 4K
    const int MAX_H = 2160;

    std::mutex mtx;
    std::condition_variable cv;

    bool needRender = false;
    bool running = true;
    bool hasExited = false;

    std::atomic<bool> Audio_visualizers = false;
    std::atomic<bool> g_WindowVisible = false;

    std::atomic<float> framerender{0.0f};

    std::string ownerWindowId; 

};
extern MPVRenderThread renderThread;
void StartMPVRenderThread();
void StartMPVRenderThread(WindowRuntime* runtime);
#endif

/// Khởi tạo mpv và thiết lập các tuỳ chọn cơ bản
bool InitMPV(mpv_handle*& mpv);

/// Tạo render context OpenGL cho mpv (render_ctx)
bool InitMPVRenderContext(mpv_handle* mpv);

/// Render video mpv ra FBO đang được ImGui/OpenGL sử dụng
void RenderMPVVideo(const ImVec2& pos, const ImVec2& size);

/// Dọn dẹp mpv + render context khi thoát
void CleanupMPV();
