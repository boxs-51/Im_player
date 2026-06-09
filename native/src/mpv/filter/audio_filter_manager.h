#pragma once
#include "utils.h"
#include "globals.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <mpv/client.h>

// Cấu trúc lưu trữ thông số của từng Parameter
struct FilterParam {
    float current;
    float min;
    float max;
    float def;
};

// Cấu trúc của một Audio Filter
struct AudioFilter {
    std::string id;       // Nhãn định danh (VD: f_bass)
    std::string name;     // Tên filter thực tế của libavfilter (VD: bass)
    bool enabled;
    std::map<std::string, FilterParam> params;
    std::string group; // Nhóm filter (VD: "Equalizer", "Dynamics", "Special")

    // Hàm tạo chuỗi khởi tạo cho MPV. 
    // Trả về định dạng: @id:name=key1=val1:key2=val2
    std::string GetInitString() const {
        std::string res = "@" + id + ":" + name;
        if (!params.empty()) {
            
            // 1. XỬ LÝ ĐẶC BIỆT CHO CHORUS: Ép cấu hình mảng tránh lỗi "Both delays & decays..."
            if (name == "chorus") {
                float in_gain = params.count("in_gain") ? params.at("in_gain").current : 0.40f;
                float out_gain = params.count("out_gain") ? params.at("out_gain").current : 0.40f;
                in_gain = std::clamp(in_gain, 0.0f, 1.0f);
                out_gain = std::clamp(out_gain, 0.0f, 1.0f);

                // Cú pháp chuẩn: chorus=in_gain:out_gain:delays:decays:speeds:depths
                res += "=" + std::to_string(in_gain) + ":" + std::to_string(out_gain) + ":40:0.25:0.3:2.0";
                return res;
            }

            // 2. XỬ LÝ ĐẶC BIỆT CHO FLANGER: Tránh lỗi phân tách tham số dạng chuỗi phức tạp của FFmpeg
            /*
            if (name == "flanger") {
                float delay = params.count("delay") ? params.at("delay").current : 0.0f;
                float depth = params.count("depth") ? params.at("depth").current : 2.0f;
                delay = std::clamp(delay, 0.0f, 30.0f);
                depth = std::clamp(depth, 0.0f, 10.0f);

                // Cú pháp định danh vị trí FFmpeg: flanger=delay:depth:regen:width:speed:shape:phase
                // Gán regen=70.0 (feedback), width=71.0, speed=0.5 (Hz), shape=sinusoidal, phase=90.0
                res += "=" + std::to_string(delay) + ":" + std::to_string(depth) + ":70:71:0.5:quad:90";
                return res;
            }*/

            // 3. Cấu trúc lặp sinh chuỗi Key-Value thông thường cho các bộ lọc còn lại
            res += "=";
            bool first = true;
            for (const auto& [key, p] : params) {
                if (!first) res += ":";
                
                float final_value = p.current;
                
                // Chuyển đổi dB sang tuyến tính (Linear Coefficient) cho acompressor threshold
                if (name == "acompressor" && key == "threshold") {
                    final_value = std::pow(10.0f, p.current / 20.0f);
                    final_value = std::clamp(final_value, 0.00001f, 1.0f);
                }

                res += key + "=" + std::to_string(final_value);
                first = false;
            }
        }
        return res;
    }

};

class AudioFilterManager {
public: 

    static AudioFilterManager& Instance();
    AudioFilterManager(const AudioFilterManager&) = delete;
    void operator=(const AudioFilterManager&) = delete;

    void Init(mpv_handle* h);
    
    void AddFilter(const std::string& id, const std::string& name, const std::string& group);
    void RegisterParam(const std::string& id, const std::string& key, float min, float max, float def);
    
    void ResetFilter(const std::string& id);
    void ToggleFilter(const std::string& id, bool state);
    void SetFilterEnabled(const std::string& id, bool enabled);
    void SetAllFiltersState(bool enabled);
    bool IsFilterEnabled(const std::string& id);
    int  GetActiveFilterCount();
    void ResetAllToDefaults();
    void UpdateParam(const std::string& id, const std::string& key, float value);
    void BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates);
    
    void SyncAll();
    void SaveToFile();
    void LoadFromFile();
    
    // New methods for audio track management
    void AddAudioTrack(const std::string& trackId, const std::string& lang, const std::string& codec);
    void SelectAudioTrack(const std::string& trackId);
    void SetChannelMode(const std::string& mode); // "mono", "stereo", "surround"
    const std::vector<std::pair<std::string, std::string>>& GetAudioTracks() const { return m_audioTracks; }
    
    AudioFilter* FindFilter(const std::string& id);
    const std::vector<AudioFilter>& GetFilters() const { return m_filters; }

    std::string GetChannelMode() const { return m_channelMode; }
    std::string GetCurrentAudioTrack() const { return m_currentAudioTrack; }

private:
    AudioFilterManager() = default;
    mpv_handle* mpv = nullptr;
    std::vector<AudioFilter> m_filters;
    std::map<std::string, int> m_filterIndex; // Cache for O(1) lookups
    std::vector<std::pair<std::string, std::string>> m_audioTracks; // (trackId, language)
    std::string m_currentAudioTrack;
    std::string m_channelMode = "stereo";
    bool m_needsSync = false; // Track if sync is needed

    std::string path;

};