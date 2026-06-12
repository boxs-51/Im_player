#include "audio_filter_manager.h"
#include <sstream>
#include <iomanip>

void AudioFilterManager::SyncAll() {
    if (!mpv) return;
    std::string full_af = "";

    if (m_channelMode == "mono") full_af += "pan=mono|c0=0.5*c0+0.5*c1";
    else if (m_channelMode == "surround") full_af += "pan=5.1|FL=c0|FR=c1|FC=c0+c1|LFE=0|BL=c0|BR=c1";

    for (const auto& f : m_filters) {
        if (f.enabled) {
            if (f.id == "f_vol_booster" || f.id == "f_out_compressor" || f.id == "f_out_limiter") continue;

            std::string init_str = "";
            if (f.name == "equalizer") {
                std::string freq = "1000"; // Tần số mặc định phòng hờ
                
                // Tự động tìm freq tương ứng với f.id trong cấu hình chung
                for (const auto& [band_id, band_freq] : m_eqBands) {
                    if (band_id == f.id) {
                        freq = band_freq;
                        break;
                    }
                }

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

    auto* vol_boost = FindFilter("f_vol_booster");
    if (vol_boost && m_enableOuterBooster) {
        vol_boost->enabled = true;
        if (!full_af.empty()) full_af += ",";
        full_af += vol_boost->GetInitString();
    }

    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim = FindFilter("f_out_limiter");
    if(m_enableOuterStabilizer) {
        if(out_comp) out_comp->enabled = true;
        if(out_lim) out_lim->enabled = true;
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
    full_af += "@ebur_measurer:lavfi=[ebur128=metadata=1:peak=all]";
    
    
    mpv_set_property_string(mpv, "af", full_af.c_str());
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    if (auto* f = FindFilter(id)) {
        auto it = f->params.find(key);
        if (it != f->params.end()) { 
            auto& param = it->second;

            value = std::clamp(value, param.min, param.max);
            param.user_target = value;

            // Kiểm tra: Chỉ bỏ qua nếu current và lastSent ĐỀU ĐÃ khớp với value
            if (std::abs(param.current - value) < 0.0001f && 
                std::abs(param.lastSent - value) < 0.0001f) {
                return; 
            }
            
            param.current = value;
            
            if (key == "g" || key == "volume") EvaluateSystemSafety();

            if (f->enabled && mpv) {
                float value_to_send = param.current;
                
                if (f->name == "acompressor" && key == "threshold") {
                    value_to_send = std::pow(10.0f, param.current / 20.0f);
                    if (value_to_send < 0.000976563f) value_to_send = 0.000976563f;
                }
                if (f->id == "f_volume" && key == "volume") {
                    value_to_send = std::pow(10.0f, param.current / 20.0f);
                }

                std::ostringstream ss;
                ss.imbue(std::locale("C")); // FIX: Ép sử dụng dấu chấm '.' cho số thập phân thay vì dấu phẩy ','

                if ((f->name == "acompressor" && (key == "threshold" || key == "makeup")) || f->id == "f_vol_booster") {
                    ss << std::fixed << std::setprecision(5) << value_to_send;
                } else {
                    ss << std::fixed << std::setprecision(2) << value_to_send;
                }
                std::string val_str = ss.str();

                const char* cmd[] = {"af-command", id.c_str(), key.c_str(), val_str.c_str(), NULL};
                mpv_command(mpv, cmd);

                // CHỈ cập nhật lastSent khi đã thực sự gửi lệnh thành công
                param.lastSent = param.current; 
            } 
        }
    }
}

void AudioFilterManager::BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates) {
    bool has_changed = false;
    for (const auto& [id, key, value] : updates) {
        if (auto* f = FindFilter(id)) {
            if (f->params.count(key)) {
                auto& param = f->params[key];
                float clamped = std::clamp(value, param.min, param.max);
                param.user_target = clamped;
                if (param.current != clamped) {
                    param.current = clamped;
                    param.lastSent = clamped;
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

void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def) {
    if (auto* f = FindFilter(id)) f->params[key] = {def, min, max, def, def}; 
}

void AudioFilterManager::ResetFilter(const std::string& id) {
    if (auto* f = FindFilter(id)) {
        for (auto& [key, p] : f->params) {
            p.current = p.def;
            p.user_target = p.def;
            p.lastSent = p.def;
        }
        if (f->enabled) SyncAll();
    }
}

void AudioFilterManager::SetAllFiltersState(bool enabled) {
    bool changed = false;
    for (auto& f : m_filters) {
        if (f.enabled != enabled) { f.enabled = enabled; changed = true; }
    }
    SetAdaptiveMode(enabled, m_currentPreset);
    if (changed) { EvaluateSystemSafety(); SyncAll(); }
}