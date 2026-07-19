// mpv_instance.h
#pragma once
#include <mpv/client.h>
#include <mpv/render.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <array>
#include <any>
#include <gl3w.h>

enum class BufferState { FREE, RENDERING, READY, DISPLAYING };

struct FrameNode {
    GLuint fbo = 0;
    GLuint texture = 0;
    int allocatedW = 0;
    int allocatedH = 0;
    int contentW = 0;
    int contentH = 0;
    GLsync fence = nullptr;
    std::atomic<BufferState> state{ BufferState::FREE };
};

struct FrameTextureInfo {
    unsigned int texID = 0;
    float u = 1.0f;
    float v = 1.0f;
};

// Đóng gói toàn bộ tài nguyên của 1 thực thể Player độc lập
class MPVInstance {
public:
    mpv_handle* mpv = nullptr;
    mpv_render_context* render_ctx = nullptr;
    
    // Trạng thái Render Thread phụ
    std::atomic<bool> running{ false };
    std::atomic<bool> needRender{ false };
    std::atomic<bool> hasExited{ false };
    std::atomic<int> framerender{ 0 };

    std::mutex mtx;
    std::condition_variable cv;
    
    // Window & Graphic Context link
    std::string ownerWindowId;
    struct SDL_Window* window = nullptr;
    std::any graphicsContext;

    // Kích thước bề mặt render
    struct {
        int drawW = 0;
        int drawH = 0;
        int newW = 0;
        int newH = 0;
        bool needResize = false;
    } surface;

    // Cơ chế Triple Buffering
    std::array<FrameNode, 3> frames;
    const int MAX_W = 3840;
    const int MAX_H = 2160;
    const int MAX_SAFE_TEXTURE_SIZE = 4096;

    ~MPVInstance() {
        // Đảm bảo giải phóng tài nguyên đồ họa nếu chưa gọi Cleanup
        for (int i = 0; i < 3; ++i) {
            if (frames[i].fence) glDeleteSync(frames[i].fence);
        }
    }
};