#include "mpv/mpv_basic_formats.h"
#include "globals.h"
#include "utils.h"
#include "json.hpp"
#include "cpr.h"


#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <regex>

VideoAudioFormats all_formats;
using json = nlohmann::json;
static std::string ytdl_json_buffer;

void BuildVideoOptions(const std::vector<ResolutionOption>& videoFormats, FormatGroup &videoGroup)
{
    videoGroup.labels.clear();
    videoGroup.formats.clear();
    videoGroup.ids.clear();

    videoGroup.labels.push_back("Auto");
    videoGroup.formats.push_back("bestvideo");
    videoGroup.ids.push_back("auto");

    std::unordered_set<std::string> seen;
    auto simplifyCodec = [](const std::string& codec) -> std::string {
        size_t pos = codec.find_first_of(".");
        return pos != std::string::npos ? codec.substr(0, pos) : codec;
    };

    for (const auto& vf : videoFormats)
    {
        if (vf.vcodec.empty() || vf.vcodec == "none")
            continue;

        std::string shortCodec = simplifyCodec(vf.vcodec);
        int height = 0;
        try { height = std::stoi(vf.resolution.substr(vf.resolution.find('x') + 1)); } catch (...) {}

        std::string key = shortCodec + "#" + std::to_string(height);
        if (seen.count(key))
            continue;

        // --- Label chi tiết ---
        std::string label;
        if (!vf.resolution.empty()) label += vf.resolution;
        if (!vf.frame_rate.empty()) label += " @" + vf.frame_rate ;
        if (!shortCodec.empty()) label += " • " + shortCodec;
        if (!vf.bitrate_video.empty()) label += " • " + vf.bitrate_video;
        if (!vf.size.empty()) label += " • " + vf.size;
        if (!vf.format_id.empty())label += "  [" + vf.format_id + "]";
        
        // --- Format chi tiết ---
        std::string format;
        if(!vf.resolution.empty())format += "bestvideo[height<=" + std::to_string(height) + "]";
        if(!shortCodec.empty()) format += "[vcodec^=" + shortCodec + "]";

                                
        videoGroup.labels.push_back(label);
        videoGroup.formats.push_back(format);
        videoGroup.ids.push_back(vf.format_id);
        videoGroup.urls.push_back(vf.url);

        seen.insert(key);
    }
}

void BuildAudioOptions(const std::vector<ResolutionOption>& audioFormats, FormatGroup &audioGroup)
{
    audioGroup.labels.clear();
    audioGroup.formats.clear();

    audioGroup.labels.push_back("Auto");
    audioGroup.formats.push_back("bestaudio/best");

    std::unordered_set<std::string> seen;

    for (const auto& af : audioFormats) {
        if (af.acodec.empty() || af.acodec=="none") continue;

        std::string key = af.acodec + "#" + af.audio_sample_rate;
        if (seen.count(key)) continue;

        std::string label;
        if (!af.acodec.empty()) label += af.acodec;
        if (!af.audio_sample_rate.empty()) label += " (" + af.audio_sample_rate + ")";
        if (!af.format_id.empty()) label += "  [" + af.format_id + "]";

        std::string format;
        if (!af.acodec.empty()) format += "bestaudio[acodec^=" + af.acodec + "]" + "/" + "best";

        audioGroup.labels.push_back(label);
        audioGroup.formats.push_back(format);
        audioGroup.ids.push_back(af.format_id);  
        audioGroup.urls.push_back(af.url);

        seen.insert(key);
    }
}
std::string BuildCombinedFormat(const VideoAudioFormats &allFormats)
{
    std::string videoFmt = allFormats.active_video.value_or("bestvideo");
    std::string audioFmt = allFormats.active_audio.value_or("bestaudio");
    std::string format = videoFmt + "+" + audioFmt;
    v_Settings.selectedResolution = format;
    // Ghép video + audio
    return format;
}
std::string GetCurrentMPVFormat() {
    if (!mpv) return "";
    return mpv_get_property_string(mpv, "ytdl-format");
}

void BuildAllFormats(const VideoInfoResult &info, VideoAudioFormats &allFormats)
{
    BuildVideoOptions(info.video_formats, allFormats.video);
    
    // --- Xác định video active ---
    std::string userVideo = v_Settings.selectedFormat;
    allFormats.video_index = 0; // mặc định combo chọn "Auto"
    allFormats.active_video = std::nullopt;
    if (!userVideo.empty()) {
        auto it = std::find(allFormats.video.formats.begin(), allFormats.video.formats.end(), userVideo);
        if (it != allFormats.video.formats.end()) {
            allFormats.active_video = *it;
            allFormats.video_index = std::distance(allFormats.video.formats.begin(), it);

        } else if (!allFormats.video.formats.empty()) {
            allFormats.active_video = allFormats.video.formats[0];
            v_Settings.selectedFormat = allFormats.video.formats[0];
            allFormats.video_index = 0;
        } else {
            allFormats.active_video = std::nullopt;
            allFormats.video_index = -1;
        }
    } else {
        if (!allFormats.video.formats.empty()) {
            allFormats.active_video = allFormats.video.formats[0];
            v_Settings.selectedFormat = allFormats.video.formats[0];
            allFormats.video_index = 0;

        } else {
            allFormats.active_video = std::nullopt;
            allFormats.video_index = -1;
        }
    }


    BuildAudioOptions(info.audio_formats, allFormats.audio);
    std::string userAudio = v_Settings.selectedAudio;
    allFormats.audio_index = 0;
    if (!userAudio.empty()) {
        auto it = std::find(allFormats.audio.formats.begin(), allFormats.audio.formats.end(), userAudio);
        if (it != allFormats.audio.formats.end()) {
            allFormats.active_audio = *it;
            allFormats.audio_index = std::distance(allFormats.audio.formats.begin(), it);
        } else if (!allFormats.audio.formats.empty()) {
            allFormats.active_audio = allFormats.audio.formats[0];
            v_Settings.selectedAudio = allFormats.audio.formats[0];
            allFormats.audio_index = 0;

        } else {
            allFormats.active_audio = std::nullopt;
            allFormats.audio_index = -1;
        }
    } else {
        if (!allFormats.audio.formats.empty()) {
            allFormats.active_audio = allFormats.audio.formats[0];
            v_Settings.selectedAudio = allFormats.audio.formats[0];
            allFormats.audio_index = 0;
        } else {
            allFormats.active_audio = std::nullopt;
            allFormats.audio_index = -1;
        }
    }
    std::string tpyevideo;
    if(is_live) tpyevideo = "livestream";
    else if(file_local) tpyevideo = "file";
    else tpyevideo = "vod";
    ApplyDynamicMPVConfig(mpv,tpyevideo);
    std::string combinedFormat = BuildCombinedFormat(allFormats);
    // Áp dụng cho mpv
    const char* cmd[] = { "set", "ytdl-format", combinedFormat.c_str(), nullptr };
    int res = mpv_command(mpv, cmd);
    //mpv_command(mpv, (const char*[]){"set_property", "pause", "no", nullptr});
    SaveSettings_Video();
}
// ---------------------------- Utils ----------------------------
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

// ---------------------------- JSON ----------------------------
VideoInfoResult ExtractAllFormats(const std::string& json_str) {
    VideoInfoResult result;
    try {
        json j = json::parse(json_str);

        // --- Basic metadata ---
        if (j.contains("title") && j["title"].is_string()) result.title = j["title"].get<std::string>();
        if (j.contains("uploader") && j["uploader"].is_string()) result.uploader = j["uploader"].get<std::string>();
        if (j.contains("duration") && !j["duration"].is_null() && j["duration"].is_number())
            result.duration = j["duration"].get<double>();

        // --- Live detection (top-level fields yt-dlp commonly provides) ---
        if (j.contains("is_live") && j["is_live"].is_boolean())
            result.is_live = j["is_live"].get<bool>();

        if (j.contains("live_status") && j["live_status"].is_string())
            result.live_status = j["live_status"].get<std::string>();

        if (j.contains("start_time") && j["start_time"].is_number_integer())
            result.start_time = j["start_time"].get<int64_t>();
        if (j.contains("end_time") && j["end_time"].is_number_integer())
            result.end_time = j["end_time"].get<int64_t>();
        if (j.contains("release_timestamp") && j["release_timestamp"].is_number_integer())
            result.release_timestamp = j["release_timestamp"].get<int64_t>();

        // fallback: if duration is null -> likely live
        if (!result.is_live && j.contains("duration") && j["duration"].is_null())
            result.is_live = true;

        // top-level manifest_url sometimes exists for HLS live
        if (j.contains("manifest_url") && j["manifest_url"].is_string())
            result.hls_manifest = j["manifest_url"].get<std::string>();

        // --- Iterate formats and fill video/audio + infer HLS/DVR ---
        if (!j.contains("formats") || !j["formats"].is_array()) return result;

        for (auto& f : j["formats"]) {
            bool has_video = f.contains("vcodec") && f["vcodec"] != "none" && f["vcodec"] != "";
            bool has_audio = f.contains("acodec") && f["acodec"] != "none" && f["acodec"] != "";

            // collect protocol/manifest clues
            std::string proto;
            if (f.contains("protocol") && f["protocol"].is_string()) proto = f["protocol"].get<std::string>();
            if (f.contains("format_note") && f["format_note"].is_string()) {
                std::string note = f["format_note"];
                if (note.find("DASH") != std::string::npos) { /*dash*/ }
                if (note.find("hls") != std::string::npos || note.find("HLS") != std::string::npos) {
                    result.hls_manifest = result.hls_manifest.value_or(std::string());
                    // prefer manifest_url if available
                }
            }

            ResolutionOption r;
            r.format_id = f.value("format_id", "");
            r.vcodec = f.value("vcodec", "");
            r.acodec = f.value("acodec", "");

            if (f.contains("filesize") && f["filesize"].is_number_unsigned())
                r.size = scale_filesize(f["filesize"].get<uint64_t>());
            else if (f.contains("filesize_approx") && f["filesize_approx"].is_number_unsigned())
                r.size = "~" + scale_filesize(f["filesize_approx"].get<uint64_t>());
            else r.size = "";

            if (f.contains("fps") && f["fps"].is_number())
                r.frame_rate = std::to_string(f["fps"].get<int>()) + "fps";
            if (f.contains("tbr") && f["tbr"].is_number())
                r.bitrate_total = scale_bitrate(f["tbr"].get<double>());
            if (f.contains("vbr") && f["vbr"].is_number())
                r.bitrate_video = scale_bitrate(f["vbr"].get<double>());
            if (f.contains("abr") && f["abr"].is_number())
                r.bitrate_audio = scale_bitrate(f["abr"].get<double>());
            if (f.contains("asr") && f["asr"].is_number())
                r.audio_sample_rate = std::to_string(f["asr"].get<int>()) + "Hz";

            if (f.contains("width") && f.contains("height") && f["width"].is_number() && f["height"].is_number())
                r.resolution = std::to_string(f["width"].get<int>()) + "x" + std::to_string(f["height"].get<int>());

            // urls: prefer manifest_url or url or fragment_base_url
            if (f.contains("url") && f["url"].is_string())
                r.url = f["url"].get<std::string>();
            if (f.contains("manifest_url") && f["manifest_url"].is_string())
                r.url = f["manifest_url"].get<std::string>();
            if (f.contains("fragment_base_url") && f["fragment_base_url"].is_string() && r.url.empty())
                r.url = f["fragment_base_url"].get<std::string>();

            // also capture protocol-based HLS manifest if found
            if (proto == "m3u8" || proto == "m3u8_native") {
                if (!r.url.empty() && !result.hls_manifest.has_value()) result.hls_manifest = r.url;
            }

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

        // Infer DVR support: presence of playlist_type == "DVR" or playlist_duration present
        if (j.contains("playlist_type") && j["playlist_type"].is_string()) {
            std::string pt = j["playlist_type"].get<std::string>();
            if (pt.find("DVR") != std::string::npos || pt.find("dvr") != std::string::npos)
                result.has_dvr = true;
        }
        if (j.contains("playlist_duration") && j["playlist_duration"].is_number()) result.has_dvr = true;

    } catch (const json::parse_error& e) {
        //SafePushLog(std::string("[ERROR] JSON parse failed: ") + e.what());
    } catch (const std::exception& e) {
        //SafePushLog(std::string("[ERROR] ExtractAllFormats: ") + e.what());
    } catch (...) {
        //SafePushLog("[ERROR] ExtractAllFormats unknown exception");
    }
    return result;
}

json VideoInfoResultToJson(const VideoInfoResult& info) {
    json j;
    if (info.video_active_id) j["video_active_id"] = *info.video_active_id;
    if (info.audio_active_id) j["audio_active_id"] = *info.audio_active_id;

    j["video_formats"] = json::array();
    for (auto& r : info.video_formats) {
        j["video_formats"].push_back({
            {"format_id", r.format_id},
            {"resolution", r.resolution},
            {"vcodec", r.vcodec},
            {"acodec", r.acodec},
            {"size", r.size},
            {"frame_rate", r.frame_rate},
            {"bitrate_total", r.bitrate_total},
            {"bitrate_video", r.bitrate_video},
            {"bitrate_audio", r.bitrate_audio},
            {"codec_video", r.codec_video},
            {"codec_audio", r.codec_audio},
            {"audio_sample_rate", r.audio_sample_rate},
            {"url", r.url}  // ✅ thêm dòng này
        });
    }

    j["audio_formats"] = json::array();
    for (auto& r : info.audio_formats) {
        j["audio_formats"].push_back({
            {"format_id", r.format_id},
            {"vcodec", r.vcodec},
            {"acodec", r.acodec},
            {"size", r.size},
            {"frame_rate", r.frame_rate},
            {"bitrate_total", r.bitrate_total},
            {"bitrate_video", r.bitrate_video},
            {"bitrate_audio", r.bitrate_audio},
            {"codec_video", r.codec_video},
            {"codec_audio", r.codec_audio},
            {"audio_sample_rate", r.audio_sample_rate},
            {"url", r.url} 
        });
    }

    return j;
}

// ---------------------------- MPV Event ----------------------------
void HandleYTDLLog(const std::string& text) {
    const std::string marker = "user-data/mpv/ytdl/json-subprocess-result=";
    size_t pos = text.find(marker);
    if (pos == std::string::npos) return;

    std::string chunk = text.substr(pos + marker.size());
    ytdl_json_buffer += chunk;

    // thử parse nếu có outer json hoàn chỉnh (check dấu {..})
    size_t first = ytdl_json_buffer.find('{');
    size_t last  = ytdl_json_buffer.rfind('}');
    if (first != std::string::npos && last != std::string::npos && last > first) {
        std::string json_str = ytdl_json_buffer.substr(first, last-first+1);
        ytdl_json_buffer.clear(); // reset buffer sau khi parse
        try {
            json outer = json::parse(json_str);
            if (!outer.contains("stdout") || outer["stdout"].is_null()) return;

            std::string inner_json = outer["stdout"];
            // unescape chuỗi inner
            if (!inner_json.empty() && inner_json.front()=='"' && inner_json.back()=='"') {
                inner_json = inner_json.substr(1, inner_json.size()-2);
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\n)"), "\n");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\r)"), "\r");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\t)"), "\t");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\")"), "\"");
                inner_json = std::regex_replace(inner_json, std::regex(R"(\\\\)"), "\\");
            }

            VideoInfoResult info = ExtractAllFormats(inner_json);
            //json j_out = VideoInfoResultToJson(info);
            //std::ofstream ofs("ytdl_video_info.json"); if (ofs.is_open()) ofs << j_out.dump(2);
            is_live = info.is_live;
            BuildAllFormats(info,all_formats);
        } catch (const std::exception& e) {

        }
    }
}




