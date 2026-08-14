#include "thread_manager.h"
#include <iostream>
#include "log.h"

void ThreadManager::Run(ThreadID id, std::function<void()> task, bool allowDuplicate) {
    if (!task) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!allowDuplicate && (activeIDs_.count(id) > 0 || registeredThreads_.count(id) > 0)) {
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::System,
                std::cout << "[⚠️] Thread [" << id.ToString() << "] is already active/registered.\n";
            );
            return;
        }
        activeIDs_.insert(id);
    }

    std::thread t([this, id, task = std::move(task)]() {
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            std::cout << "🧵 Start dynamic thread [" << id.ToString() << "]\n";
        );
        try {
            task();
        } catch (const std::exception& e) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::System,
                std::cout << "[❌] Exception in [" << id.ToString() << "]: " << e.what() << "\n";
            );
        } catch (...) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::System,
                std::cout << "[❌] Unknown exception in [" << id.ToString() << "]\n";
            );
        }

        std::lock_guard<std::mutex> lock(mutex_);
        activeIDs_.erase(id);
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            std::cout << "✅ Finished dynamic thread [" << id.ToString() << "]\n";
        );
    });

    t.detach();
}

void ThreadManager::Register(ThreadID id, std::thread* thread) {
    if (!thread) return;

    std::lock_guard<std::mutex> lock(mutex_);
    
    RegisteredThreadInfo info;
    info.threadPtr = thread;
    info.nativeId = thread->get_id();

    registeredThreads_[id] = info;
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
        std::cout << "📌 Registered manual thread [" << id.ToString() << "] (Native ID: " << info.nativeId << ")\n";
    );
}

void ThreadManager::Unregister(ThreadID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (registeredThreads_.erase(id) > 0) {
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            std::cout << "🗑️ Unregistered thread [" << id.ToString() << "]\n";
        );
    } else {
        LOG_NO_KEY(1, LogLevel::Warning, LogCategory::System,
            std::cout << "[⚠️] Attempted to unregister unregistered thread [" << id.ToString() << "]\n";
        );
    }
}

bool ThreadManager::IsRegisteredAlive(ThreadID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registeredThreads_.find(id);
    if (it != registeredThreads_.end() && it->second.threadPtr != nullptr) {
        // Luồng còn sống nếu con trỏ hợp lệ và std::thread vẫn joinable
        return it->second.threadPtr->joinable();
    }
    return false;
}

bool ThreadManager::IsRunning(ThreadID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 1. Kiểm tra trong danh sách luồng chạy tự động (Run)
    if (activeIDs_.count(id) > 0) {
        return true;
    }

    // 2. Kiểm tra trong danh sách luồng đăng ký thủ công (Register)
    auto it = registeredThreads_.find(id);
    if (it != registeredThreads_.end() && it->second.threadPtr != nullptr) {
        return it->second.threadPtr->joinable();
    }

    return false;
}

bool ThreadManager::JoinRegistered(ThreadID id) {
    std::thread* threadToJoin = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registeredThreads_.find(id);
        if (it != registeredThreads_.end()) {
            threadToJoin = it->second.threadPtr;
        }
    }

    if (threadToJoin && threadToJoin->joinable()) {
        threadToJoin->join();
        Unregister(id);
        return true;
    }

    return false;
}

void ThreadManager::JoinAllRegistered() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
        std::cout << "🧹 Cleaning up all registered threads...\n";
    );

    for (auto& [id, info] : registeredThreads_) {
        if (info.threadPtr && info.threadPtr->joinable()) {
            LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
                std::cout << "⏳ Waiting for thread [" << id.ToString() << "] to finish...\n";
            );
            info.threadPtr->join();
        }
    }
    registeredThreads_.clear();
}

ThreadManager& GetThreadManager() {
    static ThreadManager instance;
    return instance;
}