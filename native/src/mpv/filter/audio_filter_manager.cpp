#include <mpv/filter/audio_filter_manager.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

AudioFilterManager& AudioFilterManager::Instance() {
    static AudioFilterManager instance;
    return instance;
}

void AudioFilterManager::Init(mpv_handle* h) { 
    mpv = h; 
    path = AutoPath<std::string>("%ROOT%", "data", "audio_filter.json");
    m_channelMode = "stereo";
    m_autoMode = false;
    m_globalBypass = false;
    m_currentPreset = AudioPreset::Flat;

    // =========================================================================
    // 1. Nhóm Equalizer nâng cấp (10-Band ISO Standard Graphic EQ)
    // =========================================================================
    const std::vector<std::pair<std::string, std::string>> eqBands = {
        {"eq_b0", "31"},   {"eq_b1", "63"},   {"eq_b2", "125"},  {"eq_b3", "250"},
        {"eq_b4", "500"},  {"eq_b5", "1000"}, {"eq_b6", "2000"}, {"eq_b7", "4000"},
        {"eq_b8", "8000"}, {"eq_b9", "16000"}
    };

    for (const auto& [band_id, freq] : eqBands) {
        AddFilter(band_id, "equalizer", "equalizer_group");
        RegisterParam(band_id, "g", -20.0f, 20.0f, 0.0f); 
    }

    AddFilter("f_bass", "bass", "tone_booster");
    RegisterParam("f_bass", "g", -20.0f, 20.0f, 0.0f);
    RegisterParam("f_bass", "f", 20.0f, 500.0f, 100.0f);

    AddFilter("f_treble", "treble", "tone_booster");
    RegisterParam("f_treble", "g", -20.0f, 20.0f, 0.0f);
    RegisterParam("f_treble", "f", 1000.0f, 20000.0f, 3500.0f);

    // =========================================================================
    // 2. Nhóm Dynamic Range & Không gian âm thanh
    // =========================================================================
    AddFilter("f_volume", "volume", "gain_node");
    RegisterParam("f_volume", "volume", -30.0f, 30.0f, 0.0f);

    AddFilter("f_comp", "acompressor", "dynamics");
    RegisterParam("f_comp", "threshold", -60.0f, 0.0f, -12.0f);
    RegisterParam("f_comp", "ratio", 1.0f, 20.0f, 2.0f);
    RegisterParam("f_comp", "attack", 0.01f, 2000.0f, 20.0f);
    RegisterParam("f_comp", "release", 0.01f, 9000.0f, 250.0f);

    AddFilter("f_norm", "loudnorm", "dynamics");

    AddFilter("f_stereo", "extrastereo", "spatial");
    RegisterParam("f_stereo", "m", 0.0f, 10.0f, 2.5f); 

    AddFilter("f_crystalizer", "crystalizer", "spatial");
    RegisterParam("f_crystalizer", "i", 0.0f, 10.0f, 2.0f); 

    AddFilter("f_bs2b", "bs2b", "spatial"); 
    RegisterParam("f_bs2b", "profile", 0.0f, 2.0f, 0.0f); 

    AddFilter("f_pitch", "atempo", "time_pitch");
    RegisterParam("f_pitch", "tempo", 0.5f, 2.0f, 1.0f);

    AddFilter("f_chorus", "chorus", "effects");
    RegisterParam("f_chorus", "in_gain", 0.0f, 1.0f, 0.4f);
    RegisterParam("f_chorus", "out_gain", 0.0f, 1.0f, 0.4f);

    AddFilter("f_flanger", "flanger", "effects");
    RegisterParam("f_flanger", "delay", 0.0f, 30.0f, 0.0f);
    RegisterParam("f_flanger", "depth", 0.0f, 10.0f, 2.0f);

    // =========================================================================
    // 3. Hệ thống Tăng cường & Ổn định âm thanh ngoại vi (0 -> 130)
    // =========================================================================
    // Bộ tăng cường âm lượng: Map hệ số thực từ 0.0f (Mute) -> 1.3f (130%)
    AddFilter("f_vol_booster", "volume", "outer_gain_node");
    RegisterParam("f_vol_booster", "volume", 0.0f, 1.3f, 1.0f); 

    // Bộ nén chuyên dụng: Gom đều tín hiệu, kéo sàn âm nhỏ lên
    AddFilter("f_out_compressor", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_compressor", "threshold", -30.0f, 0.0f, -18.0f); // Ngưỡng bắt nén sớm
    RegisterParam("f_out_compressor", "ratio",     1.0f, 20.0f, 4.0f);    // Tỷ lệ nén 4:1 mượt mà
    RegisterParam("f_out_compressor", "attack",    0.01f, 100.0f, 5.0f);   // Phản ứng nhanh 5ms
    RegisterParam("f_out_compressor", "release",   10.0f, 1000.0f, 50.0f); // Xả nén nhanh 50ms
    RegisterParam("f_out_compressor", "makeup",    1.0f, 10.0f, 2.0f);     // Kích âm lượng nền lên gấp đôi (2.0)

    // Bộ giới hạn Brickwall Limiter: Chốt chặn cuối cùng, chống rè tuyệt đối
    AddFilter("f_out_limiter", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_limiter", "threshold", -30.0f, 0.0f, -1.0f);   // Khóa cứng sát đỉnh an toàn (-1dB)
    RegisterParam("f_out_limiter", "ratio",     1.0f, 20.0f, 20.0f);  // Tỷ lệ nén kịch trần biến thành Limiter
    RegisterParam("f_out_limiter", "attack",    0.01f, 100.0f, 1.0f);   // Chặn đứng tức thì trong 1ms
    RegisterParam("f_out_limiter", "release",   10.0f, 1000.0f, 100.0f);
    RegisterParam("f_out_limiter", "makeup",    1.0f, 10.0f, 1.0f);     // Giữ nguyên âm lượng đỉnh
}

void AudioFilterManager::AddFilter(const std::string& id, const std::string& name, const std::string& group) {
    m_filters.push_back({id, name, false, {}, group, false}); // Đảm bảo struct có trường isBypassManagement ở cuối
    m_filterIndex[id] = m_filters.size() - 1; 
}

void AudioFilterManager::SetFilterEnabled(const std::string& id, bool enabled) {
    auto* f = FindFilter(id);
    if (!f || f->enabled == enabled) return;

    f->enabled = enabled;

    // QUẢN LÝ XUNG ĐỘT NHÓM
    if (enabled && !m_globalBypass && !f->isBypassManagement) {
        if (!f->group.empty() && f->group != "equalizer_group") {
            for (auto& other : m_filters) {
                if (other.id != id && other.group == f->group && other.enabled && !other.isBypassManagement) {
                    other.enabled = false;
                    std::string msg = "[Conflict Managed] Auto-disabled " + other.id + " due to group conflict: " + f->group;
                    AddLog(msg, LogLevel::Warning);
                }
            }
        }
    }

    EvaluateSystemSafety();
    SyncAll(); 
}

void AudioFilterManager::SetFilterBypassMode(const std::string& id, bool bypassState) {
    if (auto* f = FindFilter(id)) {
        f->isBypassManagement = bypassState;

        std::string msg = "[Bypass System] Filter " + id + " set Bypass Mode -> " + (bypassState ? "ON" : "OFF");
        AddLog(msg, LogLevel::Info);

        EvaluateSystemSafety();
        SyncAll();
    }
}

bool AudioFilterManager::IsFilterBypassMode(const std::string& id) {
    if (auto* f = FindFilter(id)) return f->isBypassManagement;
    return false;
}

void AudioFilterManager::SetGlobalBypassMode(bool bypassState) {
    m_globalBypass = bypassState;

    std::string msg = "[Bypass System] GLOBAL Bypass Manager set -> " + bypassState ? "ENABLED" : "DISABLED";
    AddLog(msg, LogLevel::Info);

    EvaluateSystemSafety();
    SyncAll();
}

void AudioFilterManager::EvaluateSystemSafety() {
    if (m_globalBypass) return;

    // Cờ chống đệ quy vô hạn (Reentrancy Guard)
    static bool isEvaluating = false;
    if (isEvaluating) return;
    isEvaluating = true;

    float totalGainAccumulation = 0.0f;

    // 1. Quét tính tổng độ lợi Gain khuếch đại dương
    for (const auto& f : m_filters) {
        if (!f.enabled || f.isBypassManagement) continue;

        if ((f.name == "equalizer" || f.name == "bass" || f.name == "treble") && f.params.count("g")) {
            float g_val = f.params.at("g").current;
            if (g_val > 0.0f) totalGainAccumulation += g_val; 
        }
    }

    // 2. Điều tiết master node volume chống cháy màng loa
    auto* vol_node = FindFilter("f_volume");
    if (vol_node && !vol_node->isBypassManagement) {
        if (totalGainAccumulation > 15.0f) {
            float safetyReduction = -(totalGainAccumulation - 15.0f) * 0.5f;
            float targetVol = std::clamp(safetyReduction, -30.0f, 0.0f);
            
            if (std::abs(vol_node->params["volume"].current - targetVol) > 0.01f) {
                vol_node->params["volume"].current = targetVol;

                std::string msg = "[SAFETY ENGINE] Total gain Warning (" + std::to_string(totalGainAccumulation) + "dB). Auto attenuating master node volume to: " + std::to_string(targetVol) + "dB";
                AddLog(msg, LogLevel::Error);

                // Đẩy trực tiếp xuống MPV mà không chạy lại hàm EvaluateSystemSafety
                if (vol_node->enabled && mpv) {
                    std::string val_str = std::to_string(targetVol);
                    const char* cmd[] = {"af-command", "f_volume", "volume", val_str.c_str(), NULL};
                    mpv_command(mpv, cmd);
                }
            }
        } else {
            if (vol_node->params["volume"].current < 0.0f && totalGainAccumulation <= 5.0f) {
                vol_node->params["volume"].current = 0.0f; 
                if (vol_node->enabled && mpv) {
                    const char* cmd[] = {"af-command", "f_volume", "volume", "0.00", NULL};
                    mpv_command(mpv, cmd);
                }
            }
        }
    }
    isEvaluating = false; // Mở khóa Guard
}

void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def) {
    if (auto* f = FindFilter(id)) {
        f->params[key] = {def, min, max, def}; 
    }
}

void AudioFilterManager::ResetFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        for (auto& [key, p] : f->params) {
            p.current = p.def;
        }
        if (f->enabled) SyncAll();
    }
}

void AudioFilterManager::SaveToFile() {
    if (path.empty()) return;
    std::ofstream f(path);
    if (!f.is_open()) return;
    for (const auto& filter : m_filters) {
        f << "[Filter]:" << filter.id << "|" << (filter.enabled ? "1" : "0") << "\n";
        for (const auto& [key, p] : filter.params) {
            f << key << ":" << p.current << "\n";
        }
    }
}

void AudioFilterManager::ResetAllToDefaults() {
    for (auto& filter : m_filters) {
        filter.enabled = false; 
        for (auto& [key, p] : filter.params) {
            p.current = p.def; 
        }
    }
    m_channelMode = "stereo"; 
    SyncAll(); 
}

void AudioFilterManager::LoadFromFile() {
    if (path.empty()) { ResetAllToDefaults(); return; }
    
    std::ifstream f(path);
    if (!f.is_open()) { ResetAllToDefaults(); return; }

    std::string line;
    std::string current_id = "";
    bool parse_success = false;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        if (line.rfind("[Filter]:", 0) == 0) {
            size_t delim = line.find('|');
            if (delim != std::string::npos) {
                current_id = line.substr(9, delim - 9);
                bool enabled = (line.substr(delim + 1) == "1");
                if (auto* filter = FindFilter(current_id)) {
                    filter->enabled = enabled;
                    parse_success = true;
                } else {
                    current_id = "";
                }
            }
        } else if (!current_id.empty()) {
            size_t delim = line.find(':');
            if (delim != std::string::npos) {
                std::string key = line.substr(0, delim);
                try {
                    float val = std::stof(line.substr(delim + 1));
                    if (auto* filter = FindFilter(current_id)) {
                        if (filter->params.count(key)) {
                            auto& param = filter->params[key];
                            param.current = std::clamp(val, param.min, param.max);
                        }
                    }
                } catch (...) {
                    continue;
                }
            }
        }
    }
    if (!parse_success) ResetAllToDefaults();
    else SyncAll();
}

void AudioFilterManager::SyncAll() {
    if (!mpv) return;
    
    std::string full_af = "";

    if (m_channelMode == "mono") {
        full_af += "pan=mono|c0=0.5*c0+0.5*c1";
    } else if (m_channelMode == "surround") {
        full_af += "pan=5.1|FL=c0|FR=c1|FC=c0+c1|LFE=0|BL=c0|BR=c1";
    }

    for (const auto& f : m_filters) {
        if (f.enabled) {
            std::string init_str = "";
            if (f.name == "equalizer") {
                std::string freq = "1000";
                if (f.id == "eq_b0") freq = "31";
                else if (f.id == "eq_b1") freq = "63";
                else if (f.id == "eq_b2") freq = "125";
                else if (f.id == "eq_b3") freq = "250";
                else if (f.id == "eq_b4") freq = "500";
                else if (f.id == "eq_b5") freq = "1000";
                else if (f.id == "eq_b6") freq = "2000";
                else if (f.id == "eq_b7") freq = "4000";
                else if (f.id == "eq_b8") freq = "8000";
                else if (f.id == "eq_b9") freq = "16000";

                float gain = f.params.count("g") ? f.params.at("g").current : 0.0f;
                init_str = "@" + f.id + ":equalizer=f=" + freq + ":width_type=o:w=1.0:g=" + std::to_string(gain);
            } else {
                init_str = f.GetInitString(); 
            }

            if (!init_str.empty()) {
                if (!full_af.empty()) full_af += ",";
                full_af += init_str;
            }
        }
    }
    // 3. ĐƯA BỘ TĂNG CƯỜNG ÂM LƯỢNG (0->130) VÀO CHUỖI
    auto* vol_boost = FindFilter("f_vol_booster");
    if (vol_boost && vol_boost->enabled) {
        if (!full_af.empty()) full_af += ",";
        full_af += vol_boost->GetInitString();
    }

    // 4. ĐƯA BỘ NÈN ĐẨY SÀN ÂM LÊN
    auto* out_comp = FindFilter("f_out_compressor");
    if (out_comp && out_comp->enabled) {
        if (!full_af.empty()) full_af += ",";
        full_af += out_comp->GetInitString();
    }

    // 5. LUÔN LUÔN BỌC BỘ LIMITER CHỐNG VỠ TIẾNG Ở CUỐI CÙNG
    auto* out_lim = FindFilter("f_out_limiter");
    if (out_lim && out_lim->enabled) {
        if (!full_af.empty()) full_af += ",";
        full_af += out_lim->GetInitString();
    }
    mpv_set_property_string(mpv, "af", full_af.c_str());
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    if (auto* f = FindFilter(id)) {
        auto it = f->params.find(key);
        if (it != f->params.end()) { 
            auto& param = it->second;

            if (id == "f_vol_booster" && key == "volume") {
                if (value > 1.5f) { 
                    value = value / 100.0f;
                }
            }

            value = std::clamp(value, param.min, param.max);
            if (param.current == value) return; // Không đổi thì bỏ qua

            param.current = value;
            
            if (key == "g" || key == "volume") {
                EvaluateSystemSafety();
            }

            if (f->enabled && mpv) {
                // Biến tạm để giữ giá trị thực tế đẩy xuống MPV
                float value_to_send = param.current;

                // XỬ LÝ ĐẶC BIỆT CHO COMPRESSOR THRESHOLD
                if (f->name == "acompressor" && key == "threshold") {
                    value_to_send = std::pow(10.0f, param.current / 20.0f);
                    if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                    if (value_to_send > 1.0f) value_to_send = 1.0f;
                }

                std::ostringstream ss;
                // Nếu là threshold thì cần độ chính xác cao (5 chữ số thập phân), ngược lại giữ 2
                if (f->name == "acompressor" && key == "threshold") {
                    ss << std::fixed << std::setprecision(5) << value_to_send;
                } else {
                    ss << std::fixed << std::setprecision(2) << value_to_send;
                }
                std::string val_str = ss.str();

                const char* cmd[] = {"af-command", id.c_str(), key.c_str(), val_str.c_str(), NULL};
                mpv_command(mpv, cmd);
            }
        }
    }
}

void AudioFilterManager::BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates) {
    bool has_changed = false;
    for (const auto& [id, key, value] : updates) {
        if (auto* f = FindFilter(id)) {
            auto it = f->params.find(key);
            if (it != f->params.end()) {
                auto& param = it->second;
                float clamped = std::clamp(value, param.min, param.max);
                if (param.current != clamped) {
                    param.current = clamped;
                    has_changed = true;
                }
            }
        }
    }
    if (has_changed) {
        EvaluateSystemSafety();
        SyncAll(); 
    }
}

void AudioFilterManager::ToggleFilter(const std::string& id, bool state) {
    SetFilterEnabled(id, state); 
}

void AudioFilterManager::SetAllFiltersState(bool enabled) {
    bool changed = false;
    for (auto& f : m_filters) {
        if (f.enabled != enabled) {
            f.enabled = enabled;
            changed = true;
        }
    }
    if (changed) {
        EvaluateSystemSafety();
        SyncAll();
    }
}

bool AudioFilterManager::IsFilterEnabled(const std::string& id) {
    if (auto* f = FindFilter(id)) return f->enabled;
    return false;
}

int AudioFilterManager::GetActiveFilterCount() {
    int count = 0;
    for (const auto& f : m_filters) {
        if (f.enabled) count++;
    }
    return count;
}

AudioFilter* AudioFilterManager::FindFilter(const std::string& id) {
    auto it = m_filterIndex.find(id);
    if (it != m_filterIndex.end() && it->second < m_filters.size()) {
        return &m_filters[it->second];
    }
    return nullptr;
}

void AudioFilterManager::AddAudioTrack(const std::string& trackId, const std::string& lang, const std::string& codec) {
    m_audioTracks.push_back({trackId + " [" + lang + "] (" + codec + ")", trackId});
}

void AudioFilterManager::SelectAudioTrack(const std::string& trackId) {
    if (!mpv) return;
    m_currentAudioTrack = trackId;
    mpv_set_property_string(mpv, "aid", trackId.c_str());
}

void AudioFilterManager::SetChannelMode(const std::string& mode) {
    if (m_channelMode != mode) {
        m_channelMode = mode;
        if (mode == "mono") {
            if (auto* f = FindFilter("f_stereo")) f->enabled = false;
            if (auto* f = FindFilter("f_bs2b")) f->enabled = false;
        }
        SyncAll(); 
    }
}

void AudioFilterManager::UpdateAdaptiveFilters() {
    // 1. KIỂM TRA ĐIỀU KIỆN TIÊN QUYẾT BẰNG BIẾN THÀNH VIÊN THỰC TẾ
    if (!m_autoMode || !mpv || m_globalBypass) return;

    // 2. THROTTLING - GIỚI HẠN TẦN SUẤT QUÉT (150ms một lần giúp giảm CPU tuyệt đối)
    static auto lastUpdateTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count();
    if (elapsedTime < 150) return; 
    lastUpdateTime = currentTime;

    // 3. TRÍCH XUẤT TRẠNG THÁI REAL-TIME TỪ STRUCT GỐC CỦA BẠN
    double current_volume = (double)g_playbackStatus.volume;
    double current_speed  = g_playbackStatus.speed;
    
    // Đọc thông số Demuxer/Stream chính xác của file đang phát
    int64_t sample_rate   = (int64_t)g_videoInfo.g_audioarams.asamplerate;
    int64_t channel_count = (int64_t)g_videoInfo.g_audioarams.channel_count;
    double bitrate_kbps   = (double)(g_videoInfo.abitrate / 1000); // g_videoInfo.abitrate lưu bps
    
    std::string codec_name = g_videoInfo.acodec;         // Codec âm thanh (vd: "aac", "mp3", "flac")
    bool is_audio_only     = g_videoInfo.width == 0 && g_videoInfo.height == 0; // Nếu width/height = 0 tức là file nhạc thuần

    // 4. HYSTERESIS - BỘ LỌC NHIỄU BIẾN ĐỘNG THÔNG SỐ (Chống spam tính toán)
    static double last_volume = -999.0;
    static double last_bitrate = -999.0;
    static double last_speed = -1.0;
    static int64_t last_channels = -1;
    static AudioPreset last_preset = static_cast<AudioPreset>(-1);

    // Phát hiện thay đổi môi trường đủ lớn
    bool environment_changed = (std::abs(current_volume - last_volume) > 1.5) || 
                               (std::abs(bitrate_kbps - last_bitrate) > 10.0) ||
                               (std::abs(current_speed - last_speed) > 0.05) ||
                               (channel_count != last_channels) ||
                               (m_currentPreset != last_preset);
    if (environment_changed) {
        std::string ai_msg = "[AI Matrix] Context altered. Volume: " + std::to_string((int)current_volume) + 
                            "%, Codec: " + codec_name + ", Speed: " + std::to_string(current_speed);
        AddLog(ai_msg, LogLevel::AI_Action);
    }
    
    last_volume = current_volume;
    last_bitrate = bitrate_kbps;
    last_speed = current_speed;
    last_channels = channel_count;
    last_preset = m_currentPreset;

    // 5. KHỞI TẠO MA TRẬN TARGET GAINS CHO 10 BĂNG TẦN EQUALIZER (Từ eq_b0 đến eq_b9)
    std::vector<float> targetGains(10, 0.0f);

    switch (m_currentPreset) {
        case AudioPreset::Pop:
        {
            targetGains = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 4.0f, 3.5f, 2.0f, 1.0f, 0.0f};
            std::string msg = "[Preset Applied] Pop preset activated: Boosting mid-high frequencies for vocal clarity and brightness.";
            AddLog(msg, LogLevel::Info);
            break;
        }
        case AudioPreset::Rock:
        {
            targetGains = {5.0f, 4.0f, 3.0f, 1.0f, -1.0f, -1.0f, 1.5f, 3.0f, 4.5f, 5.0f};
            std::string msg_rock = "[Preset Applied] Rock preset activated: Emphasizing bass and treble for a more aggressive sound.";
            AddLog(msg_rock, LogLevel::Info);
            break;
        }
        case AudioPreset::EDM_Dance:
        {
            targetGains = {6.5f, 5.5f, 4.0f, 1.5f, 0.0f, 0.0f, 2.0f, 3.5f, 5.0f, 6.0f};
            std::string msg_edm = "[Preset Applied] EDM/Dance preset activated: Supercharging bass and adding sparkle to highs for club vibes.";
            AddLog(msg_edm, LogLevel::Info);
            break;
         }   
        case AudioPreset::Classical:
        {
            targetGains = {-2.0f, -1.0f, 1.0f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 3.0f, 2.0f};
            std::string msg_classical = "[Preset Applied] Classical preset activated: Enhancing clarity and presence while maintaining warmth.";
            AddLog(msg_classical, LogLevel::Info);
            break;
        }    
        case AudioPreset::Flat:
        default:
        {
            targetGains = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
            std::string msg_flat = "[Preset Applied] Flat preset activated: Neutral frequency response for accurate audio reproduction.";
            AddLog(msg_flat, LogLevel::Info);
            break;
        }    
    }

    // Các biến trạng thái mục tiêu của node hiệu ứng bổ trợ
    bool target_crystalizer_enabled = false;
    float target_crystalizer_i = 2.0f;
    bool target_stereo_enabled = false;
    float target_stereo_m = 2.5f;
    bool target_comp_enabled = false;
    float target_comp_threshold = -12.0f;
    float target_comp_ratio = 2.0f;

    // =========================================================================
    // TOÁN THUẬT AI: ĐIỀU CHỈNH CHI TIẾT THEO NGỮ CẢNH DỮ LIỆU
    // =========================================================================

    // Kịch bản A: Nhận biết Codec và độ nén dữ liệu (Phục hồi âm thanh)
    if (codec_name == "mp3" || codec_name == "aac" || (bitrate_kbps > 0.0 && bitrate_kbps < 192.0)) {
        targetGains[0] += 2.5f; // Bù dải Sub-bass bị mất mát khi nén nén lossy
        targetGains[9] += 3.5f; // Kích dải Treble cao bị lẹm tần số (High-shelf cut)
        target_crystalizer_enabled = true;
        target_crystalizer_i = 3.8f; // Tăng cường độ tinh khiết âm thanh
    } else if (codec_name == "flac" || codec_name == "wav" || sample_rate > 48000) {
        // Audio chất lượng cao / Studio: Giữ nguyên âm mộc gốc, tắt các hiệu ứng can thiệp giả tạo
        target_crystalizer_enabled = false;
    }

    // Kịch bản B: Phân tách hành vi giữa Xem Phim (Có Luồng Video) và Nghe Nhạc Mộc
    if (!is_audio_only) {
        // ĐANG XEM PHIM/VIDEO: Đẩy dải trung âm để làm nổi bật giọng thoại (Dialogue Boost)
        targetGains[4] += 2.0f; // Tần số ~400Hz làm ấm giọng nói
        targetGains[5] += 3.0f; // Tần số ~1kHz làm rõ lời thoại, không bị tiếng bom nổ/tiếng động nền át
        
        // Giả lập âm thanh vòm rộng nếu đang dùng thiết bị 2 kênh (Tai nghe/Loa Stereo)
        if (channel_count == 2) {
            target_stereo_enabled = true;
            target_stereo_m = 3.5f; // Không gian rộng hơn dành cho phim ảnh
        }
    } else {
        // NGHE NHẠC THUẦN TÚY: Chỉ áp dụng Stereo Extender nếu nghe EDM
        if (channel_count == 2 && m_currentPreset == AudioPreset::EDM_Dance) {
            target_stereo_enabled = true;
            target_stereo_m = 2.5f;
        }
    }

    // Kịch bản C: Loudness Contour (Bù tai người khi Volume nhỏ) & Động học Compressor
    if (current_volume < 30.0) {
        targetGains[0] += 4.0f; targetGains[1] += 3.0f; // Kích Bass khi nghe nhỏ
        targetGains[8] += 2.0f; targetGains[9] += 3.0f; // Kích Treble
        target_comp_enabled = true;
        target_comp_threshold = -22.0f; // Thu hẹp dải động để nghe rõ chi tiết nhỏ ban đêm
        target_comp_ratio = 1.8f;
    } else if (current_volume > 85.0) {
        // Bảo vệ màng loa / Chống vỡ tiếng (Clipping) khi âm lượng quá lớn
        for (int i = 0; i < 10; ++i) {
            if (targetGains[i] > 1.0f) targetGains[i] *= 0.4f; 
        }
        target_comp_enabled = true;
        target_comp_threshold = -5.0f;
        target_comp_ratio = 4.0f; // Ép đỉnh âm thanh lớn xuống cứng rắn hơn
    }

    // Kịch bản D: Tự động bù trừ chói tai khi người dùng tua nhanh tốc độ phát (Speed Compensation)
    if (current_speed > 1.25) {
        // Tua nhanh làm tần số giọng nói bị đẩy cao lên dải chói tai (Pitch-shift giả lập). Hạ bớt Treble.
        targetGains[7] -= 1.5f;
        targetGains[8] -= 2.5f;
        targetGains[9] -= 3.5f;
    }

    // =========================================================================
    // NỘI SUY TUYẾN TÍNH (LERP) ĐỂ CHUYỂN ÂM MƯỢT MÀ VÀ KIỂM TRA ĐỒNG BỘ
    // =========================================================================
    bool need_sync_structure = false; 
    bool parameter_changed = false;   
    
    const float alpha = 0.20f;  // Hệ số vuốt mịn (20% mỗi chu kỳ quét)
    const float epsilon = 0.05f;

    // 1. Áp dụng Lerp cho 10 băng tần EQ từ cấu trúc lưu trữ nội bộ m_filters
    for (int i = 0; i < 10; ++i) {
        std::string band_id = "eq_b" + std::to_string(i);
        auto* filter = FindFilter(band_id);
        if (filter && !filter->isBypassManagement) {
            if (!filter->enabled) {
                filter->enabled = true;
                need_sync_structure = true;
            }
            
            float current_g = filter->params["g"].current;
            float dest_g = targetGains[i];
            
            if (std::abs(current_g - dest_g) > epsilon) {
                filter->params["g"].current = current_g + alpha * (dest_g - current_g);
                parameter_changed = true;
            } else {
                filter->params["g"].current = dest_g;
            }
        }
    }

    // Lambda Helper xử lý Lerp mượt cho các Node bổ trợ tự động
    auto processSmoothFilter = [&](const std::string& id, bool target_state, const std::string& param_key, float target_val) {
        auto* filter = FindFilter(id);
        if (!filter || filter->isBypassManagement) return;

        if (filter->enabled != target_state) {
            filter->enabled = target_state;
            need_sync_structure = true;
        }

        if (filter->enabled && filter->params.count(param_key)) {
            float current_val = filter->params[param_key].current;
            if (std::abs(current_val - target_val) > epsilon) {
                filter->params[param_key].current = current_val + alpha * (target_val - current_val);
                parameter_changed = true;
            } else {
                filter->params[param_key].current = target_val;
            }
        }
    };

    // 2. Thực thi mượt cho Crystalizer & Stereo Extender
    processSmoothFilter("f_crystalizer", target_crystalizer_enabled, "i", target_crystalizer_i);
    processSmoothFilter("f_stereo", target_stereo_enabled, "m", target_stereo_m);

    // 3. Thực thi mượt cho Compressor
    auto* comp = FindFilter("f_comp");
    if (comp && !comp->isBypassManagement) {
        if (comp->enabled != target_comp_enabled) {
            comp->enabled = target_comp_enabled;
            need_sync_structure = true;
        }
        if (comp->enabled) {
            float curr_th = comp->params["threshold"].current;
            float curr_rt = comp->params["ratio"].current;

            if (std::abs(curr_th - target_comp_threshold) > epsilon) {
                comp->params["threshold"].current = curr_th + alpha * (target_comp_threshold - curr_th);
                parameter_changed = true;
            }
            if (std::abs(curr_rt - target_comp_ratio) > epsilon) {
                comp->params["ratio"].current = curr_rt + alpha * (target_comp_ratio - curr_rt);
                parameter_changed = true;
            }
        }
    }

    // =========================================================================
    // GIAO TIẾP IPC XUỐNG MPV CORE: CHỐNG SPAM LỆNH ĐỒNG BỘ VÔ ÍCH
    // =========================================================================
    if (need_sync_structure) {
        // Chỉ chạy khi có sự thay đổi về mặt kiến trúc chuỗi (Bật/Tắt hẳn một Node bộ lọc)
        EvaluateSystemSafety();
        SyncAll(); 
    } 
    else if (parameter_changed) {
        // Hiệu năng cao: Giữ nguyên chuỗi bộ lọc 'af' hiện tại, chỉ đẩy các vi thông số thay đổi qua af-command
        EvaluateSystemSafety();

        for (const auto& filter : m_filters) {
            if (filter.enabled && !filter.isBypassManagement) {
                for (const auto& [key, p] : filter.params) {
                    // Biến tạm lưu giá trị an toàn trước khi đẩy qua IPC
                    float value_to_send = p.current;

                    // XỬ LÝ ĐẶC BIỆT CHO COMPRESSOR THRESHOLD TRONG LUỒNG AI
                    if (filter.name == "acompressor" && key == "threshold") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                        if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                        if (value_to_send > 1.0f) value_to_send = 1.0f;
                    }

                    std::ostringstream ss;
                    if (filter.name == "acompressor" && key == "threshold") {
                        ss << std::fixed << std::setprecision(5) << value_to_send;
                    } else {
                        ss << std::fixed << std::setprecision(2) << value_to_send;
                    }
                    std::string val_str = ss.str();

                    // Sử dụng hàm chuẩn cấu trúc mảng char* của mpv_command giống header của bạn
                    const char* cmd[] = {"af-command", filter.id.c_str(), key.c_str(), val_str.c_str(), NULL};
                    mpv_command(mpv, cmd);
                }
            }
        }
    }
    // TRẠNG THÁI TĨNH: Nếu tất cả giá trị thực đạt trạng thái cân bằng mục tiêu -> Khóa hoàn toàn, không gọi gì xuống MPV.
}

void AudioFilterManager::SetCurrentPreset(AudioPreset preset) {
    if (m_currentPreset != preset) {
        m_currentPreset = preset;
        if (m_autoMode) {
            UpdateAdaptiveFilters(); 
        }
    }
}

void AudioFilterManager::SetAdaptiveMode(bool enabled, AudioPreset preset) {
    m_autoMode = enabled;
    m_currentPreset = preset;
    if (enabled) {
        UpdateAdaptiveFilters();
    } else {
        // CHỈ trả các bộ lọc AI quản lý về trạng thái phẳng (Flat), giữ nguyên các bộ lọc cá nhân khác
        for (int i = 0; i < 10; ++i) {
            if (auto* filter = FindFilter("eq_b" + std::to_string(i))) {
                if (!filter->isBypassManagement) filter->params["g"].current = 0.0f;
            }
        }
        if (auto* c = FindFilter("f_crystalizer")) if (!c->isBypassManagement) c->enabled = false;
        if (auto* s = FindFilter("f_stereo")) if (!s->isBypassManagement) s->enabled = false;
        if (auto* cp = FindFilter("f_comp")) if (!cp->isBypassManagement) cp->enabled = false;
        
        EvaluateSystemSafety();
        SyncAll();
    }
}

void AudioFilterManager::AddLog(const std::string& message, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_logMutex);

    // Lấy thời gian hiện tại định dạng hh:mm:ss
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time_t_now);
#else
    localtime_r(&time_t_now, &buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&buf, "%H:%M:%S");

    // Tạo LogEntry mới
    LogEntry entry{ ss.str(), message, level };
    m_logs.push_back(entry);

    // Nếu vượt quá giới hạn tối đa, xóa log cũ nhất (FIFO)
    if (m_logs.size() > MAX_LOG_SIZE) {
        m_logs.erase(m_logs.begin());
    }

    // Vẫn in ra Console/Output để Dev khi cần debug
    std::cout << "[" << entry.timestamp << "] " << message << "\n";
}

std::vector<LogEntry> AudioFilterManager::GetLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logs; // Trả về một bản sao an toàn cho luồng UI render
}

void AudioFilterManager::ClearLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.clear();
}