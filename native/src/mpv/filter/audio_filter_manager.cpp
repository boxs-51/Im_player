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

    // Khởi tạo trạng thái biến quản lý riêng biệt ngoại vi
    m_enableOuterStabilizer = true;
    m_enableOuterBooster = true;

    // =========================================================================
    // 1. Nhóm Equalizer nâng cấp (12-Band ISO Extended Graphic EQ)
    // =========================================================================
    const std::vector<std::pair<std::string, std::string>> eqBands = {
        {"eq_b0", "20"},    {"eq_b1", "31"},    {"eq_b2", "63"},    {"eq_b3", "125"},
        {"eq_b4", "250"},   {"eq_b5", "500"},   {"eq_b6", "1000"},  {"eq_b7", "1500"},
        {"eq_b8", "3000"},  {"eq_b9", "4000"},  {"eq_b10", "8000"}, {"eq_b11", "16000"}
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
    RegisterParam("f_volume", "volume", -60.0f, 30.0f, 0.0f);

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
    // 3. Hệ thống Tăng cường & Ổn định âm thanh ngoại vi
    // =========================================================================
    AddFilter("f_vol_booster", "volume", "outer_gain_node");
    RegisterParam("f_vol_booster", "volume", 0.0f, 1.5f, 1.0f); 

    AddFilter("f_out_compressor", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_compressor", "threshold", -30.0f, 0.0f, -18.0f); 
    RegisterParam("f_out_compressor", "ratio",     1.0f, 20.0f, 4.0f);    
    RegisterParam("f_out_compressor", "attack",    0.01f, 100.0f, 5.0f);   
    RegisterParam("f_out_compressor", "release",   10.0f, 1000.0f, 50.0f); 
    RegisterParam("f_out_compressor", "makeup",    1.0f, 10.0f, 2.0f);     

    AddFilter("f_out_limiter", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_limiter", "threshold", -30.0f, 0.0f, -1.0f);   
    RegisterParam("f_out_limiter", "ratio",     1.0f, 20.0f, 20.0f);  
    RegisterParam("f_out_limiter", "attack",    0.01f, 100.0f, 1.0f);   
    RegisterParam("f_out_limiter", "release",   10.0f, 1000.0f, 100.0f);
    RegisterParam("f_out_limiter", "makeup",    1.0f, 10.0f, 1.0f);     
}

void AudioFilterManager::AddFilter(const std::string& id, const std::string& name, const std::string& group) {
    m_filters.push_back({id, name, false, {}, group, false}); 
    m_filterIndex[id] = m_filters.size() - 1; 
}

void AudioFilterManager::ToggleFilter(const std::string& id, bool enabled) {
auto* f = FindFilter(id);
    if (!f || f->enabled == enabled) return;


    // 1. THIẾT LẬP ĐỒNG BỘ NGƯỢC CHO CÁC CỜ TRẠNG THÁI NGOẠI VI
    if (id == "f_out_compressor" || id == "f_out_limiter") {
        auto* comp = FindFilter("f_out_compressor");
        auto* lim = FindFilter("f_out_limiter");

        if (id == "f_out_compressor") comp->enabled = enabled;
        if (id == "f_out_limiter") lim->enabled = enabled;

        m_enableOuterStabilizer = comp->enabled && lim->enabled;
        
        std::string msg = "[Sync] Outer Stabilizer State updated via filter action -> " + std::string(enabled ? "ON" : "OFF");
        AddLog(msg, LogLevel::Info);
    } 
    else if (id == "f_vol_booster") {
        m_enableOuterBooster = enabled;
        f->enabled = enabled;
        
        std::string msg = "[Sync] Outer Booster State updated via filter action -> " + std::string(enabled ? "ON" : "OFF");
        AddLog(msg, LogLevel::Info);
    } 
    else {
        // Áp dụng cho các filter nội bộ thông thường (EQ, Crystalizer, Stereo, Comp, Reverb...)
        f->enabled = enabled;
    }

    // 2. QUẢN LÝ XUNG ĐỘT NHÓM (Chỉ áp dụng cho filter nội bộ, bỏ qua cụm ngoại vi)
    if (enabled && !m_globalBypass && !f->isBypassManagement) {
        if (!f->group.empty() && f->group != "equalizer_group" && 
            f->group != "outer_stabilizer" && f->group != "outer_gain_node") {
            
            for (auto& other : m_filters) {
                if (other.id != id && other.group == f->group && other.enabled && !other.isBypassManagement) {
                    other.enabled = false;
                    std::string msg = "[Conflict Managed] Auto-disabled " + other.id + " due to group conflict: " + f->group;
                    AddLog(msg, LogLevel::Warning);
                }
            }
        }
    }

    // 3. TÍNH TOÁN LẠI HỆ THỐNG VÀ ĐẨY LỆNH XUỐNG MPV
    EvaluateSystemSafety();
    SyncAll(); 
}
void AudioFilterManager::SetOuterStabilizerEnabled(bool enabled) {
    m_enableOuterStabilizer = enabled;
    
    // Đồng bộ trực tiếp thuộc tính của filter core để UI đọc đúng trạng thái bool
    if (auto* comp = FindFilter("f_out_compressor")) comp->enabled = enabled;
    if (auto* lim = FindFilter("f_out_limiter")) lim->enabled = enabled;

    std::string msg = "[Stabilizer] Outer Safety System changed to -> " + std::string(enabled ? "ON" : "OFF");
    AddLog(msg, LogLevel::Info);

    EvaluateSystemSafety();
    SyncAll();
}
void AudioFilterManager::SetOuterBoosterEnabled(bool enabled) {
    m_enableOuterBooster = enabled;
    if (auto* boost = FindFilter("f_vol_booster")) boost->enabled = enabled;

    std::string msg = "[Booster] Outer Volume Booster changed to -> " + std::string(enabled ? "ON" : "OFF");
    AddLog(msg, LogLevel::Info);

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

    std::string msg = "[Bypass System] GLOBAL Bypass Manager set -> " + std::string(bypassState ? "ENABLED" : "DISABLED");
    AddLog(msg, LogLevel::Info);

    if (m_globalBypass) {
        // Tắt cưỡng bách chế độ AI thích ứng để giải phóng quyền kiểm soát
        m_autoMode = false; 
        SetAdaptiveMode(m_autoMode, m_currentPreset);
    }

    EvaluateSystemSafety();
    SyncAll();
}

void AudioFilterManager::EvaluateSystemSafety() {
    if (m_globalBypass) return;

    static bool isEvaluating = false;
    if (isEvaluating) return;

    struct ScopeGuard {
        bool& flag;
        ScopeGuard(bool& f) : flag(f) { flag = true; }
        ~ScopeGuard() { flag = false; }
    };
    ScopeGuard guard(isEvaluating);

    float totalGainAccumulation = 0.0f;

    // 1. Quét toàn diện 12 dải EQ + Bass + Treble
    for (const auto& f : m_filters) {
        if (!f.enabled) continue; 

        if ((f.name == "equalizer" || f.name == "bass" || f.name == "treble") && f.params.count("g")) {
            float g_val = f.params.at("g").current;
            if (g_val > 0.0f) {
                totalGainAccumulation += g_val; 
            }
        }
    }

    // 2. Tính thêm năng lượng từ mạch kích âm ngoại vi (Booster)
    auto* booster_node = FindFilter("f_vol_booster");
    if (booster_node && m_enableOuterBooster && booster_node->enabled) {
        if (booster_node->params.count("volume")) {
            float boost_val = booster_node->params.at("volume").current;
            if (boost_val > 1.0f) {
                float boost_in_db = 20.0f * std::log10(boost_val);
                totalGainAccumulation += boost_in_db;
            }
        }
    }

    // 3. ĐIỀU TIẾT MASTER NODE VOLUME CHỈ BẰNG CÁCH SET VALUE
    auto* vol_node = FindFilter("f_volume");
    if (vol_node) {
        const float GAIN_THRESHOLD = 18.0f; 
        float userTarget = vol_node->params["volume"].user_target;
        if (totalGainAccumulation > GAIN_THRESHOLD) {
            // Thuật toán giảm suy hao động (Dynamic Attenuation)
            float safetyReduction = -(totalGainAccumulation - GAIN_THRESHOLD) * 0.6f;
            // Ép dải bảo vệ: Giảm âm lượng bắt đầu từ mốc userTarget xuống tối đa -60dB
            float targetVol = std::clamp(userTarget + safetyReduction, -60.0f, userTarget);
            
            if (std::abs(vol_node->params["volume"].current - targetVol) > 0.05f) {
                vol_node->params["volume"].current = targetVol;
                
                std::string msg = "[SAFETY ENGINE] Danger! Accumulated Gain reached " + 
                                  std::to_string(static_cast<int>(totalGainAccumulation)) + 
                                  "dB. Limiting Master Node to: " + std::to_string(targetVol) + "dB";
                AddLog(msg, LogLevel::Error);
                
                // KHÔNG gọi mpv_command nữa, để DispatchParametersToMPV tự lo
            }
        } else {
            // THUẬT TOÁN PHỤC HỒI TUYẾN TÍNH 
            // Nếu volume hiện tại đang nhỏ hơn mức mong muốn của User (do hệ thống hạ lúc trước)
            if (vol_node->params["volume"].current < userTarget) {
                float current_vol = vol_node->params["volume"].current;
                
                // Bộ hoãn chống dội: Đợi AI uốn lượn hạ dải EQ xuống hẳn
                if (totalGainAccumulation > (GAIN_THRESHOLD - 3.0f)) {
                    return;
                }

                float recoveryVol = current_vol + 0.2f; 
                if (recoveryVol >= userTarget - 0.05f || totalGainAccumulation <= 6.0f) {
                    recoveryVol = userTarget;
                }

                vol_node->params["volume"].current = recoveryVol; 
                
                if (recoveryVol == userTarget) {
                    AddLog("[SAFETY ENGINE] System stabilized. Master Node returned to normal (0dB).", LogLevel::Info);
                }
                
                // KHÔNG gọi mpv_command nữa, để DispatchParametersToMPV tự lo
            }
        }
    }
}

void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def) {
    if (auto* f = FindFilter(id)) {
        f->params[key] = {def, min, max, def, def}; 
    }
}

void AudioFilterManager::ResetFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        for (auto& [key, p] : f->params) {
            p.current = p.def;
            p.user_target = p.def;
        }
        if (f->enabled) SyncAll();
    }
}

void AudioFilterManager::SaveToFile() {
    if (path.empty()) return;
    std::ofstream f(path);
    if (!f.is_open()) return;

    // 1. Lưu các biến trạng thái hệ thống lõi và cờ quản lý riêng biệt
    f << "[SystemMetadata]\n";
    f << "auto_mode:" << (m_autoMode ? "1" : "0") << "\n";
    f << "global_bypass:" << (m_globalBypass ? "1" : "0") << "\n";
    f << "channel_mode:" << m_channelMode << "\n";
    f << "current_preset:" << static_cast<int>(m_currentPreset) << "\n";
    f << "outer_stabilizer_manager:" << (m_enableOuterStabilizer ? "1" : "0") << "\n";
    f << "outer_booster_manager:" << (m_enableOuterBooster ? "1" : "0") << "\n";
    f << "\n";

    // 2. Lưu trạng thái các bộ lọc và các tham số thuần túy của người dùng
    for (const auto& filter : m_filters) {
        f << "[Filter]:" << filter.id << "|" << (filter.enabled ? "1" : "0") << "|" << (filter.isBypassManagement ? "1" : "0") << "\n";
        for (const auto& [key, p] : filter.params) {
            // NẾU ĐANG BẬT AUTO MODE: Ưu tiên ghi giá trị gốc user_target thay vì ghi giá trị biến thiên do AI điều chỉnh
            float value_to_save = m_autoMode ? p.user_target : p.current;
            f << key << ":" << value_to_save << "\n";
        }
    }
    AddLog("[Config Storage] Successfully saved configuration (AI dynamic data omitted).", LogLevel::Info);
}

void AudioFilterManager::ResetAllToDefaults() {
    for (auto& filter : m_filters) {
        filter.enabled = false;
        for (auto& [key, p] : filter.params) {
            p.current = p.def;
            p.user_target = p.def;
        }
    }
    m_channelMode = "stereo";
    m_currentPreset = AudioPreset::Flat;
    m_autoMode = false;
    m_globalBypass = false;
    m_enableOuterStabilizer = true;
    m_enableOuterBooster = true;
    SyncAll();
}

void AudioFilterManager::LoadFromFile() {
    if (path.empty()) { ResetAllToDefaults(); return; }
    std::ifstream f(path);
    if (!f.is_open()) { ResetAllToDefaults(); return; }

    std::string line;
    std::string current_id = "";
    bool parse_success = false;

    bool target_auto_mode = false;
    AudioPreset target_preset = AudioPreset::Flat;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        // Đọc Metadata hệ thống
        if (line.rfind("auto_mode:", 0) == 0) { target_auto_mode = (line.substr(10) == "1"); continue; }
        if (line.rfind("global_bypass:", 0) == 0) { m_globalBypass = (line.substr(14) == "1"); continue; }
        if (line.rfind("channel_mode:", 0) == 0) { m_channelMode = line.substr(13); continue; }
        if (line.rfind("current_preset:", 0) == 0) { target_preset = static_cast<AudioPreset>(std::stoi(line.substr(15))); continue; }
        if (line.rfind("outer_stabilizer_manager:", 0) == 0) { m_enableOuterStabilizer = (line.substr(25) == "1"); continue; }
        if (line.rfind("outer_booster_manager:", 0) == 0) { m_enableOuterBooster = (line.substr(22) == "1"); continue; }

        // Đọc cấu hình Filter
        if (line.rfind("[Filter]:", 0) == 0) {
            size_t d1 = line.find('|');
            size_t d2 = line.find('|', d1 + 1);
            if (d1 != std::string::npos) {
                current_id = line.substr(9, d1 - 9);
                std::string enabled_str = (d2 == std::string::npos) ? line.substr(d1 + 1) : line.substr(d1 + 1, d2 - d1 - 1);
                bool enabled = (enabled_str == "1");
                bool bypass = (d2 != std::string::npos && line.substr(d2 + 1) == "1");

                if (auto* filter = FindFilter(current_id)) {
                    filter->enabled = enabled;
                    filter->isBypassManagement = bypass;
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
                            if (current_id == "f_vol_booster" && key == "volume" && val > 1.5f) {
                                val /= 100.0f;
                            }
                            param.user_target = std::clamp(val, param.min, param.max);
                            param.current = param.user_target; // Đồng bộ ban đầu
                        }
                    }
                } catch (...) { continue; }
            }
        }
    }
    if (!parse_success) {
        ResetAllToDefaults();
    } else {
        m_currentPreset = target_preset;
        SetAdaptiveMode(target_auto_mode, m_currentPreset);
    }
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

            if (f.id == "f_vol_booster" || 
                f.id == "f_out_compressor" || f.id == "f_out_limiter")
                continue;

            std::string init_str = "";
            if (f.name == "equalizer") {
                std::string freq = "1000";
                if (f.id == "eq_b0") freq = "20";
                else if (f.id == "eq_b1") freq = "31";
                else if (f.id == "eq_b2") freq = "63";
                else if (f.id == "eq_b3") freq = "125";
                else if (f.id == "eq_b4") freq = "250";
                else if (f.id == "eq_b5") freq = "500";
                else if (f.id == "eq_b6") freq = "1000";
                else if (f.id == "eq_b7") freq = "1500";
                else if (f.id == "eq_b8") freq = "3000";
                else if (f.id == "eq_b9") freq = "4000";
                else if (f.id == "eq_b10") freq = "8000";
                else if (f.id == "eq_b11") freq = "16000";

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

    // Đưa bộ kích âm Booster vào cuối chuỗi
    auto* vol_boost = FindFilter("f_vol_booster");
    if (vol_boost && m_enableOuterBooster) {
        vol_boost->enabled = true;
        if (!full_af.empty()) full_af += ",";
        full_af += vol_boost->GetInitString();
    }

    // Đưa bộ ổn định bảo vệ tầng cuối cùng vào chuỗi xử lý
    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim = FindFilter("f_out_limiter");
    if(m_enableOuterStabilizer) {
        out_comp->enabled = true;
        out_lim->enabled = true;
    }
    if (out_comp && out_comp->enabled) {
        if (!full_af.empty()) full_af += ",";
        full_af += out_comp->GetInitString();
    }

    if (out_lim && out_lim->enabled) {
        if (!full_af.empty()) full_af += ",";
        full_af += out_lim->GetInitString();
    }

    if (!full_af.empty()) full_af += ",";
    full_af += "@ebur_measurer:lavfi=[ebur128=metadata=1:peak=true]";
    
    mpv_set_property_string(mpv, "af", full_af.c_str());
    
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    if (auto* f = FindFilter(id)) {
        auto it = f->params.find(key);
        if (it != f->params.end()) { 
            auto& param = it->second;

            if (id == "f_vol_booster" && key == "volume") {
                if (value > 1.5f) value /= 100.0f;
            }

            value = std::clamp(value, param.min, param.max);
            
            // Ghi nhớ thiết lập gốc của con người tương tác từ UI
            param.user_target = value;

            if (param.current == value) return; 
            param.current = value;
            
            if (key == "g" || key == "volume") EvaluateSystemSafety();

            if (f->enabled && mpv) {
                float value_to_send = param.current;

                // VÁ LỖI ÉP DẢI COMPRESSOR THRESHOLD (CẢ CHO BỘ NỘI BỘ VÀ NGOẠI VI)
                if (f->name == "acompressor" && key == "threshold") {
                    value_to_send = std::pow(10.0f, param.current / 20.0f);
                    if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                    if (value_to_send > 1.0f) value_to_send = 1.0f;
                }

                if (f->id == "f_volume" && key == "volume") {
                    value_to_send = std::pow(10.0f, param.current / 20.0f);
                }

                std::ostringstream ss;
                if (f->name == "acompressor" && key == "threshold") {
                    ss << std::fixed << std::setprecision(5) << value_to_send; // Gửi giá trị tuyến tính [0.00097... -> 1]
                } else if (f->name == "acompressor" && key == "makeup") {
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
                param.user_target = clamped; // Ghi nhận từ luồng UI tương tác loạt
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

void AudioFilterManager::SetAllFiltersState(bool enabled) {
    bool changed = false;
    for (auto& f : m_filters) {
        // Chỉ bật tắt các bộ lọc thông thường, CHỪA CÁC NODE NGOẠI VI VÀ MASTER VOLUME RA

            if (f.enabled != enabled) {
                f.enabled = enabled;
                changed = true;
            }
        //}
    }
    
    // Bật/Tắt chế độ AI thích ứng theo lệnh tổng
    SetAdaptiveMode(enabled, m_currentPreset);

    // KHÔNG ép tắt Stabilizer và Booster ở đây nữa để giữ trạng thái độc lập của mạch ngoại vi
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

AudioFilterManager::AudioContext AudioFilterManager::ExtractCurrentContext() {
    AudioContext ctx;
    ctx.volume = (double)g_playbackStatus.volume;
    ctx.speed  = g_playbackStatus.speed;
    ctx.sample_rate   = (int64_t)g_videoInfo.g_audioarams.asamplerate;
    ctx.channel_count = (int64_t)g_videoInfo.g_audioarams.channel_count;
    ctx.bitrate_kbps   = (double)(g_videoInfo.abitrate / 1000);
    ctx.codec = g_videoInfo.acodec;
    ctx.is_audio_only = g_videoInfo.width == 0 && g_videoInfo.height == 0;

    mpv_node node;
    // Lấy metadata thời gian thực từ bộ lọc có nhãn @ebur_measurer
    if (mpv && mpv_get_property(mpv, "af-metadata/ebur_measurer", MPV_FORMAT_NODE, &node) >= 0) {
        if (node.format == MPV_FORMAT_NODE_MAP) {
            mpv_node_list* list = node.u.list;
            
            // Hàm chuyển đổi an toàn từ chuỗi sang Double hệ dB
            auto parse_db = [](const char* str) -> double {
                if (!str || std::string(str) == "-inf") return -99.0;
                try { return std::stod(str); } catch (...) { return -99.0; }
            };

            for (int i = 0; i < list->num; i++) {
                std::string key = list->keys[i];
                if (list->values[i].format != MPV_FORMAT_STRING) continue;
                const char* val_str = list->values[i].u.string;

                if (key == "lavfi.r128.M") {
                    ctx.loudness_momentary = parse_db(val_str);
                } 
                else if (key == "lavfi.r128.S") {
                    ctx.loudness_shortterm = parse_db(val_str);
                } 
                else if (key == "lavfi.r128.I") {
                    ctx.loudness_integrated = parse_db(val_str);
                } 
                else if (key == "lavfi.r128.LRA") {
                    try { ctx.loudness_range = std::stod(val_str); } catch (...) {}
                } 
                else if (key == "lavfi.r128.LRA.low") {
                    ctx.loudness_lra_low = parse_db(val_str);
                }
                else if (key == "lavfi.r128.LRA.high") {
                    ctx.loudness_lra_high = parse_db(val_str);
                }
                else if (key == "lavfi.r128.true_peak") {
                    double db = parse_db(val_str);
                    ctx.true_peak = (db <= -99.0) ? 0.0 : std::pow(10.0, db / 20.0);
                } 
                else if (key == "lavfi.r128.true_peaks_ch0") {
                    double db = parse_db(val_str);
                    ctx.true_peak_ch0 = (db <= -99.0) ? 0.0 : std::pow(10.0, db / 20.0);
                } 
                else if (key == "lavfi.r128.true_peaks_ch1") {
                    double db = parse_db(val_str);
                    ctx.true_peak_ch1 = (db <= -99.0) ? 0.0 : std::pow(10.0, db / 20.0);
                }

            }
        }
        mpv_free_node_contents(&node);
    }


    return ctx;
}

bool AudioFilterManager::CheckEnvironmentHysteresis(const AudioContext& ctx) {
    static double last_volume = -999.0;
    static double last_bitrate = -999.0;
    static double last_speed = -1.0;
    static int64_t last_channels = -1;
    static AudioPreset last_preset = static_cast<AudioPreset>(-1);

    bool changed = (std::abs(ctx.volume - last_volume) > 1.5) || 
                   (std::abs(ctx.bitrate_kbps - last_bitrate) > 10.0) ||
                   (std::abs(ctx.speed - last_speed) > 0.05) ||
                   (ctx.channel_count != last_channels) ||
                   (m_currentPreset != last_preset);

    if (changed) {
        last_volume = ctx.volume;
        last_bitrate = ctx.bitrate_kbps;
        last_speed = ctx.speed;
        last_channels = ctx.channel_count;
        last_preset = m_currentPreset;
    }
    return changed;
}

AdaptiveTargets AudioFilterManager::AnalyzeContextAndCalculateTargets(const AudioContext& ctx) 
{   
    AdaptiveTargets targets;
    // 1. Khởi tạo cấu hình Preset cơ bản
    switch (m_currentPreset) {
        case AudioPreset::Pop:          targets.eq_gains = { -2.0f, -1.0f, 0.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f }; break;
        case AudioPreset::Rock:         targets.eq_gains = { 5.0f, 6.0f, 5.0f, 3.0f, -1.0f, -3.0f, -2.0f, 0.0f, 2.0f, 4.0f, 5.0f, 4.0f }; break;
        case AudioPreset::EDM_Dance:    targets.eq_gains = { 6.0f, 8.0f, 7.0f, 4.0f, 1.0f, -1.0f, 0.0f, 2.0f, 4.0f, 5.0f, 6.0f, 5.0f }; break;
        case AudioPreset::Classical:    targets.eq_gains = { -3.0f, -2.0f, -1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f }; break;
        case AudioPreset::Acoustic:     targets.eq_gains = { 2.0f, 2.0f, 3.0f, 1.0f, 0.0f, 2.0f, 3.0f, 4.0f, 4.0f, 3.0f, 3.0f, 2.0f }; break;
        case AudioPreset::Gaming_FPS:   targets.eq_gains = { -8.0f, -6.0f, -4.0f, -2.0f, 1.0f, 3.0f, 4.0f, 7.0f, 6.0f, 5.0f, 2.0f, 0.0f }; break;
        case AudioPreset::Movie_Cinema: targets.eq_gains = { 7.0f, 6.0f, 4.0f, 1.0f, -1.0f, 2.0f, 4.0f, 5.0f, 3.0f, 2.0f, 3.0f, 4.0f }; break;
        case AudioPreset::Deep_Bass:    targets.eq_gains = { 9.0f, 9.0f, 8.0f, 5.0f, 2.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 2.0f, 2.0f }; break;
        case AudioPreset::Flat:
        default:                        targets.eq_gains = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; break;
    }

    // --- BIẾN ĐIỀU PHỐI AI TRUNG GIAN CHO CÁC BỘ LỌC NGOẠI VI ---
    // Ngăn chặn việc ghi đè trực tiếp lên cấu trúc .current của filter làm hỏng mạch mượt tiếng
    bool is_transient_shock = false;
    bool is_night_mode_active = false;
    float recommended_vocal_boost = 0.0f;

    // 2. Phân tích đặc tính động học (EBUR128 Metrics)
    double transient_delta = ctx.loudness_momentary - ctx.loudness_shortterm;
    if (transient_delta > 7.0) {
        is_transient_shock = true; // Phát hiện tiếng súng, tiếng nổ đột ngột
    }

    // Thích ứng dải động rộng (LRA)
    if (ctx.loudness_range > 12.0) {
        targets.comp_enabled = true;
        targets.comp_th = -24.0f; 
        targets.comp_rt = 3.0f;   
        recommended_vocal_boost += 2.0f; // Đề xuất đẩy dải trung cho phim bom tấn
    } 
    else if (ctx.loudness_range > 0.1 && ctx.loudness_range < 4.0) {
        if (!targets.comp_enabled) {
            targets.comp_th = -10.0f;
            targets.comp_rt = 1.5f;
        }
    }

    // Quản lý năng lượng lớn tích lũy (Integrated Loudness)
    if (ctx.loudness_integrated > -13.0) {
        for (size_t i = 0; i < targets.eq_gains.size(); ++i) { // Thay bằng kích thước thực m_filters hoặc vector
            if (targets.eq_gains[i] > 0.0f) targets.eq_gains[i] *= 0.7f;
        }
    }

    // Kiểm tra vỡ đỉnh độc lập kênh trái / phải để tự vệ
    bool channel_clipping = (ctx.true_peak_ch0 > 0.98 || ctx.true_peak_ch1 > 0.98);

    // 3. Phân tích thuộc tính File & Môi trường nghe
    if (ctx.codec == "mp3" || ctx.codec == "aac" || (ctx.bitrate_kbps > 0.0 && ctx.bitrate_kbps < 192.0)) {
        targets.eq_gains[0] += 2.5f; targets.eq_gains[9] += 3.5f;
        targets.crystalizer_enabled = true; targets.crystalizer_i = 3.8f;
    }

    if (!ctx.is_audio_only) {
        targets.eq_gains[4] += 2.0f; targets.eq_gains[5] += 3.0f; 
        if (ctx.channel_count == 2) { targets.stereo_enabled = true; targets.stereo_m = 3.5f; }
    } else {
        if (ctx.channel_count == 2 && m_currentPreset == AudioPreset::EDM_Dance) { targets.stereo_enabled = true; targets.stereo_m = 2.5f; }
    }

    // Sửa lỗi vòng lặp cứng (12 -> targets.eq_gains.size() để quét hết 12-Band)
    if (ctx.volume < 30.0) {
        targets.eq_gains[0] += 4.0f; targets.eq_gains[9] += 3.0f; 
        targets.comp_enabled = true; targets.comp_th = -22.0f; targets.comp_rt = 1.8f;
    } else if (ctx.volume > 85.0) {
        for (size_t i = 0; i < targets.eq_gains.size(); ++i) { 
            if (targets.eq_gains[i] > 1.0f) targets.eq_gains[i] *= 0.4f;
        }
        targets.comp_enabled = true; targets.comp_th = -5.0f; targets.comp_rt = 4.0f;
    }

    if (ctx.speed > 1.25) {
        targets.eq_gains[7] -= 1.5f; targets.eq_gains[8] -= 2.5f; targets.eq_gains[9] -= 3.5f;
    }

    if (ctx.is_audio_only && m_currentPreset == AudioPreset::Classical) {
        targets.reverb_enabled = true; targets.reverb_preset = "large_hall";
    } else {
        targets.reverb_enabled = false;
    }

    // Phân tích trạng thái Night Mode độc lập
    if (ctx.loudness_lra_low < -36.0 && ctx.loudness_lra_low > -99.0) {
        is_night_mode_active = true;
        recommended_vocal_boost += 1.5f;
    }

    // Áp dụng tăng cường dải Trung (Vocal) một lần duy nhất dựa trên các phân tích phía trên
    if (recommended_vocal_boost > 0.0f) {
        targets.eq_gains[5] += std::min(2.0f, recommended_vocal_boost * 0.8f); // 500Hz
        targets.eq_gains[6] += recommended_vocal_boost;                       // 1000Hz (Vocal Core)
        targets.eq_gains[7] += std::min(2.0f, recommended_vocal_boost * 0.8f); // 1500Hz
    }

    // =========================================================================
    // ĐIỀU PHỐI HỆ THỐNG NGOẠI VI KHÔNG XUNG ĐỘT (SMOOTH REAL-TIME FEEDBACK)
    // =========================================================================
    
    // 1. Quản lý linh hoạt Bộ kích âm (Outer Booster)
    if (m_enableOuterBooster) {
        auto* vol_boost = FindFilter("f_vol_booster");
        if (vol_boost && vol_boost->enabled) {
            float active_boost = vol_boost->params["volume"].current;
            
            // Nếu vỡ đỉnh kênh đơn lẻ HOẶC vỡ đỉnh tổng quát -> Hạ nhiệt khẩn cấp
            if (channel_clipping || ctx.true_peak > 0.98) {
                targets.booster_volume = std::max(1.0f, active_boost - 0.06f);
            } 
            else if (ctx.loudness_shortterm < -35.0 && active_boost < vol_boost->params["volume"].user_target) {
                // Chỉ hồi phục khi môi trường thực sự yên tĩnh an toàn
                targets.booster_volume = std::min(vol_boost->params["volume"].user_target, active_boost + 0.01f);
            }
        }
    }

    // 2. Quản lý Bộ ổn định đa tầng (Outer Stabilizer - Compressor & Limiter)
    if (m_enableOuterStabilizer) {
        auto* out_comp = FindFilter("f_out_compressor");
        auto* out_lim  = FindFilter("f_out_limiter");

        if (out_comp && out_lim) {
            // Bước 1: Thu thập trạng thái hiện tại
            float next_comp_th = out_comp->params["threshold"].current;
            float next_comp_mk = out_comp->params["makeup"].current;
            float next_lim_th  = out_lim->params["threshold"].current;

            // Bước 2: Đánh giá mạch điều kiện theo ĐỘ ƯU TIÊN GIẢM DẦN

            // --- CẤP ĐỘ 1: BẢO VỆ KHẨN CẤP (Ập tới khi vỡ đỉnh sóng / Shock tín hiệu) ---
            if (is_transient_shock) {
                targets.out_comp_threshold = -28.0f; // Ép sâu khẩn cấp hãm tiếng súng/tiếng nổ
                targets.out_comp_makeup = 1.0f;   // Không kích nền âm thanh dội lại
            }
            else if (ctx.true_peak > 0.95) {
                targets.out_lim_threshold  = std::max(-3.0f, next_lim_th - 0.2f);   // Khóa trần trên chặn Clip số
                targets.out_comp_threshold = std::max(-28.0f, next_comp_th - 0.5f);
                targets.out_comp_makeup = std::max(1.0f, next_comp_mk - 0.1f);
            }
            // --- CẤP ĐỘ 2: THÍCH ỨNG TRẠNG THÁI MÔI TRƯỜNG (Chỉ chạy khi không bị đe dọa vỡ đỉnh) ---
            else {
                // Xử lý kịch bản Night Mode / Clear Voice
                if (is_night_mode_active) {
                    targets.out_comp_threshold = -23.0f; // Hạ ngưỡng vừa phải để bắt được lời thì thầm
                    
                    float dynamic_gap = (float)(ctx.loudness_lra_high - ctx.loudness_lra_low);
                    targets.out_comp_makeup = (dynamic_gap > 15.0f) ? 3.2f : 2.0f; // Bù âm thông minh tăng âm lượng hội thoại nhỏ
                }
                // Xử lý kịch bản âm lượng nhỏ cần bù đắp thông thường
                else if (ctx.loudness_shortterm < -30.0 && ctx.volume > 50.0) {
                    targets.out_lim_threshold  = std::min(-0.5f, next_lim_th + 0.1f);
                    targets.out_comp_makeup = std::min(4.0f, next_comp_mk + 0.05f);
                    targets.out_comp_threshold = std::min(-12.0f, next_comp_th + 0.2f);
                }

                // --- KIỂM SOÁT CHỐNG NGHẸT TIẾNG (ANTI-PUMPING) ---
                // Áp dụng làm bộ lọc giới hạn chặn dưới (Constraint filter) để bảo vệ đoạn cao trào
                if (ctx.loudness_shortterm >= (ctx.loudness_lra_high - 2.0)) {
                    targets.out_comp_threshold = std::max(next_comp_th, -16.0f); // Không cho phép nén quá sâu khi nhạc đang cao trào đều
                }
            }
        }
    }
    return targets;
}

void AudioFilterManager::DispatchParametersToMPV(bool need_sync_structure, bool parameter_changed) {
    if (need_sync_structure) {
        EvaluateSystemSafety();
        SyncAll(); 
    } 
    else if (parameter_changed) {
        EvaluateSystemSafety();

        for (const auto& filter : m_filters) {
            if (filter.enabled && !filter.isBypassManagement) {
                for (const auto& [key, p] : filter.params) {
                    float value_to_send = p.current;

                    // VÁ LỖI ÉP LẠI KHÓA DẢI CHO COMPRESSOR KHI AI LERP ĐẨY XUỐNG
                    if (filter.name == "acompressor" && key == "threshold") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                        if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                        if (value_to_send > 1.0f) value_to_send = 1.0f;
                    }

                    if (filter.id == "f_volume" && key == "volume") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                    }

                    std::ostringstream ss;
                    if ((filter.name == "acompressor" && (key == "threshold" || key == "makeup")) || filter.id == "f_vol_booster") {
                        ss << std::fixed << std::setprecision(5) << value_to_send;
                    } else {
                        ss << std::fixed << std::setprecision(2) << value_to_send;
                    }

                    std::string val_str = ss.str();

                    const char* cmd[] = {"af-command", filter.id.c_str(), key.c_str(), val_str.c_str(), NULL};
                    mpv_command(mpv, cmd);
                }
            }
        }
    }
}

void AudioFilterManager::UpdateAdaptiveFilters() {
    
    // Giới hạn tần suất xử lý chu kỳ real-time (Throttling)
    static auto lastUpdateTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count() < 150) return;

    lastUpdateTime = currentTime;

    AudioContext ctx =  ExtractCurrentContext();
    //CheckEnvironmentHysteresis(ctx);
    
    if (!m_autoMode || !mpv || m_globalBypass) return;

    AdaptiveTargets targets = AnalyzeContextAndCalculateTargets(ctx);
 
    std::vector<float> eq_gains(12, 0.0f);

    const float alpha = 0.20f;  // Hệ số nội suy mượt Lerp
    const float epsilon = 0.05f;

    bool need_sync_structure = false;
    bool parameter_changed = true;
    // Trợ thủ 1: Đồng bộ trạng thái ON/OFF của Filter
    auto syncFilterState = [&](const std::string& id, bool target_state) -> AudioFilter* {
        auto* filter = FindFilter(id);
        if (!filter || filter->isBypassManagement) return nullptr;
        if (filter->enabled != target_state) {
            filter->enabled = target_state;
            need_sync_structure = true;
        }
        return filter;
    };

    // Trợ thủ 2: Thực hiện Lerp cho Filter Nội bộ (Sử dụng .current trực tiếp)
    auto lerpInternalParam = [&](AudioFilter* filter, const std::string& param_key, float target_val) {
        if (!filter || filter->isBypassManagement || !filter->enabled || !filter->params.count(param_key)) return;
        float& current = filter->params[param_key].current;
        if (std::abs(current - target_val) > epsilon) {
            current += alpha * (target_val - current);
            parameter_changed = true;
        } else {
            current = target_val;
        }
    };

    // Trợ thủ 3: Thực hiện Lerp cho Filter Ngoại vi (Có cơ chế kiểm tra kênh lệnh cũ lastSent)
    auto lerpExternalParam = [&](AudioFilter* filter, const std::string& param_key, float target_val, float custom_alpha, float custom_eps) {
        if (!filter || filter->isBypassManagement || !filter->enabled || !filter->params.count(param_key)) return;
        auto& p = filter->params[param_key];
        float current_val = (p.lastSent == 999999.0f) ? p.user_target : p.lastSent;
        
        if (std::abs(current_val - target_val) > custom_eps) {
            float next_val = current_val + custom_alpha * (target_val - current_val);
            p.current = next_val;
            p.lastSent = next_val;
            parameter_changed = true;
        }
    };

    // =========================================================================
    // THỰC THI CHUỖI XỬ LÝ LERPPING SAU REFRACTOR: CỰC KỲ SẠCH SẼ
    // =========================================================================

    // 1. Lerp mượt mà cho 12 dải tần EQ Graphic
    for (int i = 0; i < 12; ++i) {
        if (auto* filter = syncFilterState("eq_b" + std::to_string(i), true)) {
            lerpInternalParam(filter, "g", targets.eq_gains[i]);
        }
    }

    // 2. Lerp mượt mà cho các Filter chức năng không gian nội bộ
    if (auto* f = syncFilterState("f_crystalizer", targets.crystalizer_enabled)) {
        lerpInternalParam(f, "i", targets.crystalizer_i);
    }
    if (auto* f = syncFilterState("f_stereo", targets.stereo_enabled)) {
        lerpInternalParam(f, "m", targets.stereo_m);
    }
    if (auto* f = syncFilterState("f_comp", targets.comp_enabled)) {
        lerpInternalParam(f, "threshold", targets.comp_th);
        lerpInternalParam(f, "ratio", targets.comp_rt);

    }

    // 3. Lerp mạch bảo vệ ngoại vi (Booster & Stabilizer) thông qua Trợ thủ 3
    if (m_enableOuterBooster) {
        if (auto* f = FindFilter("f_vol_booster")) { // Booster giữ nguyên trạng thái bật tắt ngoại vi
            lerpExternalParam(f, "volume", targets.booster_volume, 0.15f, 0.01f);
        }
    }

    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim  = FindFilter("f_out_limiter");
    if (out_comp && out_comp->enabled && out_lim && out_lim->enabled) {
        lerpExternalParam(out_comp, "threshold", targets.out_comp_threshold, alpha, epsilon);
        lerpExternalParam(out_comp, "makeup", targets.out_comp_makeup, alpha, epsilon);
        lerpExternalParam(out_lim, "threshold", targets.out_lim_threshold, alpha, epsilon);
    }

    if (auto* reverb = syncFilterState("f_reverb", targets.reverb_enabled)) {
    }

    DispatchParametersToMPV(need_sync_structure, parameter_changed);

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
    if (m_globalBypass) return;
    m_autoMode = enabled;
    m_currentPreset = preset;
    
    if (enabled) {
        // Khi bật AI, ép buộc Master Volume Node phải bật để hệ thống bảo vệ hoạt động
        if (auto* vol_node = FindFilter("f_volume")) {
            vol_node->enabled = true;
        }
        UpdateAdaptiveFilters();
    } else {
        // TẮT AI: Trả các bộ lọc do AI quản lý về Flat, khôi phục các tham số về user_target gốc
        for (auto& filter : m_filters) {

            if (!filter.isBypassManagement) {
                for (auto& [key, p] : filter.params) {
                    p.current = p.user_target; 
                }

                filter.enabled = false;
            }
        }
        
        //if (auto* c = FindFilter("f_crystalizer")) if (!c->isBypassManagement) c->enabled = false;
        //if (auto* s = FindFilter("f_stereo")) if (!s->isBypassManagement) s->enabled = false;
        //if (auto* cp = FindFilter("f_comp")) if (!cp->isBypassManagement) cp->enabled = false;
        
        EvaluateSystemSafety();
        SyncAll();
    }
}

void AudioFilterManager::AddLog(const std::string& message, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_logMutex);
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

    LogEntry entry{ ss.str(), message, level };
    m_logs.push_back(entry);

    if (m_logs.size() > MAX_LOG_SIZE) {
        m_logs.erase(m_logs.begin());
    }
    std::cout << "[" << entry.timestamp << "] " << message << "\n";
}

const std::vector<LogEntry>& AudioFilterManager::GetLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logs; 
}

void AudioFilterManager::ClearLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.clear();
}