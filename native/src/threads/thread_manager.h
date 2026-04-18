#pragma once

#include"thread_id.h"

#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>


// thread_manager.h
class ThreadManager {
public:
    void Run(ThreadID id, std::function<void()> task, bool allowDuplicate = false);
    bool IsRunning(ThreadID id);

    std::string ThreadIDToString(ThreadID id);

private:
    std::unordered_set<ThreadID> activeIDs_;
    std::mutex mutex_;
};
ThreadManager& GetThreadManager();



