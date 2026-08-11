#include "OpenGLFrameBufferPool.h"
#include "backends/IGraphicsBackend.h"
#include <algorithm>
#include <cstdio>
#include <any>

OpenGLFrameBufferPool::OpenGLFrameBufferPool(int maxW, int maxH) : MAX_W(maxW), MAX_H(maxH) {}

OpenGLFrameBufferPool::~OpenGLFrameBufferPool() {
    Shutdown();
}

void OpenGLFrameBufferPool::ClearFence(GraphicsFenceHandle& fence) {
    if (fence) {
        GLsync sync = static_cast<GLsync>(fence);
        if (glIsSync(sync)) {
            glDeleteSync(sync);
        }
        fence = nullptr;
    }
}

void OpenGLFrameBufferPool::Init(IGraphicsBackend* backend) {
    if (!backend) return;
    m_backend = backend;

    for (int i = 0; i < 3; i++) {
        GLuint fbo = 0, texture = 0;
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);

        m_frames[i].fbo = static_cast<GraphicsFboHandle>(fbo);
        m_frames[i].texture = static_cast<GraphicsTextureHandle>(texture);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, m_backend->GetGLInternalFormat(), MAX_W, MAX_H, 0, m_backend->GetGLFormat(), m_backend->GetGLType(), nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("FBO[%d] not complete\n", i);
        }
        m_frames[i].allocatedW = MAX_W;
        m_frames[i].allocatedH = MAX_H;
        m_frames[i].fence = nullptr;
        m_frames[i].state.store(BufferState::FREE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFrameBufferPool::Shutdown() {
    for (int i = 0; i < 3; ++i) {
        ClearFence(m_frames[i].fence);

        if (m_frames[i].fbo) {
            GLuint fbo = static_cast<GLuint>(m_frames[i].fbo);
            glDeleteFramebuffers(1, &fbo);
            m_frames[i].fbo = 0;
        }
        if (m_frames[i].texture) {
            GLuint texture = static_cast<GLuint>(m_frames[i].texture);
            glDeleteTextures(1, &texture);
            m_frames[i].texture = 0;
        }

        m_frames[i].allocatedW = 0;
        m_frames[i].allocatedH = 0;
        m_frames[i].state.store(BufferState::FREE, std::memory_order_release);
    }
    m_currentDisplayIndex.store(-1, std::memory_order_release);
}

int OpenGLFrameBufferPool::AcquireFreeBuffer() {
    // Bước 1: Thử lấy buffer có trạng thái FREE
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::FREE;
        if (m_frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }

    // Bước 2: Thử đè lên buffer READY cũ hơn chưa kịp vẽ
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (m_frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
            return i;
        }
    }

    // Bước 3: Fallback an toàn - Chọn buffer nào KHÔNG PHẢI là buffer đang được UI hiển thị (m_currentDisplayIndex)
    for (int i = 0; i < 3; ++i) {
        if (i != m_currentDisplayIndex) {
            m_frames[i].state.store(BufferState::RENDERING, std::memory_order_release);
            return i;
        }
    }

    return (m_currentDisplayIndex + 1) % 3; // Trường hợp cực đoan nhất
}

FrameTextureInfo OpenGLFrameBufferPool::GetStableFrame() {
    int newReadyIndex = -1;

    // Tìm frame vừa render xong ở trạng thái READY
    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (m_frames[i].state.compare_exchange_strong(expected, BufferState::DISPLAYING, std::memory_order_acq_rel)) {
            newReadyIndex = i;
            break;
        }
    }

    if (newReadyIndex != -1) {
        // Đồng bộ hóa GL Fence trên UI Thread
        if (m_frames[newReadyIndex].fence) {
            GLsync sync = static_cast<GLsync>(m_frames[newReadyIndex].fence);
            GLenum waitResult = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 16000000); // 16ms

            if (waitResult == GL_WAIT_FAILED || waitResult == GL_TIMEOUT_EXPIRED) {
                // Wait thất bại/Timeout: Hoàn lại trạng thái READY để thử lại lượt sau
                m_frames[newReadyIndex].state.store(BufferState::READY, std::memory_order_release);
                newReadyIndex = -1;
            } else {
                // Wait thành công: Dọn dẹp fence sau khi GPU đã hoàn tất render
                ClearFence(m_frames[newReadyIndex].fence);
            }
        }

        if (newReadyIndex != -1) {
            int oldDisplay = m_currentDisplayIndex.exchange(newReadyIndex, std::memory_order_acq_rel);
            if (oldDisplay != -1 && oldDisplay != newReadyIndex) {
                // Giải phóng frame hiển thị cũ về lại trạng thái FREE
                m_frames[oldDisplay].state.store(BufferState::FREE, std::memory_order_release);
            }
        }
    }

    FrameTextureInfo info;
    int activeDisplayIndex = m_currentDisplayIndex.load(std::memory_order_acquire);
    if (activeDisplayIndex != -1) {
        FrameNode& frame = m_frames[activeDisplayIndex];
        info.texID = reinterpret_cast<void*>(static_cast<uintptr_t>(frame.texture));

        if (frame.allocatedW > 0 && frame.allocatedH > 0) {
            info.u = static_cast<float>(frame.contentW) / frame.allocatedW;
            info.v = static_cast<float>(frame.contentH) / frame.allocatedH;
        }
    }
    return info;
}

void OpenGLFrameBufferPool::MarkAsReady(int index) {
    if (index < 0 || index >= 3) return;

    // Xóa fence cũ trước khi khởi tạo fence mới
    ClearFence(m_frames[index].fence);

    // Tạo GL Fence Sync mới tại Render Thread
    m_frames[index].fence = static_cast<GraphicsFenceHandle>(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
    glFlush(); // Bắt buộc glFlush để fence truyền lệnh sang GPU driver ngay lập tức

    m_frames[index].state.store(BufferState::READY, std::memory_order_release);
}

FrameNode& OpenGLFrameBufferPool::GetFrame(int index) {
    return m_frames[index];
}

void OpenGLFrameBufferPool::ResizeFrame(int index, int targetW, int targetH) {
    if (!m_backend) return;

    FrameNode& frame = m_frames[index];
    bool needsRealloc = false;

    if (targetW > frame.allocatedW || targetH > frame.allocatedH) {
        int newW = std::min(MAX_SAFE_TEXTURE_SIZE, (int)(frame.allocatedW * 1.5f));
        int newH = std::min(MAX_SAFE_TEXTURE_SIZE, (int)(frame.allocatedH * 1.5f));

        frame.allocatedW = std::max(targetW, newW);
        frame.allocatedH = std::max(targetH, newH);
        needsRealloc = true;
    }

    if (needsRealloc) {
        GLuint texture = static_cast<GLuint>(frame.texture);
        GLuint fbo = static_cast<GLuint>(frame.fbo);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, m_backend->GetGLInternalFormat(), frame.allocatedW, frame.allocatedH, 0, m_backend->GetGLFormat(), m_backend->GetGLType(), nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    }
}