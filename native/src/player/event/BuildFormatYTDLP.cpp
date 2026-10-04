#include "PlaybackObserver.h"
#include "PlayerDataModels.h"
#include "PlayerStateSystem.h"
#include "PlaybackCommand.h"
#include "common/LifecycleEvidence.h"
#include <log.h>

#include <settings_manager.h>

#include <json.hpp>
#include <globals.h>

#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <regex>
#include <SDL2/SDL.h>

using json = nlohmann::json;

// ---------------------------- Helper Utilities ----------------------------
namespace {
    inline std::string scale_filesize(uint64_t size) {
        double s = size; int counter = 0;
        while (s > 1024 && counter < 4) { s /= 1024; counter++; }
        char buf[32];
        if (counter == 3) snprintf(buf, 32, "%.1fGiB", s);
        else if (counter == 2) snprintf(buf, 32, "%.1fMiB", s);
        else if (counter == 1) snprintf(buf, 32, "%.1fKiB", s);
        else snprintf(buf, 32, "%.1fB", s);
        return buf;
    }

    inline std::string scale_bitrate(double b) {
        int counter = 0;
        while (b > 1000 && counter < 3) { b /= 1000; counter++; }
        char buf[32];
        if (counter == 2) snprintf(buf, 32, "%.1fGbps", b);
        else if (counter == 1) snprintf(buf, 32, "%.1fMbps", b);
        else snprintf(buf, 32, "%.1fKbps", b);
        return buf;
    }

    inline bool is_video(const json& f) { return f.contains("vcodec") && f["vcodec"] != "none"; }
    inline bool is_audio(const json& f) { return f.contains("acodec") && f["acodec"] != "none"; }

    VideoInfoResult ExtractAllFormats(const std::string& json_str) {
        VideoInfoResult result;
        try {
            json j = json::parse(json_str);

            if (j.contains("title") && j["title"].is_string()) result.title = j["title"].get<std::string>();
            if (j.contains("uploader") && j["uploader"].is_string()) result.uploader = j["uploader"].get<std::string>();
            if (j.contains("duration") && !j["duration"].is_null() && j["duration"].is_number())
                result.duration = j["duration"].get<double>();

            if (j.contains("webpage_url") && j["webpage_url"].is_string()) {
                std::string url = j["webpage_url"].get<std::string>();
                if (url.rfind("file://", 0) == 0) result.file_local = true;
            } else {
                result.file_local = false;
            }

            if (!result.file_local && j.contains("url") && j["url"].is_string()) {
                std::string url = j["url"].get<std::string>();
                if (url.size() > 2 && std::isalpha(url[0]) && url[1] == ':' && (url[2] == '\\' || url[2] == '/'))
                    result.file_local = true;
                if (url.rfind("file://", 0) == 0)
                    result.file_local = true;
            }

            if (j.contains("is_live") && j["is_live"].is_boolean())
                result.is_live = j["is_live"].get<bool>();

            if (!result.is_live && j.contains("duration") && j["duration"].is_null())
                result.is_live = true;

            if (!j.contains("formats") || !j["formats"].is_array()) return result;

            for (auto& f : j["formats"]) {
                ResolutionOption r;
                r.format_id = f.value("format_id", "");
                r.vcodec = f.value("vcodec", "");
                r.acodec = f.value("acodec", "");

                if (f.contains("filesize") && f["filesize"].is_number_unsigned())
                    r.size = scale_filesize(f["filesize"].get<uint64_t>());
                else if (f.contains("filesize_approx") && f["filesize_approx"].is_number_unsigned())
                    r.size = "~" + scale_filesize(f["filesize_approx"].get<uint64_t>());

                if (f.contains("fps") && f["fps"].is_number())
                    r.frame_rate = std::to_string(f["fps"].get<int>()) + "fps";
                if (f.contains("vbr") && f["vbr"].is_number())
                    r.bitrate_video = scale_bitrate(f["vbr"].get<double>());
                if (f.contains("asr") && f["asr"].is_number())
                    r.audio_sample_rate = std::to_string(f["asr"].get<int>()) + "Hz";
                if (f.contains("width") && f.contains("height") && f["width"].is_number() && f["height"].is_number())
                    r.resolution = std::to_string(f["width"].get<int>()) + "x" + std::to_string(f["height"].get<int>());

                if (f.contains("url") && f["url"].is_string())
                    r.url = f["url"].get<std::string>();

                r.codec_video = r.vcodec;
                r.codec_audio = r.acodec;

                bool added = false;
                if (is_video(f)) {
                    result.video_formats.push_back(r);
                    if (!result.video_active_id) result.video_active_id = r.format_id;
                    added = true;
                }
                if (is_audio(f)) {
                    result.audio_formats.push_back(r);
                    if (!result.audio_active_id) result.audio_active_id = r.format_id;
                    added = true;
                }
                if (!added && !r.format_id.empty()) result.video_formats.push_back(r);
            }
        } catch (...) {}
        return result;
    }

    int GetDisplayMonitorHeight() {
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.h > 0) {
            return mode.h;
        }
        return 1080;
    }
}

// ---------------------------- Member Methods implementation ----------------------------

void PlaybackObserver::UpdateVideoTypeInState(const VideoInfoResult& info) {
    VideoType type = VideoType::Vio;
    if (info.is_live) type = VideoType::Live;
    else if (info.file_local) type = VideoType::Local;

    m_state.WritePlayback([type](PlaybackModel& m) {
        m.videoType = type;
    });
}

void PlaybackObserver::BuildVideoOptions(const std::vector<ResolutionOption>& videoFormats, FormatGroup& videoGroup, int screenHeight) {
    videoGroup.full_labels.clear();
    videoGroup.short_labels.clear();
    videoGroup.formats.clear();
    videoGroup.ids.clear();
    videoGroup.urls.clear();

    std::string auto_fmt = "bestvideo[height<=" + std::to_string(screenHeight) + "]";
    std::string auto_label = "Auto (Up to " + std::to_string(screenHeight) + "p)";

    videoGroup.full_labels.push_back(auto_label);
    videoGroup.short_labels.push_back("Auto");
    videoGroup.formats.push_back(auto_fmt);
    videoGroup.ids.push_back("auto");
    videoGroup.urls.push_back("");

    std::unordered_set<std::string> seen;
    auto simplifyCodec = [](const std::string& codec) -> std::string {
        size_t pos = codec.find_first_of(".");
        return pos != std::string::npos ? codec.substr(0, pos) : codec;
    };

    for (const auto& vf : videoFormats) {
        if (vf.vcodec.empty() || vf.vcodec == "none") continue;

        std::string shortCodec = simplifyCodec(vf.vcodec);
        int height = 0;
        try { height = std::stoi(vf.resolution.substr(vf.resolution.find('x') + 1)); } catch (...) {}

        std::string key = shortCodec + "#" + std::to_string(height) + "#" + vf.frame_rate;
        if (seen.count(key)) continue;

        std::string short_label = vf.resolution.empty() ? "Unknown" : vf.resolution;
        if (!vf.frame_rate.empty() && vf.frame_rate != "30fps") short_label += " (" + vf.frame_rate + ")";
        std::string full_label = short_label;
        if (!vf.frame_rate.empty()) full_label += " @" + vf.frame_rate;
        if (!shortCodec.empty()) full_label += " • " + shortCodec;
        if (!vf.bitrate_video.empty()) full_label += " • " + vf.bitrate_video;
        if (!vf.size.empty()) full_label += " • " + vf.size;
        if (!vf.format_id.empty()) full_label += " [" + vf.format_id + "]";

        std::string format;
        if (!vf.resolution.empty()) format += "bestvideo[height<=" + std::to_string(height) + "]";
        if (!shortCodec.empty()) format += "[vcodec^=" + shortCodec + "]";

        if (format.empty()) format = "bestvideo";

        videoGroup.full_labels.push_back(full_label);
        videoGroup.short_labels.push_back(short_label);
        videoGroup.formats.push_back(format);
        videoGroup.ids.push_back(vf.format_id);
        videoGroup.urls.push_back(vf.url);

        seen.insert(key);
    }
}

void PlaybackObserver::BuildAudioOptions(const std::vector<ResolutionOption>& audioFormats, FormatGroup& audioGroup) {
    audioGroup.full_labels.clear();
    audioGroup.short_labels.clear();
    audioGroup.formats.clear();
    audioGroup.ids.clear();
    audioGroup.urls.clear();

    audioGroup.full_labels.push_back("Auto");
    audioGroup.short_labels.push_back("Auto");
    audioGroup.formats.push_back("bestaudio/best");
    audioGroup.ids.push_back("auto");
    audioGroup.urls.push_back("");

    std::unordered_set<std::string> seen;

    for (const auto& af : audioFormats) {
        if (af.acodec.empty() || af.acodec == "none") continue;

        std::string key = af.acodec + "#" + af.audio_sample_rate;
        if (seen.count(key)) continue;

        std::string short_label = af.acodec;
        if (af.acodec == "opus") short_label = "High Quality (Opus)";
        else if (af.acodec == "aac") short_label = "AAC";

        std::string full_labels = af.acodec;
        if (!af.audio_sample_rate.empty()) full_labels += " (" + af.audio_sample_rate + ")";
        if (!af.format_id.empty()) full_labels += " [" + af.format_id + "]";

        std::string format = "bestaudio[acodec^=" + af.acodec + "]/best";

        audioGroup.short_labels.push_back(short_label);
        audioGroup.full_labels.push_back(full_labels);
        audioGroup.formats.push_back(format);
        audioGroup.ids.push_back(af.format_id);
        audioGroup.urls.push_back(af.url);

        seen.insert(key);
    }
}

std::string PlaybackObserver::BuildCombinedFormat(const MediaFormatsModel& allFormats) {
    std::string videoFmt = allFormats.active_video.value_or("bestvideo[height<=1080]");
    std::string audioFmt = allFormats.active_audio.value_or("bestaudio/best");
    return videoFmt + "+" + audioFmt;
}

void PlaybackObserver::BuildAllFormats(const VideoInfoResult& info) {
    auto& Cfg = ConfigManager::Instance();
    MediaFormatsModel localFormats;

    int screenHeight = GetDisplayMonitorHeight();
    BuildVideoOptions(info.video_formats, localFormats.video, screenHeight);

    // --- Active Video ---
    std::string userVideo = Cfg.GetVideoSettings().selectedFormat;

    if (userVideo.empty()) {
        userVideo = "bestvideo[height<=" + std::to_string(screenHeight) + "]";
        Cfg.UpdateVideoSettings([userVideo](AppSettings& s) { s.selectedFormat = userVideo; });
    }

    localFormats.video_index = 0;
    localFormats.active_video = std::nullopt;

    if (!userVideo.empty()) {
        auto it = std::find(localFormats.video.formats.begin(), localFormats.video.formats.end(), userVideo);
        if (it != localFormats.video.formats.end()) {
            localFormats.active_video = *it;
            localFormats.video_index = std::distance(localFormats.video.formats.begin(), it);
        } else if (!localFormats.video.formats.empty()) {
            localFormats.active_video = localFormats.video.formats[0];
            std::string target = localFormats.video.formats[0];
            Cfg.UpdateVideoSettings([target](AppSettings& s) { s.selectedFormat = target; });
            localFormats.video_index = 0;
        }
    } else if (!localFormats.video.formats.empty()) {
        localFormats.active_video = localFormats.video.formats[0];
    }

    BuildAudioOptions(info.audio_formats, localFormats.audio);

    // --- Active Audio ---
    std::string userAudio = Cfg.GetVideoSettings().selectedAudio;
    localFormats.audio_index = 0;
    localFormats.active_audio = std::nullopt;

    if (!userAudio.empty()) {
        auto it = std::find(localFormats.audio.formats.begin(), localFormats.audio.formats.end(), userAudio);
        if (it != localFormats.audio.formats.end()) {
            localFormats.active_audio = *it;
            localFormats.audio_index = std::distance(localFormats.audio.formats.begin(), it);
        } else if (!localFormats.audio.formats.empty()) {
            localFormats.active_audio = localFormats.audio.formats[0];
            std::string target = localFormats.audio.formats[0];
            Cfg.UpdateVideoSettings([target](AppSettings& s) { s.selectedAudio = target; });
            localFormats.audio_index = 0;
        }
    } else if (!localFormats.audio.formats.empty()) {
        localFormats.active_audio = localFormats.audio.formats[0];
    }

    UpdateVideoTypeInState(info);

    // #33: metadata discovery must not mutate the active load. The current
    // load's ytdl-format was already selected before the loadfile command.
    // Rewriting it here can trigger a second selection/restart/seek while
    // decoders and the audio transport are still starting.
    const std::string combinedFormat = BuildCombinedFormat(localFormats);
    const Uint64 loadId = m_commander.GetStartupLoadId();
    LifecycleEvidence::EmitDiagnostic(
        "STARTUP",
        FormatString(
            "event=YTDL_FORMAT_DISCOVERED load_id=%llu ts_ms=%llu action=defer_current_load format=%s",
            static_cast<unsigned long long>(loadId),
            static_cast<unsigned long long>(SDL_GetTicks64()),
            combinedFormat.c_str()));

    Cfg.SaveVideo();

    m_state.WritePlayback([localFormats](PlaybackModel& m) {
        m.formats = localFormats;
    });
}

void PlaybackObserver::HandleYTDLLog(const std::string& text) {
    const std::string marker = "user-data/mpv/ytdl/json-subprocess-result=";
    size_t pos = text.find(marker);
    if (pos == std::string::npos) return;

    std::string chunk = text.substr(pos + marker.size());
    size_t first = chunk.find('{');
    size_t last = chunk.rfind('}');

    if (first != std::string::npos && last != std::string::npos && last > first) {
        std::string json_str = chunk.substr(first, last - first + 1);
        try {
            json outer = json::parse(json_str);
            if (!outer.contains("stdout") || outer["stdout"].is_null()) return;

            std::string inner_json = outer["stdout"];
            if (!inner_json.empty() && inner_json.front() == '"' && inner_json.back() == '"') {
                inner_json = inner_json.substr(1, inner_json.size() - 2);
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\n)"), "\n");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\r)"), "\r");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\t)"), "\t");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\")"), "\"");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\\\)"), "\\");
            }

            VideoInfoResult info = ExtractAllFormats(inner_json);
            BuildAllFormats(info);
        } catch (...) {}
    }
}