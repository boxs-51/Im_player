#pragma once
#include "utils.h"
#include <mpv/mpv_data.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <tuple>
#include <mpv/client.h>

enum class AudioPreset {
    Flat,       // Cân bằng phòng thu
    Pop,        // Tập trung vào giọng ca sĩ (Vocal) và độ sáng
    Rock,       // Đẩy dải Trầm và Cao sắc nét, dải Trung hơi lõm (V-Shape)
    EDM_Dance,  // Kích dải Siêu Trầm (Sub-Bass) và Treble tí tách
    Classical   // Tối ưu dải Trung Cao (Mid-High) cho nhạc cụ dây/piano
};
// Định nghĩa các cấp độ Log để UI dễ dàng đổi màu sắc (Trắng, Vàng, Đỏ)
enum class LogLevel {
    Info,
    Warning,
    Error,
    AI_Action
};
struct LogEntry {
    std::string timestamp;
    std::string message;
    LogLevel level;
};
// Cấu trúc lưu trữ thông số của từng Parameter trong bộ lọc
struct FilterParam {
    float current; // Giá trị hiện tại
    float min;     // Giá trị nhỏ nhất cho phép
    float max;     // Giá trị lớn nhất cho phép
    float def;     // Giá trị mặc định ban đầu
};

// Cấu trúc lưu trữ thông tin của một Bộ lọc Âm thanh (Audio Filter)
struct AudioFilter {
    std::string id;       // Định danh duy nhất (Ví dụ: "eq_b0", "f_bass")
    std::string name;     // Tên bộ lọc thực tế trong FFmpeg (Ví dụ: "equalizer", "bass")
    bool enabled;         // Trạng thái Bật/Tắt
    std::map<std::string, FilterParam> params; // Danh sách các tham số đi kèm
    std::string group;    // Nhóm bộ lọc để xử lý xung đột loại trừ (Ví dụ: "spatial", "dynamics")

    bool isBypassManagement = false; // Cờ đặc biệt để loại trừ khỏi hệ thống quản lý xung đột nhóm (Dành cho bộ lọc bypass)
    // Hàm sinh chuỗi khởi tạo chuẩn Key-Value cho các bộ lọc thông thường
    std::string GetInitString() const {
        std::string res = "@" + id + ":" + name;
        if (!params.empty()) {
            
            // XỬ LÝ ĐẶC BIỆT CHO CHORUS: Ép cấu hình mảng tránh lỗi FFmpeg core
            if (name == "chorus") {
                float in_gain = params.count("in_gain") ? params.at("in_gain").current : 0.40f;
                float out_gain = params.count("out_gain") ? params.at("out_gain").current : 0.40f;
                res += "=" + std::to_string(in_gain) + ":" + std::to_string(out_gain) + ":40:0.25:0.3:2.0";
                return res;
            }

            res += "=";
            bool first = true;
            for (const auto& [key, p] : params) {
                if (!first) res += ":";
                float final_value = p.current;
                
                // Tự động chuyển đổi dB sang cấu trúc tuyến tính cho acompressor threshold
                if (name == "acompressor" && key == "threshold") {
                    final_value = std::pow(10.0f, p.current / 20.0f);
                    if (final_value < 0.00001f) final_value = 0.00001f;
                    if (final_value > 1.0f) final_value = 1.0f;
                }
                res += key + "=" + std::to_string(final_value);
                first = false;
            }
        }
        return res;
    }
};

// Cấu trúc thông tin Track âm thanh bổ trợ
struct AudioTrackInfo {
    std::string displayName;
    std::string trackId;
};

// Lớp Quản lý Hệ thống Bộ lọc và Tinh chỉnh Âm thanh (Singleton Pattern)
class AudioFilterManager {
public:
    static AudioFilterManager& Instance();

    // Cấm sao chép Instance (Bảo vệ tính toàn vẹn của Singleton)
    AudioFilterManager(const AudioFilterManager&) = delete;
    AudioFilterManager& operator=(const AudioFilterManager&) = delete;

    // Khởi tạo và cấu hình core hệ thống
    void Init(mpv_handle* h);
    
    // Quản lý trạng thái và đồng bộ bộ lọc xuống libmpv
    void SetFilterEnabled(const std::string& id, bool enabled);
    void ToggleFilter(const std::string& id, bool state);
    void SetAllFiltersState(bool enabled);
    bool IsFilterEnabled(const std::string& id);
    int GetActiveFilterCount();

    void SetAdaptiveMode(bool enabled, AudioPreset preset = AudioPreset::Flat);
    bool IsAdaptiveMode() const { return m_autoMode; }
    AudioPreset GetCurrentPreset() const { return m_currentPreset; }
    void SetCurrentPreset(AudioPreset preset);
    void UpdateAdaptiveFilters();

    // CƠ CHẾ BYPASS MỚI
    void SetFilterBypassMode(const std::string& id, bool bypassState);
    bool IsFilterBypassMode(const std::string& id);
    void SetGlobalBypassMode(bool bypassState);
    bool IsGlobalBypassEnabled() const { return m_globalBypass; }
    
    // Cập nhật giá trị thông số Real-time
    void UpdateParam(const std::string& id, const std::string& key, float value);
    void BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates);
    
    // Reset cấu hình bộ lọc
    void ResetFilter(const std::string& id);
    void ResetAllToDefaults();
    
    // Giao tiếp Đọc/Ghi File lưu trữ dữ liệu (JSON/TXT)
    void SaveToFile();
    void LoadFromFile();
    
    // Quản lý luồng Kênh Loa (Channel Mode) và Track âm thanh
    void SetChannelMode(const std::string& mode);
    std::string GetChannelMode() const { return m_channelMode; }
    
    void AddAudioTrack(const std::string& trackId, const std::string& lang, const std::string& codec);
    void SelectAudioTrack(const std::string& trackId);
    std::string GetCurrentAudioTrack() const { return m_currentAudioTrack; }
    const std::vector<AudioTrackInfo>& GetAudioTracks() const { return m_audioTracks; }

    // Tìm kiếm nhanh bộ lọc theo ID để UI truy cập dữ liệu vẽ Slider
    AudioFilter* FindFilter(const std::string& id);
    const std::vector<AudioFilter>& GetFilters() const { return m_filters; }

    mpv_handle* GetMpvHandle() { return mpv; }

public:
    // Hàm đẩy Log vào hệ thống (Dùng trong nội bộ Manager)
    void AddLog(const std::string& message, LogLevel level = LogLevel::Info);

    // Hàm public cho UI gọi lấy dữ liệu lên vẽ
    std::vector<LogEntry> GetLogs();
    void ClearLogs();

private:
    std::vector<LogEntry> m_logs;
    std::mutex m_logMutex;
    const size_t MAX_LOG_SIZE = 100; // Giới hạn bộ nhớ tránh tràn RAM khi chạy lâu
    
private:
    AudioFilterManager() : mpv(nullptr), m_channelMode("stereo") {}
    ~AudioFilterManager() = default;

    // Hàm helper nội bộ để đăng ký bộ lọc và tham số
    void AddFilter(const std::string& id, const std::string& name, const std::string& group);
    void RegisterParam(const std::string& id, const std::string& key, float min, float max, float def);
    
    void EvaluateSystemSafety();

    // Đẩy toàn bộ chuỗi filter logic xuống MPV Core thông qua "af" property
    void SyncAll();

private:
    mpv_handle* mpv;
    std::string path;
    std::string m_channelMode;
    std::string m_currentAudioTrack;
    
    std::vector<AudioFilter> m_filters;
    std::unordered_map<std::string, size_t> m_filterIndex; // Bảng băm index tối ưu tốc độ tìm kiếm O(1)
    std::vector<AudioTrackInfo> m_audioTracks;

    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    VideoInfo& g_videoInfo = GetVideoInfo();

    bool m_globalBypass = false; // Cờ tắt toàn bộ hệ thống quản lý xung đột (Dành cho trường hợp cực đoan)
    bool m_autoMode = false;
    AudioPreset m_currentPreset = AudioPreset::Flat;
};