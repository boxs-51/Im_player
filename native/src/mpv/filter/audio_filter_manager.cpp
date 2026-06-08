#include <mpv/filter/audio_filter_manager.h>
#include <sstream>

AudioFilterManager& AudioFilterManager::Instance() {
    static AudioFilterManager instance;
    return instance;
}

void AudioFilterManager::Init(mpv_handle* h) { 
    mpv = h; 

    // ==========================================
    // KHỞI TẠO CÁC FILTER VÀ THÔNG SỐ (PARAMETERS)
    // ==========================================

    // 1. Nhóm Equalizer (Cân bằng âm sắc)
    AddFilter("f_bass", "bass");
    RegisterParam("f_bass", "g", -20.0f, 20.0f, 0.0f);     // Gain (dB)
    RegisterParam("f_bass", "f", 20.0f, 999.0f, 100.0f);   // Frequency (Hz)

    AddFilter("f_treble", "treble");
    RegisterParam("f_treble", "g", -20.0f, 20.0f, 0.0f);   // Gain (dB)
    RegisterParam("f_treble", "f", 1000.0f, 20000.0f, 3000.0f); // Frequency (Hz)

    AddFilter("f_eq", "equalizer");
    RegisterParam("f_eq", "f", 20.0f, 20000.0f, 1000.0f);  // Central frequency
    RegisterParam("f_eq", "w", 0.1f, 100.0f, 1.0f);        // Band width
    RegisterParam("f_eq", "g", -20.0f, 20.0f, 0.0f);       // Gain

    // 2. Nhóm Dynamic Range (Kiểm soát cường độ / Âm lượng)
    AddFilter("f_volume", "volume");
    RegisterParam("f_volume", "volume", -30.0f, 30.0f, 0.0f); // Volume (dB)

    AddFilter("f_comp", "acompressor");
    RegisterParam("f_comp", "threshold", -100.0f, 0.0f, -12.5f); // Threshold (dB)
    RegisterParam("f_comp", "ratio", 1.0f, 20.0f, 2.0f);         // Ratio
    RegisterParam("f_comp", "attack", 0.01f, 2000.0f, 20.0f);    // Attack (ms)
    RegisterParam("f_comp", "release", 0.01f, 9000.0f, 250.0f);  // Release (ms)

    AddFilter("f_norm", "loudnorm"); // EBU R128 loudness normalization (Thường dùng Auto, ít khi set param thủ công)

    // 3. Nhóm Không gian âm thanh (Spatial / Stereo)
    AddFilter("f_stereo", "extrastereo");
    RegisterParam("f_stereo", "m", -10.0f, 10.0f, 2.5f); // Difference coefficient

    AddFilter("f_crystalizer", "crystalizer");
    RegisterParam("f_crystalizer", "i", -10.0f, 10.0f, 2.0f); // Intensity

    AddFilter("f_bs2b", "bs2b"); // Bauer stereo-to-binaural (Dùng cho tai nghe)
    RegisterParam("f_bs2b", "profile", 0.0f, 2.0f, 0.0f); // 0: default, 1: cmoy, 2: jmeier

    // 4. Nhóm Tốc độ và Cao độ (Time/Pitch)
    AddFilter("f_pitch", "rubberband");
    RegisterParam("f_pitch", "pitch", 0.1f, 10.0f, 1.0f); // Pitch shift ratio
    RegisterParam("f_pitch", "tempo", 0.1f, 10.0f, 1.0f); // Tempo shift ratio

    // 5. Nhóm Hiệu ứng (Effects)
    AddFilter("f_chorus", "chorus");
    RegisterParam("f_chorus", "in_gain", 0.0f, 1.0f, 0.4f);
    RegisterParam("f_chorus", "out_gain", 0.0f, 1.0f, 0.4f);

    AddFilter("f_flanger", "flanger");
    RegisterParam("f_flanger", "delay", 0.0f, 30.0f, 0.0f);
    RegisterParam("f_flanger", "depth", 0.0f, 10.0f, 2.0f);
}

void AudioFilterManager::AddFilter(const std::string& id, const std::string& name) {
    m_filters.push_back({id, name, false, {}});
    m_filterIndex[id] = m_filters.size() - 1; // Cache the index
}

void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def) {
    if (auto* f = FindFilter(id)) {
        f->params[key] = {def, min, max, def}; // current, min, max, def
    }
}

void AudioFilterManager::ResetFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        for (auto& [key, p] : f->params) {
            UpdateParam(id, key, p.def);
        }
    }
}

void AudioFilterManager::SaveToFile() {
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
        filter.enabled = true; // Mặc định  hết hoặc tùy bạn chỉnh
        for (auto& [key, p] : filter.params) {
            p.current = p.def; // Đưa về giá trị default đã Register
        }
    }
    SyncAll(); // Áp dụng ngay lập tức xuống MPV
}
void AudioFilterManager::LoadFromFile() {
    std::ifstream f(path);
    
    // TRƯỜNG HỢP 1: Không tìm thấy file hoặc file lỗi
    if (!f.is_open()) {
        ResetAllToDefaults(); 
        return; 
    }

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
                    parse_success = true; // Đánh dấu đã đọc được ít nhất 1 filter
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
                            filter->params[key].current = val;
                        }
                    }
                } catch (...) {
                    // Nếu giá trị trong file không phải là số, bỏ qua dòng đó
                    continue;
                }
            }
        }
    }

    // TRƯỜNG HỢP 2: File tồn tại nhưng trống hoặc sai định dạng hoàn toàn
    if (!parse_success) {
        ResetAllToDefaults();
    } else {
        SyncAll(); // Chỉ Sync nếu load thành công
    }
}

void AudioFilterManager::SyncAll() {
    if (!mpv) return;
    
    std::string full_af = "";
    for (const auto& f : m_filters) {
        if (f.enabled) {
            if (!full_af.empty()) full_af += ",";
            full_af += f.GetInitString(); // Format: @f_bass:bass=g=5:f=100
        }
    }
    
    // Gửi property 'af' xuống mpv
    mpv_set_property_string(mpv, "af", full_af.c_str());
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    for (auto& f : m_filters) {
        if (f.id == id) {
            // Kiểm tra giới hạn (clamp)
            if (value < f.params[key].min) value = f.params[key].min;
            if (value > f.params[key].max) value = f.params[key].max;
            
            f.params[key].current = value;
            
            // Nếu filter đang bật, gửi lệnh realtime
            if (f.enabled) {
                std::string val_str = std::to_string(value);
                const char* cmd[] = {"af-command", id.c_str(), key.c_str(), val_str.c_str(), NULL};
                mpv_command(mpv, cmd);
            }
            break;
        }
    }
}

void AudioFilterManager::BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates) {
    for (const auto& [id, key, value] : updates) {
        for (auto& f : m_filters) {
            if (f.id == id && f.params.count(key)) {
                float clamped = value;
                if (clamped < f.params[key].min) clamped = f.params[key].min;
                if (clamped > f.params[key].max) clamped = f.params[key].max;
                f.params[key].current = clamped;
                break;
            }
        }
    }
    SyncAll(); // Single sync for batch updates
}

void AudioFilterManager::ToggleFilter(const std::string& id, bool state) {
    for (auto& f : m_filters) {
        if (f.id == id) {
            f.enabled = state;
            SyncAll(); // Áp dụng lại toàn bộ chuỗi
            break;
        }
    }
}
void AudioFilterManager::SetFilterEnabled(const std::string& id, bool enabled) {
    if (auto* f = FindFilter(id)) {
        if (f->enabled != enabled) { // Chỉ xử lý nếu trạng thái thực sự thay đổi
            f->enabled = enabled;
            SyncAll(); 
        }
    }
}
void AudioFilterManager::SetAllFiltersState(bool enabled) {
    bool changed = false;
    for (auto& f : m_filters) {
        if (f.enabled != enabled) {
            f.enabled = enabled;
            changed = true;
        }
    }
    // Chỉ gọi SyncAll một lần duy nhất sau khi đã duyệt hết danh sách
    if (changed) {
        SyncAll();
    }
}
bool AudioFilterManager::IsFilterEnabled(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        return f->enabled;
    }
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
    const char* cmd[] = {"audio", trackId.c_str(), NULL};
    mpv_command(mpv, cmd);
}

void AudioFilterManager::SetChannelMode(const std::string& mode) {
    if (!mpv) return;
    m_channelMode = mode;
    
    // Apply channel upmix/downmix
    if (mode == "mono") {
        const char* cmd[] = {"af", "pan=mono|c0=0.5*c0+0.5*c1", NULL};
        mpv_command(mpv, cmd);
    } else if (mode == "stereo") {
        const char* cmd[] = {"af", "", NULL};
        mpv_command(mpv, cmd);
    } else if (mode == "surround") {
        const char* cmd[] = {"af", "pan=5.1|FL=c0|FR=c1|FC=c0+c1|LFE=0|BL=c0|BR=c1", NULL};
        mpv_command(mpv, cmd);
    }
}

