#include "MPVObserver.h"
#include "mpv/player/MPVPlayer.h"
#include "mpv/mpv_data.h"
#include "mpv/session/MPVManager.h"
#include "mpv/mpv_basic_formats.h"
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

MPVObserver::MPVObserver(MPVPlayer& player) : m_player(player), m_mpv(player.GetHandle()) {}

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

void MPVObserver::HandleMpvError(int err, const char* msgText) {
    // ... (logic from global HandleMpvError)
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
    if ((n = mpv_node_dict_find_local(node, "pixelformat")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vpixfmt = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "primaries")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vprimaries = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "gamma")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vgamma = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "colormatrix")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolormatrix = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "colorlevels")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vcolorlevels = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "stereo-in")) && n->format == MPV_FORMAT_STRING)      g_videoInfo.g_videoparams.vstereo_in = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "chroma-location")) && n->format == MPV_FORMAT_STRING)g_videoInfo.g_videoparams.vchroma_location = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "w")) && n->format == MPV_FORMAT_INT64)               g_videoInfo.g_videoparams.vwidth = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "h")) && n->format == MPV_FORMAT_INT64)               g_videoInfo.g_videoparams.vheight = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "dw")) && n->format == MPV_FORMAT_INT64)              g_videoInfo.g_videoparams.vdisp_w = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "dh")) && n->format == MPV_FORMAT_INT64)              g_videoInfo.g_videoparams.vdisp_h = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "crop-x")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_x = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "crop-y")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_y = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "crop-w")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_w = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "crop-h")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vcrop_h = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "aspect")) && n->format == MPV_FORMAT_DOUBLE)         g_videoInfo.g_videoparams.vaspect = n->u.double_;
    if ((n = mpv_node_dict_find_local(node, "aspect-name")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_videoparams.vaspect_name = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "par")) && n->format == MPV_FORMAT_DOUBLE)            g_videoInfo.g_videoparams.vpar = n->u.double_;
    if ((n = mpv_node_dict_find_local(node, "sar")) && n->format == MPV_FORMAT_DOUBLE)            g_videoInfo.g_videoparams.vsar = n->u.double_;
    if ((n = mpv_node_dict_find_local(node, "sar-name")) && n->format == MPV_FORMAT_STRING)       g_videoInfo.g_videoparams.vsar_name = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "sig-peak")) && n->format == MPV_FORMAT_DOUBLE)       g_videoInfo.g_videoparams.vsig_peak = n->u.double_;
    if ((n = mpv_node_dict_find_local(node, "light")) && n->format == MPV_FORMAT_STRING)          g_videoInfo.g_videoparams.vlight = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "rotate")) && n->format == MPV_FORMAT_INT64)          g_videoInfo.g_videoparams.vrotate = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "average-bpp")) && n->format == MPV_FORMAT_INT64)     g_videoInfo.g_videoparams.average_bpp = (int)n->u.int64;
}

void MPVObserver::UpdateAudioParams(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP) return;
    const mpv_node* n = nullptr;
    if ((n = mpv_node_dict_find_local(node, "format")) && n->format == MPV_FORMAT_STRING)         g_videoInfo.g_audioarams.aformat = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "samplerate")) && n->format == MPV_FORMAT_INT64)      g_videoInfo.g_audioarams.asamplerate = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "channel-count")) && n->format == MPV_FORMAT_INT64)   g_videoInfo.g_audioarams.channel_count = (int)n->u.int64;
    if ((n = mpv_node_dict_find_local(node, "channels")) && n->format == MPV_FORMAT_STRING)       g_videoInfo.g_audioarams.achannels_str = n->u.string;
    if ((n = mpv_node_dict_find_local(node, "hr-channels")) && n->format == MPV_FORMAT_STRING)    g_videoInfo.g_audioarams.ahr_channels = n->u.string;
}

void MPVObserver::UpdateTrackList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
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

        auto read_str = [&](const char* key, std::string& out) { if ((n = mpv_node_dict_find_local(&entry, key)) && n->format == MPV_FORMAT_STRING) out = n->u.string ? n->u.string : ""; };
        auto read_int = [&](const char* key, int& out) { if ((n = mpv_node_dict_find_local(&entry, key))) { if (n->format == MPV_FORMAT_INT64) out = (int)n->u.int64; else if (n->format == MPV_FORMAT_DOUBLE) out = (int)n->u.double_; } };
        auto read_dbl = [&](const char* key, double& out) { if ((n = mpv_node_dict_find_local(&entry, key))) { if (n->format == MPV_FORMAT_DOUBLE) out = n->u.double_; else if (n->format == MPV_FORMAT_INT64) out = (double)n->u.int64; } };
        auto read_bool = [&](const char* key, bool& out) { if ((n = mpv_node_dict_find_local(&entry, key)) && n->format == MPV_FORMAT_FLAG) out = (n->u.flag != 0); };

        read_int("id", track.common.id);
        read_str("type", track.common.type);
        if (track.common.type == "sub") found_sub = true;
        read_str("codec", track.common.codec);
        read_str("lang", track.common.language);
        read_str("title", track.common.title);
        read_bool("selected", track.common.selected);

        if (track.common.type == "video" || track.common.type == "image") {
            read_int("demux-w", track.video.demux_w);
            read_int("demux-h", track.video.demux_h);
        }
        if (track.common.type == "audio") {
            read_int("demux-samplerate", track.audio.demux_samplerate);
            read_int("demux-channel-count", track.audio.demux_channel_count);
        }
        tmp.push_back(std::move(track));
    }
    g_videoInfo.hasSubtitles = found_sub;
    g_videoInfo.g_tracks.swap(tmp);
}

void MPVObserver::UpdateAudioDeviceList(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) return;
    std::vector<AudioDeviceInfo> tmp;
    for (int i = 0; i < node->u.list->num; ++i) {
        const mpv_node& entry = node->u.list->values[i];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) continue;
        const mpv_node* n = nullptr;
        AudioDeviceInfo dev;
        if ((n = mpv_node_dict_find_local(&entry, "name")) && n->format == MPV_FORMAT_STRING) dev.name = n->u.string;
        if ((n = mpv_node_dict_find_local(&entry, "description")) && n->format == MPV_FORMAT_STRING) dev.description = n->u.string;
        tmp.push_back(dev);
    }
    g_playbackStatus.g_audioDevices.swap(tmp);
}

void MPVObserver::UpdateChapterList(const mpv_node* node) {
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
        if ((n = mpv_node_dict_find_local(&entry, "time"))) {
            if (n->format == MPV_FORMAT_INT64) dev.time = static_cast<double>(n->u.int64);
            else if (n->format == MPV_FORMAT_DOUBLE) dev.time = n->u.double_;
        }
        if ((n = mpv_node_dict_find_local(&entry, "title")) && n->format == MPV_FORMAT_STRING) { dev.title = n->u.string; }
        tmp.push_back({ dev });
    }
    g_videoInfo.g_chapters.swap(tmp);
}

void MPVObserver::UpdatePlaylist(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_ARRAY || !node->u.list) {
        g_playbackStatus.g_playlist.clear();
        return;
    }
    std::vector<PlaylistEntry> tmp;
    for (int i = 0; i < node->u.list->num; i++) {
        const mpv_node* entry = &node->u.list->values[i];
        if (entry->format != MPV_FORMAT_NODE_MAP || !entry->u.list) continue;
        const mpv_node* n = nullptr;
        PlaylistEntry dev;
        if ((n = mpv_node_dict_find_local(entry, "playing")) && n->format == MPV_FORMAT_FLAG) dev.playing = (n->u.flag != 0);
        if ((n = mpv_node_dict_find_local(entry, "current")) && n->format == MPV_FORMAT_FLAG) dev.current = (n->u.flag != 0);
        if ((n = mpv_node_dict_find_local(entry, "id")) && n->format == MPV_FORMAT_INT64) dev.id = (int)n->u.int64;;
        if ((n = mpv_node_dict_find_local(entry, "filename")) && n->format == MPV_FORMAT_STRING) dev.filename = n->u.string;
        if ((n = mpv_node_dict_find_local(entry, "title")) && n->format == MPV_FORMAT_STRING) dev.title = n->u.string;
        tmp.push_back(dev);
    }
    g_playbackStatus.g_playlist.swap(tmp);
}

void MPVObserver::UpdateMetadata(const mpv_node* node) {
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
        }
    }
    g_videoInfo.metadata.swap(tmp);
}

void MPVObserver::UpdateLoudnessMetadata(const mpv_node* node) {
    if (!node || node->format != MPV_FORMAT_NODE_MAP) return;
    mpv_node_list* list = node->u.list;
    auto parse_db = [](const char* str) -> double { if (!str || std::string(str) == "-inf") return -99.0; try { return std::stod(str); } catch (...) { return -99.0; } };
    auto& ctx = g_videoInfo.g_audioarams;
    for (int i = 0; i < list->num; i++) {
        if (list->values[i].format != MPV_FORMAT_STRING) continue;
        std::string key = list->keys[i];
        const char* val_str = list->values[i].u.string;
        if (key == "lavfi.r128.M") ctx.loudness_momentary = parse_db(val_str);
        else if (key == "lavfi.r128.S") ctx.loudness_shortterm = parse_db(val_str);
        else if (key == "lavfi.r128.I") ctx.loudness_integrated = parse_db(val_str);
        else if (key == "lavfi.r128.LRA") { try { ctx.loudness_range = std::stod(val_str); } catch (...) {} }
        else if (key == "lavfi.r128.LRA.low") ctx.loudness_lra_low = parse_db(val_str);
        else if (key == "lavfi.r128.LRA.high") ctx.loudness_lra_high = parse_db(val_str);
        else if (key == "lavfi.r128.true_peak") ctx.true_peak = parse_db(val_str);
        else if (key == "lavfi.r128.true_peaks_ch0") ctx.true_peak_ch0 = parse_db(val_str);
        else if (key == "lavfi.r128.true_peaks_ch1") ctx.true_peak_ch1 = parse_db(val_str);
        else if (key == "lavfi.r128.sample_peaks_ch0") ctx.sample_peak_ch0 = parse_db(val_str);
        else if (key == "lavfi.r128.sample_peaks_ch1") ctx.sample_peak_ch1 = parse_db(val_str);
        else if (key == "lavfi.r128.sample_peak") ctx.sample_peak = parse_db(val_str);
    }
}

void MPVObserver::ProcessEvents() {
    if (!m_mpv) return;

    while (mpv_event* event = mpv_wait_event(m_mpv, 0)) {
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
        case MPV_EVENT_SHUTDOWN:
            g_playbackStatus.hasFile = false;
            break;
        case MPV_EVENT_LOG_MESSAGE: {
            auto* msg = (mpv_event_log_message*)event->data;
            if (msg && msg->prefix && msg->text && std::string(msg->prefix) == "cplayer") HandleYTDLLog(m_mpv, msg->text);
            if (msg && msg->level && (strcmp(msg->level, "error") == 0 || strcmp(msg->level, "warn") == 0)) PushMpvError(msg->level, msg->text);
            break;
        }
        case MPV_EVENT_START_FILE:
            g_playbackStatus.ilde = false;
            g_playbackStatus.isLoadingMedia = true;
            break;
        case MPV_EVENT_END_FILE: {
            auto* data = (mpv_event_end_file*)event->data;
            if (data && data->reason == MPV_END_FILE_REASON_ERROR) {
                g_playbackStatus.isLoadingMedia = false;
                HandleMpvError(data->error, mpv_error_string(data->error));
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            auto* session = MPVManager::GetInstance().GetDefaultSession();
            if (session && session->GetProperty()) {
                if (auto index = session->GetProperty()->GetInt("playlist-pos")) g_playbackStatus.g_PlayingIndex = (int)index.value();
            }
            g_playbackStatus.isLoadingMedia = false;
            g_playbackStatus.hasFile = true;
            break;
        }
        case MPV_EVENT_IDLE:
            g_playbackStatus.hasFile = false;
            g_playbackStatus.ilde = true;
            break;
        case MPV_EVENT_SEEK:
            g_playbackStatus.isSeeking = true;
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            g_playbackStatus.isSeeking = false;
            if (pendingSeekTime >= 0.0) {
                auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                if (commander && !(GetVideoType() == VideoType::Live)) commander->Seek(pendingSeekTime, g_playbackStatus.duration);
                pendingSeekTime = -1.0;
            }
            break;
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* prop = (mpv_event_property*)event->data;
            if (!prop || !prop->data) break;
            const char* name = prop->name;

            switch (prop->format) {
            case MPV_FORMAT_STRING: {
                const char* value = *(const char**)prop->data;
                if (strcmp(name, "filename") == 0) { g_playbackStatus.hasFile = true; g_playbackStatus.filename = value; }
                else if (strcmp(name, "media-title") == 0) { g_playbackStatus.mediaTitle = value; }
                else if (strcmp(name, "hwdec-current") == 0) { g_videoInfo.hwdec = value; }
                else if (strcmp(name, "video-codec") == 0) { g_videoInfo.vcodec = value; }
                else if (strcmp(name, "audio-codec") == 0) { g_videoInfo.acodec = value; }
                break;
            }
            case MPV_FORMAT_FLAG: {
                bool value = (*(int*)prop->data) != 0;
                if (strcmp(name, "pause") == 0) g_playbackStatus.isPaused = value;
                else if (strcmp(name, "core-idle") == 0) g_playbackStatus.isCoreIdle = value;
                else if (strcmp(name, "eof-reached") == 0) g_playbackStatus.eofReached = value;
                else if (strcmp(name, "mute") == 0) g_playbackStatus.isMuted = value;
                else if (strcmp(name, "seekable") == 0) g_playbackStatus.seekable = value;
                else if (strcmp(name, "seeking") == 0) g_playbackStatus.seeking = value;
                else if (strcmp(name, "sub-visibility") == 0) g_playbackStatus.g_subinfo.sub_Visible = value;
                break;
            }
            case MPV_FORMAT_INT64: {
                int64_t value = *(int64_t*)prop->data;
                if (strcmp(name, "volume") == 0) g_playbackStatus.volume = (int)value;
                else if (strcmp(name, "playlist-pos") == 0) g_playbackStatus.g_PlayingIndex = (int)value;
                else if (strcmp(name, "playlist-count") == 0) {
                    g_playbackStatus.g_playlist_count = (int)value;
                    if (g_playbackStatus.g_playlist_count >= 2 && playImmediately) {
                        playImmediately = false;
                        int newIndex = (int)g_playbackStatus.g_playlist.size() - 1;
                        auto* commander = MPVManager::GetInstance().GetDefaultSession()->GetCommander();
                        if (commander) commander->Exec("playlist-play-index " + std::to_string(newIndex));
                    }
                }
                else if (strcmp(name, "width") == 0) g_videoInfo.width = (int)value;
                else if (strcmp(name, "height") == 0) g_videoInfo.height = (int)value;
                break;
            }
            case MPV_FORMAT_DOUBLE: {
                double value = *(double*)prop->data;
                if (strcmp(name, "duration") == 0) g_playbackStatus.duration = value;
                else if (strcmp(name, "time-pos") == 0) { g_playbackStatus.timePos = value; g_playbackStatus.hasTime = true; }
                else if (strcmp(name, "playback-time") == 0) g_playbackStatus.playbackTime = value;
                else if (strcmp(name, "speed") == 0) g_playbackStatus.speed = value;
                else if (strcmp(name, "audio-delay") == 0) g_videoInfo.audio_delay = value;
                else if (strcmp(name, "demuxer-cache-duration") == 0) g_playbackStatus.demuxer_cache_duration = value;
                else if (strcmp(name, "demuxer-cache-time") == 0) g_playbackStatus.demuxer_cache_time = value;
                break;
            }
            case MPV_FORMAT_NODE: {
                const mpv_node* node = (const mpv_node*)prop->data;
                if (strcmp(name, "track-list") == 0) UpdateTrackList(node);
                else if (strcmp(name, "metadata") == 0) UpdateMetadata(node);
                else if (strcmp(name, "playlist") == 0) UpdatePlaylist(node);
                else if (strcmp(name, "chapter-list") == 0) UpdateChapterList(node);
                else if (strcmp(name, "video-params") == 0) UpdateVideoParams(node);
                else if (strcmp(name, "audio-params") == 0) UpdateAudioParams(node);
                else if (strcmp(name, "audio-device-list") == 0) UpdateAudioDeviceList(node);
                else if (strcmp(name, "af-metadata/ebur_measurer") == 0) UpdateLoudnessMetadata(node);
                break;
            }
            default: break;
            }
            break;
        }
        default:
            break;
        }
    }
}