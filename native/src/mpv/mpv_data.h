#pragma once
#include <mpv/client.h>

#include <utils.h>
#include <string>
#include <unordered_map>
#include <vector>

enum class EndFileErrorType : uint8_t{
    None,
    FormatNotSupported,
    URLExpired,
    NetworkError,
    Other
};
enum class PlaybackState : uint8_t {
    Idle,
    Loading,
    Seeking,
    Playing,
    Paused,
    EndOfFile
};

struct AudioDeviceInfo {
    std::string name;
    std::string description;
};

struct TrackCommon {
    int id = -1;
    int ff_index = -1;
    std::string type = "";           // "audio", "video", "sub", ...
    std::string codec = "";
    std::string codec_desc = "";
    std::string codec_profile = "";
    std::string decoder = "";
    std::string decoder_desc = "";
    std::string language = "";       // "eng", "vie", ...
    std::string title = "";
    
    // Flags
    bool is_default = false;
    bool forced = false;
    bool selected = false;
    bool external = false;
};
struct VideoDetails {
    int demux_w = 0;
    int demux_h = 0;
    double demux_fps = 0.0;
    std::string format_name = "";    // "yuv420p", ...
    bool image = false;
    bool albumart = false;
};

struct AudioDetails {
    int demux_samplerate = 0;
    int demux_channel_count = 0;
    std::string demux_channels = ""; // "stereo", "5.1", ...
    std::string format_name = "";    // "fltp", ...
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
struct PlaylistEntry {
    std::string filename = "";
    std::string title = "";

    int id = -1;
    
    bool current = false;
    bool playing = false;
};
struct SubInFo {

    double sub_scale =0.0;
    double sub_Delay = 0.0;

    bool sub_Visible = false;

    std::string sub_codec = "";
    std::string sub_ass_override = "";
};

struct MPVPlaybackStatus {
    int volume = 100;
    int stream_pos = 0;
    int cache_buffering_state = 0;
    int g_PlayingIndex = -1;
    int g_PlayingIndex_1 = 0;
    int g_playlist_count = -1;

    double percent_pos =0.0;
    double duration = 0.0;
    double timePos = 0.0;
    double playbackTime = 0.0;
    double speed = 1.0;
    double demuxer_bitrate = 0.0;
    double demuxer_cache_duration = 0.0;
    double demuxer_cache_time = 0.0;
    double audio_buffer = 0.0;
    double time_remaining = 0.0;
    double audio_demuxer = 0.0;

    bool idle_active = false;
    bool hasTime = false;    
    bool isPaused = false;
    bool isCoreIdle = false;
    bool eofReached = false;
    bool isMuted = false;
    bool seekable = false;
    bool ilde =false;
    bool demuxer_via_network = false;
    bool hasFile = false;
    bool seeking = false;
    bool isSeeking = false;
    bool isLoadingMedia = false;

    std::string working_directory = "";
    std::string stream_path = "";
    std::string filename = "";
    std::string streamUrl = "";
    std::string mediaTitle = "";
    std::string Title = "";
    std::string fileFormat = "";
    std::string loopMode = "";
    std::string audio_client_name = "";
    
    SubInFo g_subinfo;
    std::vector<PlaylistEntry> g_playlist = {};
    std::vector<AudioDeviceInfo> g_audioDevices = {};
};
struct ChapterInfo {
    double time = 0.0f;     // giây
    std::string title = "";
};

struct VideoParams{
    // ==== Video ====
    std::string vpixfmt = "";         
    std::string vprimaries = "";      
    std::string vgamma = "";          
    std::string vcolormatrix = "";    
    std::string vcolorlevels = "";    
    std::string vstereo_in = "";
    std::string vchroma_location = "";
    std::string vaspect_name = "";
    std::string vlight = "";
    std::string vsar_name = "";

    int average_bpp = 0;
    int vrotate = 0;           
    int vwidth = 0;            
    int vheight = 0;            
    int vdisp_w = 0;            
    int vdisp_h = 0;      
    int vcrop_x = 0;
    int vcrop_y = 0;
    int vcrop_w = 0;
    int vcrop_h = 0;

    double vaspect = 0.0;       
    double vpar = 0.0;           
    double vsar = 0.0; 
    double vsig_peak = 0.0; 
};
struct AudioParams{
    // ==== Audio ====
    std::string aformat = "";        // audio format (vd: "s16", "f32")
    std::string ahr_channels = "";   // layout kênh chi tiết (vd: "FL FR FC LFE BL BR")
    std::string achannels_str = "";

    int channel_count = 0;          // số kênh
    int asamplerate = 0;        // sample rate (Hz)
};
struct VideoInfo {
    // ==== Video ====
    int width = 0;              // chiều rộng gốc
    int height = 0;             // chiều cao gốc
    int vbitrate = 0;           // video bitrate (bps)
    int rotate = 0;             // góc xoay (0, 90, 180, 270)    
    double aspect = 0.0;        // aspect ratio
    double estimated_vf_fps_mpv = 0.0; // fps ước lượng từ filter graph   
    double currentFPS = 0.0;    // fps thực tế (rendered, lấy từ mpv stats)
    std::string video_format = "";  // video format/container (vd: "mp4", "mkv")    
    std::string vcodec = "";         // codec video (vd: "h264")
    std::string description = "";   // mô tả chung (vd: "1920x1080 [SAR 1:1 DAR 16:9] fps 23.976")
    std::string v_out = "";

    // ==== Audio ====
    std::string acodec = "";         // codec audio (vd: "aac")
    double audio_delay = 0.0;
    int achannels = 0;          // số kênh
    
    int asamplerate = 0;        // sample rate (Hz)
    int abitrate = 0;           // audio bitrate (bps)

    int g_current_chapter = 0;

    bool hasSubtitles = false;

    // ==== Hardware/Device ====
    std::string a_out = "";
    std::string a_filter = "";
    std::string audio_device = "";   // thiết bị audio đang xuất (vd: "wasapi/{device-id}")
    std::string hwdec = "";          // hardware decoder (vd: "d3d11va", "vaapi", "cuda", "vdpau")

    std::vector<TrackInfo>g_tracks = {};
    std::vector<ChapterInfo> g_chapters = {};
    VideoParams g_videoparams;
    AudioParams g_audioarams;
    std::unordered_map<std::string, std::string> metadata = {}; 
};    


MPVPlaybackStatus& GetMPVPlaybackStatus();
VideoInfo& GetVideoInfo();

void InitMPVObservers(mpv_handle* mpv);
void ProcessMPVEvents(mpv_handle* mpv);

const char* PlaybackStateToString(PlaybackState state);
PlaybackState GetPlaybackState();
