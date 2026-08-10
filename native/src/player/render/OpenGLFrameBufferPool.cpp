#include "OpenGLFrameBufferPool.h"
#include "backends/IGraphicsBackend.h"
#include <algorithm>
#include <cstdio>
#include <any>

OpenGLFrameBufferPool::OpenGLFrameBufferPool(int maxW, int maxH) : MAX_W(maxW), MAX_H(maxH) {}

OpenGLFrameBufferPool::~OpenGLFrameBufferPool() {
    Shutdown();
}

void OpenGLFrameBufferPool::Init(IGraphicsBackend* backend) {
    if (!backend) return;
    m_backend = backend;

    for (int i = 0; i < 3; i++) {
        GLuint fbo = 0, texture = 0;
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);

        m_frames[i].fbo = fbo;
        m_frames[i].texture = texture;

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
        m_frames[i].state.store(BufferState::FREE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFrameBufferPool::Shutdown() {
    for (int i = 0; i < 3; ++i) {
        if (m_frames[i].fence.has_value()) glDeleteSync(std::any_cast<GLsync>(m_frames[i].fence));
        if (m_frames[i].fbo.has_value()) {
            GLuint fbo = std::any_cast<GLuint>(m_frames[i].fbo);
            if(fbo) glDeleteFramebuffers(1, &fbo);
        }
        if (m_frames[i].texture.has_value()) {
            GLuint texture = std::any_cast<GLuint>(m_frames[i].texture);
            if(texture) glDeleteTextures(1, &texture);
        }
        // m_frames[i] = {}; // Lỗi C2280: không thể gán vì có std::atomic
        // Reset thủ công từng thành viên
        m_frames[i].fbo.reset();
        m_frames[i].texture.reset();
        m_frames[i].fence.reset();
        m_frames[i].allocatedW = 0;
        m_frames[i].allocatedH = 0;
        m_frames[i].state.store(BufferState::FREE);
    }
    m_currentDisplayIndex = -1;
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

    for (int i = 0; i < 3; ++i) {
        BufferState expected = BufferState::READY;
        if (m_frames[i].state.compare_exchange_strong(expected, BufferState::DISPLAYING, std::memory_order_acq_rel)) {
            newReadyIndex = i;
            break;
        }
    }

    if (newReadyIndex != -1) {
        if (m_frames[newReadyIndex].fence.has_value()) {
            GLsync fence = std::any_cast<GLsync>(m_frames[newReadyIndex].fence);
            GLenum wait = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 16000000); // 16ms timeout
            if (wait == GL_WAIT_FAILED || wait == GL_TIMEOUT_EXPIRED) {
                m_frames[newReadyIndex].state.store(BufferState::READY);
                newReadyIndex = -1;
            }
        }

        if (newReadyIndex != -1) {
            if (m_currentDisplayIndex != -1 && m_currentDisplayIndex != newReadyIndex) {
                m_frames[m_currentDisplayIndex].state.store(BufferState::FREE, std::memory_order_release);
            }
            m_currentDisplayIndex = newReadyIndex;
        }
    }

    FrameTextureInfo info;
    if (m_currentDisplayIndex != -1) {
        FrameNode& frame = m_frames[m_currentDisplayIndex];
        info.texID = (ImTextureID)(intptr_t)std::any_cast<GLuint>(frame.texture);

        if (frame.allocatedW > 0 && frame.allocatedH > 0) {
            info.u = (float)frame.contentW / frame.allocatedW;
            info.v = (float)frame.contentH / frame.allocatedH;
        }
    }
    return info;
}

void OpenGLFrameBufferPool::MarkAsReady(int index) {
    if (index < 0 || index >= 3) return;

    if (m_frames[index].fence.has_value()) glDeleteSync(std::any_cast<GLsync>(m_frames[index].fence));

    m_frames[index].fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
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
        GLuint texture = std::any_cast<GLuint>(frame.texture);
        GLuint fbo = std::any_cast<GLuint>(frame.fbo);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, m_backend->GetGLInternalFormat(), frame.allocatedW, frame.allocatedH, 0, m_backend->GetGLFormat(), m_backend->GetGLType(), nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    }
}