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
    RegisterParam("f_vol_booster", "volume", 0.0f, 1.5f, 1.0f); 

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

    if (isEvaluating)
        return;

    struct ScopeGuard
    {
        bool& flag;
        ScopeGuard(bool& f) : flag(f)
        {
            flag = true;
        }

        ~ScopeGuard()
        {
            flag = false;
        }
    };

    ScopeGuard guard(isEvaluating);

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
                    int err = mpv_command(mpv, cmd);

                    if (err < 0)
                    {
                        std::string log_msg = "[MPV] af-command failed for filter: f_volume with value: " + val_str;
                        AddLog(
                            log_msg,
                            LogLevel::Error);
                    }
                }
            }
        } else {
            if (vol_node->params["volume"].current < 0.0f && totalGainAccumulation <= 5.0f) {
                vol_node->params["volume"].current = 0.0f; 
                if (vol_node->enabled && mpv) {
                    const char* cmd[] = {"af-command", "f_volume", "volume", "0.00", NULL};
                    int err = mpv_command(mpv, cmd);

                    if (err < 0)
                    {
                        std::string log_msg = "[MPV] af-command failed for filter: f_volume";
                        AddLog(
                            log_msg,
                            LogLevel::Error);
                    }
                }
            }
        }
    }
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
        f << "[Filter]:" 
        << filter.id 
        << "|" 
        << (filter.enabled ? "1" : "0") 
        << "|" 
        << (filter.isBypassManagement ? "1" : "0")
        << "\n";
        for (const auto& [key, p] : filter.params) {
            f << key << ":" << p.current << "\n";
        }
    }
}

void AudioFilterManager::ResetAllToDefaults()
{
    for (auto& filter : m_filters)
    {
        filter.enabled = false;

        for (auto& [key, p] : filter.params)
        {
            p.current = p.def;
        }
    }

    m_channelMode = "stereo";

    m_currentPreset = AudioPreset::Flat;
    m_autoMode = false;
    m_globalBypass = false;

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
            size_t d1 = line.find('|');
            size_t d2 = line.find('|', d1 + 1);
            if (d2 != std::string::npos) {
                current_id = line.substr(9, d1 - 9);
                bool enabled = line.substr(d1 + 1, d2 - d1 - 1) == "1";
                bool bypass =   line.substr(d2 + 1) == "1";
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
            if (f.id == "f_vol_booster" ||
                f.id == "f_out_compressor" ||
                f.id == "f_out_limiter")
                continue;
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
                int err = mpv_command(mpv, cmd);

                if (err < 0)
                {
                    AddLog(
                        "[MPV] af-command failed for filter: " + id,
                        LogLevel::Error);
                }
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

AudioFilterManager::AudioContext AudioFilterManager::ExtractCurrentContext() {
    AudioContext ctx;
    ctx.volume = (double)g_playbackStatus.volume;
    ctx.speed  = g_playbackStatus.speed;
    ctx.sample_rate   = (int64_t)g_videoInfo.g_audioarams.asamplerate;
    ctx.channel_count = (int64_t)g_videoInfo.g_audioarams.channel_count;
    ctx.bitrate_kbps   = (double)(g_videoInfo.abitrate / 1000);
    ctx.codec = g_videoInfo.acodec;
    ctx.is_audio_only = g_videoInfo.width == 0 && g_videoInfo.height == 0;

    // Đọc dữ liệu phản hồi thực tế từ MPV Core
    double mpv_peak = 0.0;
    mpv_get_property(mpv, "audio-out-peak", MPV_FORMAT_DOUBLE, &mpv_peak);
    ctx.output_peak = mpv_peak;

    double mpv_loudness = -50.0; // Mặc định âm cực nhỏ nếu không có luồng
    mpv_get_property(mpv, "audio-out-detected-device-loudness", MPV_FORMAT_DOUBLE, &mpv_loudness);
    ctx.output_loudness = mpv_loudness;

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

        std::string ai_msg = "[AI Matrix] Context altered. Volume: " + std::to_string((int)last_volume) + 
                            "%, Codec: " + ctx.codec + ", Speed: " + std::to_string(last_speed);
        AddLog(ai_msg, LogLevel::AI_Action);
    }
    return changed;
}

void AudioFilterManager::AnalyzeContextAndCalculateTargets(
    const AudioContext& ctx, std::vector<float>& targetGains, 
    bool& target_crystalizer, float& crystalizer_i,
    bool& target_stereo, float& stereo_m,
    bool& target_comp, float& comp_th, float& comp_rt,
    bool& target_reverb, std::string& reverb_preset) 
{
    // Cài đặt Preset EQ chuẩn ban đầu
    switch (m_currentPreset) {
        case AudioPreset::Pop:
        {
            targetGains = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 4.0f, 3.5f, 2.0f, 1.0f, 0.0f};
            break;
        }
        case AudioPreset::Rock:
        {
            targetGains = {5.0f, 4.0f, 3.0f, 1.0f, -1.0f, -1.0f, 1.5f, 3.0f, 4.5f, 5.0f};
            break;
        }
        case AudioPreset::EDM_Dance:
        {
            targetGains = {6.5f, 5.5f, 4.0f, 1.5f, 0.0f, 0.0f, 2.0f, 3.5f, 5.0f, 6.0f};
            break;
         }   
        case AudioPreset::Classical:
        {
            targetGains = {-2.0f, -1.0f, 1.0f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 3.0f, 2.0f};
            break;
        }    
        case AudioPreset::Flat:
        default:
        {
            targetGains = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
            break;
        }    
    }

    // --- KỊCH BẢN 1: Phục hồi lossy codec ---
    if (ctx.codec == "mp3" || ctx.codec == "aac" || (ctx.bitrate_kbps > 0.0 && ctx.bitrate_kbps < 192.0)) {
        targetGains[0] += 2.5f; targetGains[9] += 3.5f;
        target_crystalizer = true; crystalizer_i = 3.8f;
    }

    // --- KỊCH BẢN 2: Tối ưu Phim / Nhạc & Giả lập không gian ---
    if (!ctx.is_audio_only) {
        targetGains[4] += 2.0f; targetGains[5] += 3.0f; // Đẩy lời thoại ấm hơn
        if (ctx.channel_count == 2) { target_stereo = true; stereo_m = 3.5f; }
    } else {
        if (ctx.channel_count == 2 && m_currentPreset == AudioPreset::EDM_Dance) {
            target_stereo = true; stereo_m = 2.5f;
        }
    }

    // --- KỊCH BẢN 3: Chế độ nghe đêm ban đêm / Chống quá tải màng loa ---
    if (ctx.volume < 30.0) {
        targetGains[0] += 4.0f; targetGains[9] += 3.0f; // Kích Loudness contour
        target_comp = true; comp_th = -22.0f; comp_rt = 1.8f;
    } else if (ctx.volume > 85.0) {
        for (int i = 0; i < 10; ++i) if (targetGains[i] > 1.0f) targetGains[i] *= 0.4f;
        target_comp = true; comp_th = -5.0f; comp_rt = 4.0f;
    }

    // --- KỊCH BẢN TUA NHANH (Speed Pitch Compensate) ---
    if (ctx.speed > 1.25) {
        targetGains[7] -= 1.5f; targetGains[8] -= 2.5f; targetGains[9] -= 3.5f;
    }

    // =========================================================================
    // KHÔNG GIAN PHÁT TRIỂN: THÊM CÁC HIỆU ỨNG MỚI TẠI ĐÂY
    // =========================================================================
    // Ví dụ: Kịch bản 4: Tự động bật Reverb (Vang phòng) nếu là file nhạc nhẹ Acoustic/Nhạc thuần
    if (ctx.is_audio_only && m_currentPreset == AudioPreset::Classical) {
        target_reverb = true;
        reverb_preset = "large_hall"; // Gợi ý cấu hình chuỗi vang rộng
    } else {
        target_reverb = false;
    }
    // =========================================================================
    // VÒNG PHẢN HỒI THÔNG MINH: TỰ HIỆU CHỈNH BỘ ỔN ĐỊNH NGOẠI VI
    // =========================================================================
    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim  = FindFilter("f_out_limiter");

    if (out_comp && out_lim) {
        float current_comp_th = out_comp->params["threshold"].current;
        float current_comp_mk = out_comp->params["makeup"].current;
        float current_lim_th  = out_lim->params["threshold"].current;

        // TÌNH HUỐNG 1: Âm thanh sau hiệu chỉnh bị quá tải (Quá sát ngưỡng rè)
        if (ctx.output_peak > 0.95) {
            // Hạ trần giới hạn chống rè xuống thấp hơn một chút để an toàn (-1.5dB hoặc -2.0dB)
            out_lim->params["threshold"].current = std::max(-3.0f, current_lim_th - 0.2f);
            
            // Ép bộ nén hoạt động sớm hơn để ghìm đỉnh âm thanh xuống
            out_comp->params["threshold"].current = std::max(-28.0f, current_comp_th - 0.5f);
            
            // Giảm nhẹ makeup gain để tránh kích nổ âm thanh
            out_comp->params["makeup"].current = std::max(1.0f, current_comp_mk - 0.1f);
            
            AddLog("[AI Self-Calibrate] Peak danger detected (" + std::to_string(ctx.output_peak) + "). Tightening Stabilizer.", LogLevel::AI_Action);
        }
        
        // TÌNH HUỐNG 2: Biên độ an toàn nhưng âm lượng tổng thể sau hiệu chỉnh bị sụt giảm quá sâu
        else if (ctx.output_loudness < -30.0 && ctx.volume > 50.0) {
            // Nới lỏng trần Limiter về sát trần mộc (-0.5dB)
            out_lim->params["threshold"].current = std::min(-0.5f, current_lim_th + 0.1f);
            
            // Kéo sàn âm lượng nhỏ lên bằng cách tăng dần makeup gain
            out_comp->params["makeup"].current = std::min(4.0f, current_comp_mk + 0.05f);
            
            // Đẩy nhẹ ngưỡng nén lên cao để âm thanh dynamic hơn
            out_comp->params["threshold"].current = std::min(-12.0f, current_comp_th + 0.2f);
        }
    }
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

                    // Áp dụng bộ khóa dải nén acompressor an toàn (Sửa lỗi crash popup)
                    if (filter.name == "acompressor" && key == "threshold") {
                        value_to_send = std::pow(10.0f, p.current / 20.0f);
                        if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                        if (value_to_send > 1.0f) value_to_send = 1.0f;
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
    // 1. Kiểm tra điều kiện tiên quyết
    if (!m_autoMode || !mpv || m_globalBypass) return;

    // 2. Throttling - Giới hạn tần suất tính toán (150ms)
    static auto lastUpdateTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count() < 150) return; 
    lastUpdateTime = currentTime;

    // 3. Trích xuất thông tin thông qua Module phân rã 1
    AudioContext ctx = ExtractCurrentContext();

    // 4. Kiểm tra độ nhiễu Hysteresis thông qua Module phân rã 2
    if (CheckEnvironmentHysteresis(ctx)) {
        std::string ai_msg = "[AI Matrix] Context altered. Volume: " + std::to_string((int)ctx.volume) + 
                            "%, Codec: " + ctx.codec + ", Speed: " + std::to_string(ctx.speed);
        AddLog(ai_msg, LogLevel::AI_Action);
    }
    
    // 5. Khởi tạo biến lưu trữ mục tiêu
    std::vector<float> targetGains(10, 0.0f);
    bool target_crystalizer = false; float crystalizer_i = 2.0f;
    bool target_stereo = false;      float stereo_m = 2.5f;
    bool target_comp = false;        float comp_th = -12.0f; float comp_rt = 2.0f;
    bool target_reverb = false;      std::string reverb_preset = "none"; // Biến hiệu ứng mới

    // 6. Tính toán toán thuật thông qua Module phân rã 3
    AnalyzeContextAndCalculateTargets(ctx, targetGains, target_crystalizer, crystalizer_i,
                                      target_stereo, stereo_m, target_comp, comp_th, comp_rt,
                                      target_reverb, reverb_preset);

    // 7. Nội suy tuyến tính (Lerp) làm mịn chuyển âm mượt mà
    bool need_sync_structure = false; 
    bool parameter_changed = false;   
    const float alpha = 0.20f;
    const float epsilon = 0.05f;

    // Thao tác Lerp 10 băng tần Equalizer
    for (int i = 0; i < 10; ++i) {
        if (auto* filter = FindFilter("eq_b" + std::to_string(i))) {
            if (!filter->isBypassManagement) {
                if (!filter->enabled) { filter->enabled = true; need_sync_structure = true; }
                float current_g = filter->params["g"].current;
                float dest_g = targetGains[i];
                if (std::abs(current_g - dest_g) > epsilon) {
                    filter->params["g"].current = current_g + alpha * (dest_g - current_g);
                    parameter_changed = true;
                } else { filter->params["g"].current = dest_g; }
            }
        }
    }

    // Lambda Helper xử lý các node hiệu ứng động mượt mà
    auto processSmoothFilter = [&](const std::string& id, bool target_state, const std::string& param_key, float target_val) {
        auto* filter = FindFilter(id);
        if (!filter || filter->isBypassManagement) return;
        if (filter->enabled != target_state) { filter->enabled = target_state; need_sync_structure = true; }
        if (filter->enabled && filter->params.count(param_key)) {
            float current_val = filter->params[param_key].current;
            if (std::abs(current_val - target_val) > epsilon) {
                filter->params[param_key].current = current_val + alpha * (target_val - current_val);
                parameter_changed = true;
            } else { filter->params[param_key].current = target_val; }
        }
    };

    processSmoothFilter("f_crystalizer", target_crystalizer, "i", crystalizer_i);
    processSmoothFilter("f_stereo", target_stereo, "m", stereo_m);

    // Xử lý Lerp riêng cho Compressor đa tham số
    if (auto* comp = FindFilter("f_comp")) {
        if (!comp->isBypassManagement) {
            if (comp->enabled != target_comp) { comp->enabled = target_comp; need_sync_structure = true; }
            if (comp->enabled) {
                float curr_th = comp->params["threshold"].current;
                float curr_rt = comp->params["ratio"].current;
                if (std::abs(curr_th - comp_th) > epsilon) { comp->params["threshold"].current = curr_th + alpha * (comp_th - curr_th); parameter_changed = true; }
                if (std::abs(curr_rt - comp_rt) > epsilon) { comp->params["ratio"].current = curr_rt + alpha * (comp_rt - curr_rt); parameter_changed = true; }
            }
        }
    }

    // Xử lý logic bật tắt cho bộ lọc mới Reverb (Ví dụ mẫu kết nối cấu trúc)
    if (auto* reverb = FindFilter("f_reverb")) {
        if (!reverb->isBypassManagement && reverb->enabled != target_reverb) {
            reverb->enabled = target_reverb;
            need_sync_structure = true;
        }
    }

    // 8. Đẩy dữ liệu đồng bộ xuống MPV core bằng Module phân rã 4
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

const std::vector<LogEntry>& AudioFilterManager::GetLogs()  {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logs; // Trả về một bản sao an toàn cho luồng UI render
}

void AudioFilterManager::ClearLogs() {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_logs.clear();
}