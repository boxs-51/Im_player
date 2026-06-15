#pragma once
#include "audio_filter_types.h"
#include "utils.h"
#include <mpv/mpv_data.h>
#include <mpv/client.h>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

class AudioFilterManager {
public:
    static AudioFilterManager& Instance();

    AudioFilterManager(const AudioFilterManager&) = delete;
    AudioFilterManager& operator=(const AudioFilterManager&) = delete;

    void Init(mpv_handle* h);
    
    void ToggleFilter(const std::string& id, bool state);
    void SetAllFiltersState(bool enabled);
    bool IsFilterEnabled(const std::string& id);
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

    void AddLog(const std::string& message, LogLevel level = LogLevel::Info);
    const std::vector<LogEntry>& GetLogs();
    void ClearLogs();

private:
    std::vector<LogEntry> m_logs;
    std::mutex m_logMutex;
    const size_t MAX_LOG_SIZE = 100;
    
private:
    AudioFilterManager() : mpv(nullptr), m_channelMode("stereo"), m_autoMode(false), m_enableOuterStabilizer(true), m_enableOuterBooster(true) {}
    ~AudioFilterManager() {
        StopAudioAnalysis();
        CleanupAudioAnalysis();
    };

    void AddFilter(const std::string& id, const std::string& name, const std::string& group);
    void RegisterParam(const std::string& id, const std::string& key, float min, float max, float def);
    
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

    MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    VideoInfo& g_videoInfo = GetVideoInfo();
    
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

public:
 
    // Khởi động và dừng luồng phân tích AI ngầm
    void StartAudioAnalysis();
    void StopAudioAnalysis();

    // Hàm để tầng nhận dữ liệu (MPV Callback hoặc Microphone) bơm dữ liệu PCM vào hệ thống
    void PushAudioSamples(const float* samples, size_t sampleCount);
    void FlushAnalysisPipeline();
    void CleanupAudioAnalysis();
    // Kiểm tra xem hệ thống phân tích âm thanh có đang chạy không
    bool IsAudioAnalysisRunning() const { 
        return !m_stopAnalysis && const_cast<std::thread&>(m_analysisThread).joinable(); 
    }

    // Lấy số lượng chunk hiện tại đang xếp hàng chờ AI xử lý (Dùng để đo tải hệ thống)
    size_t GetAnalysisQueueSize() { 
        std::lock_guard<std::mutex> lock(m_analysisMutex); 
        return m_audioDataQueue.size(); 
    }
    float GetAnalysisRMS() const { return m_analysisRMS.load(); }
    float GetAnalysisPeak() const { return m_analysisPeak.load(); }
    uint64_t GetTotalSamplesCaptured() const { return m_totalSamplesCaptured.load(); }
    
    std::vector<float> GetRMSHistory() {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        return m_rmsHistory;
    }
    std::string GetCurrentSubtitle() {
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        return m_currentSubtitle;
    }
private:
private:
#ifdef _WIN32
    HANDLE m_hAudioPipe = INVALID_HANDLE_VALUE;
#endif

    // --- BỔ SUNG CHO CỬA SỔ TRƯỢT WHISPER ---
    std::vector<float> m_rollingSpeechBuffer;      // Bộ đệm cuốn tích lũy âm thanh
    size_t m_samplesSinceLastInference = 0;        // Bộ đếm mẫu để biết khi nào đủ 500ms

    std::mutex m_subtitleMutex;                    // Mutex bảo vệ chuỗi chữ sub
    std::string m_currentSubtitle;                 // Chuỗi sub hiện tại để ImGui vẽ

    // Các biến giám sát Telemetry (Thời gian thực)
    std::atomic<float> m_analysisRMS{0.0f};
    std::atomic<float> m_analysisPeak{0.0f};
    std::atomic<uint64_t> m_totalSamplesCaptured{0};

    // Vùng lưu lịch sử biên độ phục vụ vẽ biểu đồ đồ họa ImGui (Cần mutex bảo vệ)
    std::mutex m_historyMutex;
    std::vector<float> m_rmsHistory = std::vector<float>(100, 0.0f);

    // Thành phần quản lý Đa luồng (Multi-threading) cho AI
    std::thread m_analysisThread;
    std::mutex m_analysisMutex;
    std::condition_variable m_analysisCV;
    bool m_stopAnalysis = false;

    // Hàng đợi lưu trữ các block âm thanh thô (PCM 32-bit Float)
    std::queue<std::vector<float>> m_audioDataQueue;

    // Vòng lặp chạy ngầm của luồng AI
    void AIAnalysisLoop();
};
