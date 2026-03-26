#include "mpv/mpv_settings.h"
#include "globals.h"
#include "utils.h"
#include "json.hpp"
#include "cpr.h"


#include <string>
#include <Windows.h>
#include <codecvt>
#include <algorithm>
#include <mpv/client.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>   
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <regex>

using json = nlohmann::json;
static std::string ytdl_json_buffer;

AppSettings v_Settings;  
CommonSettings c_Settings;  
std::string CleanUrl(const std::string& url) {
    std::string cleaned = url;

    // Ví dụ đơn giản: loại bỏ dấu '/' cuối cùng nếu có
    if (!cleaned.empty() && cleaned.back() == '/')
        cleaned.pop_back();

    // TODO: Thêm các bước chuẩn hóa khác nếu cần, vd:
//    - Loại bỏ các tham số query không cần thiết
//    - Chuyển hostname về chữ thường
//    - Sắp xếp tham số query
//    - Chuẩn hóa http/https, v.v.

    return cleaned;
}


std::string Sha1Hash(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::string CreateGroupIDFromFormats(const std::vector<std::string>& formats) {
    std::stringstream ss;
    for (const auto& f : formats) {
        ss << f << "|";
    }
    return Sha1Hash(ss.str());
}

void LoadSettings_Video() {
    std::ifstream file(SETTINGS_PATH_VIDEO);
    if (!file.is_open()) {
        std::cerr << "Settings file not found, loading defaults\n";
        return;
    }

    try {
        json j;
        file >> j;

        v_Settings.enableSubtitles = j.value("EnableSubtitles", v_Settings.enableSubtitles);
        v_Settings.playbackSpeed = j.value("PlaybackSpeed", v_Settings.playbackSpeed);
        v_Settings.audiodelay = j.value("AudioDelay", v_Settings.audiodelay);
        v_Settings.repeatVideo = j.value("RepeatVideo", v_Settings.repeatVideo);
        v_Settings.repeatlist = j.value("RepeatList", v_Settings.repeatlist);
        v_Settings.autoPlayNext = j.value("AutoPlayNext", v_Settings.autoPlayNext);
        v_Settings.defaultVolume = j.value("DefaultVolume", v_Settings.defaultVolume);
        v_Settings.selectedResolution = j.value("SelectedResolution", v_Settings.selectedResolution);
        v_Settings.selectedAudio = j.value("selectedAudio", v_Settings.selectedAudio);
        v_Settings.selectedFormat = j.value("selectedFormat", v_Settings.selectedFormat);

    } catch (const std::exception& e) {
        std::cerr << "Failed to load settings JSON: " << e.what() << std::endl;
    }
}

void LoadSettings_Common() {
    std::ifstream in(SETTINGS_PATH_COMMOM);
    if (!in.is_open()) return;

    try {
        nlohmann::json j;
        in >> j;
        in.close();

        if (j.contains("fontIndex"))   c_Settings.fontIndex          = j["fontIndex"];
        if (j.contains("fontScale"))   c_Settings.fontScale          = j["fontScale"];
        if (j.contains("wrapWidth"))   c_Settings.wrapWidth          = j["wrapWidth"];
        if (j.contains("lineSpacing")) c_Settings.lineSpacing        = j["lineSpacing"];
        if (j.contains("Family"))      c_Settings.defaultFamily      = j["Family"];
        if (j.contains("Style"))       c_Settings.defaultStyle       = j["Style"];

        if (j.contains("fontColor") && j["fontColor"].is_array() && j["fontColor"].size() == 4) {
            c_Settings.fontColor = ImVec4(
                j["fontColor"][0],
                j["fontColor"][1],
                j["fontColor"][2],
                j["fontColor"][3]
            );
        }

        if (j.contains("fontName")) c_Settings.fontName = j["fontName"];

        if (j.contains("windowPos") && j["windowPos"].is_array() && j["windowPos"].size() == 2) {
            c_Settings.windowPos = ImVec2(j["windowPos"][0], j["windowPos"][1]);
        }

        if (j.contains("windowSize") && j["windowSize"].is_array() && j["windowSize"].size() == 2) {
            c_Settings.windowSize = ImVec2(j["windowSize"][0], j["windowSize"][1]);
        }

        if (j.contains("theme"))   c_Settings.theme   = j["theme"];
        if (j.contains("opacity")) c_Settings.opacity = j["opacity"];

        if (j.contains("option1")) c_Settings.option1 = j["option1"];
        if (j.contains("option2")) c_Settings.option2 = j["option2"];
    }
    catch (...) {
        // Lỗi đọc JSON → giữ mặc định
    }
}

void LoadSettings(){
    LoadSettings_Video();
    LoadSettings_Common();
}
void SaveSettings_Common() {
    nlohmann::json j;
    j["fontIndex"]   = c_Settings.fontIndex;
    j["fontScale"]   = c_Settings.fontScale;
    j["wrapWidth"]   = c_Settings.wrapWidth;
    j["lineSpacing"] = c_Settings.lineSpacing;
    j["Family"]      = c_Settings.defaultFamily;
    j["Style"]       = c_Settings.defaultStyle;

    // Lưu thêm màu font
    j["fontColor"] = {
        c_Settings.fontColor.x,
        c_Settings.fontColor.y,
        c_Settings.fontColor.z,
        c_Settings.fontColor.w
    };

    // Lưu thêm tên font
    j["fontName"] = c_Settings.fontName;

    // Lưu thêm vị trí/kích thước cửa sổ
    j["windowPos"]  = { c_Settings.windowPos.x, c_Settings.windowPos.y };
    j["windowSize"] = { c_Settings.windowSize.x, c_Settings.windowSize.y };

    // Tuỳ chọn khác
    j["theme"]   = c_Settings.theme;
    j["opacity"] = c_Settings.opacity;

    // Các option cũ
    j["option1"] = c_Settings.option1;
    j["option2"] = c_Settings.option2;

    std::ofstream out(SETTINGS_PATH_COMMOM);
    if (!out.is_open()) return;
    out << j.dump(4);
    out.close();
}

void SaveSettings_Video() {
    json j;

    j["EnableSubtitles"] = v_Settings.enableSubtitles;
    j["PlaybackSpeed"] = v_Settings.playbackSpeed;
    j["AudioFelay"] = v_Settings.audiodelay;
    j["RepeatVideo"] = v_Settings.repeatVideo;
    j["RepeatList"] = v_Settings.repeatlist;
    j["AutoPlayNext"] = v_Settings.autoPlayNext;
    j["DefaultVolume"] = v_Settings.defaultVolume;
    j["SelectedResolution"] = v_Settings.selectedResolution;
    j["selectedAudio"] = v_Settings.selectedAudio;
    j["selectedFormat"] = v_Settings.selectedFormat;

    std::ofstream file(SETTINGS_PATH_VIDEO);
    if (!file.is_open()) {
        std::cerr << "Failed to open Settings file for writing\n";
        return;
    }

    file << j.dump(4); // indent 4 spaces
    file.close();
}



