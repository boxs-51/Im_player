#pragma once
#include <string>
#include <vector>
#include <shared_mutex>
#include <functional>
#include <memory>
#include <gui/gui.h>


// Giữ nguyên các Struct dữ liệu của bạn
struct AppSettings {
    std::string selectedFormat = "";
    std::string selectedAudio = "";
    std::string selectedResolution = "";
    bool enableSubtitles = false;
    float playbackSpeed = 1.0f;
    float audiodelay = 0.0f;
    bool repeatVideo = false;
    bool autoPlayNext = false;
    int defaultVolume = 100;    
    bool repeatlist = false;
};

struct CommonSettings {
    ThemeType themetype = ThemeType::DarkMode;
    std::string Videofilter = "";
    std::string Audiofilter = "";
    std::string VideoDecode = "";
    std::string AudioDecode = "";
    int fontsize = 20;
};

// Khai báo Enum để phân loại nhóm cấu hình khi có thay đổi
enum class ConfigGroup {
    Video,
    Common
};

// Các hàm Helper convert theme (Giữ nguyên logic của bạn)
inline std::string ThemeToString(ThemeType t) {
    switch (t) {
        case ThemeType::DarkMode:      return "Dark Mode";
        case ThemeType::LightMode:     return "Light Mode";
        case ThemeType::MidnightMode:  return "Mid Night Mode";
        case ThemeType::RetroMode:     return "Retro Mode";
        default:                       return "Dark Mode";
    }
}
inline ThemeType StringToTheme(const std::string& s) {
    if (s == "Dark Mode")       return ThemeType::DarkMode;
    if (s == "Light Mode")      return ThemeType::LightMode;
    if (s == "Mid Night Mode")  return ThemeType::MidnightMode;
    if (s == "Retro Mode")      return ThemeType::RetroMode;
    return ThemeType::DarkMode;
}
// Định nghĩa Callback: Khi có thay đổi, trả về Nhóm cấu hình bị thay đổi
using ConfigChangedCallback = std::function<void(ConfigGroup)>;

class ConfigManager {
private:
    AppSettings v_Settings;
    CommonSettings c_Settings;
    
    mutable std::shared_mutex rwMutex; // Read-Write Lock bảo vệ data
    std::vector<ConfigChangedCallback> listeners; // Danh sách các bên đăng ký nhận sự kiện

    ConfigManager() = default; // Ẩn Constructor để làm Singleton

public:
    // Lấy instance duy nhất
    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    // ---------------- THAO TÁC ĐỌC (Thread-safe) ----------------
    // Trả về bản sao (Copy) để luồng khác dùng an toàn mà không sợ bị sửa giữa chừng
    AppSettings GetVideoSettings() const {
        std::shared_lock lock(rwMutex);
        return v_Settings;
    }

    CommonSettings GetCommonSettings() const {
        std::shared_lock lock(rwMutex);
        return c_Settings;
    }

    // ---------------- THAO TÁC GHI TỪ RUNTIME (Thread-safe) ----------------
    void UpdateVideoSettings(std::function<void(AppSettings&)> modifier) {
        {
            std::unique_lock lock(rwMutex);
            modifier(v_Settings); // Thực hiện sửa đổi
        }
        Notify(ConfigGroup::Video);
    }

    void UpdateCommonSettings(std::function<void(CommonSettings&)> modifier) {
        {
            std::unique_lock lock(rwMutex);
            modifier(c_Settings); // Thực hiện sửa đổi
        }
        Notify(ConfigGroup::Common);
    }

    // ---------------- CƠ CHẾ ĐĂNG KÝ SỰ KIỆN ----------------
    void RegisterListener(ConfigChangedCallback callback) {
        std::unique_lock lock(rwMutex);
        listeners.push_back(callback);
    }

    // ---------------- CÁC HÀM FILE I/O ----------------
    void LoadAll();
    void SaveVideo();
    void SaveCommon();

private:
    void Notify(ConfigGroup group) {
        std::vector<ConfigChangedCallback> callbacks;

        {
            std::shared_lock lock(rwMutex);
            callbacks = listeners;
        }

        for (auto& callback : callbacks) {
            if (callback) {
                callback(group);
            }
        }
    }
};