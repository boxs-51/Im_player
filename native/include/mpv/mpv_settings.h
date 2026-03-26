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

struct CommonSettings {
    int fontIndex   = 0;

    float fontScale   = 1.0f;
    float wrapWidth   = 500.0f;
    float lineSpacing = 1.0f; 
    
    float fontsize = 18.0f;

    ImVec4 fontColor;

    std::string fontName;
    std::string defaultFamily = "Times New Roman";
    std::string defaultStyle = "Regular"; 

    ImVec2 windowPos;
    ImVec2 windowSize;

    int theme;
    float opacity;

    bool option1 = false;
    bool option2 = false;
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


