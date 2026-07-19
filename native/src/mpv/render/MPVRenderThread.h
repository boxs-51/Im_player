#pragma once

#include "mpv/mpv_instance.h"
#include "WindowTemplate.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

class MPVRenderThread {
public:
    MPVRenderThread();
    ~MPVRenderThread();

    void Start();
    void Stop();
    void RequestRender();
    void Notify(); // Hàm mới không khóa mutex
    void SetVideoSize(int w, int h);

    // Public access to state for UI
    MPVInstance state;

private:
    void Run();

    std::thread m_thread;
    std::atomic<bool> m_running{false};
};