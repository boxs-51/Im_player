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
};