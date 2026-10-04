#include "PlaybackObserver.h"

#include "player/player/Player.h"
#include "player/PlayerDataModels.h"
#include "player/PlayerUtils.h"
#include "player/command/PlaybackCommand.h"
#include "common/LifecycleEvidence.h"
#include "globals.h"
#include "settings_manager.h"

#include <log.h>
#include <mutex>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <SDL.h>

static std::mutex g_retry_mutex;
static std::unordered_map<int, int> g_retryCount;
static constexpr int MAX_RETRY = 1;

PlaybackObserver::PlaybackObserver(Player& player, PlayerStateSystem& state, PlaybackCommand& commander) 
    : m_player(player), m_mpv(player.GetHandle()), m_state(state), m_commander(commander)
{}

void PlaybackObserver::ObserveProps(const std::vector<std::pair<const char*, mpv_format>>& props, const char* groupName) {
    for (const auto& [name, fmt] : props) {
        int ret = mpv_observe_property(m_mpv, 0, name, fmt);
        if (ret < 0) {
            LOG(1, LogLevel::Warning, LogCategory::System, "[MPV] Failed to observe %s (%s): %s", name ,groupName ,mpv_error_string(ret));
        } else {
            LOG(1, LogLevel::Info, LogCategory::System, " [MPV] Observing %s (%s)", name ,groupName);
        }
    }
}

void PlaybackObserver::Init() {
    if (!m_mpv) return;

    LOG(1, LogLevel::Info, LogCategory::System, "=================== [MPV] Initializing observers ===================");

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

    ObserveProps({
        {"vo-configured", MPV_FORMAT_FLAG},
        {"display-sync-active", MPV_FORMAT_FLAG},
        {"frame-drop-count", MPV_FORMAT_INT64},
        {"decoder-frame-drop-count", MPV_FORMAT_INT64},
        {"packet-drop-count", MPV_FORMAT_INT64},
        {"vsync-jitter", MPV_FORMAT_DOUBLE},
        {"file-size", MPV_FORMAT_INT64},
        {"demuxer-cache-state", MPV_FORMAT_NODE},
        {"pause-for-cache", MPV_FORMAT_FLAG}
    }, "Advanced");
    mpv_observe_property(m_mpv, 0, "af-metadata/ebur_measurer", MPV_FORMAT_NODE);

    LOG(1, LogLevel::Info, LogCategory::System, "=================== [MPV] Observer registration complete ===================");
}

void PlaybackObserver::HandleMpvError(int err , const char* msgText)
{
    if (!m_mpv) return;
    switch (err) {
        // ===== Lỗi có thể tự sửa hoặc fallback =====
        case MPV_ERROR_EVENT_QUEUE_FULL:    
        case MPV_ERROR_PROPERTY_UNAVAILABLE:
        {
            if  (err == MPV_ERROR_EVENT_QUEUE_FULL)             LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_EVENT_QUEUE_FULL..."); 
            else if (err == MPV_ERROR_PROPERTY_UNAVAILABLE)     LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_PROPERTY_UNAVAILABLE..."); 
            break;
        }
        case MPV_ERROR_PROPERTY_FORMAT:     
        case MPV_ERROR_OPTION_FORMAT:       
        case MPV_ERROR_UNKNOWN_FORMAT:      
        {
            if(err == MPV_ERROR_PROPERTY_FORMAT)                LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_PROPERTY_FORMAT..."); 
            else if (err == MPV_ERROR_OPTION_FORMAT)            LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_OPTION_FORMAT...");
            else if (err == MPV_ERROR_UNKNOWN_FORMAT)           LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_UNKNOWN_FORMAT...");

            auto& Cfg = ConfigManager::Instance();
            auto videoCfg = Cfg.GetVideoSettings();
            const char* chosenFormat = videoCfg.selectedResolution.c_str();
            m_commander.SetPropertyString("ytdl-format", chosenFormat);
            LOG(1, LogLevel::Error, LogCategory::System, "[WARNING] [MPV] Reset ytdl-format to default and retrying...");
            break;
        }
        case MPV_ERROR_GENERIC:             
        case MPV_ERROR_LOADING_FAILED:      
        case MPV_ERROR_NOTHING_TO_PLAY :  
        {
            if(err == MPV_ERROR_GENERIC)                        LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_GENERIC...");
            else if (err == MPV_ERROR_LOADING_FAILED)           LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_LOADING_FAILED...");
            else if (err == MPV_ERROR_NOTHING_TO_PLAY)          LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_NOTHING_TO_PLAY...");
            
            int PlayingIndex = -1;
            m_state.ReadPlaylist([&PlayingIndex](auto const& m){
                PlayingIndex = m.g_PlayingIndex;
            });

            if (PlayingIndex >= 0) {
                int idx = PlayingIndex;
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
                    LOG(1, LogLevel::Warning, LogCategory::System,"[WARNING] [MPV] Retrying playback for index %d...", idx);
                    auto* commanderPtr = &m_commander;
                    std::thread([idx,commanderPtr]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        std::string idx_str = std::to_string(idx);
                        const char* args[] = { "playlist-play-index", idx_str.c_str(), nullptr };
                        commanderPtr->Exec(args); 
                    }).detach();
                } else {
                    LOG(1, LogLevel::Warning, LogCategory::System, "[WARNING] [MPV] Max retries reached for index %d, removing from playlist.", idx);
                    std::string idx_str = std::to_string(idx);
                    const char* args[] = { "playlist-remove", idx_str.c_str(), nullptr };
                    m_commander.Exec(args); 
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
            if(err == MPV_ERROR_PROPERTY_ERROR)                 LOG(1,LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_PROPERTY_ERROR..."); 
            else if(err == MPV_ERROR_COMMAND)                   LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_COMMAND..."); 
            else if(err == MPV_ERROR_PROPERTY_NOT_FOUND)        LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_PROPERTY_NOT_FOUND..."); 
            else if(err == MPV_ERROR_OPTION_ERROR)              LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_OPTION_ERROR..."); 
            else if(err == MPV_ERROR_OPTION_NOT_FOUND)          LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_OPTION_NOT_FOUND..."); 
            else if(err == MPV_ERROR_UNSUPPORTED)               LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_UNSUPPORTED..."); 
            else if(err == MPV_ERROR_NOT_IMPLEMENTED)           LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_NOT_IMPLEMENTED..."); 
            else if(err == MPV_ERROR_AO_INIT_FAILED)            LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_AO_INIT_FAILED..."); 
            else if(err == MPV_ERROR_VO_INIT_FAILED)            LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_VO_INIT_FAILED..."); 
            else if(err == MPV_ERROR_NOMEM)                     LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_NOMEM..."); 
            else if(err == MPV_ERROR_INVALID_PARAMETER)         LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_INVALID_PARAMETER..."); 
            else if(err == MPV_ERROR_UNINITIALIZED)             LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] MPV_ERROR_UNINITIALIZED...");
             
            int PlayingIndex = -1;
            m_state.ReadPlaylist([&PlayingIndex](auto const& m){
                PlayingIndex = m.g_PlayingIndex;
            });
            
            if (PlayingIndex >= 0) {
                std::string idx_str = std::to_string(PlayingIndex);
                const char* args[] = { "playlist-remove", idx_str.c_str(), nullptr };
                m_commander.Exec(args); 
            }
            break;
        }

        // ===== Lỗi khác (bỏ qua) =====
        default:
            LOG(1, LogLevel::Error, LogCategory::System,  "[ERROR] Other MPV error code: %d", err);
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

void PlaybackObserver::UpdateVideoParams(const mpv_node* node) {
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
}

void PlaybackObserver::UpdateAudioParams(const mpv_node* node) {
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
}

void PlaybackObserver::UpdateTrackList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WriteTrack([&](auto& m){ m.tracks.clear(); });
        m_state.WriteSubtitle([&](auto& m){ m.hasSubtitles = false; });
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
    LOG(1, LogLevel::Info, LogCategory::System, "Total tracks: %d",tmp.size());

    // Cập nhật vào State System mới
    m_state.WriteTrack([&](TrackModel& m) { m.tracks = tmp; });
    m_state.WriteSubtitle([&](SubtitleModel& m) { m.hasSubtitles = found_sub; });
}

void PlaybackObserver::UpdateAudioDeviceList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WriteAudio([&](auto& m){ m.device.audioDevices.clear(); });
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
        LOG(1, LogLevel::Info, LogCategory::System,
            "[DEBUG] [INFO] [Audio Device] %s (%s)", dev.name, dev.description);

        tmp.push_back(dev);
    }

    // Cập nhật vào State System mới
    m_state.WriteAudio([&](AudioModel& m) { m.device.audioDevices = tmp; });
}

void PlaybackObserver::UpdateChapterList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list || node->u.list->num == 0) {
        m_state.WriteTrack([&](auto& m){ m.chapters.clear(); });
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
        LOG(1, LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [Chapters] Chapter found: %s at %f seconds.", dev.title , dev.time);
    }

    // Cập nhật vào State System mới
    m_state.WriteTrack([&](TrackModel& m) { m.chapters = tmp; });
}

void PlaybackObserver::UpdatePlaylist(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        m_state.WritePlaylist([&](auto& m){ m.playlist.clear(); });
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

        LOG(1,  LogLevel::Info, LogCategory::System,
            "[Playlist] Entry: %d Filename: %s Title: %s Current: %s Playing: %s ID: %d", 
            i, 
            dev.filename, 
            dev.title, 
            (dev.current ? "True" : "False"), 
            (dev.playing ? "True" : "False"), 
            dev.id);

            
        tmp.push_back(dev);
    }

    // Cập nhật vào State System mới
    m_state.WritePlaylist([&](PlaylistModel& m) { m.playlist = tmp; });
}

void PlaybackObserver::UpdateMetadata(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list) {
        m_state.WriteMedia([&](auto& m){ m.metadata.clear(); });
        return;
    }

    std::unordered_map<std::string, std::string> tmp;

    const mpv_node_list* list = node->u.list;
    for (int i = 0; i < list->num; ++i) {
        const char* key = list->keys[i];
        const mpv_node& value = list->values[i];

        if (key && value.format == MPV_FORMAT_STRING && value.u.string) {
            tmp[key] = value.u.string;

            LOG(1,  LogLevel::Info, LogCategory::System,
                "[Metadata] %s = %s", key, value.u.string);
        }
    }

    // Cập nhật vào State System mới
    m_state.WriteMedia([&](MediaModel& m) { m.metadata = tmp; });
}

void PlaybackObserver::UpdateLoudnessMetadata(const mpv_node* node) {
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
}


void PlaybackObserver::ProcessEvents() {
    if (!m_mpv) return;
    while (mpv_event* event = mpv_wait_event(m_mpv, 0)) {
        if(event->event_id == MPV_EVENT_NONE) break;
        switch (event->event_id) {

        case MPV_EVENT_SHUTDOWN: {
            LOG(1, LogLevel::Info, LogCategory::System, "[MPV] MPV is shutting down."); 
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {

            if(auto* msg = (mpv_event_log_message*)event->data) {
                if (msg->prefix && msg->text && (strcmp(msg->prefix, "cplayer") == 0)) {
                    HandleYTDLLog(msg->text);
                }
                if (msg && msg->level) {
                    if (strcmp(msg->level, "error")  == 0) {
                        LOG(1, LogLevel::Error, LogCategory::System, msg->text);
                    }
                    if (strcmp(msg->level, "warn")  == 0) {        
                        LOG(1, LogLevel::Warning, LogCategory::System, msg->text);
                    }
                }
            }
            break;
        }
        case MPV_EVENT_GET_PROPERTY_REPLY: break;
        case MPV_EVENT_SET_PROPERTY_REPLY: break;
        case MPV_EVENT_COMMAND_REPLY: break;
        case MPV_EVENT_START_FILE: {
            Uint64 loadId = 0;
            double pendingSeek = -1.0;
            m_state.WritePlayback([&](auto& m) {
                //m.flags.isIdleActive = false;
                m.isLoadingMedia = true;
                loadId = static_cast<Uint64>(m.startupLoadId);
                pendingSeek = m.pendingseektime;
            });
            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=START_FILE load_id=%llu ts_ms=%llu pending_seek=%.3f",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(SDL_GetTicks64()),
                pendingSeek);
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=START_FILE load_id=%llu ts_ms=%llu pending_seek=%.3f",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64()),
                    pendingSeek));
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto* data = (mpv_event_end_file*)event->data;
            if (!data) break;

            const Uint64 loadId = m_commander.GetStartupLoadId();
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=END_FILE load_id=%llu ts_ms=%llu reason=%d error=%d error_text=%s",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64()),
                    static_cast<int>(data->reason),
                    data->error,
                    mpv_error_string(data->error)));
 
            switch (data->reason) {
            // --- Phát hết file bình thường ---
            case MPV_END_FILE_REASON_EOF: 

                LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [MPV] Playback reached end of file.");
                break;
            
            // --- Lỗi trong quá trình phát / load ---
            case MPV_END_FILE_REASON_ERROR: 
            {
                m_state.WritePlayback([&](auto& m) {
                    m.isLoadingMedia = false;
                });
                int err = data->error;

                std::string errStr = mpv_error_string(err);
                LOG(1,  LogLevel::Info, LogCategory::System,  "Playback error occurred: %s", errStr);
                HandleMpvError(err, errStr.c_str());
                break;
            }

            // --- Dừng thủ công ---
            case MPV_END_FILE_REASON_STOP:
                LOG(1, LogLevel::Info, LogCategory::System, "Playback stopped manually.");
                break;

            // --- Redirect URL ---
            case MPV_END_FILE_REASON_REDIRECT:
                LOG(1,  LogLevel::Warning, LogCategory::System, "Playback ended due to redirect.");
                break;

            default:
                LOG(1,  LogLevel::Warning, LogCategory::System, "Playback ended for unknown reason: %d.", data->reason);
                break;
            }

            break;
        }
        case MPV_EVENT_FILE_LOADED:{
            Uint64 loadId = 0;
            VideoType videotype = VideoType::None;
            m_state.WritePlayback([&](auto& m) {
                m.isLoadingMedia = false;
                loadId = static_cast<Uint64>(m.startupLoadId);
                videotype = m.videoType;
            });

            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=FILE_LOADED load_id=%llu ts_ms=%llu video_type=%d",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(SDL_GetTicks64()),
                static_cast<int>(videotype));
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=FILE_LOADED load_id=%llu ts_ms=%llu video_type=%d",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64()),
                    static_cast<int>(videotype)));

            // #33 keeps the existing behavior intentionally, but makes the
            // timing observable: source-specific MPV config is applied only
            // after FILE_LOADED. Runtime evidence decides whether a later
            // issue should move this boundary.
            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=DYNAMIC_CONFIG_APPLY_BEGIN load_id=%llu ts_ms=%llu timing=post_file_loaded",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(SDL_GetTicks64()));
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=DYNAMIC_CONFIG_APPLY_BEGIN load_id=%llu ts_ms=%llu timing=post_file_loaded",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64())));
            ApplyDynamicMPVConfig(m_mpv, videotype);
            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=DYNAMIC_CONFIG_APPLY_END load_id=%llu ts_ms=%llu",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(SDL_GetTicks64()));
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=DYNAMIC_CONFIG_APPLY_END load_id=%llu ts_ms=%llu",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64())));

            m_commander.Play();
 
            break;
        }
        case MPV_EVENT_IDLE: {
            LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [MPV] MPV is now idle."); 
            break;
        }
        case MPV_EVENT_TICK: break;
        case MPV_EVENT_CLIENT_MESSAGE: break;
        case MPV_EVENT_VIDEO_RECONFIG: {
            LOG(1, LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [VIDEO RECONFIG] Video configuration changed."); 
            break;
        }
        case MPV_EVENT_AUDIO_RECONFIG: {
            LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [AUDIO RECONFIG] Audio configuration changed.");
            break;
        }
        case MPV_EVENT_SEEK:{
            // Cập nhật State System mới
//            m_state.WritePlayback([&](auto& m) {
//                m.flags.isSeeking = true;
//            });
            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=SEEK load_id=%llu ts_ms=%llu source=mpv_event",
                static_cast<unsigned long long>(m_commander.GetStartupLoadId()),
                static_cast<unsigned long long>(SDL_GetTicks64()));
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=SEEK load_id=%llu ts_ms=%llu source=mpv_event",
                    static_cast<unsigned long long>(m_commander.GetStartupLoadId()),
                    static_cast<unsigned long long>(SDL_GetTicks64())));
            LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [MPV] Seek operation started."); 
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART: 
        {   
            double targetSeek = -1.0;
            double duration = 0.0;
            double pendingSeekBefore = -1.0;
            Uint64 loadId = 0;
            uint32_t restartCount = 0;

            // Read + consume the explicitly armed format-switch seek under one
            // state lock. A fresh/direct load starts at -1 and cannot enter
            // this path unless an explicit action armed a seek.
            m_state.WritePlayback([&](PlaybackModel& m) {
                loadId = static_cast<Uint64>(m.startupLoadId);
                restartCount = ++m.startupRestartCount;
                pendingSeekBefore = m.pendingseektime;
                if (m.pendingseektime >= 0.0) {
                    targetSeek = m.pendingseektime;
                    duration = m.timing.duration;
                    m.pendingseektime = -1.0;
                }
            });

            LOG(1, LogLevel::Info, LogCategory::System,
                "[STARTUP] event=PLAYBACK_RESTART load_id=%llu ts_ms=%llu count=%u pending_seek_before=%.3f",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(SDL_GetTicks64()),
                restartCount,
                pendingSeekBefore);
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=PLAYBACK_RESTART load_id=%llu ts_ms=%llu count=%u pending_seek_before=%.3f",
                    static_cast<unsigned long long>(loadId),
                    static_cast<unsigned long long>(SDL_GetTicks64()),
                    restartCount,
                    pendingSeekBefore));

            // Execute the explicitly armed seek outside the state lock.
            if (targetSeek >= 0.0) {
                bool shouldSeek = false;
                m_state.ReadPlayback([&](PlaybackModel const& m) {
                    shouldSeek = m.videoType != VideoType::Live;
                });
                if (shouldSeek) {
                    LOG(1, LogLevel::Info, LogCategory::System,
                        "[STARTUP] event=SEEK_REQUEST load_id=%llu ts_ms=%llu source=pending_format_switch target=%.3f",
                        static_cast<unsigned long long>(loadId),
                        static_cast<unsigned long long>(SDL_GetTicks64()),
                        targetSeek);
                    LifecycleEvidence::EmitDiagnostic(
                        "STARTUP",
                        FormatString(
                            "event=SEEK_REQUEST load_id=%llu ts_ms=%llu source=pending_format_switch target=%.3f",
                            static_cast<unsigned long long>(loadId),
                            static_cast<unsigned long long>(SDL_GetTicks64()),
                            targetSeek));
                    m_commander.Seek(targetSeek, duration);
                }
                LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [MPV] Performing pending seek to %.2f seconds", targetSeek);
            }

            LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] [MPV] Playback restarted.");
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            HandlePropertyChange((mpv_event_property*)event->data);
            HandlePlaybackState();
            break;
        }
        case MPV_EVENT_QUEUE_OVERFLOW: break;
        case MPV_EVENT_HOOK: break; // Không còn xử lý audio hook ở đây
        default: break;  
        }
    }
}

void PlaybackObserver::HandlePlaybackState() {
    m_state.WritePlayback([](auto& m) {
        // 1. Xác định state mới
        PlaybackState newState = PlaybackState::Playing;

        if (m.flags.isIdleActive)            newState = PlaybackState::Idle;
        else if (m.flags.eofReached && 
                 !m.flags.isSeeking)         newState = PlaybackState::EndOfFile;
        else if (m.isLoadingMedia)           newState = PlaybackState::Loading;
        else if (m.flags.isSeeking)          newState = PlaybackState::Seeking;
        else if (m.flags.isPaused)           newState = PlaybackState::Paused;

        // 2. Chỉ cập nhật nếu thực sự có sự thay đổi (Tránh dirty state)
        if (m.state != newState) {
            m.state = newState;
            // (Nếu có Event Manager, bạn có thể trigger event StateChanged tại đây)
        }
    });
}

void PlaybackObserver::HandlePropertyChange(mpv_event_property* prop) {
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

void PlaybackObserver::HandleStringProperty(const char* name, const char* value) {
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
}

void PlaybackObserver::HandleFlagProperty(const char* name, bool value) {
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
    else if (strcmp(name, "vo-configured") == 0);
    else if (strcmp(name, "display-sync-active") == 0);
    else if (strcmp(name, "pause-for-cache") == 0);
}

void PlaybackObserver::HandleInt64Property(const char* name, int64_t value) {
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
    else if (strcmp(name, "frame-drop-count") == 0);
    else if (strcmp(name, "decoder-frame-drop-count") == 0);
    else if (strcmp(name, "packet-drop-count") == 0);
    else if (strcmp(name, "file-size") == 0);
}

void PlaybackObserver::HandleDoubleProperty(const char* name, double value) {
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
    else if (strcmp(name, "vsync-jitter") == 0);
}

void PlaybackObserver::HandleNodeProperty(const char* name, const mpv_node* node) {
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
                }
            }
        }
    }
    else if (strcmp(name, "audio-device-list") == 0)          UpdateAudioDeviceList(node);
    else if (strcmp(name, "af-metadata/ebur_measurer") == 0)  UpdateLoudnessMetadata(node);
    else if (strcmp(name, "demuxer-cache-state") == 0);
}
