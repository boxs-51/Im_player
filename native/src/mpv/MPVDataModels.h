#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "mpv_basic_formats.h" // For VideoType enum

// --- Enums ---
enum class PlaybackState : uint8_t {
    Idle,
    Loading,
    Playing,
    Paused,
    Seeking,
    EndOfFile,
    NoFile
};

// --- Basic Data Structures (mostly from mpv_data.h) ---
struct AudioDeviceInfo {
    std::string name;
    std::string description;
};

struct PlaylistEntry {
    std::string filename;
    std::string title;
    int id = -1;
    bool current = false;
    bool playing = false;
};

struct ChapterInfo {
    double time = 0.0;
    std::string title;
};

struct TrackCommon {
    int id = -1;
    int ff_index = -1;
    std::string type;
    std::string codec;
    std::string codec_desc;
    std::string codec_profile;
    std::string decoder;
    std::string decoder_desc;
    std::string language;
    std::string title;
    bool is_default = false;
    bool forced = false;
    bool selected = false;
    bool external = false;
};

struct VideoDetails {
    int demux_w = 0, demux_h = 0;
    double demux_fps = 0.0;
    std::string format_name;
    bool image = false;
    bool albumart = false;
};

struct AudioDetails {
    int demux_samplerate = 0;
    int demux_channel_count = 0;
    std::string demux_channels;
    std::string format_name;
};

struct AccessibilityFlags {
    bool visual_impaired = false;
    bool hearing_impaired = false;
    bool dependent = false;
};

struct TrackInfo {
    TrackCommon common;
    VideoDetails video;
    AudioDetails audio;
    AccessibilityFlags access;
    int main_selection = 0;
};

// --- Refactored Subsystem Data Models ---

struct PlaybackModel {
    PlaybackState state = PlaybackState::NoFile;
    VideoType videoType = VideoType::None;
    bool isPaused = true;
    bool isSeeking = false;
    bool isLoadingMedia = false;
    bool eofReached = false;
    bool isCoreIdle = false;
    bool hasFile = false;
    bool seekable = false;
    double timePos = 0.0;
    double duration = 0.0;
    double playbackTime = 0.0;
    double percent_pos = 0.0;
    double time_remaining = 0.0;
    double speed = 1.0;
    std::string loopMode;
};

struct MediaModel {
    std::string title;
    std::string mediaTitle;
    std::string filename;
    std::string fileFormat;
    std::string working_directory;
    std::string streamUrl;
    std::string stream_path;
    std::unordered_map<std::string, std::string> metadata;
};

struct VideoModel {
    int width = 0, height = 0, rotate = 0;
    int vbitrate = 0;
    double aspect = 0.0;
    double estimated_vf_fps_mpv = 0.0;
    double currentFPS = 0.0;
    std::string video_format;
    std::string vcodec;
    std::string v_out;
    std::string hwdec;
    // Video Parameters
    std::string vpixfmt, vprimaries, vgamma, vcolormatrix, vcolorlevels;
    std::string vstereo_in, vchroma_location, vaspect_name, vlight, vsar_name;
    int average_bpp = 0, vdisp_w = 0, vdisp_h = 0;
    int vcrop_x = 0, vcrop_y = 0, vcrop_w = 0, vcrop_h = 0;
    double vpar = 0.0, vsar = 0.0, vsig_peak = 0.0;
};

struct AudioModel {
    int volume = 100;
    bool isMuted = false;
    double audio_delay = 0.0;
    int abitrate = 0;
    std::string acodec;
    std::string a_out;
    std::string a_filter;
    std::string audio_device;
    std::string audio_client_name;
    std::vector<AudioDeviceInfo> audioDevices;
    // Audio Parameters
    std::string aformat, ahr_channels, achannels_str;
    int channel_count = 0, asamplerate = 0;
    // Loudness
    double loudness_momentary = 0.0, loudness_shortterm = 0.0, loudness_integrated = 0.0;
    double loudness_range = 0.0, loudness_lra_low = 0.0, loudness_lra_high = 0.0;
    double true_peak = 0.0, true_peak_ch0 = 0.0, true_peak_ch1 = 0.0;
    double sample_peak = 0.0, sample_peak_ch0 = 0.0, sample_peak_ch1 = 0.0;
};

struct SubtitleModel {
    bool sub_Visible = false;
    bool hasSubtitles = false;
    double sub_scale = 0.0;
    double sub_Delay = 0.0;
    std::string sub_codec;
    std::string sub_ass_override;
};

struct TrackModel {
    int g_current_chapter = 0;
    std::vector<TrackInfo> tracks;
    std::vector<ChapterInfo> chapters;
};

struct PlaylistModel {
    int g_PlayingIndex = -1;
    int g_PlayingIndex_1 = 0;
    int g_playlist_count = -1;
    std::vector<PlaylistEntry> playlist;
};

struct NetworkModel {
    bool demuxer_via_network = false;
    int cache_buffering_state = 0;
    long long stream_pos = 0;
    double demuxer_bitrate = 0.0;
    double demuxer_cache_duration = 0.0;
    double demuxer_cache_time = 0.0;
    double audio_buffer = 0.0;
};

// Helper functions to get state, will be defined elsewhere
const char* PlaybackStateToString(PlaybackState state);
PlaybackState GetPlaybackState();