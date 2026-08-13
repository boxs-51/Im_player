#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "PlayBackRenderThreadState.h"
#include "WindowTemplate.h"

class PlayBackRenderThread {
public:
    PlayBackRenderThread();
    ~PlayBackRenderThread();

    void Start();
    void Stop();
    void RequestRender();
    void Notify(); // Hàm mới không khóa mutex
    void SetVideoSize(int w, int h);

    // Các hàm kiểm tra trạng thái render
    uint64_t GetLastRenderedFrameId() const { return m_lastRenderedFrameId.load(std::memory_order_acquire); }
    int GetLastAcquiredBufferId() const { return m_lastAcquiredBufferId.load(std::memory_order_acquire); }
    uint32_t GetDroppedFramesCount() const { return m_droppedFrames.load(std::memory_order_acquire); }

    void SetSessionId(const std::string& sessionId) { m_sessionId = sessionId; }
    const std::string& GetSessionId() const { return m_sessionId; }

    // Public access to state for UI
    PlayBackRenderThreadState state;

private:
    void Run();

    std::string m_sessionId;
    std::string m_registeredThreadName;
    std::thread m_thread;
    std::atomic<bool> m_running{false};

    // Các biến trạng thái render nội bộ
    std::atomic<uint64_t> m_frameCounter{0};
    std::atomic<uint64_t> m_lastRenderedFrameId{0};
    std::atomic<int> m_lastAcquiredBufferId{-1};
    std::atomic<uint32_t> m_droppedFrames{0};
};