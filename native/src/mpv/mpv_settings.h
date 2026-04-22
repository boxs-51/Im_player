#pragma once
#include <string>
#include <gui/gui.h>
#include <gui/gui_widgets.h> 
struct AppSettings {
    
    std::string selectedFormat = "";
    std::string selectedAudio = "";
    std::string selectedResolution = "";     // VD: "bestvideo[height<=720]+bestaudio"

    bool enableSubtitles = false;
    float playbackSpeed = 1.0f;
    float audiodelay = 0.0f;
    bool repeatVideo = false;
    bool autoPlayNext = false;
    int defaultVolume = 100;    
    bool repeatlist = false;
};

inline std::string ThemeToString(ThemeType t) {
    switch (t) {
        case ThemeType::DarkMode:    return "Dark Mode";
        case ThemeType::LightMode:   return "Light Mode";
        case ThemeType::MidnightMode:return "Mid Night Mode";
        case ThemeType::RetroMode:   return "Retro Mode";
        default:                 return "Dark Mode";
    }
}
inline ThemeType StringToTheme(const std::string& s) {
    if (s == "Dark Mode")       return ThemeType::DarkMode;
    if (s == "Light Mode")      return ThemeType::LightMode;
    if (s == "Mid Night Mode")  return ThemeType::MidnightMode;
    if (s == "Retro Mode")      return ThemeType::RetroMode;

    return ThemeType::DarkMode; // fallback
}

struct CommonSettings {

    ThemeType themetype = ThemeType::DarkMode;

    std::string Videofilter = "";
    std::string Audiofilter = "";
    std::string VideoDecode = "";
    std::string AudioDecode = "";

    int fontsize = 20;
};

extern AppSettings v_Settings;
extern CommonSettings c_Settings;  


void LoadSettings();
void LoadSettings_Video();
void SaveSettings_Video();
void LoadSettings_Common();
void SaveSettings_Common();


