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

    // Hàm tạo chuỗi khởi tạo cho MPV. 
    // Trả về định dạng: @id:name=key1=val1:key2=val2
    std::string GetInitString() const {
        std::string res = "@" + id + ":" + name;
        if (!params.empty()) {
            res += "=";
            bool first = true;
            for (const auto& [key, p] : params) {
                if (!first) res += ":";
                res += key + "=" + std::to_string(p.current);
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
    
    void AddFilter(const std::string& id, const std::string& name);
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

private:
    AudioFilterManager() = default;
    mpv_handle* mpv = nullptr;
    std::vector<AudioFilter> m_filters;
    std::map<std::string, int> m_filterIndex; // Cache for O(1) lookups
    std::vector<std::pair<std::string, std::string>> m_audioTracks; // (trackId, language)
    std::string m_currentAudioTrack;
    std::string m_channelMode = "stereo";
    bool m_needsSync = false; // Track if sync is needed

    std::string path ;//= { AutoPath<std::string>("%ROOT%" ,"data" ,"fillter_audio.json")};

};