#include "popup_audio_control.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <log.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==================== AudioControlState ====================

std::string AudioControlState::ToString() const {
    std::stringstream ss;
    
    // Build filter chain for MPV af property
    std::vector<std::string> filters;
    
    // Add equalizer (5-band EQ)
    std::stringstream eq_params;
    eq_params << "f=";
    // Format: frequency in Hz, gain in dB, bandwidth in octaves
    // 60 Hz, 250 Hz, 1 kHz, 4 kHz, 16 kHz
    const float freqs[] = {60.0f, 250.0f, 1000.0f, 4000.0f, 16000.0f};
    const float bandwidth = 0.9f; // Standard bandwidth for graphic EQ
    
    for (int i = 0; i < 5; i++) {
        if (i > 0) eq_params << " ";
        eq_params << freqs[i] << "Hz:" << eq_bands[i] << "dB:" << bandwidth << "oct";
    }
    filters.push_back("@eq:equalizer=" + eq_params.str());
    
    // Add bass filter
    if (std::abs(bass) > 0.01f) {
        std::stringstream bass_params;
        bass_params << "g=" << bass << ":f=100:w_type=h";
        filters.push_back("@bass:bass=" + bass_params.str());
    }
    
    // Add treble filter
    if (std::abs(treble) > 0.01f) {
        std::stringstream treble_params;
        treble_params << "g=" << treble << ":f=10000:w_type=h";
        filters.push_back("@treble:treble=" + treble_params.str());
    }
    
    // Add normalization
    if (normalize) {
        filters.push_back("@norm:loudnorm=I=-23:TP=-1.5:LRA=11");
    }
    
    // Add surround upmix if enabled
    if (surround_enabled) {
        filters.push_back("@surround:pan=stereo|FL=0.5*FL+0.707*FC+0.707*SL|FR=0.5*FR+0.707*FC+0.707*SR");
    }
    
    // Join all filters
    for (size_t i = 0; i < filters.size(); i++) {
        ss << filters[i];
        if (i < filters.size() - 1) ss << ",";
    }
    
    return ss.str();
}

// ==================== AudioControlManager ====================

AudioControlManager& AudioControlManager::Instance() {
    static AudioControlManager instance;
    return instance;
}

void AudioControlManager::Init(mpv_handle* handle) {
    mpv_handle_ = handle;
    config_path_ = AutoPath<std::string>("%ROOT%", "data", "audio_control.json");
    LoadFromFile();
}

void AudioControlManager::Shutdown() {
    SaveToFile();
    mpv_handle_ = nullptr;
}

void AudioControlManager::SetMasterVolume(float volume) {
    state_.master_volume = std::clamp(volume, 0.0f, 100.0f);
    if (mpv_handle_) {
        int volume_int = static_cast<int>(state_.master_volume);
        mpv_set_property(mpv_handle_, "volume", MPV_FORMAT_INT64, &volume_int);
    }
    SyncToMPV();
}

void AudioControlManager::SetBass(float db) {
    state_.bass = std::clamp(db, -24.0f, 24.0f);
    SyncToMPV();
}

void AudioControlManager::SetTreble(float db) {
    state_.treble = std::clamp(db, -24.0f, 24.0f);
    SyncToMPV();
}

void AudioControlManager::SetEQBand(int band_idx, float db) {
    if (band_idx >= 0 && band_idx < 5) {
        state_.eq_bands[band_idx] = std::clamp(db, -24.0f, 24.0f);
    }
    SyncToMPV();
}

void AudioControlManager::SetNormalize(bool enabled) {
    state_.normalize = enabled;
    SyncToMPV();
}

void AudioControlManager::SetSurroundMode(bool enabled) {
    state_.surround_enabled = enabled;
    SyncToMPV();
}

void AudioControlManager::ApplyPreset(AudioPreset preset) {
    state_.current_preset = preset;
    
    // Reset to defaults first
    state_.master_volume = 100.0f;
    state_.bass = 0.0f;
    state_.treble = 0.0f;
    state_.normalize = false;
    state_.surround_enabled = false;
    for (auto& band : state_.eq_bands) {
        band = 0.0f;
    }
    
    switch (preset) {
        case AudioPreset::Normal:
            // Already reset to defaults
            break;
            
        case AudioPreset::BassBoost:
            state_.bass = 12.0f;
            state_.treble = -6.0f;
            state_.eq_bands[0] = 6.0f;  // 60 Hz
            state_.eq_bands[1] = 6.0f;  // 250 Hz
            break;
            
        case AudioPreset::TrebleBoost:
            state_.treble = 12.0f;
            state_.bass = -6.0f;
            state_.eq_bands[3] = 6.0f;  // 4 kHz
            state_.eq_bands[4] = 6.0f;  // 16 kHz
            break;
            
        case AudioPreset::Flat:
            // All zeros, no normalization
            state_.normalize = false;
            break;
            
        case AudioPreset::Custom:
            // Keep current state
            break;
    }
    
    SyncToMPV();
}

void AudioControlManager::SavePreset(const std::string& name) {
    json presets_json;
    std::string presets_path = AutoPath<std::string>("%ROOT%", "data", "audio_presets.json");
    
    // Load existing presets
    if (fs::exists(presets_path)) {
        try {
            std::ifstream file(presets_path);
            presets_json = json::parse(file);
        } catch (...) {
            presets_json = json::object();
        }
    }
    
    // Save current state as preset
    json preset_data = {
        {"master_volume", state_.master_volume},
        {"bass", state_.bass},
        {"treble", state_.treble},
        {"eq_bands", state_.eq_bands},
        {"normalize", state_.normalize},
        {"surround_enabled", state_.surround_enabled}
    };
    
    presets_json[name] = preset_data;
    
    try {
        std::ofstream file(presets_path);
        file << presets_json.dump(2);
    } catch (const std::exception& e) {
        LOG(logERROR) << "Failed to save audio preset: " << e.what();
    }
}

void AudioControlManager::LoadPreset(const std::string& name) {
    std::string presets_path = AutoPath<std::string>("%ROOT%", "data", "audio_presets.json");
    
    try {
        if (fs::exists(presets_path)) {
            std::ifstream file(presets_path);
            json presets_json = json::parse(file);
            
            if (presets_json.contains(name)) {
                auto& preset = presets_json[name];
                state_.master_volume = preset.value("master_volume", 100.0f);
                state_.bass = preset.value("bass", 0.0f);
                state_.treble = preset.value("treble", 0.0f);
                state_.normalize = preset.value("normalize", false);
                state_.surround_enabled = preset.value("surround_enabled", false);
                
                auto eq = preset.value("eq_bands", std::array<float, 5>{});
                for (size_t i = 0; i < 5 && i < eq.size(); i++) {
                    state_.eq_bands[i] = eq[i];
                }
                
                state_.current_preset = AudioPreset::Custom;
                SyncToMPV();
            }
        }
    } catch (const std::exception& e) {
        LOG(logERROR) << "Failed to load audio preset: " << e.what();
    }
}

void AudioControlManager::SyncToMPV() {
    if (!mpv_handle_) return;
    
    try {
        // Set master volume
        int volume_int = static_cast<int>(state_.master_volume);
        mpv_set_property(mpv_handle_, "volume", MPV_FORMAT_INT64, &volume_int);
        
        // Build and set audio filter chain
        std::string filter_string = state_.ToString();
        if (!filter_string.empty()) {
            const char* filter_c_str = filter_string.c_str();
            mpv_set_property(mpv_handle_, "af", MPV_FORMAT_STRING, &filter_c_str);
        } else {
            // Clear filters
            const char* empty = "";
            mpv_set_property(mpv_handle_, "af", MPV_FORMAT_STRING, &empty);
        }
    } catch (const std::exception& e) {
        LOG(logERROR) << "Failed to sync audio controls to MPV: " << e.what();
    }
}

void AudioControlManager::SaveToFile() {
    try {
        fs::create_directories(AutoPath<std::string>("%ROOT%", "data"));
        
        json config = {
            {"master_volume", state_.master_volume},
            {"bass", state_.bass},
            {"treble", state_.treble},
            {"eq_bands", state_.eq_bands},
            {"normalize", state_.normalize},
            {"surround_enabled", state_.surround_enabled},
            {"current_preset", static_cast<int>(state_.current_preset)}
        };
        
        std::ofstream file(config_path_);
        file << config.dump(2);
    } catch (const std::exception& e) {
        LOG(logERROR) << "Failed to save audio control settings: " << e.what();
    }
}

void AudioControlManager::LoadFromFile() {
    try {
        if (fs::exists(config_path_)) {
            std::ifstream file(config_path_);
            json config = json::parse(file);
            
            state_.master_volume = config.value("master_volume", 100.0f);
            state_.bass = config.value("bass", 0.0f);
            state_.treble = config.value("treble", 0.0f);
            state_.normalize = config.value("normalize", false);
            state_.surround_enabled = config.value("surround_enabled", false);
            
            auto eq = config.value("eq_bands", std::array<float, 5>{});
            for (size_t i = 0; i < 5 && i < eq.size(); i++) {
                state_.eq_bands[i] = eq[i];
            }
            
            int preset_int = config.value("current_preset", 0);
            state_.current_preset = static_cast<AudioPreset>(preset_int);
        }
    } catch (const std::exception& e) {
        LOG(logERROR) << "Failed to load audio control settings: " << e.what();
    }
}

// ==================== UI Functions ====================

void DrawAudioControlPanel() {
    auto& mgr = AudioControlManager::Instance();
    auto& state = mgr.GetMutableState();
    
    ImGui::SetNextItemWidth(300.0f);
    
    // Master Volume Slider (0-100%)
    if (ImGui::SliderFloat("Master Volume##vol", &state.master_volume, 0.0f, 100.0f, "%.0f%%")) {
        mgr.SetMasterVolume(state.master_volume);
    }
    
    ImGui::Separator();
    
    // Bass Control (-24 to +24 dB)
    if (ImGui::SliderFloat("Bass##bass", &state.bass, -24.0f, 24.0f, "%.1f dB")) {
        mgr.SetBass(state.bass);
    }
    
    // Treble Control (-24 to +24 dB)
    if (ImGui::SliderFloat("Treble##treble", &state.treble, -24.0f, 24.0f, "%.1f dB")) {
        mgr.SetTreble(state.treble);
    }
    
    ImGui::Separator();
    ImGui::Text("Equalizer Bands:");
    ImGui::Separator();
    
    // 5-Band EQ (60Hz, 250Hz, 1kHz, 4kHz, 16kHz)
    const char* band_labels[] = {"60 Hz", "250 Hz", "1 kHz", "4 kHz", "16 kHz"};
    for (int i = 0; i < 5; i++) {
        ImGui::SetNextItemWidth(250.0f);
        std::string slider_id = "##eq_" + std::to_string(i);
        if (ImGui::SliderFloat(slider_id.c_str(), &state.eq_bands[i], -24.0f, 24.0f, "%.1f dB")) {
            mgr.SetEQBand(i, state.eq_bands[i]);
        }
        ImGui::SameLine();
        ImGui::Text("%s", band_labels[i]);
    }
    
    ImGui::Separator();
    ImGui::Text("Audio Presets:");
    ImGui::Separator();
    
    // Presets Section
    if (ImGui::Button("Normal##preset", ImVec2(120.0f, 0.0f))) {
        mgr.ApplyPreset(AudioPreset::Normal);
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Bass Boost##preset", ImVec2(120.0f, 0.0f))) {
        mgr.ApplyPreset(AudioPreset::BassBoost);
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Treble Boost##preset", ImVec2(120.0f, 0.0f))) {
        mgr.ApplyPreset(AudioPreset::TrebleBoost);
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Flat##preset", ImVec2(120.0f, 0.0f))) {
        mgr.ApplyPreset(AudioPreset::Flat);
    }
    
    ImGui::Separator();
    
    // Normalize & Surround
    if (ImGui::Checkbox("Normalize Audio##norm", &state.normalize)) {
        mgr.SetNormalize(state.normalize);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Surround Mode##surr", &state.surround_enabled)) {
        mgr.SetSurroundMode(state.surround_enabled);
    }
    
    ImGui::Separator();
    
    // Save/Load buttons
    if (ImGui::Button("Save Settings##audio", ImVec2(150.0f, 0.0f))) {
        mgr.SaveToFile();
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Reset to Defaults##audio", ImVec2(150.0f, 0.0f))) {
        mgr.ApplyPreset(AudioPreset::Normal);
    }
}

// Static popup state
static bool g_audio_popup_open = false;

void OpenAudioControlPopup() {
    g_audio_popup_open = true;
}

void RenderAudioControlPopup(bool& is_open) {
    if (!is_open) return;
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f), ImGuiCond_Appearing);
    
    if (ImGui::Begin("Audio Controls##popup", &is_open, ImGuiWindowFlags_NoMove)) {
        DrawAudioControlPanel();
        ImGui::End();
    }
}
