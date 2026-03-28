#pragma once
#include "globals.h"
#include "utils.h"
#include <string>
#include <vector>
#include <optional>

enum class  VideoType{
    Vio,
    Live,
    File_Local
};

struct FormatGroup {
    std::vector<std::string> formats; 
    std::vector<std::string> labels;  
    std::vector<std::string> urls;
    std::vector<std::string> ids;
};

struct VideoAudioFormats {
    FormatGroup video;  
    FormatGroup audio;  
    std::optional<std::string> active_video; 
    std::optional<std::string> active_audio; 
    int video_index = 0;  
    int audio_index = 0;  
};
struct ResolutionOption {
    std::string format_id, vcodec, acodec, size, frame_rate;
    std::string bitrate_total, bitrate_video, bitrate_audio;
    std::string codec_video, codec_audio, audio_sample_rate;
    std::string resolution;
    std::string url, ids;
};
struct VideoInfoResult {
    std::vector<ResolutionOption> video_formats;
    std::vector<ResolutionOption> audio_formats;
    std::optional<std::string> video_active_id;
    std::optional<std::string> audio_active_id;


    // --- Live details ---
    bool is_live = false;                   
    std::string live_status;                 
    std::optional<int64_t> start_time;     
    std::optional<int64_t> end_time;         
    std::optional<int64_t> release_timestamp; 
    bool has_dvr = false;                    
    std::optional<std::string> hls_manifest;  
    std::string title;
    std::string uploader;
    std::optional<double> duration;          

    bool file_local =false;
};

extern VideoAudioFormats all_formats;
void HandleYTDLLog(mpv_handle* mpv,const std::string& text);

void SetVideoTypeLocal();

VideoType GetVideoType();

const char* VideoTypeToString(VideoType videotype);