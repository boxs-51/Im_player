#pragma once
#include "api_types.h"
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <sstream>

class APIManager {
public:
    // Singleton pattern
    static APIManager& Instance() {
        static APIManager instance;
        return instance;
    }
    void InitConfigs(const std::string& configFilePath);

    // Đăng ký một dịch vụ mới (ví dụ: GeminiProvider)
    void RegisterProvider(const std::string& name, std::shared_ptr<IAPIProvider> provider);

    // Thay đổi trạng thái dịch vụ (Dùng cho UI ImGui)
    void SetProviderState(const std::string& name, ProviderState state);

    // Hàm public để AudioFilterManager gọi (KHÔNG block UI)
    void EnqueueTask(const std::string& providerName, const std::string& payload);

    // Dừng toàn bộ hệ thống
    void Shutdown();

private:
    APIManager();
    ~APIManager();

    std::unordered_map<std::string, std::string> LoadKeysFromFile(const std::string& filepath);

    // Vòng lặp chạy ngầm để xử lý Queue
    void WorkerLoop();

private:
    std::unordered_map<std::string, std::shared_ptr<IAPIProvider>> m_providers;
    
    
    // Cơ chế Thread-safe Queue
    std::queue<APITask> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    
    // Luồng công nhân (Worker Thread)
    std::thread m_workerThread;
    bool m_isRunning;
};