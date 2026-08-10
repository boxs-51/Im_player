#include "af_m.h"
#include "af_m_log.h"
#include <iostream>

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
    m_enableOuterStabilizer = true;
    m_enableOuterBooster = true;

    // 1. Nhóm Equalizer (12-Band)
    m_eqBands = {
        {"eq_b0", "20"},    {"eq_b1", "31"},    {"eq_b2", "63"},    {"eq_b3", "125"},
        {"eq_b4", "250"},   {"eq_b5", "500"},   {"eq_b6", "1000"},  {"eq_b7", "1500"},
        {"eq_b8", "3000"},  {"eq_b9", "4000"},  {"eq_b10", "8000"}, {"eq_b11", "16000"}
    };

    for (const auto& [band_id, freq] : m_eqBands) {
        AddFilter(band_id, "equalizer", "equalizer_group");
        RegisterParam(band_id, "g", -20.0f, 20.0f, 0.0f); 
    }

    // 2. Nhóm Tone & Dynamics & Spatial
    AddFilter("f_bass", "bass", "tone_booster");
    RegisterParam("f_bass", "g", -20.0f, 20.0f, 0.0f);
    RegisterParam("f_bass", "f", 20.0f, 500.0f, 100.0f);

    AddFilter("f_treble", "treble", "tone_booster");
    RegisterParam("f_treble", "g", -20.0f, 20.0f, 0.0f);
    RegisterParam("f_treble", "f", 1000.0f, 20000.0f, 3500.0f);

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

    // 3. Hệ thống mạch bảo vệ ngoại vi tầng cuối
    AddFilter("f_vol_booster", "volume", "outer_gain_node");
    RegisterParam("f_vol_booster", "volume", 0.0f, 1.5f, 1.0f); 

    AddFilter("f_out_compressor", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_compressor", "threshold", -30.0f, 0.0f, -18.0f); 
    RegisterParam("f_out_compressor", "ratio", 1.0f, 20.0f, 4.0f);    
    RegisterParam("f_out_compressor", "attack", 0.01f, 100.0f, 5.0f);   
    RegisterParam("f_out_compressor", "release", 10.0f, 1000.0f, 50.0f); 
    RegisterParam("f_out_compressor", "makeup", 1.0f, 64.0f, 2.0f);     

    AddFilter("f_out_limiter", "acompressor", "outer_stabilizer");
    RegisterParam("f_out_limiter", "threshold", -30.0f, 0.0f, -1.0f);   
    RegisterParam("f_out_limiter", "ratio", 1.0f, 20.0f, 20.0f);  
    RegisterParam("f_out_limiter", "attack", 0.01f, 100.0f, 1.0f);   
    RegisterParam("f_out_limiter", "release", 10.0f, 1000.0f, 100.0f);
    RegisterParam("f_out_limiter", "makeup", 1.0f, 64.0f, 1.0f);     

    AddFilter("f_ebur_measurer", "lavfi", "system_internal");

}

void AudioFilterManager::AddFilter(const std::string& id, const std::string& name, const std::string& group) {
    m_filters.push_back({id, name, false, {}, group, false}); 
    m_filterIndex[id] = m_filters.size() - 1; 
}

AudioFilter* AudioFilterManager::FindFilter(const std::string& id) {
    auto it = m_filterIndex.find(id);
    if (it != m_filterIndex.end() && it->second < m_filters.size()) {
        return &m_filters[it->second];
    }
    return nullptr;
}

void AudioFilterManager::ToggleFilter(const std::string& id, bool enabled) {
    auto* f = FindFilter(id);
    if (enabled && f->isFailed) {
        AddLog("[Architecture] Cannot enable " + id + " because it was isolated due to a critical error.", LogLevel::Warning);
        return; 
    }
    if (!f || f->enabled == enabled) return;

    if (id == "f_out_compressor" || id == "f_out_limiter") {
        auto* comp = FindFilter("f_out_compressor");
        auto* lim = FindFilter("f_out_limiter");
        if (id == "f_out_compressor") comp->enabled = enabled;
        if (id == "f_out_limiter") lim->enabled = enabled;
        m_enableOuterStabilizer = comp->enabled && lim->enabled;
        AddLog("[Sync] Outer Stabilizer State updated -> " + std::string(enabled ? "ON" : "OFF"), LogLevel::Info);
    } 
    else if (id == "f_vol_booster") {
        m_enableOuterBooster = enabled;
        f->enabled = enabled;
        AddLog("[Sync] Outer Booster State updated -> " + std::string(enabled ? "ON" : "OFF"), LogLevel::Info);
    } 
    else {
        f->enabled = enabled;
    }

    if (enabled && !m_globalBypass && !f->isBypassManagement) {
        if (!f->group.empty() && f->group != "equalizer_group" 
                            && f->group != "outer_stabilizer" 
                            && f->group != "outer_gain_node"
                            && f->group != "system_internal") {
            for (auto& other : m_filters) {
                if (other.id != id && other.group == f->group && other.enabled && !other.isBypassManagement) {
                    other.enabled = false;
                    AddLog("[Conflict Managed] Auto-disabled " + other.id, LogLevel::Warning);
                }
            }
        }
    }
    EvaluateSystemSafety();
    SyncAll(); 
}

bool AudioFilterManager::IsFilterEnabled(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        if (f->isFailed) return false;
        return f->enabled;
    }
    return false;
}
void AudioFilterManager::RecoverFailedFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        if (f->isFailed) {
            f->isFailed = false;
            f->enabled = true; // Thử bật lại
            AddLog("[Architecture] Attempting to recover and reinstall filter: " + id, LogLevel::Info);
            SyncAll();
        }
    }
}
int AudioFilterManager::GetActiveFilterCount() {
    int count = 0;
    for (const auto& f : m_filters) { if (f.enabled) count++; }
    return count;
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