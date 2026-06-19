#include "api/api_manager.h"

class GeminiProvider : public IAPIProvider {
private:
    ProviderState m_state = ProviderState::Paused; // Mặc định tạm dừng cho an toàn
    std::string m_key;
public:
    bool Initialize(const std::string& apiKey) override { m_key = apiKey; return true; }
    void SetState(ProviderState state) override { m_state = state; }
    ProviderState GetState() const override { return m_state; }
    
    std::string ExecuteRequest(const std::string& payload) override {
        // Giả lập thời gian trễ của mạng (Ví dụ: gọi API mất 2 giây)
        std::this_thread::sleep_for(std::chrono::seconds(2)); 
        return "Gemini AI Analysis: Đã phát hiện biến động Peak = 0.95";
    }
};