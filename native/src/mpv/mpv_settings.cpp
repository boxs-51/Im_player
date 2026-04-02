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
    
    // Trường hợp 1: Không có file -> Giữ nguyên giá trị mặc định đã khởi tạo ở struct
    if (!file.is_open()) {
        std::cerr << "[Video] Settings file not found. Using hardcoded defaults.\n";
        return;
    }

    try {
        nlohmann::json j;
        file >> j;

        // .value(key, fallback) tự động dùng fallback nếu key thiếu
        v_Settings.enableSubtitles   = j.value("EnableSubtitles",   v_Settings.enableSubtitles);
        v_Settings.playbackSpeed     = j.value("PlaybackSpeed",     v_Settings.playbackSpeed);
        v_Settings.audiodelay        = j.value("AudioDelay",        v_Settings.audiodelay);
        v_Settings.repeatVideo       = j.value("RepeatVideo",       v_Settings.repeatVideo);
        v_Settings.repeatlist        = j.value("RepeatList",        v_Settings.repeatlist);
        v_Settings.autoPlayNext      = j.value("AutoPlayNext",      v_Settings.autoPlayNext);
        v_Settings.defaultVolume     = j.value("DefaultVolume",     v_Settings.defaultVolume);
        v_Settings.selectedResolution = j.value("SelectedResolution", v_Settings.selectedResolution);
        v_Settings.selectedAudio      = j.value("selectedAudio",      v_Settings.selectedAudio);
        v_Settings.selectedFormat     = j.value("selectedFormat",     v_Settings.selectedFormat);

    } catch (const nlohmann::json::exception& e) {
        // Trường hợp 2 & 3: File lỗi format hoặc lỗi đọc dữ liệu
        std::cerr << "[Video] JSON Parse Error: " << e.what() << ". Falling back to defaults.\n";
    }
}
void LoadSettings_Common() {
    std::ifstream in(SETTINGS_PATH_COMMOM);
    if (!in.is_open()) {
        std::cerr << "[Common] Settings file not found.\n";
        return;
    }

    try {
        nlohmann::json j;
        in >> j;

        // Xử lý Theme (vì cần convert string -> enum nên cần check kỹ hơn)
        if (j.contains("themetype") && j["themetype"].is_string()) {
            c_Settings.themetype = StringToTheme(j["themetype"].get<std::string>());
        }

        // Dùng .value() để tránh crash và handle việc thiếu trường
        c_Settings.fontsize    = j.value("fontsize",    c_Settings.fontsize);
        c_Settings.Videofilter = j.value("Videofilter", c_Settings.Videofilter);
        c_Settings.VideoDecode = j.value("VideoDecode", c_Settings.VideoDecode);
        c_Settings.Audiofilter = j.value("Audiofilter", c_Settings.Audiofilter);
        c_Settings.AudioDecode = j.value("AudioDecode", c_Settings.AudioDecode);

    } catch (const std::exception& e) {
        std::cerr << "[Common] Load Error: " << e.what() << std::endl;
    }
}

void LoadSettings(){
    LoadSettings_Video();
    LoadSettings_Common();
}
void SaveSettings_Common() {
    nlohmann::json j;

    j["themetype"]          = ThemeToString(c_Settings.themetype);
    j["fontsize"]       = c_Settings.fontsize;
    j["Videofilter"]    = c_Settings.Videofilter;
    j["VideoDecode"]    = c_Settings.VideoDecode;
    j["Audiofilter"]    = c_Settings.Audiofilter;
    j["AudioDecode"]    = c_Settings.AudioDecode;


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



