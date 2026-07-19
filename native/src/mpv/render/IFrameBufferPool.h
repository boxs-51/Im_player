#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <any>

class IGraphicsBackend;

enum class BufferState { FREE, RENDERING, READY, DISPLAYING };

struct FrameNode {
    std::any fbo;       // Sẽ là GLuint cho OpenGL, ID3D11RenderTargetView* cho D3D11
    std::any texture;   // Sẽ là GLuint cho OpenGL, ID3D11ShaderResourceView* cho D3D11
    int allocatedW = 0, allocatedH = 0;
    int contentW = 0, contentH = 0;
    std::any fence;     // Sẽ là GLsync cho OpenGL
    std::atomic<BufferState> state{ BufferState::FREE };
};

struct FrameTextureInfo {
    std::any texID{}; // ImTextureID (void*). Dùng {} để khởi tạo rỗng.
    float u = 1.0f, v = 1.0f;
};

class IFrameBufferPool {
public:
    virtual ~IFrameBufferPool() = default;

    virtual void Init(IGraphicsBackend* backend) = 0;
    virtual void Shutdown() = 0;
    virtual int AcquireFreeBuffer() = 0;
    virtual FrameTextureInfo GetStableFrame() = 0;
    virtual void MarkAsReady(int index) = 0;
    virtual FrameNode& GetFrame(int index) = 0;
    virtual void ResizeFrame(int index, int targetW, int targetH) = 0;
};