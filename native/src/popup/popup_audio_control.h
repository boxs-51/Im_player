#pragma once

#include <string>
#include <array>
#include <mpv/client.h>

// Audio control presets
enum class AudioPreset {
    Normal = 0,
    BassBoost = 1,
    TrebleBoost = 2,
    Flat = 3,
    Custom = 4
};

struct AudioControlState {
    float master_volume = 100.0f;    // 0-100%
    float bass = 0.0f;               // -24 to +24 dB
    float treble = 0.0f;             // -24 to +24 dB
    std::array<float, 5> eq_bands = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // 60Hz, 250Hz, 1kHz, 4kHz, 16kHz (-24 to +24 dB)
    bool normalize = false;           // Audio normalization
    bool surround_enabled = false;    // Surround/Stereo mode
    AudioPreset current_preset = AudioPreset::Normal;
    
    // Serialize to string for MPV
    std::string ToString() const;
};

class AudioControlManager {
public:
    static AudioControlManager& Instance();
    
    void Init(mpv_handle* handle);
    void Shutdown();
    
    // Control methods
    void SetMasterVolume(float volume);           // 0-100%
    void SetBass(float db);                       // -24 to +24 dB
    void SetTreble(float db);                     // -24 to +24 dB
    void SetEQBand(int band_idx, float db);       // band 0-4, -24 to +24 dB
    void SetNormalize(bool enabled);
    void SetSurroundMode(bool enabled);
    
    // Preset management
    void ApplyPreset(AudioPreset preset);
    void SavePreset(const std::string& name);
    void LoadPreset(const std::string& name);
    
    // State getters
    const AudioControlState& GetState() const { return state_; }
    AudioControlState& GetMutableState() { return state_; }
    
    // Sync to MPV
    void SyncToMPV();
    void SaveToFile();
    void LoadFromFile();
    
private:
    AudioControlManager() = default;
    ~AudioControlManager() = default;
    
    AudioControlManager(const AudioControlManager&) = delete;
    void operator=(const AudioControlManager&) = delete;
    
    mpv_handle* mpv_handle_ = nullptr;
    AudioControlState state_;
    std::string config_path_;
};

// UI rendering functions
void DrawAudioControlPanel();
void OpenAudioControlPopup();
void RenderAudioControlPopup(bool& is_open);
