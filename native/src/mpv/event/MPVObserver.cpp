#include "MPVObserver.h"
#include "mpv/player/MPVPlayer.h"
#include "mpv/mpv_data.h"
#include "mpv/session/MPVManager.h"
#include "mpv/mpv_basic_formats.h"
#include "mpv/MPVDataModels.h"
#include "globals.h"
#include <log.h>
#include <mutex>
#include <thread>
#include <iostream>
#include <unordered_map>

static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
static VideoInfo& g_videoInfo = GetVideoInfo();
static std::mutex g_retry_mutex;
static std::unordered_map<int, int> g_retryCount;
static constexpr int MAX_RETRY = 1;

MPVObserver::MPVObserver(MPVPlayer& player, MPVStateSystem& state) : m_player(player), m_mpv(player.GetHandle()), m_state(state) {}

void MPVObserver::ObserveProps(const std::vector<std::pair<const char*, mpv_format>>& props, const char* groupName) {
    for (const auto& [name, fmt] : props) {
        int ret = mpv_observe_property(m_mpv, 0, name, fmt);
        if (ret < 0) {
            RATE_LIMITED_COUT(observeprops, 1, std::cerr << "[DEBUG] [WARNING] [MPV] Failed to observe " << name << " (" << groupName << "): " << mpv_error_string(ret) << "\n");
        } else {
            RATE_LIMITED_COUT(observeprops, 1, std::cout << "[DEBUG] [INFO] [MPV] Observing " << name << " (" << groupName << ")" << "\n");
        }
    }
}

void MPVObserver::Init() {
    if (!m_mpv) return;

    RATE_LIMITED_COUT(initmpvobservers_start, 1, std::cout << "=================== [MPV] Initializing observers ===================");

    ObserveProps({
        {"duration", MPV_FORMAT_DOUBLE}, {"pause", MPV_FORMAT_FLAG}, {"playback-time", MPV_FORMAT_DOUBLE},
        {"time-pos", MPV_FORMAT_DOUBLE}, {"percent-pos", MPV_FORMAT_DOUBLE}, {"eof-reached", MPV_FORMAT_FLAG},
        {"core-idle", MPV_FORMAT_FLAG}, {"speed", MPV_FORMAT_DOUBLE}, {"seekable", MPV_FORMAT_FLAG},
        {"start", MPV_FORMAT_DOUBLE}, {"end", MPV_FORMAT_DOUBLE}, {"loop", MPV_FORMAT_STRING},
        {"idle-active", MPV_FORMAT_FLAG}, {"seeking", MPV_FORMAT_FLAG}, {"time-remaining", MPV_FORMAT_DOUBLE}
    }, "Playback");

    ObserveProps({
        {"mute", MPV_FORMAT_FLAG}, {"volume", MPV_FORMAT_INT64}, {"audio-params", MPV_FORMAT_NODE},
        {"audio-out-params", MPV_FORMAT_NODE}, {"audio-device", MPV_FORMAT_STRING}, {"audio-device-list", MPV_FORMAT_NODE},
        {"audio-channels", MPV_FORMAT_INT64}, {"audio-bitrate", MPV_FORMAT_INT64}, {"audio-samplerate", MPV_FORMAT_INT64},
        {"audio-codec", MPV_FORMAT_STRING}, {"audio-buffer", MPV_FORMAT_DOUBLE}, {"audio-client-name", MPV_FORMAT_STRING},
        {"audio-delay", MPV_FORMAT_DOUBLE}, {"ao", MPV_FORMAT_STRING}, {"af", MPV_FORMAT_STRING}
    }, "Audio");

    ObserveProps({
        {"width", MPV_FORMAT_INT64}, {"height", MPV_FORMAT_INT64}, {"display-fps", MPV_FORMAT_DOUBLE},
        {"estimated-vf-fps", MPV_FORMAT_DOUBLE}, {"video-bitrate", MPV_FORMAT_INT64}, {"video-params", MPV_FORMAT_NODE},
        {"video-aspect-override", MPV_FORMAT_DOUBLE}, {"hwdec-current", MPV_FORMAT_STRING}, {"hwdec-active", MPV_FORMAT_FLAG},
        {"hwdec", MPV_FORMAT_STRING}, {"video-codec", MPV_FORMAT_STRING}, {"video-format", MPV_FORMAT_STRING},
        {"video-rotate", MPV_FORMAT_INT64}, {"video-out-params", MPV_FORMAT_NODE}, {"vo", MPV_FORMAT_STRING},
        {"vf", MPV_FORMAT_STRING}, {"osd-width", MPV_FORMAT_INT64}, {"osd-height", MPV_FORMAT_INT64}
    }, "Video");

    ObserveProps({
        {"sub-delay", MPV_FORMAT_DOUBLE}, {"sub-visibility", MPV_FORMAT_FLAG}, {"sub-codec", MPV_FORMAT_STRING},
        {"sub-text", MPV_FORMAT_STRING}, {"sid", MPV_FORMAT_INT64}, {"secondary-sid", MPV_FORMAT_INT64},
        {"sub-scale", MPV_FORMAT_DOUBLE}, {"sub-ass-override", MPV_FORMAT_STRING}, {"sub-streams", MPV_FORMAT_NODE}
    }, "Subtitles");

    ObserveProps({
        {"filename", MPV_FORMAT_STRING}, {"stream-open-filename", MPV_FORMAT_STRING}, {"file-format", MPV_FORMAT_STRING},
        {"metadata", MPV_FORMAT_NODE}, {"track-list", MPV_FORMAT_NODE}, {"chapter-list", MPV_FORMAT_NODE},
        {"chapter", MPV_FORMAT_INT64}, {"chapter-count", MPV_FORMAT_INT64}, {"chapter-metadata", MPV_FORMAT_NODE},
        {"playlist", MPV_FORMAT_NODE}, {"playlist-pos", MPV_FORMAT_INT64}, {"playlist-pos-1", MPV_FORMAT_INT64},
        {"playlist-count", MPV_FORMAT_INT64}, {"media-title", MPV_FORMAT_STRING}, {"title", MPV_FORMAT_STRING},
        {"working-directory", MPV_FORMAT_STRING}, {"edition-list", MPV_FORMAT_NODE}
    }, "Metadata");

    ObserveProps({
        {"demuxer-cache-duration", MPV_FORMAT_DOUBLE}, {"demuxer-cache-time", MPV_FORMAT_DOUBLE}, {"demuxer-bitrate", MPV_FORMAT_DOUBLE},
        {"demuxer-via-network", MPV_FORMAT_FLAG}, {"cache-buffering-state", MPV_FORMAT_INT64}, {"cache", MPV_FORMAT_DOUBLE},
        {"stream-path", MPV_FORMAT_STRING}, {"stream-pos", MPV_FORMAT_INT64}, {"network-time", MPV_FORMAT_DOUBLE}
    }, "Cache");

    mpv_observe_property(m_mpv, 0, "af-metadata/ebur_measurer", MPV_FORMAT_NODE);

    RATE_LIMITED_COUT(initmpvobservers_complete, 1, std::cout << "=================== [MPV] Observer registration complete ===================");
}

void MPVObserver::HandleMpvError(int err , const char* msgText)
{
    if (!m_mpv) return;
    switch (err) {
        // ===== Lỗi có thể tự sửa hoặc fallback =====
        case MPV_ERROR_EVENT_QUEUE_FULL:    
        case MPV_ERROR_PROPERTY_UNAVAILABLE:
        {
            if  (err == MPV_ERROR_EVENT_QUEUE_FULL)             RATE_LIMITED_COUT(mpv_error_event_queue_full, 1, std::cout << "[ERROR] MPV_ERROR_EVENT_QUEUE_FULL..."); 
            else if (err == MPV_ERROR_PROPERTY_UNAVAILABLE)     RATE_LIMITED_COUT(mpv_error_property_unavailable, 1, std::cout << "[ERROR] MPV_ERROR_PROPERTY_UNAVAILABLE..."); 
            break;
        }
        case MPV_ERROR_PROPERTY_FORMAT:     
        case MPV_ERROR_OPTION_FORMAT:       
        case MPV_ERROR_UNKNOWN_FORMAT:      
        {
            if(err == MPV_ERROR_PROPERTY_FORMAT)                RATE_LIMITED_COUT(mpv_error_property_format, 1,std::cout << "[ERROR] MPV_ERROR_PROPERTY_FORMAT..."); 
            else if (err == MPV_ERROR_OPTION_FORMAT)            RATE_LIMITED_COUT(mpv_error_option_format, 1,std::cout << "[ERROR] MPV_ERROR_OPTION_FORMAT...");
            else if (err == MPV_ERROR_UNKNOWN_FORMAT)           RATE_LIMITED_COUT(mpv_error_unknown_format, 1,std::cout << "[ERROR] MPV_ERROR_UNKNOWN_FORMAT...");

            const char* cmd[] = { "set", "ytdl-format", "bestvideo[height<=1080]+bestaudio/best", nullptr };
            auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
            if (commander) commander->Exec(cmd); 
            RATE_LIMITED_COUT(mpv_error_loading_failed_ytdl_format_reset, 1,std::cout << "[WARNING] [MPV] Reset ytdl-format to default and retrying...");
            break;
        }
        case MPV_ERROR_GENERIC:             
        case MPV_ERROR_LOADING_FAILED:      
        case MPV_ERROR_NOTHING_TO_PLAY :  
        {
            if(err == MPV_ERROR_GENERIC)                        RATE_LIMITED_COUT(mpv_error_generic, 1, std::cout << "[ERROR] MPV_ERROR_GENERIC...");
            else if (err == MPV_ERROR_LOADING_FAILED)           RATE_LIMITED_COUT(mpv_error_loading_failed, 1,std::cout << "[ERROR] MPV_ERROR_LOADING_FAILED...");
            else if (err == MPV_ERROR_NOTHING_TO_PLAY)          RATE_LIMITED_COUT(mpv_error_nothing_to_play, 1, std::cout << "[ERROR] MPV_ERROR_NOTHING_TO_PLAY...");
            if (g_playbackStatus.g_PlayingIndex >= 0) {
                int idx = g_playbackStatus.g_PlayingIndex;
                bool doRetry = false;

                {
                    std::lock_guard<std::mutex> lk(g_retry_mutex);
                    int retry = 0;
                    auto it = g_retryCount.find(idx);
                    if (it != g_retryCount.end()) retry = it->second;
                    if (retry < MAX_RETRY) {
                        g_retryCount[idx] = retry + 1;
                        doRetry = true;
                    } else {
                        g_retryCount.erase(idx);
                    }
                }

                if (doRetry) {
                    RATE_LIMITED_COUT(mpv_generic_error_retry, 1,std::cout << "[WARNING] [MPV] Retrying playback for index " << idx << "...");
                    std::thread([ idx]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        std::string idx_str = std::to_string(idx);
                        const char* args[] = { "playlist-play-index", idx_str.c_str(), nullptr };
                        auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                        if (commander) commander->Exec(args); 
                    }).detach();
                } else {
                    RATE_LIMITED_COUT(mpv_generic_error_max_retries, 1,std::cout << "[WARNING] [MPV] Max retries reached for index " << idx << ", removing from playlist.");
                    std::string idx_str = std::to_string(idx);
                    const char* args[] = { "playlist-remove", idx_str.c_str(), nullptr };
                    auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                    if (commander) commander->Exec(args); 
                }
            }
            break;
        }

        // ===== Lỗi nghiêm trọng, không thể tiếp tục =====
        case MPV_ERROR_PROPERTY_ERROR :     
        case MPV_ERROR_COMMAND :            
        case MPV_ERROR_PROPERTY_NOT_FOUND : 
        case MPV_ERROR_OPTION_ERROR :       
        case MPV_ERROR_OPTION_NOT_FOUND :   
        case MPV_ERROR_UNSUPPORTED:         
        case MPV_ERROR_NOT_IMPLEMENTED:     
        case MPV_ERROR_AO_INIT_FAILED:      
        case MPV_ERROR_VO_INIT_FAILED:      
        case MPV_ERROR_NOMEM:               
        case MPV_ERROR_INVALID_PARAMETER:   
        case MPV_ERROR_UNINITIALIZED: 
        { 
            if(err == MPV_ERROR_PROPERTY_ERROR)                 RATE_LIMITED_COUT(mpv_error_property_error, 1, std::cout << "[ERROR] MPV_ERROR_PROPERTY_ERROR..."); 
            else if(err == MPV_ERROR_COMMAND)                   RATE_LIMITED_COUT(mpv_error_command, 1, std::cout << "[ERROR] MPV_ERROR_COMMAND..."); 
            else if(err == MPV_ERROR_PROPERTY_NOT_FOUND)        RATE_LIMITED_COUT(mpv_error_property_not_found, 1, std::cout << "[ERROR] MPV_ERROR_PROPERTY_NOT_FOUND..."); 
            else if(err == MPV_ERROR_OPTION_ERROR)              RATE_LIMITED_COUT(mpv_error_option_error, 1, std::cout << "[ERROR] MPV_ERROR_OPTION_ERROR..."); 
            else if(err == MPV_ERROR_OPTION_NOT_FOUND)          RATE_LIMITED_COUT(mpv_error_option_not_found, 1, std::cout << "[ERROR] MPV_ERROR_OPTION_NOT_FOUND..."); 
            else if(err == MPV_ERROR_UNSUPPORTED)               RATE_LIMITED_COUT(mpv_error_unsupported, 1,std::cout << "[ERROR] MPV_ERROR_UNSUPPORTED..."); 
            else if(err == MPV_ERROR_NOT_IMPLEMENTED)           RATE_LIMITED_COUT(mpv_error_not_implemented, 1,std::cout << "[ERROR] MPV_ERROR_NOT_IMPLEMENTED..."); 
            else if(err == MPV_ERROR_AO_INIT_FAILED)            RATE_LIMITED_COUT(mpv_error_ao_init_failed, 1,std::cout << "[ERROR] MPV_ERROR_AO_INIT_FAILED..."); 
            else if(err == MPV_ERROR_VO_INIT_FAILED)            RATE_LIMITED_COUT(mpv_error_vo_init_failed, 1,std::cout << "[ERROR] MPV_ERROR_VO_INIT_FAILED..."); 
            else if(err == MPV_ERROR_NOMEM)                     RATE_LIMITED_COUT(mpv_error_nOMEM, 1,std::cout << "[ERROR] MPV_ERROR_NOMEM..."); 
            else if(err == MPV_ERROR_INVALID_PARAMETER)         RATE_LIMITED_COUT(mpv_error_invalid_parameter, 1,std::cout << "[ERROR] MPV_ERROR_INVALID_PARAMETER..."); 
            else if(err == MPV_ERROR_UNINITIALIZED)             RATE_LIMITED_COUT(mpv_error_uninitialized, 1,std::cout << "[ERROR] MPV_ERROR_UNINITIALIZED...");
             
            if (g_playbackStatus.g_PlayingIndex >= 0) {
                std::string idx_str = std::to_string(g_playbackStatus.g_PlayingIndex);
                const char* args[] = { "playlist-remove", idx_str.c_str(), nullptr };
                auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                if (commander) commander->Exec(args); 
            }
            break;
        }

        // ===== Lỗi khác (bỏ qua) =====
        default:
            RATE_LIMITED_COUT(mpv_error_other, 1,std::cout << "[ERROR] Other MPV error code: " << err << "";);
            break;
        break;
    }
}

const mpv_node* mpv_node_dict_find_local(const mpv_node* node, const char* key) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP)
        return nullptr;

    const mpv_node_list* list = node->u.list;
    for (int i = 0; i < list->num; i++) {
        if (strcmp(list->keys[i], key) == 0) {
            return &list->values[i];
        }
    }
    return nullptr;
}

void MPVObserver::UpdateVideoParams(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP)return;
    const mpv_node* n = nullptr;

    // Cập nhật vào State System mới (an toàn luồng)
    m_state.WriteVideo([&](VideoModel& m) {
        if ((n = mpv_node_dict_find_local(node, "pixelformat")) && n->format == MPV_FORMAT_STRING)    m.params.vpixfmt = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "primaries")) && n->format == MPV_FORMAT_STRING)      m.params.vprimaries = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "gamma")) && n->format == MPV_FORMAT_STRING)          m.params.vgamma = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "colormatrix")) && n->format == MPV_FORMAT_STRING)    m.params.vcolormatrix = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "colorlevels")) && n->format == MPV_FORMAT_STRING)    m.params.vcolorlevels = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "stereo-in")) && n->format == MPV_FORMAT_STRING)      m.params.vstereo_in = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "chroma-location")) && n->format == MPV_FORMAT_STRING)m.params.vchroma_location = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "aspect-name")) && n->format == MPV_FORMAT_STRING)    m.params.vaspect_name = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "sar-name")) && n->format == MPV_FORMAT_STRING)       m.params.vsar_name = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "light")) && n->format == MPV_FORMAT_STRING)          m.params.vlight = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "w")) && n->format == MPV_FORMAT_INT64)               m.dimensions.width = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "h")) && n->format == MPV_FORMAT_INT64)               m.dimensions.height = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "dw")) && n->format == MPV_FORMAT_INT64)              m.params.vdisp_w = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "dh")) && n->format == MPV_FORMAT_INT64)              m.params.vdisp_h = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "crop-x")) && n->format == MPV_FORMAT_INT64)          m.params.vcrop_x = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "crop-y")) && n->format == MPV_FORMAT_INT64)          m.params.vcrop_y = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "crop-w")) && n->format == MPV_FORMAT_INT64)          m.params.vcrop_w = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "crop-h")) && n->format == MPV_FORMAT_INT64)          m.params.vcrop_h = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "rotate")) && n->format == MPV_FORMAT_INT64)          m.dimensions.rotate = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "average-bpp")) && n->format == MPV_FORMAT_INT64)     m.params.average_bpp = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "aspect")) && n->format == MPV_FORMAT_DOUBLE)         m.dimensions.aspect = n->u.double_;
        if ((n = mpv_node_dict_find_local(node, "par")) && n->format == MPV_FORMAT_DOUBLE)            m.params.vpar = n->u.double_;
        if ((n = mpv_node_dict_find_local(node, "sar")) && n->format == MPV_FORMAT_DOUBLE)            m.params.vsar = n->u.double_;
        if ((n = mpv_node_dict_find_local(node, "sig-peak")) && n->format == MPV_FORMAT_DOUBLE)       m.params.vsig_peak = n->u.double_;
    });
    return;
    // Cập nhật vào biến global cũ để tương thích ngược
    auto& params = g_videoInfo.g_videoparams;
    {
        if ((n = mpv_node_dict_find_local(node, "pixelformat")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vpixfmt = n->u.string;            RATE_LIMITED_COUT(video_pixfmt, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Pixel Format: " << g_videoInfo.g_videoparams.vpixfmt);
        if ((n = mpv_node_dict_find_local(node, "primaries")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vprimaries = n->u.string;         RATE_LIMITED_COUT(video_primaries, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Primaries: " << g_videoInfo.g_videoparams.vprimaries);
        if ((n = mpv_node_dict_find_local(node, "gamma")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vgamma = n->u.string;             RATE_LIMITED_COUT(video_gamma, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Gamma: " << g_videoInfo.g_videoparams.vgamma);
        if ((n = mpv_node_dict_find_local(node, "colormatrix")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolormatrix = n->u.string;       RATE_LIMITED_COUT(video_colormatrix, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Matrix: " << g_videoInfo.g_videoparams.vcolormatrix);
        if ((n = mpv_node_dict_find_local(node, "colorlevels")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolorlevels = n->u.string;       RATE_LIMITED_COUT(video_colorlevels, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Levels: " << g_videoInfo.g_videoparams.vcolorlevels);
        if ((n = mpv_node_dict_find_local(node, "stereo-in")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vstereo_in = n->u.string;         RATE_LIMITED_COUT(video_stereo_in, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Stereo In: " << g_videoInfo.g_videoparams.vstereo_in);
        if ((n = mpv_node_dict_find_local(node, "chroma-location")) && n->format == MPV_FORMAT_STRING)g_videoInfo.g_videoparams.vchroma_location = n->u.string;   RATE_LIMITED_COUT(video_chroma_location, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Chroma Location: " << g_videoInfo.g_videoparams.vchroma_location);
        if ((n = mpv_node_dict_find_local(node, "aspect-name")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vaspect_name = n->u.string;       RATE_LIMITED_COUT(video_aspect_name, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Aspect Name: " << g_videoInfo.g_videoparams.vaspect_name);
        if ((n = mpv_node_dict_find_local(node, "sar-name")) && n->format == MPV_FORMAT_STRING)       g_videoInfo.g_videoparams.vsar_name = n->u.string;          RATE_LIMITED_COUT(video_vsar_name, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Storage Aspect Ratio Name " << g_videoInfo.g_videoparams.vsar_name);
        if ((n = mpv_node_dict_find_local(node, "light")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vlight = n->u.string;             RATE_LIMITED_COUT(video_light, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Light: " << g_videoInfo.g_videoparams.vlight);
    }
    {
        if ((n = mpv_node_dict_find_local(node, "w")) && n->format == MPV_FORMAT_INT64)               params.vwidth = (int)n->u.int64;         RATE_LIMITED_COUT(video_w, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Width: " << params.vwidth);
        if ((n = mpv_node_dict_find_local(node, "h")) && n->format == MPV_FORMAT_INT64)               params.vheight = (int)n->u.int64;        RATE_LIMITED_COUT(video_h, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Height: " << params.vheight);
        if ((n = mpv_node_dict_find_local(node, "dw")) && n->format == MPV_FORMAT_INT64)              params.vdisp_w = (int)n->u.int64;        RATE_LIMITED_COUT(video_dw, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Display Width: " << params.vdisp_w);
        if ((n = mpv_node_dict_find_local(node, "dh")) && n->format == MPV_FORMAT_INT64)              params.vdisp_h = (int)n->u.int64;        RATE_LIMITED_COUT(video_dh, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Display Height: " << params.vdisp_h);
        if ((n = mpv_node_dict_find_local(node, "crop-x")) && n->format == MPV_FORMAT_INT64)          params.vcrop_x = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_x, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop X: " << params.vcrop_x);
        if ((n = mpv_node_dict_find_local(node, "crop-y")) && n->format == MPV_FORMAT_INT64)          params.vcrop_y = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_y, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Y: " << params.vcrop_y);
        if ((n = mpv_node_dict_find_local(node, "crop-w")) && n->format == MPV_FORMAT_INT64)          params.vcrop_w = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_w, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Width: " << params.vcrop_w);
        if ((n = mpv_node_dict_find_local(node, "crop-h")) && n->format == MPV_FORMAT_INT64)          params.vcrop_h = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_h, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Height: " << params.vcrop_h);
        if ((n = mpv_node_dict_find_local(node, "rotate")) && n->format == MPV_FORMAT_INT64)          params.vrotate = (int)n->u.int64;        RATE_LIMITED_COUT(video_rotate, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Rotate: " << params.vrotate);
        if ((n = mpv_node_dict_find_local(node, "average-bpp")) && n->format == MPV_FORMAT_INT64)     params.average_bpp = (int)n->u.int64;    RATE_LIMITED_COUT(video_average_bpp, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Average bpp: " << params.average_bpp);
    }
    {
        if ((n = mpv_node_dict_find_local(node, "aspect")) && n->format == MPV_FORMAT_DOUBLE)         params.vaspect = n->u.double_;           RATE_LIMITED_COUT(video_aspect, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Aspect: " << params.vaspect);
        if ((n = mpv_node_dict_find_local(node, "par")) && n->format == MPV_FORMAT_DOUBLE)            params.vpar = n->u.double_;              RATE_LIMITED_COUT(video_par, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Pixel Aspect Ratio: " << params.vpar);
        if ((n = mpv_node_dict_find_local(node, "sar")) && n->format == MPV_FORMAT_DOUBLE)            params.vsar = n->u.double_;              RATE_LIMITED_COUT(video_sar, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Storage Aspect Ratio: " << params.vsar);
        if ((n = mpv_node_dict_find_local(node, "sig-peak")) && n->format == MPV_FORMAT_DOUBLE)       params.vsig_peak = n->u.double_;         RATE_LIMITED_COUT(video_sig_peak, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Sig Peak: " << params.vsig_peak);
    }
}

void MPVObserver::UpdateAudioParams(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP) return;
    const mpv_node* n = nullptr;

    // Cập nhật vào State System mới
    m_state.WriteAudio([&](AudioModel& m) {
        if ((n = mpv_node_dict_find_local(node, "format")) && n->format == MPV_FORMAT_STRING)         m.params.aformat = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "samplerate")) && n->format == MPV_FORMAT_INT64)      m.params.asamplerate = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "channel-count")) && n->format == MPV_FORMAT_INT64)   m.params.channel_count = (int)n->u.int64;
        if ((n = mpv_node_dict_find_local(node, "channels")) && n->format == MPV_FORMAT_STRING)       m.params.achannels_str = n->u.string;
        if ((n = mpv_node_dict_find_local(node, "hr-channels")) && n->format == MPV_FORMAT_STRING)    m.params.ahr_channels = n->u.string;
    });
    return;
    // Cập nhật vào biến global cũ để tương thích ngược
    auto& params = g_videoInfo.g_audioarams;
    if ((n = mpv_node_dict_find_local(node, "format")) && n->format == MPV_FORMAT_STRING)         params.aformat = n->u.string;              RATE_LIMITED_COUT(audio_format, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Format: " << params.aformat << "");
    if ((n = mpv_node_dict_find_local(node, "samplerate")) && n->format == MPV_FORMAT_INT64)      params.asamplerate = (int)n->u.int64;      RATE_LIMITED_COUT(audio_samplerate, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Sample Rate: " << params.asamplerate << " Hz");
    if ((n = mpv_node_dict_find_local(node, "channel-count")) && n->format == MPV_FORMAT_INT64)   params.channel_count = (int)n->u.int64;    RATE_LIMITED_COUT(audio_channel_count, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Channel Count: " << params.channel_count << "");
    if ((n = mpv_node_dict_find_local(node, "channels")) && n->format == MPV_FORMAT_STRING)       params.achannels_str = n->u.string;        RATE_LIMITED_COUT(audio_channels_str, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Channels: " << params.achannels_str << "");
    if ((n = mpv_node_dict_find_local(node, "hr-channels")) && n->format == MPV_FORMAT_STRING)    params.ahr_channels = n->u.string;         RATE_LIMITED_COUT(audio_hr_channels, 1,std::cout << "[DEBUG] [INFO] [AudioParams] HR Channels: " << params.ahr_channels << "");
}

void MPVObserver::UpdateTrackList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WriteTrack([&](auto& m){ m.tracks.clear(); });
        m_state.WriteSubtitle([&](auto& m){ m.hasSubtitles = false; });
        return;
        g_videoInfo.g_tracks.clear();
        g_videoInfo.hasSubtitles = false;
        return;
    }

    std::vector<TrackInfo> tmp;
    tmp.reserve(node->u.list->num);
    bool found_sub = false;
    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        TrackInfo track;
        const mpv_node* n = nullptr;

        // --- Helpers nội bộ ---
        auto read_str = [&](const char* key, std::string& out) {
            if ((n = mpv_node_dict_find_local(&entry, key)) && n->format == MPV_FORMAT_STRING)
                out = n->u.string ? n->u.string : "";
        };
        auto read_int = [&](const char* key, int& out) {
            if ((n = mpv_node_dict_find_local(&entry, key))) {
                if (n->format == MPV_FORMAT_INT64) out = (int)n->u.int64;
                else if (n->format == MPV_FORMAT_DOUBLE) out = (int)n->u.double_;
            }
        };
        auto read_dbl = [&](const char* key, double& out) {
            if ((n = mpv_node_dict_find_local(&entry, key))) {
                if (n->format == MPV_FORMAT_DOUBLE) out = n->u.double_;
                else if (n->format == MPV_FORMAT_INT64) out = (double)n->u.int64;
            }
        };
        auto read_bool = [&](const char* key, bool& out) {
            if ((n = mpv_node_dict_find_local(&entry, key)) && n->format == MPV_FORMAT_FLAG)
                out = (n->u.flag != 0);
        };

        // --- Mapping dữ liệu ---
        
        // 1. Common info
        read_int("id", track.common.id);
        read_int("ff-index", track.common.ff_index);
        read_str("type", track.common.type);
        if (track.common.type == "sub") {
            found_sub = true;
        }
        read_str("codec", track.common.codec);
        read_str("codec-desc", track.common.codec_desc);
        read_str("codec-profile", track.common.codec_profile);
        read_str("decoder", track.common.decoder);
        read_str("decoder-desc", track.common.decoder_desc);
        read_str("lang", track.common.language);
        read_str("title", track.common.title);
        read_bool("default", track.common.is_default);
        read_bool("forced", track.common.forced);
        read_bool("selected", track.common.selected);
        read_bool("external", track.common.external);

        // 2. Video specifics (Chỉ đọc nếu type là video hoặc image)
        if (track.common.type == "video" || track.common.type == "image") {
            read_int("demux-w", track.video.demux_w);
            read_int("demux-h", track.video.demux_h);
            read_dbl("demux-fps", track.video.demux_fps);
            read_str("format-name", track.video.format_name);
            read_bool("image", track.video.image);
            read_bool("albumart", track.video.albumart);
        }

        // 3. Audio specifics
        if (track.common.type == "audio") {
            read_int("demux-samplerate", track.audio.demux_samplerate);
            read_int("demux-channel-count", track.audio.demux_channel_count);
            read_int("audio-channels", track.audio.demux_channel_count); // Dự phòng
            read_str("demux-channels", track.audio.demux_channels);
            read_str("format-name", track.audio.format_name);
        }

        // 4. Accessibility & Others
        read_bool("visual-impaired", track.access.visual_impaired);
        read_bool("hearing-impaired", track.access.hearing_impaired);
        read_bool("dependent", track.access.dependent);
        read_int("main-selection", track.main_selection);

        tmp.push_back(std::move(track));
    }
    RATE_LIMITED_COUT(track_list, 1,std::cout << "[DEBUG] [INFO] [Track List] Total tracks: " << tmp.size() << "\n";);

    // Cập nhật vào State System mới
    m_state.WriteTrack([&](TrackModel& m) { m.tracks = tmp; });
    m_state.WriteSubtitle([&](SubtitleModel& m) { m.hasSubtitles = found_sub; });
    return;
    // Cập nhật vào biến global cũ
    g_videoInfo.hasSubtitles = found_sub;
    g_videoInfo.g_tracks.swap(tmp);
}

void MPVObserver::UpdateAudioDeviceList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WriteAudio([&](auto& m){ m.device.audioDevices.clear(); });
        return;
        g_playbackStatus.g_audioDevices.clear();
        return;
    }
    
    std::vector<AudioDeviceInfo> tmp;

    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        const mpv_node* n = nullptr;
        AudioDeviceInfo dev;

        if ((n = mpv_node_dict_find_local(&entry, "name")) && n->format == MPV_FORMAT_STRING)dev.name = n->u.string;
        if ((n = mpv_node_dict_find_local(&entry, "description")) && n->format == MPV_FORMAT_STRING)dev.description = n->u.string;
        RATE_LIMITED_COUT(audio_device, 1,
            std::cout << "[DEBUG] [INFO] [Audio Device] " 
                      << dev.name << " (" << dev.description << ")"
                      << "\n";);

        tmp.push_back(dev);
    }

    // Cập nhật vào State System mới
    m_state.WriteAudio([&](AudioModel& m) { m.device.audioDevices = tmp; });
    return;
    // Cập nhật vào biến global cũ
    g_playbackStatus.g_audioDevices.swap(tmp);
}

void MPVObserver::UpdateChapterList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list || node->u.list->num == 0) {
        m_state.WriteTrack([&](auto& m){ m.chapters.clear(); });
        return;
        g_videoInfo.g_chapters.clear();
        return;
    }
    std::vector<ChapterInfo> tmp;
    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        const mpv_node* n = nullptr;
        ChapterInfo dev;

        if ((n = mpv_node_dict_find_local(&entry, "time"))) {
            if (n->format == MPV_FORMAT_INT64)       dev.time = static_cast<double>(n->u.int64);
            else if (n->format == MPV_FORMAT_DOUBLE) dev.time = n->u.double_;
        }
        if ((n = mpv_node_dict_find_local(&entry, "title")) && n->format == MPV_FORMAT_STRING) {dev.title = n->u.string;}

        tmp.push_back({dev});
        RATE_LIMITED_COUT(fetch_chapter_list, 1,std::cout << "[DEBUG] [INFO] [Chapters] Chapter found: " << dev.title << " at " << dev.time << " seconds.";);
    }

    // Cập nhật vào State System mới
    m_state.WriteTrack([&](TrackModel& m) { m.chapters = tmp; });
    return;
    // Cập nhật vào biến global cũ
    g_videoInfo.g_chapters.swap(tmp);
}

void MPVObserver::UpdatePlaylist(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WritePlaylist([&](auto& m){ m.playlist.clear(); });
        return;
        g_playbackStatus.g_playlist.clear();
        return;
    }
    std::vector<PlaylistEntry> tmp;
    for (int i = 0; i < node->u.list->num; i++) {
        const mpv_node* entry = &node->u.list->values[i];
        if (entry->format != MPV_FORMAT_NODE_MAP || !entry->u.list) 
            continue;
        const mpv_node* n = nullptr;
        PlaylistEntry dev ;
        if ((n = mpv_node_dict_find_local(entry, "playing")) && n->format == MPV_FORMAT_FLAG) dev.playing = (n->u.flag != 0);
        if ((n = mpv_node_dict_find_local(entry, "current")) && n->format == MPV_FORMAT_FLAG) dev.current = (n->u.flag != 0);
        if ((n = mpv_node_dict_find_local(entry, "id")) && n->format == MPV_FORMAT_INT64) dev.id = (int)n->u.int64;;
        if ((n = mpv_node_dict_find_local(entry, "filename")) && n->format == MPV_FORMAT_STRING) dev.filename = n->u.string;
        if ((n = mpv_node_dict_find_local(entry, "title")) && n->format == MPV_FORMAT_STRING) dev.title = n->u.string;

        RATE_LIMITED_COUT(playlist_item, 1,
            std::cout << "[DEBUG] [INFO] [Playlist] Entry " << i
                    << " Filename: " << dev.filename
                    << " Title: " << dev.title
                    << " Current: " << (dev.current ? "True" : "False")
                    << " Playing: " << (dev.playing ? "True" : "False")
                    << " ID: " << dev.id
                    << std::endl;
        );
            
        tmp.push_back(dev);
    }

    // Cập nhật vào State System mới
    m_state.WritePlaylist([&](PlaylistModel& m) { m.playlist = tmp; });
    return;
    // Cập nhật vào biến global cũ
    g_playbackStatus.g_playlist.swap(tmp);
}

void MPVObserver::UpdateMetadata(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list) {
        m_state.WriteMedia([&](auto& m){ m.metadata.clear(); });
        return;
        g_videoInfo.metadata.clear();
        return;
    }

    std::unordered_map<std::string, std::string> tmp;

    const mpv_node_list* list = node->u.list;
    for (int i = 0; i < list->num; ++i) {
        const char* key = list->keys[i];
        const mpv_node& value = list->values[i];

        if (key && value.format == MPV_FORMAT_STRING && value.u.string) {
            tmp[key] = value.u.string;

            RATE_LIMITED_COUT(fetch_metadata, 1,
                std::cout << "[DEBUG] [INFO] [Metadata] "
                          << key << " = " << value.u.string << "\n");
        }
    }

    // Cập nhật vào State System mới
    m_state.WriteMedia([&](MediaModel& m) { m.metadata = tmp; });
    return;
    // Cập nhật vào biến global cũ
    g_videoInfo.metadata.swap(tmp);

}

void MPVObserver::UpdateLoudnessMetadata(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP) return;
    mpv_node_list* list = node->u.list;
    
    // Hàm chuyển đổi an toàn từ chuỗi sang Double hệ dB
    auto parse_db = [](const char* str) -> double {
        if (!str || std::string(str) == "-inf") return -99.0;
        try { return std::stod(str); } catch (...) { return -99.0; }
    };

    // Cập nhật vào State System mới
    m_state.WriteAudio([&](AudioModel& m) {
        for (int i = 0; i < list->num; i++) {
            if (list->values[i].format != MPV_FORMAT_STRING) continue;
            std::string key = list->keys[i];
            const char* val_str = list->values[i].u.string;

            if (key == "lavfi.r128.M") { m.loudness.loudness_momentary = parse_db(val_str); }
            else if (key == "lavfi.r128.S") { m.loudness.loudness_shortterm = parse_db(val_str); }
            else if (key == "lavfi.r128.I") { m.loudness.loudness_integrated = parse_db(val_str); }
            else if (key == "lavfi.r128.LRA") { m.loudness.loudness_range = parse_db(val_str); }
            else if (key == "lavfi.r128.LRA.low") { m.loudness.loudness_lra_low = parse_db(val_str); }
            else if (key == "lavfi.r128.LRA.high") { m.loudness.loudness_lra_high = parse_db(val_str); }
            else if (key == "lavfi.r128.true_peak") { m.loudness.true_peak = parse_db(val_str); }
            else if (key == "lavfi.r128.true_peaks_ch0") { m.loudness.true_peak_ch0 = parse_db(val_str); }
            else if (key == "lavfi.r128.true_peaks_ch1") { m.loudness.true_peak_ch1 = parse_db(val_str); }
            else if (key == "lavfi.r128.sample_peaks_ch0") { m.loudness.sample_peak_ch0 = parse_db(val_str); }
            else if (key == "lavfi.r128.sample_peaks_ch1") { m.loudness.sample_peak_ch1 = parse_db(val_str); }
            else if (key == "lavfi.r128.sample_peak") { m.loudness.sample_peak = parse_db(val_str); }
        }
    });
    return;
    // Cập nhật vào biến global cũ
    auto& ctx = g_videoInfo.g_audioarams;
    for (int i = 0; i < list->num; i++) {
        if (list->values[i].format != MPV_FORMAT_STRING) continue;
        std::string key = list->keys[i];
        const char* val_str = list->values[i].u.string;

        if (key == "lavfi.r128.M") { ctx.loudness_momentary = parse_db(val_str); }
        else if (key == "lavfi.r128.S") { ctx.loudness_shortterm = parse_db(val_str); }
        else if (key == "lavfi.r128.I") { ctx.loudness_integrated = parse_db(val_str); }
        else if (key == "lavfi.r128.LRA") { ctx.loudness_range = parse_db(val_str); }
        else if (key == "lavfi.r128.LRA.low") { ctx.loudness_lra_low = parse_db(val_str); }
        else if (key == "lavfi.r128.LRA.high") { ctx.loudness_lra_high = parse_db(val_str); }
        else if (key == "lavfi.r128.true_peak") { ctx.true_peak = parse_db(val_str); }
        else if (key == "lavfi.r128.true_peaks_ch0") { ctx.true_peak_ch0 = parse_db(val_str); }
        else if (key == "lavfi.r128.true_peaks_ch1") { ctx.true_peak_ch1 = parse_db(val_str); }
        else if (key == "lavfi.r128.sample_peaks_ch0") { ctx.sample_peak_ch0 = parse_db(val_str); }
        else if (key == "lavfi.r128.sample_peaks_ch1") { ctx.sample_peak_ch1 = parse_db(val_str); }
        else if (key == "lavfi.r128.sample_peak") { ctx.sample_peak = parse_db(val_str); }
    }
}


void MPVObserver::ProcessEvents() {
    if (!m_mpv) return;
    while (mpv_event* event = mpv_wait_event(m_mpv, 0)) {
        if(event->event_id == MPV_EVENT_NONE) break;
        switch (event->event_id) {

        case MPV_EVENT_SHUTDOWN: {
            //g_playbackStatus.hasFile = false;   
            RATE_LIMITED_COUT(mpv_shutdown, 1,std::cout << "[DEBUG] [INFO] [MPV] MPV is shutting down."); 
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            auto* msg = (mpv_event_log_message*)event->data;

            if (msg && msg->prefix && msg->text && std::string(msg->prefix) == "cplayer")
                HandleYTDLLog(m_mpv,msg->text);
            
            if (msg && msg->level && (strcmp(msg->level, "error")  == 0 ||
                                      strcmp(msg->level, "warn")  == 0 ))

                PushMpvError(msg->level, msg->text);
            break;
        }
        case MPV_EVENT_GET_PROPERTY_REPLY: break;
        case MPV_EVENT_SET_PROPERTY_REPLY: break;
        case MPV_EVENT_COMMAND_REPLY: break;
        case MPV_EVENT_START_FILE: {
            m_state.WritePlayback([&](auto& m) {
                //m.flags.isIdleActive = false;
                m.isLoadingMedia = true;
            });
            //g_playbackStatus.ilde = false;
            //g_playbackStatus.isLoadingMedia = true;
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto* data = (mpv_event_end_file*)event->data;
            if (!data) break;

            switch (data->reason) {
            // --- Phát hết file bình thường ---
            case MPV_END_FILE_REASON_EOF: 

                RATE_LIMITED_COUT(end_of_file, 1,std::cout << "[DEBUG] [INFO] [MPV] Playback reached end of file.");
                break;
            
            // --- Lỗi trong quá trình phát / load ---
            case MPV_END_FILE_REASON_ERROR: 
            {
                m_state.WritePlayback([&](auto& m) {
                    m.isLoadingMedia = false;
                    //m.flags.hasFile = false;
                });
                //g_playbackStatus.isLoadingMedia = false;
                int err = data->error;
                std::string errStr = mpv_error_string(err);
                RATE_LIMITED_COUT(mpv_end_file_error, 1,std::cout << "[ERROR] [MPV ERROR] Playback error occurred: " << errStr << "";);
                HandleMpvError(err, errStr.c_str());
                break;
            }

            // --- Dừng thủ công ---
            case MPV_END_FILE_REASON_STOP:
                RATE_LIMITED_COUT(manual_stop, 1,std::cout << "[DEBUG] [INFO] [MPV] Playback stopped manually.");
                break;

            // --- Redirect URL ---
            case MPV_END_FILE_REASON_REDIRECT:
                RATE_LIMITED_COUT(redirect, 1,std::cout << "[DEBUG] [WARNING] [MPV] Playback ended due to redirect.");
                break;

            default:
                RATE_LIMITED_COUT(unknown_end_file_reason, 1,std::cout << "[DEBUG] [WARNING] [MPV] Playback ended for unknown reason: " << data->reason << ".");
                break;
            }

            break;
        }
        case MPV_EVENT_FILE_LOADED:{
            /*
            auto* session = MPVManager::GetInstance().GetDefaultSession();
            if (session && session->GetProperty()) {
                if (auto index = session->GetProperty()->GetInt("playlist-pos")) g_playbackStatus.g_PlayingIndex = (int)index.value();
                if (auto index = session->GetProperty()->GetInt("playlist-pos-1")) g_playbackStatus.g_PlayingIndex_1 = (int)index.value();
            }*/
            m_state.WritePlayback([&](auto& m) {
                m.isLoadingMedia = false;
                m.flags.hasFile = true;
            });
 
            //g_playbackStatus.isLoadingMedia = false;
            //g_playbackStatus.hasFile = true;
            break;
        }
        case MPV_EVENT_IDLE: {
            // Cập nhật State System mới
//            m_state.WritePlayback([&](auto& m) {
                //m.flags.isIdleActive = true;
//                m.flags.hasFile = false;
//            });
            // Cập nhật biến global cũ
            //g_playbackStatus.hasFile = false; 
            //g_playbackStatus.ilde = true ;
            RATE_LIMITED_COUT(mpv_idle, 1,std::cout << "[DEBUG] [INFO] [MPV] MPV is now idle."); 
            break;
        }
        case MPV_EVENT_TICK: break;
        case MPV_EVENT_CLIENT_MESSAGE: break;
        case MPV_EVENT_VIDEO_RECONFIG: {
            RATE_LIMITED_COUT(video_reconfig, 1,std::cout << "[DEBUG] [INFO] [VIDEO RECONFIG] Video configuration changed."); 
            break;
        }
        case MPV_EVENT_AUDIO_RECONFIG: {
            RATE_LIMITED_COUT(audio_reconfig, 1,std::cout << "[DEBUG] [INFO] [AUDIO RECONFIG] Audio configuration changed.");
            break;
        }
        case MPV_EVENT_SEEK:{
            // Cập nhật State System mới
//            m_state.WritePlayback([&](auto& m) {
//                m.flags.isSeeking = true;
//            });
            // Cập nhật biến global cũ
            //g_playbackStatus.isSeeking = true; 
            RATE_LIMITED_COUT(mpv_seek, 1,std::cout << "[DEBUG] [INFO] [MPV] Seek operation started."); 
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART: 
        {   
            // m_state.WritePlayback([&](auto& m) {
            //    m.flags.isSeeking = true;
            //});
            //g_playbackStatus.isSeeking = false; 
            if(pendingSeekTime >= 0.0){
                auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                if (commander && !(GetVideoType() == VideoType::Live)) commander->Seek(pendingSeekTime, g_playbackStatus.duration);
                RATE_LIMITED_COUT(playback_restart_seek, 1,std::cout << "[DEBUG] [INFO] [MPV] Performing pending seek to " << pendingSeekTime << " seconds.");
                pendingSeekTime = -1.0;
            }
            RATE_LIMITED_COUT(playback_restart, 1,std::cout << "[DEBUG] [INFO] [MPV] Playback restarted.");
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            HandlePropertyChange((mpv_event_property*)event->data);
            break;
        }
        case MPV_EVENT_QUEUE_OVERFLOW: break;
        case MPV_EVENT_HOOK: break;
        default: break;
        }
    }
}

void MPVObserver::HandlePropertyChange(mpv_event_property* prop) {
    if (!prop || !prop->data) return;

    switch (prop->format) {
        case MPV_FORMAT_STRING:
            HandleStringProperty(prop->name, *(const char**)prop->data);
            break;
        case MPV_FORMAT_FLAG:
            HandleFlagProperty(prop->name, (*(int*)prop->data) != 0);
            break;
        case MPV_FORMAT_INT64:
            HandleInt64Property(prop->name, *(int64_t*)prop->data);
            break;
        case MPV_FORMAT_DOUBLE:
            HandleDoubleProperty(prop->name, *(double*)prop->data);
            break;
        case MPV_FORMAT_NODE:
            HandleNodeProperty(prop->name, (const mpv_node*)prop->data);
            break;
        default:
            // Các format khác không được xử lý
            break;
    }
}

void MPVObserver::HandleStringProperty(const char* name, const char* value) {
    if (!value) return;

    // Cập nhật State System (an toàn luồng)
    if (strcmp(name, "loop") == 0) m_state.WritePlayback([&](auto& m) { m.config.loopMode = value; });
    else if (strcmp(name, "title") == 0) m_state.WriteMedia([&](auto& m) { m.title = value; });
    else if (strcmp(name, "media-title") == 0) m_state.WriteMedia([&](auto& m) { m.mediaTitle = value; });
    else if (strcmp(name, "filename") == 0) m_state.WriteMedia([&](auto& m) { m.filename = value; });
    else if (strcmp(name, "file-format") == 0) m_state.WriteMedia([&](auto& m) { m.fileFormat = value; });
    else if (strcmp(name, "working-directory") == 0) m_state.WriteMedia([&](auto& m) { m.working_directory = value; });
    else if (strcmp(name, "stream-open-filename") == 0) m_state.WriteMedia([&](auto& m) { m.streamUrl = value; });
    else if (strcmp(name, "stream-path") == 0) m_state.WriteMedia([&](auto& m) { m.stream_path = value; });
    else if (strcmp(name, "video-format") == 0) m_state.WriteVideo([&](auto& m) { m.codec.video_format = value; });
    else if (strcmp(name, "video-codec") == 0) m_state.WriteVideo([&](auto& m) { m.codec.vcodec = value; });
    else if (strcmp(name, "vo") == 0) m_state.WriteVideo([&](auto& m) { m.codec.v_out = value; });
    else if (strcmp(name, "hwdec-current") == 0) m_state.WriteVideo([&](auto& m) { m.codec.hwdec = value; });
    else if (strcmp(name, "audio-codec") == 0) m_state.WriteAudio([&](auto& m) { m.codec.acodec = value; });
    else if (strcmp(name, "ao") == 0) m_state.WriteAudio([&](auto& m) { m.codec.a_out = value; });
    else if (strcmp(name, "af") == 0) m_state.WriteAudio([&](auto& m) { m.codec.a_filter = value; });
    else if (strcmp(name, "audio-device") == 0) m_state.WriteAudio([&](auto& m) { m.device.audio_device = value; });
    else if (strcmp(name, "audio-client-name") == 0) m_state.WriteAudio([&](auto& m) { m.device.audio_client_name = value; });
    else if (strcmp(name, "sub-codec") == 0) m_state.WriteSubtitle([&](auto& m) { m.sub_codec = value; });
    else if (strcmp(name, "sub-ass-override") == 0) m_state.WriteSubtitle([&](auto& m) { m.sub_ass_override = value; });
    return;
    // Cập nhật biến global cũ để tương thích ngược
    if (strcmp(name, "loop") == 0)                          {g_playbackStatus.loopMode = value;                                    RATE_LIMITED_COUT(loop_mode, 1,std::cout << "[DEBUG] [INFO] [Playback] Loop Mode updated: " << g_playbackStatus.loopMode << "");}
    else if (strcmp(name, "working-directory") == 0)        {g_playbackStatus.working_directory  = value;}
    else if (strcmp(name, "filename") == 0)                 {g_playbackStatus.hasFile = true; g_playbackStatus.filename = value;   RATE_LIMITED_COUT(filename, 1,std::cout << "[DEBUG] [INFO] [Media] Filename updated: " << g_playbackStatus.filename << "");}
    else if (strcmp(name, "stream-open-filename") == 0)     {g_playbackStatus.streamUrl = value;                                   RATE_LIMITED_COUT(stream_open_filename, 1,std::cout << "[DEBUG] [INFO] [Media] Stream Open Filename updated: " << g_playbackStatus.streamUrl << "");}
    else if (strcmp(name, "media-title") == 0)              {g_playbackStatus.mediaTitle = value;                                  RATE_LIMITED_COUT(media_title, 1,std::cout << "[DEBUG] [INFO] [Media] Media Title updated: " << g_playbackStatus.mediaTitle << "");}
    else if (strcmp(name, "file-format") == 0)              {g_playbackStatus.fileFormat = value;                                  RATE_LIMITED_COUT(file_format, 1,std::cout << "[DEBUG] [INFO] [Media] File Format updated: " << g_playbackStatus.fileFormat << "");}
    else if (strcmp(name, "title") == 0)                    {g_playbackStatus.Title = value;                                       RATE_LIMITED_COUT(title, 1,std::cout << "[DEBUG] [INFO] [Media] Title updated: " << g_playbackStatus.Title << "");}
    else if (strcmp(name, "sub-ass-override") == 0)         {g_playbackStatus.g_subinfo.sub_ass_override  = value;}
    else if (strcmp(name, "audio-client-name") == 0)        {g_playbackStatus.audio_client_name = value;                           RATE_LIMITED_COUT(video_bitrate, 1,std::cout << "[DEBUG] [INFO] [Video] Audio Client Name updated: " << g_playbackStatus.audio_client_name << " ");}
    else if (strcmp(name, "sub-codec") == 0)                {g_playbackStatus.g_subinfo.sub_codec  = value;                        RATE_LIMITED_COUT(sub_codec, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Codec updated: " << g_playbackStatus.g_subinfo.sub_codec << "");}
    else if (strcmp(name, "audio-codec") == 0)              {g_videoInfo.acodec = value;                                           RATE_LIMITED_COUT(audio_codec, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Codec updated: " << g_videoInfo.acodec << "");}
    else if (strcmp(name, "ao") == 0)                       {g_videoInfo.a_out = value;}
    else if (strcmp(name, "audio-device") == 0)             {g_videoInfo.audio_device = value;                                     RATE_LIMITED_COUT(audio_device, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Device updated: " << g_videoInfo.audio_device << "");}
    else if (strcmp(name, "af") == 0)                       {g_videoInfo.a_filter = value;}
    else if (strcmp(name, "vo") == 0)                       {g_videoInfo.v_out = value;}
    else if (strcmp(name, "hwdec-current") == 0 )           {g_videoInfo.hwdec = value;                                            RATE_LIMITED_COUT(hwdec_current, 1,std::cout << "[DEBUG] [INFO] [Video] HWDEC updated: " << g_videoInfo.hwdec << "");}
    else if (strcmp(name, "video-codec") == 0)              {g_videoInfo.vcodec = value;                                           RATE_LIMITED_COUT(video_codec, 1,std::cout << "[DEBUG] [INFO] [Video] Video Codec updated: " << g_videoInfo.vcodec << "");}
    else if (strcmp(name, "video-format") == 0)             {g_videoInfo.video_format = value;                                     RATE_LIMITED_COUT(video_format, 1,std::cout << "[DEBUG] [INFO] [Video] Video Format updated: " << g_videoInfo.video_format << "");}
    else if (strcmp(name, "stream-path") == 0)              {g_playbackStatus.stream_path  = value;}
}

void MPVObserver::HandleFlagProperty(const char* name, bool value) {
    // Cập nhật State System (an toàn luồng)
    if (strcmp(name, "pause") == 0) m_state.WritePlayback([&](auto& m) { m.flags.isPaused = value; });
    else if (strcmp(name, "seeking") == 0) m_state.WritePlayback([&](auto& m) { m.flags.isSeeking = value; });
    else if (strcmp(name, "eof-reached") == 0) m_state.WritePlayback([&](auto& m) { m.flags.eofReached = value; });
    else if (strcmp(name, "core-idle") == 0) m_state.WritePlayback([&](auto& m) { m.flags.isCoreIdle = value; });
    else if (strcmp(name, "seekable") == 0) m_state.WritePlayback([&](auto& m) { m.flags.seekable = value; });
    else if (strcmp(name, "idle-active") == 0) m_state.WritePlayback([&](auto& m) { m.flags.isIdleActive = value; });
    else if (strcmp(name, "mute") == 0) m_state.WriteAudio([&](auto& m) { m.volume.isMuted = value; });
    else if (strcmp(name, "sub-visibility") == 0) m_state.WriteSubtitle([&](auto& m) { m.sub_Visible = value; });
    else if (strcmp(name, "demuxer-via-network") == 0) m_state.WriteNetwork([&](auto& m) { m.demuxer_via_network = value; });
    return;
    // Cập nhật biến global cũ để tương thích ngược
    if (strcmp(name, "pause") == 0)                          {g_playbackStatus.isPaused = value;                                   RATE_LIMITED_COUT(pause_state, 1,std::cout << "[DEBUG] [INFO] [Playback] Pause state updated: " << (g_playbackStatus.isPaused ? "Paused" : "Playing") << "");}
    else if (strcmp(name, "core-idle") == 0)                 {g_playbackStatus.isCoreIdle = value;                                 RATE_LIMITED_COUT(core_idle, 1,std::cout << "[DEBUG] [INFO] [Playback] Core Idle state updated: " << (g_playbackStatus.isCoreIdle ? "Idle" : "Active") << "");}
    else if (strcmp(name, "idle-active") == 0)               {g_playbackStatus.idle_active = value;}
    else if (strcmp(name, "eof-reached") == 0)               {g_playbackStatus.eofReached = value;                                 RATE_LIMITED_COUT(eof_reached, 1,std::cout << "[DEBUG] [INFO] [Playback] EOF Reached state updated: " << (g_playbackStatus.eofReached ? "Yes" : "No") << "");}
    else if (strcmp(name, "mute") == 0)                      {g_playbackStatus.isMuted = value;                                    RATE_LIMITED_COUT(mute_state, 1,std::cout << "[DEBUG] [INFO] [Audio] Mute state updated: " << (g_playbackStatus.isMuted ? "Muted" : "Unmuted") << "");}
    else if (strcmp(name, "seekable") == 0)                  {g_playbackStatus.seekable = value;                                   RATE_LIMITED_COUT(seekable_state, 1,std::cout << "[DEBUG] [INFO] [Playback] Seekable state updated: " << (g_playbackStatus.seekable ? "Yes" : "No") << "");}
    else if (strcmp(name, "seeking") == 0)                   {g_playbackStatus.seeking = value; g_playbackStatus.isSeeking = value; RATE_LIMITED_COUT(seeking, 1,std::cout << "[DEBUG] [INFO] [Playback] Seeking updated: " << g_playbackStatus.seeking << "");}
    else if (strcmp(name, "sub-visibility") == 0)            {g_playbackStatus.g_subinfo.sub_Visible = value;                      RATE_LIMITED_COUT(sub_visibility, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Visibility updated: " << (g_playbackStatus.g_subinfo.sub_Visible ? "Visible" : "Hidden") << "");}
    else if (strcmp(name, "demuxer-via-network") == 0)       {g_playbackStatus.demuxer_via_network = value;}
}

void MPVObserver::HandleInt64Property(const char* name, int64_t value) {
    // Cập nhật State System (an toàn luồng)
    if (strcmp(name, "width") == 0) m_state.WriteVideo([&](auto& m) { m.dimensions.width = (int)value; });
    else if (strcmp(name, "height") == 0) m_state.WriteVideo([&](auto& m) { m.dimensions.height = (int)value; });
    else if (strcmp(name, "video-rotate") == 0) m_state.WriteVideo([&](auto& m) { m.dimensions.rotate = (int)value; });
    else if (strcmp(name, "video-bitrate") == 0) m_state.WriteVideo([&](auto& m) { m.stats.vbitrate = (int)value; });
    else if (strcmp(name, "volume") == 0) m_state.WriteAudio([&](auto& m) { m.volume.volume = (int)value; });
    else if (strcmp(name, "audio-bitrate") == 0) m_state.WriteAudio([&](auto& m) { m.codec.abitrate = (int)value; });
    else if (strcmp(name, "audio-channels") == 0) m_state.WriteAudio([&](auto& m) { m.params.channel_count = (int)value; });
    else if (strcmp(name, "audio-samplerate") == 0) m_state.WriteAudio([&](auto& m) { m.params.asamplerate = (int)value; });
    else if (strcmp(name, "chapter") == 0) m_state.WriteTrack([&](auto& m) { m.g_current_chapter = (int)value; });
    else if (strcmp(name, "playlist-pos") == 0) m_state.WritePlaylist([&](auto& m) { m.g_PlayingIndex = (int)value; });
    else if (strcmp(name, "playlist-pos-1") == 0) m_state.WritePlaylist([&](auto& m) { m.g_PlayingIndex_1 = (int)value; });
    else if (strcmp(name, "playlist-count") == 0) m_state.WritePlaylist([&](auto& m) { m.g_playlist_count = (int)value; });
    else if (strcmp(name, "stream-pos") == 0) m_state.WriteNetwork([&](auto& m) { m.stream_pos = value; });
    else if (strcmp(name, "cache-buffering-state") == 0) m_state.WriteNetwork([&](auto& m) { m.cache_buffering_state = (int)value; });
    return;
    // Cập nhật biến global cũ để tương thích ngược
    if (strcmp(name, "volume") == 0)                       {g_playbackStatus.volume = (int)value;                                 RATE_LIMITED_COUT(volume_level, 1,std::cout << "[DEBUG] [INFO] [Audio] Volume updated: " << g_playbackStatus.volume << "}");}
    else if (strcmp(name, "playlist-pos") == 0)            {g_playbackStatus.g_PlayingIndex = (int)value;                           RATE_LIMITED_COUT(playlist_pos, 1,std::cout << "[DEBUG] [INFO] [Playlist] Current Playing Index updated: " << g_playbackStatus.g_PlayingIndex << "");}
    else if (strcmp(name, "playlist-pos-1") == 0)          {g_playbackStatus.g_PlayingIndex_1 = (int)value;                         RATE_LIMITED_COUT(playlist_pos_1, 1,std::cout << "[DEBUG] [INFO] [Playlist] Current Playing Index -1 updated: " << g_playbackStatus.g_PlayingIndex_1 << "");}
    else if (strcmp(name, "playlist-count") == 0) {
        g_playbackStatus.g_playlist_count = (int)value;
        if (g_playbackStatus.g_playlist_count >= 2 && playImmediately) {
            playImmediately = false;
            int newIndex = (int)g_playbackStatus.g_playlist.size() - 1;
            auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
            if (commander) commander->Exec("playlist-play-index " + std::to_string(newIndex));
            RATE_LIMITED_COUT(playlist_play_index, 1,std::cout << "[DEBUG] [INFO] [Playlist] Auto-playing newly added item at index: " << newIndex << "");
        }
    }
    else if (strcmp(name, "chapter") == 0)                   {g_videoInfo.g_current_chapter = (int)value;                           RATE_LIMITED_COUT(chapter_index, 1,std::cout << "[DEBUG] [INFO] [Chapters] Current Chapter Index updated: " << g_videoInfo.g_current_chapter << "");}
    else if (strcmp(name, "audio-bitrate") == 0)             {g_videoInfo.abitrate = (int)value;                                    RATE_LIMITED_COUT(audio_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Audio] Audio Bitrate updated: " << g_videoInfo.abitrate/1000 << " kbps");}
    else if (strcmp(name, "audio-samplerate") == 0)          {g_videoInfo.asamplerate = (int)value;                                 RATE_LIMITED_COUT(audio_samplerate, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Sample Rate updated: " << g_videoInfo.asamplerate << " Hz");}
    else if (strcmp(name, "audio-channels") == 0)            {g_videoInfo.achannels = (int)value;                                   RATE_LIMITED_COUT(audio_channels, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Channels updated: " << g_videoInfo.achannels << "");}
    else if (strcmp(name, "video-bitrate") == 0)             {g_videoInfo.vbitrate = (int)value;                                    RATE_LIMITED_COUT(video_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Video] Video Bitrate updated: " << g_videoInfo.vbitrate/1000 << " kbps");}
    else if (strcmp(name, "video-rotate") == 0)            {g_videoInfo.rotate = (int)value;                                      RATE_LIMITED_COUT(video_rotation, 1,std::cout << "[DEBUG] [INFO] [Video] Video Rotation updated: " << g_videoInfo.rotate << "");}
    else if (strcmp(name, "width") == 0)                     {g_videoInfo.width = (int)value;                                       RATE_LIMITED_COUT(width, 1,std::cout << "[DEBUG] [INFO] [Video] Video Width updated: " << g_videoInfo.width << "");}
    else if (strcmp(name, "height") == 0)                    {g_videoInfo.height = (int)value;                                      RATE_LIMITED_COUT(height, 1,std::cout << "[DEBUG] [INFO] [Video] Video Height updated: " << g_videoInfo.height << "");}
    else if (strcmp(name, "stream-pos") == 0)                {g_playbackStatus.stream_pos = (int)value;}
    else if (strcmp(name, "cache-buffering-state") == 0 )    {g_playbackStatus.cache_buffering_state = (int)value;}
}

void MPVObserver::HandleDoubleProperty(const char* name, double value) {
    // Cập nhật State System (an toàn luồng)
    if (strcmp(name, "duration") == 0) m_state.WritePlayback([&](auto& m) { m.timing.duration = value; });
    else if (strcmp(name, "time-pos") == 0) m_state.WritePlayback([&](auto& m) { m.timing.timePos = value; });
    else if (strcmp(name, "percent-pos") == 0) m_state.WritePlayback([&](auto& m) { m.timing.percent_pos = value; });
    else if (strcmp(name, "playback-time") == 0) m_state.WritePlayback([&](auto& m) { m.timing.playbackTime = value; });
    else if (strcmp(name, "speed") == 0) m_state.WritePlayback([&](auto& m) { m.config.speed = value; });
    else if (strcmp(name, "time-remaining") == 0) m_state.WritePlayback([&](auto& m) { m.timing.time_remaining = value; });
    else if (strcmp(name, "video-aspect-override") == 0) m_state.WriteVideo([&](auto& m) { m.dimensions.aspect = value; });
    else if (strcmp(name, "estimated-vf-fps") == 0) m_state.WriteVideo([&](auto& m) { m.stats.estimated_vf_fps_mpv = value; });
    else if (strcmp(name, "audio-delay") == 0) m_state.WriteAudio([&](auto& m) { m.codec.audio_delay = value; });
    else if (strcmp(name, "sub-scale") == 0) m_state.WriteSubtitle([&](auto& m) { m.sub_scale = value; });
    else if (strcmp(name, "sub-delay") == 0) m_state.WriteSubtitle([&](auto& m) { m.sub_Delay = value; });
    else if (strcmp(name, "demuxer-cache-duration") == 0) m_state.WriteNetwork([&](auto& m) { m.demuxer_cache_duration = value; });
    else if (strcmp(name, "demuxer-cache-time") == 0) m_state.WriteNetwork([&](auto& m) { m.demuxer_cache_time = value; });
    else if (strcmp(name, "demuxer-bitrate") == 0) m_state.WriteNetwork([&](auto& m) { m.demuxer_bitrate = value; });
    else if (strcmp(name, "audio-buffer") == 0) m_state.WriteNetwork([&](auto& m) { m.audio_buffer = value; });
    return;
    // Cập nhật biến global cũ để tương thích ngược
    if      (strcmp(name, "duration") == 0)                 {g_playbackStatus.duration = value;                                       RATE_LIMITED_COUT(duration, 1000,std::cout << "[DEBUG] [INFO] [Playback] Duration updated: " << g_playbackStatus.duration << " seconds");}
    else if (strcmp(name, "percent-pos") == 0)              {g_playbackStatus.percent_pos = value;}
    else if (strcmp(name, "time-pos") == 0)                 {g_playbackStatus.timePos = value; g_playbackStatus.hasTime = true;       RATE_LIMITED_COUT(time_pos, 1000,std::cout << "[DEBUG] [INFO] [Playback] Time Position updated: " << g_playbackStatus.timePos << " seconds");}
    else if (strcmp(name, "playback-time") == 0)            {g_playbackStatus.playbackTime = value;                                   RATE_LIMITED_COUT(playback_time, 1000,std::cout << "[DEBUG] [INFO] [Playback] Playback Time updated: " << g_playbackStatus.playbackTime << " seconds");}
    else if (strcmp(name, "speed") == 0)                    {g_playbackStatus.speed = value;                                          RATE_LIMITED_COUT(playback_speed, 1,std::cout << "[DEBUG] [INFO] [Playback] Speed updated: " << g_playbackStatus.speed << "x");}
    else if (strcmp(name, "time-remaining") == 0)           {g_playbackStatus.time_remaining = value;                                 RATE_LIMITED_COUT(time_remaining, 1,std::cout << "[DEBUG] [INFO] [Playback] Time Remaining updated: " << g_playbackStatus.time_remaining << "");}
    else if (strcmp(name, "sub-scale") == 0)                {g_playbackStatus.g_subinfo.sub_scale = value;}
    else if (strcmp(name, "sub-delay") == 0)                {g_playbackStatus.g_subinfo.sub_Delay = value;                            RATE_LIMITED_COUT(sub_delay, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Delay updated: " << g_playbackStatus.g_subinfo.sub_Delay << " seconds");}
    else if (strcmp(name, "audio-delay") == 0)              {g_videoInfo.audio_delay = value;                                         RATE_LIMITED_COUT(audio_delay, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Delay updated: " << g_videoInfo.audio_delay << " s");}
    else if (strcmp(name, "video-aspect-override") == 0)    {g_videoInfo.aspect = value;                                              RATE_LIMITED_COUT(video_aspect_override, 1,std::cout << "[DEBUG] [INFO] [Video] Video Aspect Ratio updated: " << g_videoInfo.aspect << "");}
    else if (strcmp(name, "estimated-vf-fps") == 0)         {g_videoInfo.estimated_vf_fps_mpv = value;                                RATE_LIMITED_COUT(estimated_vf_fps, 1,std::cout << "[DEBUG] [INFO] [Video] Estimated VF FPS updated: " << g_videoInfo.estimated_vf_fps_mpv << "");}
    else if (strcmp(name, "audio-buffer") == 0)             {g_playbackStatus.audio_buffer = value;                                   RATE_LIMITED_COUT(audio_buffer, 1000,std::cout << "[DEBUG] [INFO] [Network] Audio Buffer updated: " << g_playbackStatus.audio_buffer << " seconds");}
    else if (strcmp(name, "demuxer-cache-duration") == 0)   {g_playbackStatus.demuxer_cache_duration = value;                         RATE_LIMITED_COUT(demuxer_cache_duration, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Cache Duration updated: " << g_playbackStatus.demuxer_cache_duration << " seconds");}
    else if (strcmp(name, "demuxer-cache-time") == 0)       {g_playbackStatus.demuxer_cache_time = value;                             RATE_LIMITED_COUT(demuxer_cache_time, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Cache Time updated: " << g_playbackStatus.demuxer_cache_time << " seconds");}
    else if (strcmp(name, "demuxer-bitrate") == 0)          {g_playbackStatus.demuxer_bitrate = value;                                RATE_LIMITED_COUT(demuxer_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Bitrate updated: " << g_playbackStatus.demuxer_bitrate << " kbps");}
}

void MPVObserver::HandleNodeProperty(const char* name, const mpv_node* node) {
    if (strcmp(name, "track-list") == 0)                      UpdateTrackList(node);
    else if (strcmp(name, "metadata") == 0)                   UpdateMetadata(node);
    else if (strcmp(name, "playlist") == 0)                   UpdatePlaylist(node);
    else if (strcmp(name, "chapter-list") == 0)               UpdateChapterList(node);
    else if (strcmp(name, "video-params") == 0)               UpdateVideoParams(node);
    else if (strcmp(name, "video-out-params") == 0);
    else if (strcmp(name, "edition-list") == 0);
    else if (strcmp(name, "audio-params") == 0)               UpdateAudioParams(node);
    else if (strcmp(name, "audio-out-params") == 0) ;
    else if (strcmp(name, "sub-streams") == 0) {
        if (node && node->format == MPV_FORMAT_NODE_ARRAY) {
            for (int i = 0; i < node->u.list->num; i++) {
                const mpv_node* track = &node->u.list->values[i];
                if (track->format != MPV_FORMAT_NODE_MAP) continue;
                const mpv_node* codec = mpv_node_dict_find_local(track, "codec");
                if (codec && codec->format == MPV_FORMAT_STRING) {
                    m_state.WriteSubtitle([&](auto& m) { m.sub_codec = codec->u.string; });
                    g_playbackStatus.g_subinfo.sub_codec = codec->u.string;
                }
            }
        }
    }
    else if (strcmp(name, "audio-device-list") == 0)          UpdateAudioDeviceList(node);
    else if (strcmp(name, "af-metadata/ebur_measurer") == 0)  UpdateLoudnessMetadata(node);
}
