#pragma once

#include "IFrameBufferPool.h"
#include <gl3w.h>

class OpenGLFrameBufferPool : public IFrameBufferPool {
public:
    OpenGLFrameBufferPool(int maxW = 3840, int maxH = 2160);
    ~OpenGLFrameBufferPool() override;

    // Triển khai các phương thức từ interface
    void Init(IGraphicsBackend* backend) override;
    void Shutdown() override;
    int AcquireFreeBuffer() override;
    FrameTextureInfo GetStableFrame() override;
    void MarkAsReady(int index, uint64_t frameId) override;
    FrameNode& GetFrame(int index) override;
    void ResizeFrame(int index, int targetW, int targetH) override;

private:
    void ClearFence(GraphicsFenceHandle& fence);

private:
    static constexpr int NUM_BUFFERS = 5; // Tăng số lượng buffer từ 3 lên 5

    std::array<FrameNode, NUM_BUFFERS> m_frames;
    std::atomic<int> m_currentDisplayIndex{ -1 };
    const int MAX_W;
    const int MAX_H;
    const int MAX_SAFE_TEXTURE_SIZE = 4096;

    // Con trỏ tới backend để lấy các thuộc tính GL
    IGraphicsBackend* m_backend = nullptr;
};