#include "af_m.h"
#include "af_m_log.h"

#include <sstream>
#include <iomanip>

void AudioFilterManager::SyncAll() {
    if (!mpv) return;

    LOG(sync_all_trace, 500, std::cout << "[DEBUG] [AudioFilter] Syncing filter chain..." << std::endl);

    std::string full_af = "";

    if (m_channelMode == "mono") full_af += "pan=mono|c0=0.5*c0+0.5*c1";
    else if (m_channelMode == "surround") full_af += "pan=5.1|FL=c0|FR=c1|FC=c0+c1|LFE=0|BL=c0|BR=c1";

    auto append_filter = [&](AudioFilter* f, const std::string& custom_str = "") {
        if (!f || !f->enabled || f->isFailed) return; // BỎ QUA NẾU FILTER ĐÃ BỊ ĐÁNH DẤU LỖI
        
        std::string init_str = custom_str.empty() ? f->GetInitString() : custom_str;
        if (!init_str.empty()) {
            if (!full_af.empty()) full_af += ",";
            full_af += init_str;
        }
    };

    for (auto& f : m_filters) {
        if (f.id == "f_vol_booster" || f.id == "f_out_compressor" || f.id == "f_out_limiter" || 
            f.id == "f_ebur_measurer" || f.id == "f_ai_splitter") {
            continue;
        }

        if (f.enabled && !f.isFailed) {
            if (f.name == "equalizer") {
                std::string freq = "1000";
                for (const auto& [band_id, band_freq] : m_eqBands) {
                    if (band_id == f.id) { freq = band_freq; break; }
                }
                float gain = f.params.count("g") ? f.params.at("g").current : 0.0f;
                std::string eq_str = "@" + f.id + ":equalizer=f=" + freq + ":width_type=o:w=1.0:g=" + std::to_string(gain);
                append_filter(&f, eq_str);
            } else {
                append_filter(&f);
            }
        }
    }

    auto* vol_boost = FindFilter("f_vol_booster");
    if (vol_boost && m_enableOuterBooster) {
        vol_boost->enabled = true;
        append_filter(vol_boost);
    }

    auto* out_comp = FindFilter("f_out_compressor");
    auto* out_lim = FindFilter("f_out_limiter");
    if (m_enableOuterStabilizer) {
        if (out_comp) out_comp->enabled = true;
        if (out_lim) out_lim->enabled = true;
    }
    append_filter(out_comp);
    append_filter(out_lim);

    auto* ebur = FindFilter("f_ebur_measurer");
    if (ebur) {
        ebur->enabled = true;
        append_filter(ebur);
    }

    // =================================================================
    // CƠ CHẾ KIỂM TRA LỖI VÀ PHỤC HỒI (FALLBACK CRITICAL)
    // =================================================================
    
    // Thử áp dụng chuỗi filter đầy đủ lên MPV
    int error_code = mpv_set_property_string(mpv, "af", full_af.c_str());

    if (error_code < 0) { 
        // Lỗi xảy ra! Tiến hành cô lập filter lỗi.
        AddLog("[Architecture] Failed to apply full filter chain. Error code: " + std::to_string(error_code) + ". Initiating isolation...", LogLevel::Error);

        if (ebur && ebur->enabled && !ebur->isFailed) {
            ebur->isFailed = true;
            ebur->enabled = false;
            AddLog("[Architecture] Isolated 'f_ebur_measurer' due to initialization failure.", LogLevel::Warning);
            LOG(af_apply_error, 1000, std::cout << "[ERROR] [AudioFilter] Failed to apply chain. MPV Error: " << error_code << std::endl);
            SyncAll();
            return;
        }
        
        AddLog("[Architecture] Critical failure in standard filters. Clearing entire filter string to save core audio.", LogLevel::Error);
        mpv_set_property_string(mpv, "af", ""); 
    }
}

void AudioFilterManager::UpdateParam(const std::string& id, const std::string& key, float value) {
    if (auto* f = FindFilter(id)) {
        if (f->isFailed) return;
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
                char val_str[32];

                if ((f->name == "acompressor" && (key == "threshold" || key == "makeup")) || f->id == "f_vol_booster") {
                    snprintf(val_str, sizeof(val_str), "%.5f", value_to_send);
                } else {
                    snprintf(val_str, sizeof(val_str), "%.2f", value_to_send);
                }

                const char* cmd[] = {"af-command", id.c_str(), key.c_str(), val_str, NULL};
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

void AudioFilterManager::RegisterParam(const std::string& id, const std::string& key, float min, float max, float def, bool ai_controllable) {
    if (auto* f = FindFilter(id)) f->params[key] = {def, min, max, def, def, ai_controllable}; 
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
    if (changed) { 
        LOG(filter_state_change, 500, std::cout << "[INFO] [AudioFilter] All filters enabled: " << (enabled ? "TRUE" : "FALSE") << std::endl);
        EvaluateSystemSafety(); 
        SyncAll(); 
    }
}