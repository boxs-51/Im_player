#include "thread_manager.h"
#include <iostream>
#include "log.h"

void ThreadManager::Run(ThreadID id, std::function<void()> task, bool allowDuplicate) {
    if (!task) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!allowDuplicate && (activeIDs_.count(id) > 0 || registeredThreads_.count(id) > 0)) {
            LOG_NO_KEY(1, LogLevel::Warning, LogCategory::System,
                "[⚠️] Thread [%s] is already active/registered.\n", id.ToString());

            return;
        }
        activeIDs_.insert(id);
    }

    std::thread t([this, id, task = std::move(task)]() {
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            "🧵 Start dynamic thread [%s]", id.ToString().c_str()
        );

        try {
            task();
        } catch (const std::exception& e) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::System,
                "[❌] Exception in [%s]: %s", id.ToString().c_str(), e.what()
            );
        } catch (...) {
            LOG_NO_KEY(1, LogLevel::Error, LogCategory::System,
                "[❌] Unknown exception in [%s]", id.ToString().c_str()
            );
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeIDs_.erase(id);
        }

        // Sửa: Đã xóa dấu ';' thừa
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            "✅ Finished dynamic thread [%s]", id.ToString().c_str()
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
        "📌 Registered manual thread [%s] (Native ID: %d", id.ToString() ,info.nativeId
    );
}

void ThreadManager::Unregister(ThreadID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (registeredThreads_.erase(id) > 0) {
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
            "🗑️ Unregistered thread [%s]", id.ToString()
        );
    } else {
        LOG_NO_KEY(1, LogLevel::Warning, LogCategory::System,
            "[⚠️] Attempted to unregister unregistered thread [%s]", id.ToString()
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
    std::vector<std::thread*> threads;
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
        "🧹 Cleaning up all registered threads..."
    );

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [id, info] : registeredThreads_) {
            if (info.threadPtr && info.threadPtr->joinable()) {
                LOG_NO_KEY(1, LogLevel::Info, LogCategory::System,
                    "⏳ Waiting for thread [%s] to finish...", id.ToString().c_str()
                );
                threads.push_back(info.threadPtr);
            }
        }
    }

    for (auto* thread : threads) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        registeredThreads_.clear();
    }
}

ThreadManager& GetThreadManager() {
    static ThreadManager instance;
    return instance;
}