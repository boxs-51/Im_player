
#include "globals.h"
#include "utils.h"
#include "json.hpp"
#include "windows/windows_borderless.h"
#include "thread.h"
#include "mpv_basic_formats.h"

#include <filesystem>
#include <vector>
#include <commdlg.h>  
#include <string>
#include <array>
#include <stdexcept>
#include <exception>
#include <mutex>
#include <set>
#include <unordered_set>
#include <unordered_map>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
using json = nlohmann::json;

MPVPlaybackStatus g_playbackStatus;
WindowContext ctx;
VideoInfo g_videoInfo;
BorderlessWindowState BW;
WindowLayout Windowlayout;

static bool isLoadingMedia = false;
static std::unordered_map<int, int> g_retryCount; // key = g_PlayingIndex, value = số lần retry
static constexpr int MAX_RETRY = 2;               // số lần thử tối đa
static std::mutex g_retry_mutex;
static std::mutex g_chapters_mutex;
static std::mutex g_audio_mutex;
static std::mutex g_tracks_mutex;
static std::mutex g_metadata_mutex;

static std::atomic<int> g_current_chapter_idx{-1};
static std::unordered_set<std::string> g_loadedScriptsSet;

std::vector<std::string> g_loadedScripts;

void UpdateGlobalWindowLayout(SDL_Window* sdlWindow, const BorderlessWindowState& state)
{
    if (!sdlWindow) {Windowlayout.titleBar = {0,0,0,0};Windowlayout.videoArea = {0,0,0,0};return;}

    SDL_GetWindowSize(sdlWindow, &WinW, &WinH);
    SDL_GetWindowPosition(sdlWindow, &WinX, &WinY);
    // Fullscreen: video chiếm toàn bộ, title bar ẩn
    if (state.isFullscreen_video ) {Windowlayout.titleBar = {0,0,0,0};Windowlayout.videoArea = {0,0,WinW,WinH};
    } else {
        // Windowed: title bar trên, video dưới
        Windowlayout.titleBar = {WinX , WinY,WinW,(int)state.titleHeight};
        Windowlayout.videoArea = {WinX, WinY + (int)state.titleHeight,WinW,WinH - (int)state.titleHeight};
    }

}


void HandleMpvError(mpv_handle* mpv, int err , const char* msgText)
{
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

            const char* cmd[] = { "set", "ytdl-format", "bestvideo+bestaudio/best", nullptr };
            mpv_command(mpv, cmd);
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
                    std::thread([mpv, idx]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        if (!mpv) return;
                        std::string idx_str = std::to_string(idx);
                        const char* args[] = { "playlist-play-index", idx_str.c_str(), nullptr };
                        mpv_command(mpv, args);
                    }).detach();
                } else {
                    RATE_LIMITED_COUT(mpv_generic_error_max_retries, 1,std::cout << "[WARNING] [MPV] Max retries reached for index " << idx << ", removing from playlist.");
                    std::string idx_str = std::to_string(idx);
                    const char* args[] = { "playlist-remove", idx_str.c_str(), nullptr };
                    mpv_command(mpv, args);
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
                mpv_command(mpv, args);
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

// ----------------------------------------------------
// Tự động observe tất cả property dạng node
// ----------------------------------------------------
static void InitMPVObservers_AllNode(mpv_handle* mpv) {
    // MPV_FORMAT_NODE: sẽ nhận toàn bộ giá trị có thể
    mpv_observe_property(mpv, 0, "/*", MPV_FORMAT_NODE);
}
void PrintMPVNode(const mpv_node* node, int indent = 0) {
    //
    //mpv_node result;
    //if (mpv_get_property(mpv, "config", MPV_FORMAT_NODE, &result) >= 0) {
    //    std::cout << "[DEBUG] Dump config:\n";
    //    PrintMPVNode(&result);
    //    std::cout << std::endl;
    //    mpv_free_node_contents(&result);
    //}
    //
    if (!node) return;

    std::string pad(indent, ' ');

    switch (node->format) {
    case MPV_FORMAT_STRING:
        std::cout << pad << "\"" << node->u.string << "\"";
        break;
    case MPV_FORMAT_INT64:
        std::cout << pad << node->u.int64;
        break;
    case MPV_FORMAT_DOUBLE:
        std::cout << pad << node->u.double_;
        break;
    case MPV_FORMAT_FLAG:
        std::cout << pad << (node->u.flag ? "true" : "false");
        break;
    case MPV_FORMAT_NONE:
        std::cout << pad << "null";
        break;

    case MPV_FORMAT_NODE_ARRAY: {
        std::cout << pad << "[\n";
        for (int i = 0; i < node->u.list->num; ++i) {
            PrintMPVNode(&node->u.list->values[i], indent + 2);
            if (i < node->u.list->num - 1) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << pad << "]";
        break;
    }

    case MPV_FORMAT_NODE_MAP: {
        std::cout << pad << "{\n";
        for (int i = 0; i < node->u.list->num; ++i) {
            std::cout << std::string(indent + 2, ' ')
                      << "\"" << node->u.list->keys[i] << "\": ";
            PrintMPVNode(&node->u.list->values[i], indent + 2);
            if (i < node->u.list->num - 1) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << pad << "}";
        break;
    }

    default:
        std::cout << pad << "<unknown format " << node->format << ">";
        break;
    }
}

// ----------------------------------------------------
// Xử lý MPV_EVENT_PROPERTY_CHANGE
// ----------------------------------------------------
static void HandlePropertyChange(mpv_event* event) {
    if (!event || event->event_id != MPV_EVENT_PROPERTY_CHANGE) return;
    mpv_event_property* prop = (mpv_event_property*)event->data;
    if (!prop || !prop->name) return;

    std::cout << "{\n";
    std::cout << "  \"property\": \"" << prop->name << "\",\n";
    std::cout << "  \"value\": ";

    if (prop->format == MPV_FORMAT_NODE && prop->data) {
        PrintMPVNode((mpv_node*)prop->data, 2);
    } else if (prop->format == MPV_FORMAT_STRING) {
        std::cout << "\"" << (char*)prop->data << "\"";
    } else if (prop->format == MPV_FORMAT_INT64) {
        std::cout << *(int64_t*)prop->data;
    } else if (prop->format == MPV_FORMAT_DOUBLE) {
        std::cout << *(double*)prop->data;
    } else if (prop->format == MPV_FORMAT_FLAG) {
        std::cout << (*(bool*)prop->data ? "true" : "false");
    } else {
        std::cout << "null";
    }

    std::cout << "\n}\n";
}
void SetMPVOptions(mpv_handle* mpv, const std::unordered_map<std::string, std::string>& options, bool isProperty = false) {
    for (const auto& [key, value] : options) {
        if (isProperty)
            mpv_set_property_string(mpv, key.c_str(), value.c_str());
        else
            mpv_set_option_string(mpv, key.c_str(), value.c_str());
    }
}
void ApplyStaticMPVConfig(mpv_handle* mpv) {
    if (!mpv) return;

    SetMPVOptions(mpv, {
        {"log-level", "v"},
        {"input-media-keys", "yes"},
        {"idle", "yes"},
        {"keep-open", "yes"},
        {"stop-screensaver", "yes"},
        {"input-default-bindings", "yes"},
        {"input-vo-keyboard", "no"},
        {"osc", "yes"},
        {"vo", "libmpv"},
        {"hwdec", "auto-safe"},
        {"video-rotate", "no"},
        {"audio-buffer", "50"},
        {"video-sync", "display-resample"},
        {"framedrop", "vo"}
    });

    mpv_set_property_string(mpv, "hr-seek-framedrop", "yes");
}
void ApplyDynamicMPVConfig(mpv_handle* mpv, const std::string& videoType) {
    if (!mpv) return;

    static const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dynamicConfigs = {
        {"livestream", {
            {"cache", "yes"},
            {"cache-pause", "no"},
            {"cache-secs", "10"},
            {"demuxer-max-bytes", "10M"},
            {"demuxer-max-back-bytes", "1M"},
            {"vd-lavc-skipframe", "nonkey"},
            {"hr-seek", "no"},
            {"vd-lavc-fast", "yes"}
        }},
        {"file", {
            {"cache", "auto"},
            {"cache-pause", "yes"},
            {"cache-secs", "100"},
            {"demuxer-max-bytes", "50M"},
            {"demuxer-max-back-bytes", "10M"},
            {"vd-lavc-skipframe", "nonref"},
            {"hr-seek", "keyframes"}
        }},
        {"video", {
            {"cache", "yes"},
            {"cache-pause", "yes"},
            {"cache-secs", "60"},
            {"demuxer-max-bytes", "20M"},
            {"demuxer-max-back-bytes", "5M"},
            {"vd-lavc-skipframe", "default"},
            {"hr-seek", "yes"}
        }}
    };

    const auto it = dynamicConfigs.find(videoType);
    const auto& config = (it != dynamicConfigs.end()) ? it->second : std::unordered_map<std::string, std::string>{
        {"cache", "auto"},
        {"cache-pause", "yes"},
        {"cache-secs", "10"},
        {"hr-seek", "yes"},
        {"vd-lavc-skipframe", "default"}
    };

    SetMPVOptions(mpv, config, true);
}
void InitPlaybackStatus(mpv_handle* mpv) {
    InitMPVObservers(mpv);ProcessMPVEvents(mpv);
}
static void ObserveProps(mpv_handle* mpv, const std::vector<std::pair<const char*, mpv_format>>& props, const char* groupName) {
    for (const auto& [name, fmt] : props) {
        int ret = mpv_observe_property(mpv, 0, name, fmt);
        if (ret < 0) {
            RATE_LIMITED_COUT(observeprops, 1,std::cerr << "[DEBUG] [WARNING] [MPV] Failed to observe " << name << " (" << groupName << "): " << mpv_error_string(ret) <<"\n");
        } else {
            RATE_LIMITED_COUT(observeprops, 1,std::cout << "[DEBUG] [INFO] [MPV] Observing " << name << " (" << groupName << ")" << "\n");
        }
    }
}

// ===================================================
// 🕹️ Playback observers
// ===================================================
static void InitMPVObservers_Playback(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"duration", MPV_FORMAT_DOUBLE},
        {"pause", MPV_FORMAT_FLAG},
        {"playback-time", MPV_FORMAT_DOUBLE},
        {"time-pos", MPV_FORMAT_DOUBLE},
        {"percent-pos", MPV_FORMAT_DOUBLE},
        {"eof-reached", MPV_FORMAT_FLAG},
        {"core-idle", MPV_FORMAT_FLAG},
        {"speed", MPV_FORMAT_DOUBLE},
        {"seekable", MPV_FORMAT_FLAG},
        {"start", MPV_FORMAT_DOUBLE},
        {"end", MPV_FORMAT_DOUBLE},
        {"loop", MPV_FORMAT_STRING},
        {"idle-active", MPV_FORMAT_FLAG},
        {"seeking", MPV_FORMAT_FLAG},
        {"time-remaining", MPV_FORMAT_DOUBLE}
    }, "Playback");
}

// ===================================================
// 🎧 Audio observers
// ===================================================
static void InitMPVObservers_Audio(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"mute", MPV_FORMAT_FLAG},
        {"volume", MPV_FORMAT_INT64},
        {"audio-params", MPV_FORMAT_NODE},
        {"audio-device", MPV_FORMAT_STRING},
        {"audio-device-list", MPV_FORMAT_NODE},
        {"audio-channels", MPV_FORMAT_INT64},
        {"audio-bitrate", MPV_FORMAT_INT64},
        {"audio-samplerate", MPV_FORMAT_INT64},
        {"audio-codec", MPV_FORMAT_STRING},
        {"audio-buffer", MPV_FORMAT_DOUBLE},
        {"audio-client-name", MPV_FORMAT_STRING},
        {"audio-delay", MPV_FORMAT_DOUBLE},
        {"ao", MPV_FORMAT_STRING},
        {"af", MPV_FORMAT_STRING}
    }, "Audio");
}

// ===================================================
// 🎞️ Video observers
// ===================================================
static void InitMPVObservers_Video(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"width", MPV_FORMAT_INT64},
        {"height", MPV_FORMAT_INT64},
        {"display-fps", MPV_FORMAT_DOUBLE},
        {"estimated-vf-fps", MPV_FORMAT_DOUBLE},
        {"video-bitrate", MPV_FORMAT_INT64},
        {"video-params", MPV_FORMAT_NODE},
        {"video-aspect-override", MPV_FORMAT_DOUBLE},
        {"hwdec-current", MPV_FORMAT_STRING},
        {"hwdec-active", MPV_FORMAT_FLAG},
        {"hwdec", MPV_FORMAT_STRING},
        {"video-codec", MPV_FORMAT_STRING},
        {"video-format", MPV_FORMAT_STRING},
        {"video-rotate", MPV_FORMAT_INT64},
        {"video-out-params", MPV_FORMAT_NODE},
        {"vo", MPV_FORMAT_STRING},
        {"vf", MPV_FORMAT_STRING},
        {"osd-width", MPV_FORMAT_INT64},
        {"osd-height", MPV_FORMAT_INT64}
    }, "Video");
}

// ===================================================
// 💬 Subtitle observers
// ===================================================
static void InitMPVObservers_Subtitle(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"sub-delay", MPV_FORMAT_DOUBLE},
        {"sub-visibility", MPV_FORMAT_FLAG},
        {"sub-codec", MPV_FORMAT_STRING},
        {"sub-text", MPV_FORMAT_STRING},
        {"sid", MPV_FORMAT_INT64},
        {"secondary-sid", MPV_FORMAT_INT64},
        {"sub-scale", MPV_FORMAT_DOUBLE},
        {"sub-ass-override", MPV_FORMAT_STRING},
        {"sub-streams", MPV_FORMAT_NODE}
    }, "Subtitles");
}

// ===================================================
// 🧾 Metadata / Container / Playlist observers
// ===================================================
static void InitMPVObservers_Metadata(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"filename", MPV_FORMAT_STRING},
        {"stream-open-filename", MPV_FORMAT_STRING},
        {"file-format", MPV_FORMAT_STRING},
        {"metadata", MPV_FORMAT_NODE},
        {"track-list", MPV_FORMAT_NODE},
        {"chapter-list", MPV_FORMAT_NODE},
        {"chapter", MPV_FORMAT_INT64},
        {"chapter-count", MPV_FORMAT_INT64},
        {"chapter-metadata", MPV_FORMAT_NODE},
        {"playlist", MPV_FORMAT_NODE},
        {"playlist-pos", MPV_FORMAT_INT64},
        {"playlist-pos-1", MPV_FORMAT_INT64},
        {"playlist-count", MPV_FORMAT_INT64},
        {"media-title", MPV_FORMAT_STRING},
        {"title", MPV_FORMAT_STRING},
        {"working-directory", MPV_FORMAT_STRING},
        {"edition-list", MPV_FORMAT_NODE}
    }, "Metadata");
}

// ===================================================
// 🌐 Cache / Network observers
// ===================================================
static void InitMPVObservers_Cache(mpv_handle* mpv) {
    ObserveProps(mpv, {
        {"demuxer-cache-duration", MPV_FORMAT_DOUBLE},
        {"demuxer-cache-time", MPV_FORMAT_DOUBLE},
        {"demuxer-bitrate", MPV_FORMAT_DOUBLE},
        {"demuxer-via-network", MPV_FORMAT_FLAG},
        {"cache-buffering-state", MPV_FORMAT_INT64},
        {"cache", MPV_FORMAT_DOUBLE},
        {"stream-path", MPV_FORMAT_STRING},
        {"stream-pos", MPV_FORMAT_INT64},
        {"network-time", MPV_FORMAT_DOUBLE}
    }, "Cache");
}

// ===================================================
// 🚀 Entry point
// ===================================================
void InitMPVObservers(mpv_handle* mpv) {
    RATE_LIMITED_COUT(initmpvobservers_start, 1,std::cout << "=================== [MPV] Initializing observers ===================");

    InitMPVObservers_Playback(mpv);
    InitMPVObservers_Audio(mpv);
    InitMPVObservers_Video(mpv);
    InitMPVObservers_Subtitle(mpv);
    InitMPVObservers_Metadata(mpv);
    InitMPVObservers_Cache(mpv);

    InitMPVObservers_AllNode(mpv);

    RATE_LIMITED_COUT(initmpvobservers_complete, 1,std::cout << "=================== [MPV] Observer registration complete ===================");
}

const mpv_node* mpv_node_dict_find(const mpv_node *node, const char *key) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP)
        return nullptr;

    const mpv_node_list *list = node->u.list;
    for (int i = 0; i < list->num; i++) {
        if (strcmp(list->keys[i], key) == 0) {
            return &list->values[i];
        }
    }
    return nullptr;
}
static void UpdateVideoParams(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP)return;
    const mpv_node* n = nullptr;
    if ((n = mpv_node_dict_find(node, "pixelformat")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vpixfmt = n->u.string;            RATE_LIMITED_COUT(video_pixfmt, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Pixel Format: " << g_videoInfo.g_videoparams.vpixfmt);
    if ((n = mpv_node_dict_find(node, "primaries")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vprimaries = n->u.string;         RATE_LIMITED_COUT(video_primaries, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Primaries: " << g_videoInfo.g_videoparams.vprimaries);
    if ((n = mpv_node_dict_find(node, "gamma")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vgamma = n->u.string;             RATE_LIMITED_COUT(video_gamma, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Gamma: " << g_videoInfo.g_videoparams.vgamma);
    if ((n = mpv_node_dict_find(node, "colormatrix")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolormatrix = n->u.string;       RATE_LIMITED_COUT(video_colormatrix, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Matrix: " << g_videoInfo.g_videoparams.vcolormatrix);
    if ((n = mpv_node_dict_find(node, "colorlevels")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolorlevels = n->u.string;       RATE_LIMITED_COUT(video_colorlevels, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Color Levels: " << g_videoInfo.g_videoparams.vcolorlevels);
    if ((n = mpv_node_dict_find(node, "stereo-in")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vstereo_in = n->u.string;         RATE_LIMITED_COUT(video_stereo_in, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Stereo In: " << g_videoInfo.g_videoparams.vstereo_in);
    if ((n = mpv_node_dict_find(node, "chroma-location")) && n->format == MPV_FORMAT_STRING)g_videoInfo.g_videoparams.vchroma_location = n->u.string;   RATE_LIMITED_COUT(video_chroma_location, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Chroma Location: " << g_videoInfo.g_videoparams.vchroma_location);
    if ((n = mpv_node_dict_find(node, "w")) && n->format == MPV_FORMAT_INT64)               g_videoInfo.g_videoparams.vwidth = (int)n->u.int64;         RATE_LIMITED_COUT(video_w, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Width: " << g_videoInfo.g_videoparams.vwidth);
    if ((n = mpv_node_dict_find(node, "h")) && n->format == MPV_FORMAT_INT64)               g_videoInfo.g_videoparams.vheight = (int)n->u.int64;        RATE_LIMITED_COUT(video_h, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Height: " << g_videoInfo.g_videoparams.vheight);
    if ((n = mpv_node_dict_find(node, "dw")) && n->format == MPV_FORMAT_INT64)              g_videoInfo.g_videoparams.vdisp_w = (int)n->u.int64;        RATE_LIMITED_COUT(video_dw, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Display Width: " << g_videoInfo.g_videoparams.vdisp_w);
    if ((n = mpv_node_dict_find(node, "dh")) && n->format == MPV_FORMAT_INT64)              g_videoInfo.g_videoparams.vdisp_h = (int)n->u.int64;        RATE_LIMITED_COUT(video_dh, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Display Height: " << g_videoInfo.g_videoparams.vdisp_h);
    if ((n = mpv_node_dict_find(node, "crop-x")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_x = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_x, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop X: " << g_videoInfo.g_videoparams.vcrop_x);
    if ((n = mpv_node_dict_find(node, "crop-y")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_y = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_y, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Y: " << g_videoInfo.g_videoparams.vcrop_y);
    if ((n = mpv_node_dict_find(node, "crop-w")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_w = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_w, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Width: " << g_videoInfo.g_videoparams.vcrop_w);
    if ((n = mpv_node_dict_find(node, "crop-h")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_h = (int)n->u.int64;        RATE_LIMITED_COUT(video_vcrop_h, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Crop Height: " << g_videoInfo.g_videoparams.vcrop_h);
    if ((n = mpv_node_dict_find(node, "aspect")) && n->format == MPV_FORMAT_DOUBLE)         g_videoInfo.g_videoparams.vaspect = n->u.double_;           RATE_LIMITED_COUT(video_aspect, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Aspect: " << g_videoInfo.g_videoparams.vaspect);
    if ((n = mpv_node_dict_find(node, "aspect-name")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vaspect_name = n->u.string;       RATE_LIMITED_COUT(video_aspect_name, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Aspect Name: " << g_videoInfo.g_videoparams.vaspect_name);
    if ((n = mpv_node_dict_find(node, "par")) && n->format == MPV_FORMAT_DOUBLE)            g_videoInfo.g_videoparams.vpar = n->u.double_;              RATE_LIMITED_COUT(video_par, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Pixel Aspect Ratio: " << g_videoInfo.g_videoparams.vpar);
    if ((n = mpv_node_dict_find(node, "sar")) && n->format == MPV_FORMAT_DOUBLE)            g_videoInfo.g_videoparams.vsar = n->u.double_;              RATE_LIMITED_COUT(video_sar, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Storage Aspect Ratio: " << g_videoInfo.g_videoparams.vsar);
    if ((n = mpv_node_dict_find(node, "sar-name")) && n->format == MPV_FORMAT_STRING)       g_videoInfo.g_videoparams.vsar_name = n->u.string;          RATE_LIMITED_COUT(video_vsar_name, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Storage Aspect Ratio Name " << g_videoInfo.g_videoparams.vsar_name);
    if ((n = mpv_node_dict_find(node, "sig-peak")) && n->format == MPV_FORMAT_DOUBLE)       g_videoInfo.g_videoparams.vsig_peak = n->u.double_;         RATE_LIMITED_COUT(video_sig_peak, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Sig Peak: " << g_videoInfo.g_videoparams.vsig_peak);
    if ((n = mpv_node_dict_find(node, "light")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vlight = n->u.string;             RATE_LIMITED_COUT(video_light, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Light: " << g_videoInfo.g_videoparams.vlight);
    if ((n = mpv_node_dict_find(node, "rotate")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vrotate = (int)n->u.int64;        RATE_LIMITED_COUT(video_rotate, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Rotate: " << g_videoInfo.g_videoparams.vrotate);
    if ((n = mpv_node_dict_find(node, "average-bpp")) && n->format == MPV_FORMAT_INT64)     g_videoInfo.g_videoparams.average_bpp = (int)n->u.int64;    RATE_LIMITED_COUT(video_average_bpp, 1, std::cout << "[DEBUG] [INFO] [VideoParams] Average bpp: " << g_videoInfo.g_videoparams.average_bpp);
}

static void UpdateAudioParams(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP) return;
    const mpv_node* n = nullptr;
    if ((n = mpv_node_dict_find(node, "format")) && n->format == MPV_FORMAT_STRING)         g_videoInfo.g_audioarams.aformat = n->u.string;              RATE_LIMITED_COUT(audio_format, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Format: " << g_videoInfo.g_audioarams.aformat << "");
    if ((n = mpv_node_dict_find(node, "samplerate")) && n->format == MPV_FORMAT_INT64)      g_videoInfo.g_audioarams.asamplerate = (int)n->u.int64;      RATE_LIMITED_COUT(audio_samplerate, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Sample Rate: " << g_videoInfo.g_audioarams.asamplerate << " Hz");
    if ((n = mpv_node_dict_find(node, "channel-count")) && n->format == MPV_FORMAT_INT64)   g_videoInfo.g_audioarams.channel_count = (int)n->u.int64;    RATE_LIMITED_COUT(audio_channel_count, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Channel Count: " << g_videoInfo.g_audioarams.channel_count << "");
    if ((n = mpv_node_dict_find(node, "channels")) && n->format == MPV_FORMAT_STRING)       g_videoInfo.g_audioarams.achannels_str = n->u.string;        RATE_LIMITED_COUT(audio_channels_str, 1,std::cout << "[DEBUG] [INFO] [AudioParams] Channels: " << g_videoInfo.g_audioarams.achannels_str << "");
    if ((n = mpv_node_dict_find(node, "hr-channels")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_audioarams.ahr_channels = n->u.string;         RATE_LIMITED_COUT(audio_hr_channels, 1,std::cout << "[DEBUG] [INFO] [AudioParams] HR Channels: " << g_videoInfo.g_audioarams.ahr_channels << "");
}
static void UpdateTrackList(const mpv_node* node) {
    // Nếu node không phải array, xóa danh sách tracks hiện có
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        g_videoInfo.g_tracks.clear();
        return;
    }

    std::vector<TrackInfo> tmp;
    tmp.reserve(node->u.list->num);

    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        const mpv_node* n = nullptr;
        TrackInfo track;

        // Helper: read string
        auto read_string = [&](const char* key, std::string& out) {
            if ((n = mpv_node_dict_find(&entry, key)) && n->format == MPV_FORMAT_STRING) {
                out = n->u.string ? n->u.string : "";
                return true;
            }
            return false;
        };
        // Helper: read int
        auto read_int = [&](const char* key, int& out) {
            if ((n = mpv_node_dict_find(&entry, key))) {
                if (n->format == MPV_FORMAT_INT64) { out = (int)n->u.int64; return true; }
                if (n->format == MPV_FORMAT_DOUBLE) { out = (int)n->u.double_; return true; }
            }
            return false;
        };
        // Helper: read double
        auto read_double = [&](const char* key, double& out) {
            if ((n = mpv_node_dict_find(&entry, key))) {
                if (n->format == MPV_FORMAT_DOUBLE) { out = n->u.double_; return true; }
                if (n->format == MPV_FORMAT_INT64) { out = (double)n->u.int64; return true; }
            }
            return false;
        };
        // Helper: read flag (boolean)
        auto read_flag = [&](const char* key, bool& out) {
            if ((n = mpv_node_dict_find(&entry, key)) && n->format == MPV_FORMAT_FLAG) {
                out = n->u.flag != 0;
                return true;
            }
            return false;
        };

        // ---------------------------
        // Các trường chung / thường
        // ---------------------------
        read_int("id", track.id);
        read_string("type", track.type);
        read_string("codec", track.codec);
        read_string("codec-desc", track.codec_desc);
        read_string("codec-profile", track.codec_profile);
        read_string("decoder", track.decoder);
        read_string("decoder-desc", track.decoder_desc);
        read_string("format-name", track.format_name);
        read_int("ff-index", track.ff_index);
        read_int("main-selection", track.main_selection);

        // metadata
        read_string("lang", track.language);
        read_string("title", track.title);

        // flags
        read_flag("image", track.image);
        read_flag("albumart", track.albumart);
        read_flag("default", track.is_default);
        read_flag("forced", track.forced);
        read_flag("dependent", track.dependent);
        read_flag("visual-impaired", track.visual_impaired);
        read_flag("hearing-impaired", track.hearing_impaired);
        read_flag("external", track.external);
        read_flag("selected", track.selected);

        // ---------------------------
        // Video-specific demux fields
        // ---------------------------
        read_int("demux-w", track.demux_w);
        read_int("demux-h", track.demux_h);
        // demux-fps có thể là INT hoặc DOUBLE -> dùng read_double
        read_double("demux-fps", track.demux_fps);

        // ---------------------------
        // Audio-specific demux fields
        // ---------------------------
        read_int("demux-samplerate", track.demux_samplerate);
        read_int("demux-channel-count", track.demux_channel_count);
        read_string("demux-channels", track.demux_channels);

        // Một số file dùng "audio-channels" thay cho demux-channel-count
        read_int("audio-channels", track.demux_channel_count);

        // ---------------------------
        // Debug output (rate-limited macro bạn đang dùng)
        // ---------------------------
        RATE_LIMITED_COUT(track_list, 1,
            std::cout << "[DEBUG] [INFO] [TRACK] id=" << track.id
                      << ", type=" << track.type
                      << ", codec=" << track.codec
                      << (track.codec_desc.empty() ? "" : (" (" + track.codec_desc + ")"))
                      << (track.codec_profile.empty() ? "" : (" profile=" + track.codec_profile))
                      << ", decoder=" << track.decoder
                      << ", ff-index=" << track.ff_index
                      << ", selected=" << (track.selected ? "yes" : "no")
                      << ", lang=" << track.language
                      << ", title=" << track.title
                      << ", demux_w=" << track.demux_w
                      << ", demux_h=" << track.demux_h
                      << ", demux_fps=" << track.demux_fps
                      << ", samplerate=" << track.demux_samplerate
                      << ", channels=" << track.demux_channel_count
                      << (track.demux_channels.empty() ? "" : (" (" + track.demux_channels + ")"))
                      << "\n";);

        tmp.push_back(std::move(track));
    }

    // Swap vào dữ liệu chính
    g_videoInfo.g_tracks.swap(tmp);
}

static void UpdateAudioDeviceList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) return;

    std::vector<AudioDeviceInfo> tmp;

    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        const mpv_node* n = nullptr;
        AudioDeviceInfo dev;

        if ((n = mpv_node_dict_find(&entry, "name")) && n->format == MPV_FORMAT_STRING)dev.name = n->u.string;
        if ((n = mpv_node_dict_find(&entry, "description")) && n->format == MPV_FORMAT_STRING)dev.description = n->u.string;
        RATE_LIMITED_COUT(audio_device, 1,
            std::cout << "[DEBUG] [INFO] [Audio Device] " 
                      << dev.name << " (" << dev.description << ")"
                      << "\n";);

        tmp.push_back(dev);
    }

    
    g_playbackStatus.g_audioDevices.swap(tmp);

}
static void UpdateChapterList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list || node->u.list->num == 0) {
        g_videoInfo.g_chapters.clear();
        return;
    }
    std::vector<ChapterInfo> tmp;
    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;

        const mpv_node* n = nullptr;
        ChapterInfo dev;

        if ((n = mpv_node_dict_find(&entry, "time"))) {
            if (n->format == MPV_FORMAT_INT64)       dev.time = static_cast<double>(n->u.int64);
            else if (n->format == MPV_FORMAT_DOUBLE) dev.time = n->u.double_;
        }
        if ((n = mpv_node_dict_find(&entry, "title")) && n->format == MPV_FORMAT_STRING) {dev.title = n->u.string;}

        tmp.push_back({dev});
        RATE_LIMITED_COUT(fetch_chapter_list, 1,std::cout << "[DEBUG] [INFO] [Chapters] Chapter found: " << dev.title << " at " << dev.time << " seconds.";);
    }
    g_videoInfo.g_chapters.swap(tmp);
}
static void UpdatePlayList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
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
        if ((n = mpv_node_dict_find(entry, "playing")) && n->format == MPV_FORMAT_FLAG) dev.playing = (n->u.flag != 0);
        if ((n = mpv_node_dict_find(entry, "current")) && n->format == MPV_FORMAT_FLAG) dev.current = (n->u.flag != 0);
        if ((n = mpv_node_dict_find(entry, "id")) && n->format == MPV_FORMAT_INT64) dev.id = (int)n->u.int64;;
        if ((n = mpv_node_dict_find(entry, "filename")) && n->format == MPV_FORMAT_STRING) dev.filename = n->u.string;
        if ((n = mpv_node_dict_find(entry, "title")) && n->format == MPV_FORMAT_STRING) dev.title = n->u.string;

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
    g_playbackStatus.g_playlist.swap(tmp);
}
static void UpdateMetadata(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list) {
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

    g_videoInfo.metadata.swap(tmp);

}
void ProcessMPVEvents(mpv_handle* mpv) {
    while (mpv_event* event = mpv_wait_event(mpv, 0)) {
        if(event->event_id == MPV_EVENT_NONE)  break;
        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* prop = (mpv_event_property*)event->data;
            if (!prop || !prop->data) break;
            const char* name = prop->name;
            // ==== Playback state ==== 
            //HandlePropertyChange(event);
            if      (strcmp(name, "duration") == 0)         {g_playbackStatus.duration = *(double*)prop->data;                                       RATE_LIMITED_COUT(duration, 1000,std::cout << "[DEBUG] [INFO] [Playback] Duration updated: " << g_playbackStatus.duration << " seconds");}
            else if (strcmp(name, "percent-pos") == 0)      {g_playbackStatus.percent_pos = *(double*)prop->data;}
            else if (strcmp(name, "time-pos") == 0)         {g_playbackStatus.timePos = *(double*)prop->data; g_playbackStatus.hasTime = true;       RATE_LIMITED_COUT(time_pos, 1000,std::cout << "[DEBUG] [INFO] [Playback] Time Position updated: " << g_playbackStatus.timePos << " seconds");}
            else if (strcmp(name, "playback-time") == 0)    {g_playbackStatus.playbackTime = *(double*)prop->data;                                   RATE_LIMITED_COUT(playback_time, 1000,std::cout << "[DEBUG] [INFO] [Playback] Playback Time updated: " << g_playbackStatus.playbackTime << " seconds");}
            else if (strcmp(name, "pause") == 0)            {g_playbackStatus.isPaused = (*(int*)prop->data) != 0;                                   RATE_LIMITED_COUT(pause_state, 1,std::cout << "[DEBUG] [INFO] [Playback] Pause state updated: " << (g_playbackStatus.isPaused ? "Paused" : "Playing") << "");}
            else if (strcmp(name, "core-idle") == 0)        {g_playbackStatus.isCoreIdle = (*(int*)prop->data) != 0;                                 RATE_LIMITED_COUT(core_idle, 1,std::cout << "[DEBUG] [INFO] [Playback] Core Idle state updated: " << (g_playbackStatus.isCoreIdle ? "Idle" : "Active") << "");}
            else if (strcmp(name, "idle-active") == 0)      {g_playbackStatus.idle_active = (*(int*)prop->data) != 0;}
            else if (strcmp(name, "eof-reached") == 0)      {g_playbackStatus.eofReached = (*(int*)prop->data) != 0;                                RATE_LIMITED_COUT(eof_reached, 1,std::cout << "[DEBUG] [INFO] [Playback] EOF Reached state updated: " << (g_playbackStatus.eofReached ? "Yes" : "No") << "");}
            else if (strcmp(name, "mute") == 0)             {g_playbackStatus.isMuted = (*(int*)prop->data) != 0;                                   RATE_LIMITED_COUT(mute_state, 1,std::cout << "[DEBUG] [INFO] [Audio] Mute state updated: " << (g_playbackStatus.isMuted ? "Muted" : "Unmuted") << "");}
            else if (strcmp(name, "volume") == 0)           {g_playbackStatus.volume = std::clamp((int)*(int64_t*)prop->data, 0, 100);              RATE_LIMITED_COUT(volume_level, 1,std::cout << "[DEBUG] [INFO] [Audio] Volume updated: " << g_playbackStatus.volume << "");}
            else if (strcmp(name, "speed") == 0)            {g_playbackStatus.speed = *(double*)prop->data;                                         RATE_LIMITED_COUT(playback_speed, 1,std::cout << "[DEBUG] [INFO] [Playback] Speed updated: " << g_playbackStatus.speed << "x");}
            else if (strcmp(name, "seekable") == 0)         {g_playbackStatus.seekable = (*(int*)prop->data) != 0;                                  RATE_LIMITED_COUT(seekable_state, 1,std::cout << "[DEBUG] [INFO] [Playback] Seekable state updated: " << (g_playbackStatus.seekable ? "Yes" : "No") << "");}
            else if (strcmp(name, "start") == 0)            {g_playbackStatus.startTime = *(double*)prop->data;                                     RATE_LIMITED_COUT(start_time, 1,std::cout << "[DEBUG] [INFO] [Playback] Start Time updated: " << g_playbackStatus.startTime << " seconds");}
            else if (strcmp(name, "end") == 0)              {g_playbackStatus.endTime = *(double*)prop->data;                                       RATE_LIMITED_COUT(end_time, 1,std::cout << "[DEBUG] [INFO] [Playback] End Time updated: " << g_playbackStatus.endTime << " seconds");}
            else if (strcmp(name, "loop") == 0)             {mpv_get_prop(mpv,"loop",g_playbackStatus.loopMode);                                    RATE_LIMITED_COUT(loop_mode, 1,std::cout << "[DEBUG] [INFO] [Playback] Loop Mode updated: " << g_playbackStatus.loopMode << "");}
            else if (strcmp(name, "seeking") == 0)          {g_playbackStatus.seeking = (*(int*)prop->data) != 0;                                   RATE_LIMITED_COUT(seeking, 1,std::cout << "[DEBUG] [INFO] [Playback] Seeking updated: " << g_playbackStatus.seeking << "");}
            else if (strcmp(name, "time-remaining") == 0)   {g_playbackStatus.time_remaining = *(double*)prop->data;                                RATE_LIMITED_COUT(time_remaining, 1,std::cout << "[DEBUG] [INFO] [Playback] Time Remaining updated: " << g_playbackStatus.time_remaining << "");}
            
            // ==== File / Media info ====
            else if (strcmp(name, "working-directory") == 0)    {g_playbackStatus.working_directory  = (const char*)prop->data;}
            else if (strcmp(name, "track-list") == 0)           UpdateTrackList((const mpv_node*)prop->data); 
            else if (strcmp(name, "filename") == 0)             {g_playbackStatus.hasFile = true;  mpv_get_prop(mpv,"filename",g_playbackStatus.filename);   RATE_LIMITED_COUT(filename, 1,std::cout << "[DEBUG] [INFO] [Media] Filename updated: " << g_playbackStatus.filename << "");}
            else if (strcmp(name, "stream-open-filename") == 0) {mpv_get_prop(mpv,"stream-open-filename",g_playbackStatus.streamUrl);                        RATE_LIMITED_COUT(stream_open_filename, 1,std::cout << "[DEBUG] [INFO] [Media] Stream Open Filename updated: " << g_playbackStatus.streamUrl << "");}
            else if (strcmp(name, "media-title") == 0)          {mpv_get_prop(mpv,"media-title",g_playbackStatus.mediaTitle);                                RATE_LIMITED_COUT(media_title, 1,std::cout << "[DEBUG] [INFO] [Media] Media Title updated: " << g_playbackStatus.mediaTitle << "");}
            else if (strcmp(name, "title") == 0)                {mpv_get_prop(mpv,"title",g_playbackStatus.Title);                                           RATE_LIMITED_COUT(title, 1,std::cout << "[DEBUG] [INFO] [Media] Title updated: " << g_playbackStatus.Title << "");}
            else if (strcmp(name, "file-format") == 0)          {mpv_get_prop(mpv,"file-format",g_playbackStatus.fileFormat);                                RATE_LIMITED_COUT(file_format, 1,std::cout << "[DEBUG] [INFO] [Media] File Format updated: " << g_playbackStatus.fileFormat << "");}
            else if (strcmp(name, "metadata") == 0)             UpdateMetadata((const mpv_node*)prop->data);
            else if (strcmp(name, "playlist") == 0 )            UpdatePlayList((const mpv_node*)prop->data);
                
            else if (strcmp(name, "playlist-pos") == 0 )        {g_playbackStatus.g_PlayingIndex = (int)*(int64_t*)prop->data;    RATE_LIMITED_COUT(playlist_pos, 1,std::cout << "[DEBUG] [INFO] [Playlist] Current Playing Index updated: " << g_playbackStatus.g_PlayingIndex << "");}
            else if (strcmp(name, "playlist-pos-1") == 0 )      {g_playbackStatus.g_PlayingIndex_1 = (int)*(int64_t*)prop->data;    RATE_LIMITED_COUT(playlist_pos_1, 1,std::cout << "[DEBUG] [INFO] [Playlist] Current Playing Index -1 updated: " << g_playbackStatus.g_PlayingIndex_1 << "");}
            else if (strcmp(name, "playlist-count") == 0) {
                g_playbackStatus.g_playlist_count = *(int64_t*)prop->data;
                if (g_playbackStatus.g_playlist_count < 2) playImmediately = false;
                if (g_playbackStatus.g_playlist_count >= 2 && playImmediately ) {
                    // Xử lý bình thường nếu append video mới ngoài đổi format
                    playImmediately = false;
                    int newIndex = (int)g_playbackStatus.g_playlist.size() - 1;
                    std::string indexStr = std::to_string(newIndex);
                    const char* cmdPlay[] = { "playlist-play-index", indexStr.c_str(), nullptr };   mpv_command(mpv, cmdPlay);  RATE_LIMITED_COUT(playlist_play_index, 1,std::cout << "[DEBUG] [INFO] [Playlist] Auto-playing newly added item at index: " << newIndex << "");
                }
            }
            else if (strcmp(name, "chapter-list") == 0)     UpdateChapterList((const mpv_node*)prop->data);
            else if (strcmp(name, "chapter") == 0)          {g_videoInfo.g_current_chapter = (int)*(int64_t*)prop->data;                             RATE_LIMITED_COUT(chapter_index, 1,std::cout << "[DEBUG] [INFO] [Chapters] Current Chapter Index updated: " << g_videoInfo.g_current_chapter << "");}    
            // ==== Video / Audio params ====
            else if (strcmp(name, "video-params") == 0)     UpdateVideoParams((const mpv_node*)prop->data);
            else if (strcmp(name, "video-out-params") == 0);
            else if (strcmp(name, "edition-list") == 0);
            else if (strcmp(name, "audio-params") == 0)     UpdateAudioParams((const mpv_node*)prop->data);

            // ==== Subtitles ====
            else if (strcmp(name, "sub-scale") == 0)        {g_playbackStatus.g_subinfo.sub_scale = *(double*)prop->data;}
            else if (strcmp(name, "sub-delay") == 0)        {g_playbackStatus.g_subinfo.sub_Delay = *(double*)prop->data;                         RATE_LIMITED_COUT(sub_delay, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Delay updated: " << g_playbackStatus.g_subinfo.sub_Delay << " seconds");}
            else if (strcmp(name, "sub-visibility") == 0)   {g_playbackStatus.g_subinfo.sub_Visible = (*(int*)prop->data) != 0;                   RATE_LIMITED_COUT(sub_visibility, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Visibility updated: " << (g_playbackStatus.g_subinfo.sub_Visible ? "Visible" : "Hidden") << "");}
            else if (strcmp(name, "sub-codec") == 0)        {g_playbackStatus.g_subinfo.sub_codec  = (const char*)prop->data;                          RATE_LIMITED_COUT(sub_codec, 1,std::cout << "[DEBUG] [INFO] [Subtitles] Subtitle Codec updated: " << g_playbackStatus.g_subinfo.sub_codec << "");}
            else if (strcmp(name, "sub-ass-override") == 0) {g_playbackStatus.g_subinfo.sub_ass_override  = (const char*)prop->data;}
            else if (strcmp(name, "sub-streams") == 0) {
                const mpv_node* node = (const mpv_node*)prop->data;
                if (node && node->format == MPV_FORMAT_NODE_ARRAY) {
                    for (int i = 0; i < node->u.list->num; i++) {
                        const mpv_node* track = &node->u.list->values[i];
                        if (track->format != MPV_FORMAT_NODE_MAP) continue;
                        const mpv_node* codec = mpv_node_dict_find(track, "codec");
                        if (codec && codec->format == MPV_FORMAT_STRING) {
                            g_playbackStatus.g_subinfo.sub_codec = codec->u.string;
                        }
                    }
                }
            }

            // ==== Bitrate / Aspect / HWDEC ====        
            else if (strcmp(name, "audio-device-list") == 0)        UpdateAudioDeviceList((const mpv_node*)prop->data);
            else if (strcmp(name, "audio-client-name") == 0)        {mpv_get_prop(mpv,"audio-client-name",g_playbackStatus.audio_client_name);          RATE_LIMITED_COUT(video_bitrate, 1,std::cout << "[DEBUG] [INFO] [Video] Audio Client Name updated: " << g_playbackStatus.audio_client_name << " ");}
            else if (strcmp(name, "audio-bitrate") == 0)            {g_videoInfo.abitrate = (int)*(int64_t*)prop->data;                                 RATE_LIMITED_COUT(audio_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Audio] Audio Bitrate updated: " << g_videoInfo.abitrate/1000 << " kbps");}
            else if (strcmp(name, "audio-codec") == 0)              {mpv_get_prop(mpv,"audio-codec",g_videoInfo.acodec);                                RATE_LIMITED_COUT(audio_codec, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Codec updated: " << g_videoInfo.acodec << "");}
            else if (strcmp(name, "audio-samplerate") == 0)         {g_videoInfo.asamplerate = (int)*(int64_t*)prop->data;                              RATE_LIMITED_COUT(audio_samplerate, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Sample Rate updated: " << g_videoInfo.asamplerate << " Hz");}
            else if (strcmp(name, "audio-channels") == 0)           {g_videoInfo.achannels = (int)*(int64_t*)prop->data;                                RATE_LIMITED_COUT(audio_channels, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Channels updated: " << g_videoInfo.achannels << "");}
            else if (strcmp(name, "audio-device") == 0 )            {mpv_get_prop(mpv,"audio-device",g_videoInfo.audio_device);                         RATE_LIMITED_COUT(audio_device, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Device updated: " << g_videoInfo.audio_device << "");}                                                                                 
            else if (strcmp(name, "audio-delay") == 0 )             {g_videoInfo.audio_delay = *(double*)prop->data;                                    RATE_LIMITED_COUT(audio_delay, 1,std::cout << "[DEBUG] [INFO] [Audio] Audio Delay updated: " << g_videoInfo.audio_delay << " s");}      
            else if (strcmp(name, "ao") == 0 )                      {mpv_get_prop(mpv,"ao",g_videoInfo.a_out);}
            else if (strcmp(name, "af") == 0 )                      {mpv_get_prop(mpv,"af",g_videoInfo.a_filter);}

            else if (strcmp(name, "vo") == 0 )                      {mpv_get_prop(mpv,"vo",g_videoInfo.v_out);}
            else if (strcmp(name, "video-bitrate") == 0)            {g_videoInfo.vbitrate = (int)*(int64_t*)prop->data;                             RATE_LIMITED_COUT(video_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Video] Video Bitrate updated: " << g_videoInfo.vbitrate/1000 << " kbps");}
            else if (strcmp(name, "video-aspect-override") == 0)    {g_videoInfo.aspect = *(double*)prop->data;                                     RATE_LIMITED_COUT(video_aspect_override, 1,std::cout << "[DEBUG] [INFO] [Video] Video Aspect Ratio updated: " << g_videoInfo.aspect << "");}
            else if (strcmp(name, "hwdec-current") == 0 )           {mpv_get_prop(mpv,"hwdec-current",g_videoInfo.hwdec);                           RATE_LIMITED_COUT(hwdec_current, 1,std::cout << "[DEBUG] [INFO] [Video] HWDEC updated: " << g_videoInfo.hwdec << "");}
            else if (strcmp(name, "video-rotation") == 0)           {g_videoInfo.rotate = (int)*(int64_t*)prop->data;                               RATE_LIMITED_COUT(video_rotation, 1,std::cout << "[DEBUG] [INFO] [Video] Video Rotation updated: " << g_videoInfo.rotate << "");}  
            else if (strcmp(name, "video-codec") == 0)              {mpv_get_prop(mpv,"video-codec",g_videoInfo.vcodec);                            RATE_LIMITED_COUT(video_codec, 1,std::cout << "[DEBUG] [INFO] [Video] Video Codec updated: " << g_videoInfo.vcodec << "");}                
            else if (strcmp(name, "video-format") == 0)             {mpv_get_prop(mpv,"video-format", g_videoInfo.video_format);                    RATE_LIMITED_COUT(video_format, 1,std::cout << "[DEBUG] [INFO] [Video] Video Format updated: " << g_videoInfo.video_format << "");}
            else if (strcmp(name, "width") == 0)                    {g_videoInfo.width = (int)*(int64_t*)prop->data;                                RATE_LIMITED_COUT(width, 1,std::cout << "[DEBUG] [INFO] [Video] Video Width updated: " << g_videoInfo.width << "");}
            else if (strcmp(name, "height") == 0)                   {g_videoInfo.height = (int)*(int64_t*)prop->data;                               RATE_LIMITED_COUT(height, 1,std::cout << "[DEBUG] [INFO] [Video] Video Height updated: " << g_videoInfo.height << "");}
            else if (strcmp(name, "display-fps") == 0)              {g_videoInfo.fps = *(double*)prop->data;                                        RATE_LIMITED_COUT(display_fps, 1,std::cout << "[DEBUG] [INFO] [Video] Display FPS updated: " << g_videoInfo.fps << "");}
            else if (strcmp(name, "estimated-vf-fps") == 0)         {g_videoInfo.estimated_vf_fps_mpv = *(double*)prop->data;                       RATE_LIMITED_COUT(estimated_vf_fps, 1,std::cout << "[DEBUG] [INFO] [Video] Estimated VF FPS updated: " << g_videoInfo.estimated_vf_fps_mpv << "");}

            // ==== Network / Cache ====
            else if (strcmp(name, "stream-pos") == 0 )              {g_playbackStatus.stream_pos = (int)*(int64_t*)prop->data;}
            else if (strcmp(name, "audio-buffer") == 0 )            {g_playbackStatus.audio_buffer = *(double*)prop->data;                          RATE_LIMITED_COUT(audio_buffer, 1000,std::cout << "[DEBUG] [INFO] [Network] Audio Buffer updated: " << g_playbackStatus.audio_buffer << " seconds");}
            else if (strcmp(name, "demuxer-cache-duration") == 0)   {g_playbackStatus.demuxer_cache_duration = *(double*)prop->data;                RATE_LIMITED_COUT(demuxer_cache_duration, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Cache Duration updated: " << g_playbackStatus.demuxer_cache_duration << " seconds");}
            else if (strcmp(name, "demuxer-cache-time") == 0)       {g_playbackStatus.demuxer_cache_time = *(double*)prop->data;                    RATE_LIMITED_COUT(demuxer_cache_time, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Cache Time updated: " << g_playbackStatus.demuxer_cache_time << " seconds");}
            else if (strcmp(name, "demuxer-bitrate") == 0)          {g_playbackStatus.demuxer_bitrate = *(double*)prop->data;                       RATE_LIMITED_COUT(demuxer_bitrate, 1000,std::cout << "[DEBUG] [INFO] [Network] Demuxer Bitrate updated: " << g_playbackStatus.demuxer_bitrate << " kbps");}
            else if (strcmp(name, "demuxer-via-network") == 0)      {g_playbackStatus.demuxer_via_network = (*(int*)prop->data) != 0;}
            else if (strcmp(name, "stream-path") == 0)              {g_playbackStatus.stream_path  = (const char*)prop->data;}
            else if (strcmp(name, "cache-buffering-state") == 0 )   {g_playbackStatus.cache_buffering_state = (int)*(int64_t*)prop->data;}
            break;
        }


        case MPV_EVENT_START_FILE:
            g_playbackStatus.ilde = false;
            isLoadingMedia = true;
            g_playbackStatus = {};
            break;
        case MPV_EVENT_FILE_LOADED:
        {
            int64_t index = -1;
            int64_t index_1 = -1;
            // Lấy trực tiếp giá trị tại thời điểm file đã load xong
            mpv_get_property(mpv, "playlist-pos", MPV_FORMAT_INT64, &index);
            mpv_get_property(mpv, "playlist-pos-1", MPV_FORMAT_INT64, &index_1);
            if (index >= 0) {
                g_playbackStatus.g_PlayingIndex = (int)index;
                // printf("Xác nhận Index thực tế: %d\n", g_playbackStatus.g_PlayingIndex);
            }
            if(index_1 >0){
                g_playbackStatus.g_PlayingIndex_1 = (int)index_1;
            }
            isLoadingMedia = false;
            g_playbackStatus.hasFile = true;
            break;
        }

        case MPV_EVENT_VIDEO_RECONFIG: RATE_LIMITED_COUT(video_reconfig, 1,std::cout << "[DEBUG] [INFO] [VIDEO RECONFIG] Video configuration changed."); break;
        case MPV_EVENT_AUDIO_RECONFIG: RATE_LIMITED_COUT(audio_reconfig, 1,std::cout << "[DEBUG] [INFO] [AUDIO RECONFIG] Audio configuration changed."); break;
        case MPV_EVENT_END_FILE: {
            auto* data = (mpv_event_end_file*)event->data;
            if (!data) break;

            switch (data->reason) {
            // --- Phát hết file bình thường ---
            case MPV_END_FILE_REASON_EOF: 
                //if (v_Settings.repeatVideo) {
                //   mpv_command_seek_abs(mpv, 0, g_playbackStatus.duration);
                //    mpv_command_string(mpv, "set pause no");
                //    std::cout << "[DEBUG] [MPV] Repeating video as per settings.\n";
                //    break;
                //}
                RATE_LIMITED_COUT(end_of_file, 1,std::cout << "[DEBUG] [INFO] [MPV] Playback reached end of file.");
                break;
            
            // --- Lỗi trong quá trình phát / load ---
            case MPV_END_FILE_REASON_ERROR: 
            {
                isLoadingMedia = false;
                int err = data->error;
                std::string errStr = mpv_error_string(err);
                RATE_LIMITED_COUT(mpv_end_file_error, 1,std::cout << "[ERROR] [MPV ERROR] Playback error occurred: " << errStr << "";);
                HandleMpvError(mpv ,err,errStr.c_str());
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


        case MPV_EVENT_SEEK:{
            Seekingdata.g_isSeeking = true; 
            RATE_LIMITED_COUT(mpv_seek, 1,std::cout << "[DEBUG] [INFO] [MPV] Seek operation started."); break;
        }

        case MPV_EVENT_PLAYBACK_RESTART: 
        {
            Seekingdata.g_isSeeking = false; 
            if(pendingSeekTime >= 0.0){
                if(!is_live)mpv_command_seek_abs(mpv, pendingSeekTime , g_playbackStatus.duration);
                RATE_LIMITED_COUT(playback_restart_seek, 1,std::cout << "[DEBUG] [INFO] [MPV] Performing pending seek to " << pendingSeekTime << " seconds.");
                pendingSeekTime = -1.0;
            }
            RATE_LIMITED_COUT(playback_restart, 1,std::cout << "[DEBUG] [INFO] [MPV] Playback restarted.");
            break;
        }
        case MPV_EVENT_IDLE: g_playbackStatus.hasFile = false; g_playbackStatus.ilde = true ;RATE_LIMITED_COUT(mpv_idle, 1,std::cout << "[DEBUG] [INFO] [MPV] MPV is now idle."); break;
        case MPV_EVENT_SHUTDOWN: g_playbackStatus.hasFile = false; RATE_LIMITED_COUT(mpv_shutdown, 1,std::cout << "[DEBUG] [INFO] [MPV] MPV is shutting down."); break;

        case MPV_EVENT_LOG_MESSAGE: {
            auto* msg = (mpv_event_log_message*)event->data;

            if (msg && msg->prefix && msg->text && std::string(msg->prefix) == "cplayer")
                HandleYTDLLog(msg->text);
            
            if (msg && msg->level && (strcmp(msg->level, "error")  == 0 ||
                                      strcmp(msg->level, "warn")  == 0 ))

                PushMpvError(msg->level, msg->text);
            break;
        }


        default: break;
        }
    }
}

void LoadAllScripts(mpv_handle* mpv) {
    namespace fs = std::filesystem;

    // 1. Danh sách các folder chứa script cần load
    std::vector<std::string> folders = {
        AutoPath<std::string>("%ROOT%","scripts"),                    // Thư mục scripts trong folder ứng dụng
    };

    for (auto& folderPath : folders) {
        if (!fs::exists(folderPath)) continue;

        for (auto& entry : fs::directory_iterator(folderPath)) {
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();
            if (path.size() >= 4 && path.substr(path.size() - 4) == ".lua") {
                // Load script runtime
                const char* args[] = { "load-script", path.c_str(), nullptr };
                mpv_command(mpv, args);

                // Lưu vào vector + set tránh trùng
                if (g_loadedScriptsSet.find(path) == g_loadedScriptsSet.end()) {
                    g_loadedScripts.push_back(path);
                    g_loadedScriptsSet.insert(path);
                }
            }
        }
    }
}

void TerminateHandler() {
    RATE_LIMITED_COUT(terminate_handler, 1,std::cout << "[DEBUG] [WARNING] Terminate handler called. Cleaning up");
    StopService();
    std::abort();  // Kết thúc app
}
void SignalHandler(int signal) {
    RATE_LIMITED_COUT(signal_handler, 1,std::cout << "[DEBUG] [WARNING] Signal " << signal << " received. Cleaning up.");
    StopService();
    std::_Exit(signal);  // Kết thúc app ngay, tránh gọi các destructor
}
std::wstring UTF8ToWide(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}
std::string WideToUTF8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
    return str;
}
const char* PlaybackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::Idle:        return "Idle";
        case PlaybackState::Loading:     return "Loading";
        case PlaybackState::Playing:     return "Playing";
        case PlaybackState::Paused:      return "Paused";
        case PlaybackState::Seeking:     return "Seeking";
        case PlaybackState::EndOfFile:   return "End of File";
        default:                         return "Unknown";
    }
}
PlaybackState GetPlaybackState() {
    if (isLoadingMedia )
        return PlaybackState::Loading;

    if (g_playbackStatus.ilde)
        return PlaybackState::Idle;

    if ((g_playbackStatus.eofReached) && !Seekingdata.g_isSeeking)
        return PlaybackState::EndOfFile;

    if (Seekingdata.g_isSeeking)
        return PlaybackState::Seeking;

    if (g_playbackStatus.isPaused)
        return PlaybackState::Paused;

    return PlaybackState::Playing;
}

void UpdateHoverAnim(float& animValue, bool isHovering, float speed)
{
    float animSpeed = speed * ImGui::GetIO().DeltaTime;
    if (isHovering)
        animValue = std::min(1.0f, animValue + animSpeed);
    else
        animValue = std::max(0.0f, animValue - animSpeed);
}
void DrawCardWithHole(
    ImDrawList* dl,
    const ImVec2& cardMin,
    const ImVec2& cardMax,
    const ImVec2& holeMin,
    const ImVec2& holeMax,
    ImU32 fillCol,
    ImU32 borderCol,
    const CardHoleStyle& style 
)
{
    const float r = style.rounding;

    // =========================
    // FILL
    // =========================

    // Phải
    dl->PushClipRect(
        ImVec2(holeMax.x, cardMin.y),
        cardMax,
        true
    );
    dl->AddRectFilled(
        cardMin,
        cardMax,
        fillCol,
        r,
        ImDrawFlags_RoundCornersRight
    );
    dl->PopClipRect();

    // Trên trái
    if (holeMin.y > cardMin.y)
    {
        dl->PushClipRect(
            cardMin,
            ImVec2(holeMax.x, holeMin.y),
            true
        );
        dl->AddRectFilled(
            cardMin,
            cardMax,
            fillCol,
            r,
            ImDrawFlags_RoundCornersTopLeft
        );
        dl->PopClipRect();
    }
    // ⭐ LEFT – MIDDLE (BỔ SUNG)
    dl->PushClipRect(
        ImVec2(cardMin.x, holeMin.y),
        ImVec2(holeMin.x, holeMax.y),
        true
    );
    dl->AddRectFilled(
        cardMin,
        cardMax,
        fillCol,
        0.0f,                 // KHÔNG bo góc
        ImDrawFlags_None
    );
    dl->PopClipRect();

    // Dưới trái
    if (holeMax.y < cardMax.y)
    {
        dl->PushClipRect(
            ImVec2(cardMin.x, holeMax.y),
            ImVec2(holeMax.x, cardMax.y),
            true
        );
        dl->AddRectFilled(
            cardMin,
            cardMax,
            fillCol,
            r,
            ImDrawFlags_RoundCornersBottomLeft
        );
        dl->PopClipRect();
    }

    // =========================
    // BORDER
    // =========================

    if ((borderCol >> IM_COL32_A_SHIFT) > 0)
    {
        // Phải
        dl->PushClipRect(
            ImVec2(holeMax.x, cardMin.y),
            cardMax,
            true
        );
        dl->AddRect(
            cardMin,
            cardMax,
            borderCol,
            r,
            ImDrawFlags_RoundCornersRight,
            style.borderThickness
        );
        dl->PopClipRect();

        // Trên trái
        if (holeMin.y > cardMin.y)
        {
            dl->PushClipRect(
                cardMin,
                ImVec2(holeMax.x, holeMin.y),
                true
            );
            dl->AddRect(
                cardMin,
                cardMax,
                borderCol,
                r,
                ImDrawFlags_RoundCornersTopLeft,
                style.borderThickness
            );
            dl->PopClipRect();
        }

        // ⭐ LEFT – MIDDLE (BỔ SUNG)
        dl->PushClipRect(
            ImVec2(cardMin.x, holeMin.y),
            ImVec2(holeMin.x, holeMax.y),
            true
        );
        dl->AddRect(
            cardMin,
            cardMax,
            borderCol,
            0.0f,              // không bo góc
            ImDrawFlags_None,
            style.borderThickness
        );
        dl->PopClipRect();

        // Dưới trái
        if (holeMax.y < cardMax.y)
        {
            dl->PushClipRect(
                ImVec2(cardMin.x, holeMax.y),
                ImVec2(holeMax.x, cardMax.y),
                true
            );
            dl->AddRect(
                cardMin,
                cardMax,
                borderCol,
                r,
                ImDrawFlags_RoundCornersBottomLeft,
                style.borderThickness
            );
            dl->PopClipRect();
        }
    }
}



void UpdateUIState() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos; 
    Uint32 currentTime = SDL_GetTicks();
    
    // 1. Kiểm tra vị trí chuột
    bool isMouseInsideVideo = (mousePos.x >= Windowlayout.videoArea.x && 
                               mousePos.x <= (Windowlayout.videoArea.x + Windowlayout.videoArea.w) &&
                               mousePos.y >= Windowlayout.videoArea.y && 
                               mousePos.y <= (Windowlayout.videoArea.y + Windowlayout.videoArea.h));

    // 2. Kiểm tra tương tác với UI (Hover nút, kéo slider, combo...)
    // io.WantCaptureMouse là cách nhanh nhất để biết chuột có đang đè lên bất kỳ cửa sổ ImGui nào không
    bool isInteractingWithUI = io.WantCaptureMouse && (ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered());

    
    // Lưu ý: Luôn reset timer nếu chuột đang di chuyển HOẶC đang tương tác với UI
    bool isMouseMoving = ((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) && io.WantCaptureMouse);

    // 3. XÁC ĐỊNH TIMEOUT THEO 3 TRẠNG THÁI (Ưu tiên từ cao xuống thấp)
    Uint32 currentTimeout;
    if (isInteractingWithUI ) {
        currentTimeout = 2000; // Đang tương tác UI: 2s
    } else if (isMouseInsideVideo) {
        currentTimeout = 1000; // Di chuột bình thường trong video: 1s
    } else {
        currentTimeout = 500;  // Đã rời khỏi video: 0.5s
    }
    // 4. RESET TIMER KHI CÓ HOẠT ĐỘNG
    if ((isMouseMoving || isInteractingWithUI) && isMouseInsideVideo) {
        lastInteractionTime = currentTime;
        
        // Tự động hiện lại UI nếu có hoạt động
        if (!show_ui_video) {
            show_ui_video = true;
            SDL_ShowCursor(SDL_ENABLE);
        }
    }

    // 5. LOGIC ẨN UI
    if (show_ui_video) {
        // Chỉ ẩn khi hết thời gian chờ
        if (currentTime - lastInteractionTime > currentTimeout) {
            
            // CỰC KỲ QUAN TRỌNG: Không ẩn khi đang có Popup/Combo mở hoặc đang kéo Slider
            if (!ImGui::IsAnyItemActive() && !IsAnyPopupOpen()) {
                show_ui_video = false;
                SDL_ShowCursor(SDL_DISABLE);
            }
        }
    }
}
void NotifyActivity() {
    // Đánh thức UI và hiện con trỏ
    if (!show_ui_video) {
        show_ui_video = true;
        SDL_ShowCursor(SDL_ENABLE);
    }
    // Cập nhật mốc thời gian tương tác cuối cùng
    lastInteractionTime = SDL_GetTicks();
}



