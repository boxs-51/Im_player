#pragma once
#include <string>
#include <vector>
#include <map>
#include <cmath>

enum class AudioPreset {
    Flat,           // Cân bằng phòng thu
    Pop,            // Tập trung vào giọng ca sĩ (Vocal) và độ sáng
    Rock,           // Đẩy dải Trầm và Cao sắc nét, dải Trung hơi lõm (V-Shape)
    EDM_Dance,      // Kích dải Siêu Trầm (Sub-Bass) và Treble tí tách
    Classical,      // Tối ưu dải Trung Cao cho nhạc cụ dây/piano
    // --- CÁC PRESET MỚI BỔ SUNG ---
    Acoustic,       // Chuyên trị nhạc cụ gỗ, guitar, tôn giọng mộc mạc
    Gaming_FPS,     // Lọc dải trầm, tăng dải Trung-Cao để nghe rõ tiếng bước chân/súng
    Movie_Cinema,   // Giả lập rạp phim: Đẩy Sub-bass tạo độ rung chấn và tăng rõ lời thoại
    Deep_Bass       // Chỉ tập trung tăng cường độ lực cho dải siêu trầm (Basshead)
};

enum class LogLevel { Info, Warning, Error, AI_Action };

struct LogEntry {
    std::string timestamp;
    std::string message;
    LogLevel level;
};

struct FilterParam {
    float current;     // Giá trị thời gian thực hiện tại (AI có thể thay đổi liên tục)
    float min;         // Giá trị nhỏ nhất cho phép
    float max;         // Giá trị lớn nhất cho phép
    float def;         // Giá trị mặc định ban đầu
    float user_target; // Ghi nhớ giá trị gốc do CHÍNH NGƯỜI DÙNG thiết lập/kéo trên UI

    float lastSent = 999999.0f;
};

struct AudioFilter {
    std::string id;       
    std::string name;     
    bool enabled;         
    std::map<std::string, FilterParam> params; 
    std::string group;    

    bool isBypassManagement = false; 
    bool isFailed = false;

    std::string GetInitString() const {

        if (id == "f_ebur_measurer") {
        return "@ebur_measurer:lavfi=[ebur128=metadata=1:peak=all]";
        }

        std::string res = "@" + id + ":" + name;
        if (!params.empty()) {
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
                
                // Quy đổi an toàn từ dB sang tuyến tính khi khởi tạo chuỗi cho acompressor
                if (name == "acompressor" && key == "threshold") {
                    final_value = std::pow(10.0f, p.current / 20.0f);
                    if (final_value < 0.000976563f) final_value = 0.000976563f;
                    if (final_value > 1.0f) final_value = 1.0f;
                }
                if (id == "f_volume" && key == "volume") {
                    final_value = std::pow(10.0f, p.current / 20.0f);
                }
                res += key + "=" + std::to_string(final_value);
                first = false;
            }
        }
        return res;
    }
};

struct AudioTrackInfo {
    std::string displayName;
    std::string trackId;
};

struct AudioContext {
    double volume = 0.0f;
    double speed = 0.0f;
    int64_t sample_rate = 0;
    int64_t channel_count = 0;
    double bitrate_kbps = 0.0f;
    std::string codec = "";
    bool is_audio_only = false;

    // --- HỆ THỐNG DỮ LIỆU ĐẦU VÀO TOÀN DIỆN TỪ EBUR128 ---
    double loudness_momentary = 0.0f;   // lavfi.r128.M  -> Độ to tức thời (cửa sổ 400ms), nhạy bén với tiếng nổ/vocal giật mình
    double loudness_shortterm = 0.0f;   // lavfi.r128.S  -> Độ to ngắn hạn (cửa sổ 3s), biểu thị cảm nhận âm lượng thực tế
    double loudness_integrated = 0.0f;  // lavfi.r128.I  -> Độ to trung bình tích lũy từ đầu file đến hiện tại
    double loudness_range = 0.0f;       // lavfi.r128.LRA -> Dải động (độ chênh lệch âm lượng giữa các phân đoạn)
    double loudness_lra_low = 0.0f;   // lavfi.r128.LRA.low  -> Ngưỡng đáy năng lượng tích lũy (LUFS)
    double loudness_lra_high = 0.0f;  // lavfi.r128.LRA.high -> Ngưỡng đỉnh năng lượng tích lũy (LUFS)
    
    double true_peak = 0.0f;            // lavfi.r128.true_peak     -> Đỉnh sóng thực cao nhất (Hệ tuyến tính 0.0 -> 1.0)
    double true_peak_ch0 = 0.0f;        // lavfi.r128.true_peak_ch0 -> Đỉnh sóng thực kênh trái (Linear)
    double true_peak_ch1 = 0.0f;        // lavfi.r128.true_peak_ch1 -> Đỉnh sóng thực kênh phải (Linear)

    double sample_peak = 0.0f;
    double sample_peak_ch0 = 0.0f;
    double sample_peak_ch1 = 0.0f;

    // THÀNH PHẦN METADATA MỚI ĐƯỢC TÍCH HỢP
    
    std::string aformat = "";          // "s16", "f32", "fltp"
    std::string ahr_channels = "";     // Chi tiết layout: "FL FR FC LFE BL BR"
    bool is_paused = false;            // Trạng thái tạm dừng phát
    bool is_muted = false;             // Trạng thái câm tiếng
    bool is_buffering = false;         // Đang nạp mạng (cache_buffering_state)
    bool is_network_stream = false;    // Phát trực tuyến (demuxer_via_network)
    bool hearing_impaired = false;     // Cờ hỗ trợ người khiếm thính (Dialog Boost)
};

struct AdaptiveTargets {
    // 1. Mảng 12 dải tần EQ Graphic
    std::vector<float> eq_gains = std::vector<float>(12, 0.0f);
    
    // 2. Nhóm bộ lọc chức năng không gian / cốt lõi
    bool crystalizer_enabled = false;
    float crystalizer_i = 2.0f;
    
    bool stereo_enabled = false;
    float stereo_m = 2.5f;
    
    bool comp_enabled = false;
    float comp_th = -12.0f;
    float comp_rt = 2.0f;
    
    bool reverb_enabled = false;
    std::string reverb_preset = "none";

    // 3. Nhóm mạch ngoại vi (Booster / Stabilizer) do AI tính toán hạ trần chống cháy
    float booster_volume = 0.0f;
    float out_comp_threshold = -12.0f;
    float out_comp_makeup = 0.0f;
    float out_lim_threshold = -1.0f;
};