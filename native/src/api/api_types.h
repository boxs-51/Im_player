#pragma once
#include <string>
#include <memory>

// Các trạng thái của một API Provider
enum class ProviderState {
    Active,     // Đang hoạt động, sẵn sàng gửi request
    Paused,     // Tạm dừng, không gửi request
    Error       // Đang gặp lỗi (ví dụ: sai key, hết rate limit)
};

// Cấu trúc một tác vụ (Task) gửi lên hàng đợi
struct APITask {
    std::string providerName; // "gemini", "gpt", "youtube"
    std::string payload;      // Dữ liệu JSON hoặc thông số gửi đi
};

// Giao diện chuẩn cho mọi API Service
class IAPIProvider {
public:
    virtual ~IAPIProvider() = default;
    
    virtual void SetState(ProviderState state) = 0;
    virtual ProviderState GetState() const = 0;
    virtual bool Initialize(const std::string& apiKey) = 0;
    
    // Hàm thực thi request (Sẽ được gọi bởi Worker Thread)
    virtual std::string ExecuteRequest(const std::string& payload) = 0;
};