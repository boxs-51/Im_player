#include "af_m.h"
#include "af_m_log.h"

#include <sstream>
#include <iomanip>

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

    for (const auto& f : m_filters) {
        if (!f.enabled) continue; 
        if ((f.name == "equalizer" || f.name == "bass" || f.name == "treble") && f.params.count("g")) {
            float g_val = f.params.at("g").current;
            if (g_val > 0.0f) totalGainAccumulation += g_val; 
        }
    }

    auto* booster_node = FindFilter("f_vol_booster");
    if (booster_node && m_enableOuterBooster && booster_node->enabled) {
        if (booster_node->params.count("volume")) {
            float boost_val = booster_node->params.at("volume").current;
            if (boost_val > 1.0f) totalGainAccumulation += 20.0f * std::log10(boost_val);
        }
    }

    auto* vol_node = FindFilter("f_volume");
    if (vol_node) {
        const float GAIN_THRESHOLD = 18.0f; 
        float userTarget = vol_node->params["volume"].user_target;
        if (totalGainAccumulation > GAIN_THRESHOLD) {
            float safetyReduction = -(totalGainAccumulation - GAIN_THRESHOLD) * 0.6f;
            float targetVol = std::clamp(userTarget + safetyReduction, -60.0f, userTarget);
            
            if (std::abs(vol_node->params["volume"].current - targetVol) > 0.05f) {
                vol_node->params["volume"].current = targetVol;
                LOG(1, LogLevel::Warning, LogCategory::Safety, "Danger! Accumulated Gain reached %.2f dB. Limiting Master to: %.2f dB", totalGainAccumulation, targetVol);
            }
        } else {
            if (vol_node->params["volume"].current < userTarget) {
                float current_vol = vol_node->params["volume"].current;
                if (totalGainAccumulation > (GAIN_THRESHOLD - 3.0f)) return;

                float recoveryVol = current_vol + 0.2f; 
                if (recoveryVol >= userTarget - 0.05f || totalGainAccumulation <= 6.0f) recoveryVol = userTarget;

                vol_node->params["volume"].current = recoveryVol; 
                if (recoveryVol == userTarget) {
                    LOG(1, LogLevel::Info, LogCategory::Safety, "System stabilized. Master Node returned to normal.");
                }
            }
        }
    }
}

void AudioFilterManager::SetOuterStabilizerEnabled(bool enabled) {
    m_enableOuterStabilizer = enabled;
    if (auto* comp = FindFilter("f_out_compressor")) comp->enabled = enabled;
    if (auto* lim = FindFilter("f_out_limiter")) lim->enabled = enabled;
    LOG(1, LogLevel::Info, LogCategory::Safety, "Outer Safety System changed to -> %s", enabled ? "ON" : "OFF");
    EvaluateSystemSafety();
    SyncAll();
}

void AudioFilterManager::SetOuterBoosterEnabled(bool enabled) {
    m_enableOuterBooster = enabled;
    if (auto* boost = FindFilter("f_vol_booster")) boost->enabled = enabled;
    LOG(1, LogLevel::Info, LogCategory::Safety, "Outer Volume Booster changed to -> ", enabled ? "ON" : "OFF");
    EvaluateSystemSafety();
    SyncAll();
}

void AudioFilterManager::SetFilterBypassMode(const std::string& id, bool bypassState) {
    if (auto* f = FindFilter(id)) {
        f->isBypassManagement = bypassState;
        LOG(1, LogLevel::Info, LogCategory::Bypass, "Filter %s set Bypass Mode -> %s", id, bypassState ? "ON" : "OFF");
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
    LOG(1, LogLevel::Info, LogCategory::Bypass, "GLOBAL Bypass Manager set -> ", bypassState ? "ENABLED" : "DISABLED");
    if (m_globalBypass) {
        m_autoMode = false; 
        SetAdaptiveMode(m_autoMode, m_currentPreset);
    }
    EvaluateSystemSafety();
    SyncAll();
}