#pragma once
#include "globals.h"
#include "utils.h"
#include <GL/gl3w.h> 
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

struct AppSettings {
    std::string selectedFormat;
    std::string selectedAudio;
    std::string selectedResolution;     // VD: "bestvideo[height<=720]+bestaudio"
    std::vector<std::string> resLabels; // Hiển thị: "Auto", "144p", ...
    std::vector<std::string> resFormats;// Tương ứng: "best", "bestvideo[height<=360]+bestaudio"    
    bool enableSubtitles = true;
    float playbackSpeed = 1.0f;
    float audiodelay = 0.0f;
    bool repeatVideo = false;
    bool autoPlayNext = false;
    int defaultVolume = 100;    
    bool repeatlist = false;
};
enum class Theme {DarkMode,LightMode};

inline std::string ThemeToString(Theme t) {
    switch (t) {
        case Theme::DarkMode:    return "DarkMode";
        case Theme::LightMode:   return "LightMode";
        default:                 return "DarkMode";
    }
}
inline Theme StringToTheme(const std::string& s) {
    if (s == "DarkMode")    return Theme::DarkMode;
    if (s == "LightMode")   return Theme::LightMode;

    return Theme::DarkMode; // fallback
}

struct CommonSettings {

    Theme theme = Theme::DarkMode;

    std::string Videofilter = "";
    std::string Audiofilter = "";
    std::string VideoDecode = "";
    std::string AudioDecode = "";

    int fontsize = 16;
};


std::string CleanUrl(const std::string& url);
std::string Sha1Hash(const std::string& input);
std::string CreateGroupIDFromFormats(const std::vector<std::string>& formats);


extern AppSettings v_Settings;
extern CommonSettings c_Settings;  


void LoadSettings();
void LoadSettings_Video();
void SaveSettings_Video();
void LoadSettings_Common();
void SaveSettings_Common();


