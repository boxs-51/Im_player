#include <threads/thread_manager.h>
#include "globals.h"

#include <iostream>
#include <thread>

#include <unordered_map>
#include <unordered_set>

void ThreadManager::Run(ThreadID id, std::function<void()> task, bool allowDuplicate) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!allowDuplicate && activeIDs_.count(id)) {

        std::cout << "[⚠️] Thread [" << ThreadIDToString(id) << "] is already running\n";
        return;
    }

    activeIDs_.insert(id);

    std::thread t([this, id, task]() {
        std::cout << "🧵 Start thread [" << ThreadIDToString(id) << "]\n";
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[❌] Exception in thread [" << ThreadIDToString(id) << "]: " << e.what() << "\n";
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "✅ Finish thread [" << ThreadIDToString(id) << "]\n";
        activeIDs_.erase(id);
    });

    t.detach();
}

bool ThreadManager::IsRunning(ThreadID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeIDs_.count(id) > 0;
}

// Đổi ThreadID sang string để log
std::string ThreadManager::ThreadIDToString(ThreadID id) {
    switch (id) {
    case ThreadID::URLFetch: return "URLFetch";
    case ThreadID::MPVEventLoop: return "MPVEventLoop";
    case ThreadID::PipeServer: return "PipeServer";
    case ThreadID::MPVRenderThread: return "MPVRenderThread";
    default: return "Unknown";
    }
}
// Singleton implementation
ThreadManager& GetThreadManager() {
    static ThreadManager instance;
    return instance;
}


