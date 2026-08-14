#include "api_manager.h"
#include "api_log.h"

APIManager::APIManager() : m_isRunning(true) {
    // Khởi chạy luồng chạy ngầm ngay khi Manager được tạo
    m_workerThread = std::thread(&APIManager::WorkerLoop, this);
}

APIManager::~APIManager() {
    Shutdown();
}
void APIManager::InitConfigs(const std::string& configFilePath) {
    LOG(init_api_manager, 1, LogLevel::Info, LogCategory::System, std::cout << "[APIManager] Đang khởi tạo các dịch vụ từ: " << configFilePath << "\n");
    
    // 1. Đọc toàn bộ key từ file
    auto keys = LoadKeysFromFile(configFilePath);

    // 2. Đăng ký Gemini nếu tìm thấy key
    if (keys.find("GEMINI_API_KEY") != keys.end()) {
        auto gemini = std::make_shared<GeminiProvider>();
        if (gemini->Initialize(keys["GEMINI_API_KEY"])) {
            // Mặc định để Paused cho an toàn, bạn sẽ bật Active từ UI
            gemini->SetState(ProviderState::Paused); 
            RegisterProvider("gemini", gemini);
            LOG(api_gemini, 1, LogLevel::Info,LogCategory::System, std::cout << " -> Đã đăng ký dịch vụ: Gemini\n");
        }
    }

    // 3. Đăng ký GPT nếu tìm thấy key
    if (keys.find("OPENAI_API_KEY") != keys.end()) {
        // Giả sử bạn đã tạo GPTProvider tương tự GeminiProvider
        // auto gpt = std::make_shared<GPTProvider>();
        // gpt->Initialize(keys["OPENAI_API_KEY"]);
        // gpt->SetState(ProviderState::Paused);
        // RegisterProvider("gpt", gpt);
        LOG(api_gpt, 1, LogLevel::Info, LogCategory::System, std::cout << " -> Đã đăng ký dịch vụ: GPT\n");
    }

    // 4. Đăng ký YouTube API nếu tìm thấy key
    if (keys.find("YOUTUBE_API_KEY") != keys.end()) {
        // Khởi tạo YouTubeProvider...
        LOG(api_youtube, 1, LogLevel::Info, LogCategory::System, std::cout << " -> Đã đăng ký dịch vụ: YouTube\n");
    }
    
    // Lưu ý bảo mật: Bạn có thể code thêm logic để xóa biến `keys` khỏi RAM sau khi gán xong.
}

void APIManager::RegisterProvider(const std::string& name, std::shared_ptr<IAPIProvider> provider) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_providers[name] = provider;
}

void APIManager::SetProviderState(const std::string& name, ProviderState state) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_providers.find(name) != m_providers.end()) {
        m_providers[name]->SetState(state);
    }
}

// Đẩy task vào Queue
void APIManager::EnqueueTask(const std::string& providerName, const std::string& payload) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_taskQueue.push({providerName, payload});
    }
    // Đánh thức luồng worker đang ngủ
    m_cv.notify_one();
}

// Vòng lặp xử lý ngầm
void APIManager::WorkerLoop() {
    while (m_isRunning) {
        APITask currentTask;
        std::shared_ptr<IAPIProvider> targetProvider = nullptr;

        {
            // Khóa Mutex và chờ (Wait) cho đến khi Queue có dữ liệu hoặc bị Shutdown
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this]() { 
                return !m_taskQueue.empty() || !m_isRunning; 
            });

            if (!m_isRunning && m_taskQueue.empty()) {
                break; // Thoát vòng lặp khi tắt ứng dụng
            }

            // Lấy task ra khỏi Queue
            currentTask = m_taskQueue.front();
            m_taskQueue.pop();

            // Tìm provider tương ứng
            if (m_providers.find(currentTask.providerName) != m_providers.end()) {
                targetProvider = m_providers[currentTask.providerName];
            }
        } // Unlock Mutex tại đây để UI có thể tiếp tục đẩy Task mới vào Queue

        // --- Bắt đầu quá trình xử lý Tác vụ (Nằm ngoài Mutex để không block hệ thống) ---
        if (targetProvider) {
            ProviderState state = targetProvider->GetState();
            
            if (state == ProviderState::Active) {
                // Gọi thư viện mạng (libcurl) ở hàm này
                std::cout << "[API Worker] Sending data to " << currentTask.providerName << "...\n";
                std::string response = targetProvider->ExecuteRequest(currentTask.payload);
                
                // (Tùy chọn) Gửi response về hệ thống Logs của ImGui
                std::cout << "[API Worker] Response received: " << response << "\n";
            } 
            else if (state == ProviderState::Paused) {
                std::cout << "[API Worker] " << currentTask.providerName << " is PAUSED. Task dropped.\n";
                // Hoặc bạn có thể thiết kế để đẩy Task này trở lại Queue nếu không muốn mất dữ liệu
            }
        }
    }
}

void APIManager::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_isRunning = false;
    }
    m_cv.notify_all(); // Đánh thức worker để nó tự kết thúc
    if (m_workerThread.joinable()) {
        m_workerThread.join(); // Đợi worker đóng băng an toàn
    }
}
std::unordered_map<std::string, std::string> APIManager::LoadKeysFromFile(const std::string& filepath) {
    std::unordered_map<std::string, std::string> keys;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "[APIManager] Không tìm thấy file cấu hình: " << filepath << "\n";
        return keys;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Bỏ qua dòng trống hoặc dòng comment (bắt đầu bằng #)
        if (line.empty() || line[0] == '#') continue;

        // Tách chuỗi tại dấu '='
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string keyName = line.substr(0, delimiterPos);
            std::string keyValue = line.substr(delimiterPos + 1);
            
            // Xóa khoảng trắng thừa (nếu cần)
            keys[keyName] = keyValue;
        }
    }
    return keys;
}