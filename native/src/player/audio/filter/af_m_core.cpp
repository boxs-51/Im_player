#include "af_m.h"
#include "af_m_log.h"
#include <iostream>

AudioFilterManager::AudioFilterManager() 
    : mpv(nullptr), m_channelMode("stereo"), m_autoMode(false), 
      m_enableOuterStabilizer(true), m_enableOuterBooster(true) {
        SyncAll();
      }

AudioFilterManager::~AudioFilterManager() {
    DetachPlayer();
}

void AudioFilterManager::AttachPlayer(mpv_handle* h, PlayerStateSystem* stateSystem) {
    mpv = h;
    m_stateSystem = stateSystem;
}

void AudioFilterManager::DetachPlayer() {
    mpv = nullptr;
    m_stateSystem = nullptr;
}

void AudioFilterManager::Init(mpv_handle* h, PlayerStateSystem* stateSystem) { 
    AttachPlayer(h, stateSystem);
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
        AddFilter(band_id, "equalizer", "equalizer_group", "Điều chỉnh gain cho dải tần " + freq + " Hz", true);
        RegisterParam(band_id, "g", -20.0f, 20.0f, 0.0f, true); 
    }

    // 2. Nhóm Tone & Dynamics & Spatial
    AddFilter("f_bass", "bass", "tone_booster", "Tăng/giảm dải âm trầm (bass)", false);
    RegisterParam("f_bass", "g", -20.0f, 20.0f, 0.0f, false);
    RegisterParam("f_bass", "f", 20.0f, 500.0f, 100.0f, false);

    AddFilter("f_treble", "treble", "tone_booster", "Tăng/giảm dải âm cao (treble)", false);
    RegisterParam("f_treble", "g", -20.0f, 20.0f, 0.0f, false);
    RegisterParam("f_treble", "f", 1000.0f, 20000.0f, 3500.0f, false);

    AddFilter("f_volume", "volume", "gain_node", "Bộ khuếch đại âm lượng chính, được quản lý bởi hệ thống an toàn", true);
    RegisterParam("f_volume", "volume", -60.0f, 30.0f, 0.0f, true);

    AddFilter("f_comp", "acompressor", "dynamics", "Nén dải động, làm âm thanh đồng đều hơn", true);
    RegisterParam("f_comp", "threshold", -60.0f, 0.0f, -12.0f, true);
    RegisterParam("f_comp", "ratio", 1.0f, 20.0f, 2.0f, true);
    RegisterParam("f_comp", "attack", 0.01f, 2000.0f, 20.0f, false);
    RegisterParam("f_comp", "release", 0.01f, 9000.0f, 250.0f, false);

    AddFilter("f_norm", "loudnorm", "dynamics", "Chuẩn hóa âm lượng theo tiêu chuẩn EBU R128", false);

    AddFilter("f_stereo", "extrastereo", "spatial", "Mở rộng không gian âm thanh nổi (stereo)", true);
    RegisterParam("f_stereo", "m", 0.0f, 10.0f, 2.5f, true); 

    AddFilter("f_crystalizer", "crystalizer", "spatial", "Làm âm thanh trở nên trong và sắc nét hơn", true);
    RegisterParam("f_crystalizer", "i", 0.0f, 10.0f, 2.0f, true); 

    AddFilter("f_bs2b", "bs2b", "spatial", "Giả lập âm thanh Crossfeed cho tai nghe, giảm mỏi tai", false); 
    RegisterParam("f_bs2b", "profile", 0.0f, 2.0f, 0.0f, false); 

    AddFilter("f_pitch", "atempo", "time_pitch", "Thay đổi tốc độ phát lại mà không ảnh hưởng cao độ", false);
    RegisterParam("f_pitch", "tempo", 0.5f, 2.0f, 1.0f, false);

    AddFilter("f_chorus", "chorus", "effects", "Tạo hiệu ứng đồng ca, làm dày âm thanh", false);
    RegisterParam("f_chorus", "in_gain", 0.0f, 1.0f, 0.4f, false);
    RegisterParam("f_chorus", "out_gain", 0.0f, 1.0f, 0.4f, false);

    AddFilter("f_flanger", "flanger", "effects", "Tạo hiệu ứng âm thanh 'máy bay phản lực'", false);
    RegisterParam("f_flanger", "delay", 0.0f, 30.0f, 0.0f, false);
    RegisterParam("f_flanger", "depth", 0.0f, 10.0f, 2.0f, false);

    // BỘ LỌC MỚI: TÁCH LỜI / KARAOKE
    AddFilter("f_vocal_remover", "stereotools", "specialized", "Loại bỏ giọng hát khỏi bản nhạc (chế độ Karaoke)", true);
    RegisterParam("f_vocal_remover", "mode", 0.0f, 1.0f, 0.0f, true); // 0: normal, 1: l-r (vocal removal)

    // 3. Nhóm Bộ lọc chuyên biệt (Noise Reduction, Speech Enhancement)
    AddFilter("f_noise_reduction", "anr", "specialized", "Giảm nhiễu âm thanh nền", true);
    RegisterParam("f_noise_reduction", "m", 0.0f, 1.0f, 0.5f, false); // Mode
    RegisterParam("f_noise_reduction", "l", 0.0f, 1.0f, 0.5f, true); // Level
    RegisterParam("f_noise_reduction", "f", 0.0f, 20000.0f, 5000.0f, false); // Filter frequency

    AddFilter("f_speech_enhancement", "speechnorm", "specialized", "Tăng cường và làm rõ giọng nói", true);
    RegisterParam("f_speech_enhancement", "level", -30.0f, 0.0f, -25.0f, true); // Target level
    RegisterParam("f_speech_enhancement", "expansion", 0.0f, 10.0f, 2.0f, false); // Expansion ratio
    RegisterParam("f_speech_enhancement", "compress", 0.0f, 10.0f, 2.0f, false); // Compression ratio
    RegisterParam("f_speech_enhancement", "th", -60.0f, 0.0f, -30.0f, false); // Threshold
    RegisterParam("f_speech_enhancement", "rm", 0.0f, 1.0f, 0.1f, false); // Release time multiplier

    AddFilter("f_audio_restoration", "declick", "specialized", "Phục hồi âm thanh, loại bỏ tiếng lách tách", true);
    RegisterParam("f_audio_restoration", "w", 16.0f, 256.0f, 55.0f, false); // window size
    RegisterParam("f_audio_restoration", "o", 10.0f, 95.0f, 75.0f, false); // overlap


    // 3. Hệ thống mạch bảo vệ ngoại vi tầng cuối
    AddFilter("f_vol_booster", "volume", "outer_gain_node", "Bộ khuếch đại âm lượng đầu ra (an toàn)", true);
    RegisterParam("f_vol_booster", "volume", 0.0f, 1.5f, 1.0f, true); 

    AddFilter("f_out_compressor", "acompressor", "outer_stabilizer", "Máy nén đầu ra, một phần của bộ ổn định", true);
    RegisterParam("f_out_compressor", "threshold", -30.0f, 0.0f, -18.0f, true); 
    RegisterParam("f_out_compressor", "ratio", 1.0f, 20.0f, 4.0f, false);    
    RegisterParam("f_out_compressor", "attack", 0.01f, 100.0f, 5.0f, false);   
    RegisterParam("f_out_compressor", "release", 10.0f, 1000.0f, 50.0f, false); 
    RegisterParam("f_out_compressor", "makeup", 1.0f, 64.0f, 2.0f, true);     

    AddFilter("f_out_limiter", "acompressor", "outer_stabilizer", "Bộ giới hạn đầu ra, lá chắn cuối cùng chống vỡ tiếng", true);
    RegisterParam("f_out_limiter", "threshold", -30.0f, 0.0f, -1.0f, true);   
    RegisterParam("f_out_limiter", "ratio", 1.0f, 20.0f, 20.0f, false);  
    RegisterParam("f_out_limiter", "attack", 0.01f, 100.0f, 1.0f, false);   
    RegisterParam("f_out_limiter", "release", 10.0f, 1000.0f, 100.0f, false);
    RegisterParam("f_out_limiter", "makeup", 1.0f, 64.0f, 1.0f, false);     

    AddFilter("f_ebur_measurer", "lavfi", "system_internal", "Bộ đo lường âm lượng EBU R128 (chỉ dùng nội bộ)", false);

}

void AudioFilterManager::AddFilter(const std::string& id, const std::string& name, const std::string& group, const std::string& description, bool ai_controllable) {
    m_filters.push_back({id, name, description, false, {}, group, false, ai_controllable, false}); 
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
        LOG_NO_KEY(1, LogLevel::Warning, LogCategory::System, std::cout << "Cannot enable " + id + " because it was isolated due to a critical error.");
        return; 
    }
    if (!f || f->enabled == enabled) return;

    if (id == "f_out_compressor" || id == "f_out_limiter") {
        auto* comp = FindFilter("f_out_compressor");
        auto* lim = FindFilter("f_out_limiter");
        if (id == "f_out_compressor") comp->enabled = enabled;
        if (id == "f_out_limiter") lim->enabled = enabled;
        m_enableOuterStabilizer = comp->enabled && lim->enabled;
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::Sync, std::cout << " Outer Stabilizer State updated -> " + std::string(enabled ? "ON" : "OFF"));
    } 
    else if (id == "f_vol_booster") {
        m_enableOuterBooster = enabled;
        f->enabled = enabled;
        LOG_NO_KEY(1, LogLevel::Info, LogCategory::Sync, std::cout << "Outer Booster State updated -> " + std::string(enabled ? "ON" : "OFF"));
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
                    LOG_NO_KEY(1, LogLevel::Warning, LogCategory::AI, std::cout << "Auto-disabled " + other.id);
                }
            }
        }
    }
    EvaluateSystemSafety();
    SyncAll(); 
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::Sync, std::cout << "Toggled filter '" + id + "' -> " + (enabled ? "ON" : "OFF"));
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
            LOG_NO_KEY(1, LogLevel::Info, LogCategory::System, std::cout << "Attempting to recover and reinstall filter: " + id);
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