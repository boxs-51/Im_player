#include "settings_manager.h"
#include <utils.h>
#include <json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

const std::string SETTINGS_PATH_VIDEO  = AutoPath<std::string>("%ROOT%", "data", "settings_video.json");
const std::string SETTINGS_PATH_COMMON = AutoPath<std::string>("%ROOT%", "data", "settings_common.json");

void ConfigManager::LoadAll() {
    std::unique_lock lock(rwMutex); // Lock toàn bộ để nạp dữ liệu ban đầu

    // 1. Load Video
    std::ifstream fileV(SETTINGS_PATH_VIDEO);
    if (fileV.is_open()) {
        try {
            json j; fileV >> j;
            v_Settings.enableSubtitles   = j.value("EnableSubtitles", v_Settings.enableSubtitles);
            v_Settings.playbackSpeed     = j.value("PlaybackSpeed", v_Settings.playbackSpeed);
            v_Settings.audiodelay        = j.value("AudioDelay", v_Settings.audiodelay); // Đã sửa lỗi chính tả
            v_Settings.repeatVideo       = j.value("RepeatVideo", v_Settings.repeatVideo);
            v_Settings.repeatlist        = j.value("RepeatList", v_Settings.repeatlist);
            v_Settings.autoPlayNext      = j.value("AutoPlayNext", v_Settings.autoPlayNext);
            v_Settings.defaultVolume     = j.value("DefaultVolume", v_Settings.defaultVolume);
            v_Settings.selectedResolution = j.value("SelectedResolution", v_Settings.selectedResolution);
            v_Settings.selectedAudio      = j.value("selectedAudio", v_Settings.selectedAudio);
            v_Settings.selectedFormat     = j.value("selectedFormat", v_Settings.selectedFormat);
        } catch (...) {}
    }

    // 2. Load Common
    std::ifstream fileC(SETTINGS_PATH_COMMON);
    if (fileC.is_open()) {
        try {
            json j; fileC >> j;
            if (j.contains("themetype") && j["themetype"].is_string()) {
                c_Settings.themetype = StringToTheme(j["themetype"].get<std::string>());
            }
            c_Settings.fontsize    = j.value("fontsize", c_Settings.fontsize);
            c_Settings.Videofilter = j.value("Videofilter", c_Settings.Videofilter);
            c_Settings.VideoDecode = j.value("VideoDecode", c_Settings.VideoDecode);
            c_Settings.Audiofilter = j.value("Audiofilter", c_Settings.Audiofilter);
            c_Settings.AudioDecode = j.value("AudioDecode", c_Settings.AudioDecode);
        } catch (...) {}
    }
}

void ConfigManager::SaveVideo() {
    json j;
    {
        std::shared_lock lock(rwMutex); // Chỉ cần Read-lock để đọc data xuất ra file
        j["EnableSubtitles"] = v_Settings.enableSubtitles;
        j["PlaybackSpeed"] = v_Settings.playbackSpeed;
        j["AudioDelay"] = v_Settings.audiodelay; // Sửa lỗi ghi file chính xác
        j["RepeatVideo"] = v_Settings.repeatVideo;
        j["RepeatList"] = v_Settings.repeatlist;
        j["AutoPlayNext"] = v_Settings.autoPlayNext;
        j["DefaultVolume"] = v_Settings.defaultVolume;
        j["SelectedResolution"] = v_Settings.selectedResolution;
        j["selectedAudio"] = v_Settings.selectedAudio;
        j["selectedFormat"] = v_Settings.selectedFormat;
    }

    std::ofstream file(SETTINGS_PATH_VIDEO);
    if (file.is_open()) file << j.dump(4);
}

void ConfigManager::SaveCommon() {
    json j;
    {
        std::shared_lock lock(rwMutex);
        j["themetype"]  = ThemeToString(c_Settings.themetype);
        j["fontsize"]   = c_Settings.fontsize;
        j["Videofilter"] = c_Settings.Videofilter;
        j["VideoDecode"] = c_Settings.VideoDecode;
        j["Audiofilter"] = c_Settings.Audiofilter;
        j["AudioDecode"] = c_Settings.AudioDecode;
    }

    std::ofstream out(SETTINGS_PATH_COMMON);
    if (out.is_open()) out << j.dump(4);
}