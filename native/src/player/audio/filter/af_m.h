#pragma once
#include "af_m_types.h"
#include "utils.h"

#include <client.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <vector>

class PlayerStateSystem;
class AudioFilterManager {
public:
    AudioFilterManager();
    ~AudioFilterManager();

    AudioFilterManager(const AudioFilterManager&) = delete;
    AudioFilterManager& operator=(const AudioFilterManager&) = delete;

    void Init(mpv_handle* h, PlayerStateSystem* stateSystem = nullptr);
    void AttachPlayer(mpv_handle* h, PlayerStateSystem* stateSystem = nullptr);
    void DetachPlayer();
    
    void ToggleFilter(const std::string& id, bool state);
    void SetAllFiltersState(bool enabled);
    bool IsFilterEnabled(const std::string& id);
    void RecoverFailedFilter(const std::string& id);
    int GetActiveFilterCount();

    void SetAdaptiveMode(bool enabled, AudioPreset preset = AudioPreset::Flat);
    bool IsAdaptiveMode() const { return m_autoMode; }
    AudioPreset GetCurrentPreset() const { return m_currentPreset; }
    void SetCurrentPreset(AudioPreset preset);

    void UpdateAdaptiveFilters();
    const AudioContext& GetCurrentContext() const { return m_currentContext; }

    void SetFilterBypassMode(const std::string& id, bool bypassState);
    bool IsFilterBypassMode(const std::string& id);
    void SetGlobalBypassMode(bool bypassState);
    bool IsGlobalBypassEnabled() const { return m_globalBypass; }

    void SetOuterStabilizerEnabled(bool enabled);
    void SetOuterBoosterEnabled(bool enabled);
    
    void UpdateParam(const std::string& id, const std::string& key, float value);
    void BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates);
    
    void ResetFilter(const std::string& id);
    void ResetAllToDefaults();
    
    void SaveToFile();
    void LoadFromFile();
    
    void SetChannelMode(const std::string& mode);
    std::string GetChannelMode() const { return m_channelMode; }
    
    void AddAudioTrack(const std::string& trackId, const std::string& lang, const std::string& codec);
    void SelectAudioTrack(const std::string& trackId);
    std::string GetCurrentAudioTrack() const { return m_currentAudioTrack; }
    const std::vector<AudioTrackInfo>& GetAudioTracks() const { return m_audioTracks; }

    AudioFilter* FindFilter(const std::string& id);
    const std::vector<AudioFilter>& GetFilters() const { return m_filters; }

    mpv_handle* GetMpvHandle() { return mpv; }

public:
    // Các biến quản lý độc lập được chuyển sang public để UI dễ tương tác
    bool m_autoMode;                 // Chế độ Auto của AI toàn cục
    bool m_enableOuterStabilizer;    // Quản lý riêng bộ Ổn định ngoại vi (f_out_compressor & f_out_limiter)
    bool m_enableOuterBooster;       // Quản lý riêng bộ Tăng cường âm lượng ngoài (f_vol_booster)
    SpecializedFilterState m_specializedFilterState; // Quản lý trạng thái các bộ lọc chuyên biệt

private:
    PlayerStateSystem* m_stateSystem = nullptr;

    void AddFilter(const std::string& id, const std::string& name, const std::string& group, const std::string& description, bool ai_controllable);
    void RegisterParam(const std::string& id, const std::string& key, float min, float max, float def, bool ai_controllable);
    
    void EvaluateSystemSafety();
    void SyncAll();

private:

    AudioContext ExtractCurrentContext();
    bool CheckEnvironmentHysteresis(const AudioContext& ctx);
    AdaptiveTargets AnalyzeContextAndCalculateTargets(const AudioContext& ctx);

    void DispatchParametersToMPV(bool need_sync_structure, bool parameter_changed);

private:
    
    mpv_handle* mpv;
    std::string path;
    std::string m_channelMode;
    std::string m_currentAudioTrack;
    
    std::vector<AudioFilter> m_filters;
    std::unordered_map<std::string, size_t> m_filterIndex; 
    std::vector<AudioTrackInfo> m_audioTracks;

    std::vector<std::pair<std::string, std::string>> m_eqBands;
    
    bool m_globalBypass = false; 
    AudioPreset m_currentPreset = AudioPreset::Flat;

    double m_smoothedTruePeak = 0.0f;
    double m_smoothedShortTerm = 0.0f;
    double m_smoothedSamplePeak = 0.0f;

    double m_lastVolume = -999.0;
    double m_lastBitrate = -999.0;
    double m_lastSpeed = -1.0;
    int64_t m_lastChannels = -1;

    
    AudioPreset m_lastPreset = static_cast<AudioPreset>(-1);
    std::chrono::steady_clock::time_point m_lastUpdateTime = std::chrono::steady_clock::now();

    AudioContext m_currentContext;
};
