#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <any>

class IGraphicsBackend;

enum class BufferState
{
    FREE,
    RENDERING,
    READY,
    DISPLAYING
};

// Strongly-typed Handles để thay thế std::any
using GraphicsFboHandle = uint32_t;
using GraphicsTextureHandle = uint32_t;
using GraphicsFenceHandle = void *; // Cast sang GLsync trong OpenGL backend

struct FrameNode
{
    GraphicsFboHandle fbo;
    GraphicsTextureHandle texture;

    int allocatedW = 0;
    int allocatedH = 0;
    int contentW = 0;
    int contentH = 0;

    uint64_t frameId = 0; // Sequence ID để chọn frame mới nhất
    GraphicsFenceHandle producerFence; // Fence do Render Thread đặt, UI Thread chờ
    GraphicsFenceHandle consumerFence; // Fence do UI Thread đặt, Render Thread chờ
    std::atomic<BufferState> state{BufferState::FREE};
};

struct FrameTextureInfo
{
    void *texID{nullptr};
    uint64_t frameId = 0; // ID của frame, để UI biết nó có phải frame mới không
    float u = 1.0f;
    float v = 1.0f;
};

class IFrameBufferPool
{
public:
    virtual ~IFrameBufferPool() = default;

    virtual void Init(IGraphicsBackend *backend) = 0;
    virtual void Shutdown() = 0;
    virtual int AcquireFreeBuffer() = 0;
    virtual FrameTextureInfo GetStableFrame() = 0;
    virtual void MarkAsReady(int index, uint64_t frameId) = 0;
    virtual FrameNode &GetFrame(int index) = 0;
    virtual void ResizeFrame(int index, int targetW, int targetH) = 0;
};