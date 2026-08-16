#include "OpenGLFrameBufferPool.h"
#include "backends/IGraphicsBackend.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include "log.h" // Thêm header cho LOG
#include <iostream> // Thêm để dùng std::cout
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

    for (int i = 0; i < NUM_BUFFERS; i++) {
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
        m_frames[i].producerFence = nullptr;
        m_frames[i].consumerFence = nullptr;
        m_frames[i].state.store(BufferState::FREE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFrameBufferPool::Shutdown() {
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        ClearFence(m_frames[i].producerFence);
        ClearFence(m_frames[i].consumerFence);

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
    // Vòng lặp có giới hạn để thử lại, tránh bị treo vô hạn.
    for (int attempts = 0; attempts < 20; ++attempts) {
        for (int i = 0; i < NUM_BUFFERS; ++i) {
            BufferState expected = BufferState::FREE;
            if (m_frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
                // Buffer đã FREE, an toàn để sử dụng.
                return i;
            }

            // Nếu buffer đang ở trạng thái DISPLAYING, kiểm tra consumer fence xem UI đã dùng xong chưa.
            // Đây là cơ chế giải phóng buffer an toàn.
            if (m_frames[i].state.load(std::memory_order_acquire) == BufferState::DISPLAYING && m_frames[i].consumerFence) {
                GLsync sync = static_cast<GLsync>(m_frames[i].consumerFence);
                // GL_ZERO_TIMEOUT: không block, chỉ kiểm tra trạng thái
                GLenum waitResult = glClientWaitSync(sync, 0, 1000000); // Chờ tối đa 1ms
                if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
                    // GPU đã dùng xong, dọn dẹp fence và chuyển về FREE
                    ClearFence(m_frames[i].consumerFence);
                    expected = BufferState::DISPLAYING;
                    if (m_frames[i].state.compare_exchange_strong(expected, BufferState::RENDERING, std::memory_order_acq_rel)) {
                        return i;
                    } else {
                        LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Render, "[AFB] ABNORMAL: Failed to acquire DISPLAYING buffer %s after fence signaled (state changed by another thread?).", i );
                    }
                }
            }
        }
        // Chờ một chút trước khi thử lại
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Nếu sau tất cả các lần thử vẫn không thành công, ghi log và bỏ qua frame này.
    LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Render, "[AFB] ABNORMAL: Failed to acquire any buffer after multiple attempts. Dropping frame.");

    return -1;
}

FrameTextureInfo OpenGLFrameBufferPool::GetStableFrame() {
    int newReadyIndex = -1;
    uint64_t maxFrameId = 0;

    // Bước 1: Tìm frame READY có frameId lớn nhất (mới nhất)
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (m_frames[i].state.load(std::memory_order_acquire) == BufferState::READY) {
            if (m_frames[i].frameId > maxFrameId) {
                maxFrameId = m_frames[i].frameId;
                newReadyIndex = i;
            }
        }
    }

    // Bước 2: Nếu tìm thấy frame mới nhất, thử chuyển nó sang DISPLAYING
    if (newReadyIndex != -1) {
        BufferState expected = BufferState::READY;
        if (!m_frames[newReadyIndex].state.compare_exchange_strong(expected, BufferState::DISPLAYING, std::memory_order_acq_rel)) {
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Render, "[GSF] ABNORMAL: Failed to transition buffer %d from READY to DISPLAYING (state changed by another thread?).", newReadyIndex);
            // Frame đã bị luồng khác chiếm mất, bỏ qua lần này
            // Điều này có thể xảy ra nếu GetStableFrame được gọi nhiều lần rất nhanh và một lần gọi khác đã lấy frame này.
            newReadyIndex = -1;
        }
    }

    // Bước 3: Nếu chuyển đổi thành công, xử lý fence và frame cũ
    if (newReadyIndex != -1) {
        // Chờ producer fence để đảm bảo render thread đã hoàn thành việc ghi vào texture
        if (m_frames[newReadyIndex].producerFence) {
            GLsync sync = static_cast<GLsync>(m_frames[newReadyIndex].producerFence);
            GLenum waitResult = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 16000000); // 16ms

            if (waitResult == GL_WAIT_FAILED || waitResult == GL_TIMEOUT_EXPIRED) {
                LOG_NO_KEY(1, LogLevel::Warning, LogCategory::Render, "[GSF] ABNORMAL: Producer fence wait FAILED/TIMEOUT for buffer %d. Reverting to READY.", newReadyIndex);
                // Nếu chờ thất bại, hoàn lại trạng thái READY
                m_frames[newReadyIndex].state.store(BufferState::READY, std::memory_order_release);
                newReadyIndex = -1;
            } else {
                // Chờ thành công, dọn dẹp producer fence
                ClearFence(m_frames[newReadyIndex].producerFence);
            }
        }

        // Bước 4: Cập nhật frame đang hiển thị và xử lý frame cũ
        if (newReadyIndex != -1) {
            int oldDisplay = m_currentDisplayIndex.exchange(newReadyIndex, std::memory_order_acq_rel);
            if (oldDisplay != -1 && oldDisplay != newReadyIndex) {
                // QUAN TRỌNG: Không chuyển về FREE ngay.
                // Đặt một consumer fence để báo hiệu "UI đang dùng frame này".
                // Render thread sẽ chờ fence này trước khi tái sử dụng.
                ClearFence(m_frames[oldDisplay].consumerFence); // Xóa fence cũ nếu có
                m_frames[oldDisplay].consumerFence = static_cast<GraphicsFenceHandle>(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
                glFlush(); // Đảm bảo lệnh tạo fence được gửi đi

                // Trạng thái vẫn là DISPLAYING, AcquireFreeBuffer sẽ xử lý việc chuyển về FREE
            }
        }
    }

    // Bước 5: Trả về thông tin texture của frame đang được hiển thị
    FrameTextureInfo info;
    int activeDisplayIndex = m_currentDisplayIndex.load(std::memory_order_acquire);
    if (activeDisplayIndex != -1) {
        FrameNode& frame = m_frames[activeDisplayIndex];
        info.texID = reinterpret_cast<void*>(static_cast<uintptr_t>(frame.texture));
        info.frameId = frame.frameId;

        if (frame.allocatedW > 0 && frame.allocatedH > 0) {
            info.u = static_cast<float>(frame.contentW) / frame.allocatedW;
            info.v = static_cast<float>(frame.contentH) / frame.allocatedH;
        }
    }
    return info;
}

void OpenGLFrameBufferPool::MarkAsReady(int index, uint64_t frameId) {
    if (index < 0 || index >= NUM_BUFFERS) return;

    FrameNode& frame = m_frames[index];
    frame.frameId = frameId;

    // Xóa producer fence cũ trước khi tạo cái mới
    ClearFence(frame.producerFence);

    // Tạo producer fence để báo hiệu render thread đã hoàn tất
    frame.producerFence = static_cast<GraphicsFenceHandle>(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
    glFlush(); // Bắt buộc glFlush để fence truyền lệnh sang GPU driver ngay lập tức

    frame.state.store(BufferState::READY, std::memory_order_release);
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