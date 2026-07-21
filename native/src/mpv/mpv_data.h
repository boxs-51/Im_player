#pragma once
#include <mpv/client.h>

#include "MPVDataModels.h" // Nguồn định nghĩa struct duy nhất
#include <utils.h>
#include <string>
#include <unordered_map>
#include <vector>

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

    // --- HỆ THỐNG DỮ LIỆU ĐẦU VÀO TOÀN DIỆN TỪ EBUR128 ---
    double loudness_momentary = 0.0f;   // lavfi.r128.M  -> Độ to tức thời (cửa sổ 400ms), nhạy bén với tiếng nổ/vocal giật mình
    double loudness_shortterm = 0.0f;   // lavfi.r128.S  -> Độ to ngắn hạn (cửa sổ 3s), biểu thị cảm nhận âm lượng thực tế
    double loudness_integrated = 0.0f;  // lavfi.r128.I  -> Độ to trung bình tích lũy từ đầu file đến hiện tại
    double loudness_range = 0.0f;       // lavfi.r128.LRA -> Dải động (độ chênh lệch âm lượng giữa các phân đoạn)
    double loudness_lra_low = 0.0f;   // lavfi.r128.LRA.low  -> Ngưỡng đáy năng lượng tích lũy (LUFS)
    double loudness_lra_high = 0.0f;  // lavfi.r128.LRA.high -> Ngưỡng đỉnh năng lượng tích lũy (LUFS)
    
    double true_peak = 0.0f;            // lavfi.r128.true_peak     -> Đỉnh sóng thực cao nhất (Hệ tuyến tính 0.0 -> 1.0)
    double true_peak_ch0 = 0.0f;        // lavfi.r128.true_peak_ch0 -> Đỉnh sóng thực kênh trái (Linear)
    double true_peak_ch1 = 0.0f;        // lavfi.r128.true_peak_ch1 -> Đỉnh sóng thực kênh phải (Linear)

    double sample_peak = 0.0f;
    double sample_peak_ch0 = 0.0f;
    double sample_peak_ch1 = 0.0f;
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

const char* PlaybackStateToString(PlaybackState state);
PlaybackState GetPlaybackState();
