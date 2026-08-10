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
    void MarkAsReady(int index) override;
    FrameNode& GetFrame(int index) override;
    void ResizeFrame(int index, int targetW, int targetH) override;

private:
    std::array<FrameNode, 3> m_frames;
    int m_currentDisplayIndex = -1;
    const int MAX_W;
    const int MAX_H;
    const int MAX_SAFE_TEXTURE_SIZE = 4096;

    // Con trỏ tới backend để lấy các thuộc tính GL
    IGraphicsBackend* m_backend = nullptr;
};