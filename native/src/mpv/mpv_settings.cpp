#include "mpv/mpv_settings.h"

using json = nlohmann::json;
static std::string ytdl_json_buffer;

AppSettings v_Settings;  
CommonSettings c_Settings;  


const std::string SETTINGS_PATH_VIDEO     = AutoPath<std::string>("%ROOT%", "data","settings_video.json");
const std::string SETTINGS_PATH_COMMOM    = AutoPath<std::string>("%ROOT%", "data","settings_common.json");

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



