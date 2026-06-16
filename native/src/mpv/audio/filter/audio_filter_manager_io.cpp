#include "audio_filter_manager.h"
#include <fstream>

void AudioFilterManager::SaveToFile() {
    if (path.empty()) return;
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "[SystemMetadata]\n";
    f << "auto_mode:" << (m_autoMode ? "1" : "0") << "\n";
    f << "global_bypass:" << (m_globalBypass ? "1" : "0") << "\n";
    f << "channel_mode:" << m_channelMode << "\n";
    f << "current_preset:" << static_cast<int>(m_currentPreset) << "\n";
    f << "outer_stabilizer_manager:" << (m_enableOuterStabilizer ? "1" : "0") << "\n";
    f << "outer_booster_manager:" << (m_enableOuterBooster ? "1" : "0") << "\n\n";

    for (const auto& filter : m_filters) {
        f << "[Filter]:" << filter.id << "|" << (filter.enabled ? "1" : "0") << "|" << (filter.isBypassManagement ? "1" : "0") << "\n";
        for (const auto& [key, p] : filter.params) {
            float value_to_save = m_autoMode ? p.user_target : p.current;
            f << key << ":" << value_to_save << "\n";
        }
    }
    AddLog("[Config Storage] Successfully saved configuration.", LogLevel::Info);
}

void AudioFilterManager::ResetAllToDefaults() {
    for (auto& filter : m_filters) {
        filter.enabled = false;
        for (auto& [key, p] : filter.params) {
            p.current = p.def; p.user_target = p.def;
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
        if (line.rfind("auto_mode:", 0) == 0) { target_auto_mode = (line.substr(10) == "1"); continue; }
        if (line.rfind("global_bypass:", 0) == 0) { m_globalBypass = (line.substr(14) == "1"); continue; }
        if (line.rfind("channel_mode:", 0) == 0) { m_channelMode = line.substr(13); continue; }
        if (line.rfind("current_preset:", 0) == 0) { target_preset = static_cast<AudioPreset>(std::stoi(line.substr(15))); continue; }
        if (line.rfind("outer_stabilizer_manager:", 0) == 0) { m_enableOuterStabilizer = (line.substr(25) == "1"); continue; }
        if (line.rfind("outer_booster_manager:", 0) == 0) { m_enableOuterBooster = (line.substr(22) == "1"); continue; }

        if (line.rfind("[Filter]:", 0) == 0) {
            size_t d1 = line.find('|');
            size_t d2 = line.find('|', d1 + 1);
            if (d1 != std::string::npos) {
                current_id = line.substr(9, d1 - 9);
                std::string enabled_str = (d2 == std::string::npos) ? line.substr(d1 + 1) : line.substr(d1 + 1, d2 - d1 - 1);
                if (auto* filter = FindFilter(current_id)) {
                    filter->enabled = (enabled_str == "1");
                    filter->isBypassManagement = (d2 != std::string::npos && line.substr(d2 + 1) == "1");
                    parse_success = true;
                } else { current_id = ""; }
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
                            if (current_id == "f_vol_booster" && key == "volume" && val > 1.5f) val /= 100.0f;
                            param.user_target = std::clamp(val, param.min, param.max);
                            param.current = param.user_target;
                        }
                    }
                } catch (...) { continue; }
            }
        }
    }
    if (!parse_success) ResetAllToDefaults();
    else { 
        m_currentPreset = target_preset; 
        SyncAll();
        SetAdaptiveMode(target_auto_mode, m_currentPreset); 
    }
}