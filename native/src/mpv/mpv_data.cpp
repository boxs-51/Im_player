#include "mpv_data.h"
#include "mpv_basic_formats.h"

#include <globals.h>
#include <log.h>
#include <mutex>


static MPVPlaybackStatus g_playbackStatus;
static VideoInfo g_videoInfo;
static std::mutex g_retry_mutex;
static std::unordered_map<int, int> g_retryCount; // key = g_PlayingIndex, value = số lần retry
static constexpr int MAX_RETRY = 1;               // số lần thử tối đa

void PrintMPVNode(const mpv_node* node, int indent = 0) {
    /*
    mpv_node result;
    if (mpv_get_property(mpv, "config", MPV_FORMAT_NODE, &result) >= 0) {
        std::cout << "[DEBUG] Dump config:\n";
        PrintMPVNode(&result);
        std::cout << std::endl;
        mpv_free_node_contents(&result);
    }
    */
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
    if (g_playbackStatus.isLoadingMedia )               return PlaybackState::Loading;
    if (g_playbackStatus.ilde)                          return PlaybackState::Idle;
    if (g_playbackStatus.eofReached && 
        !g_playbackStatus.isSeeking)                    return PlaybackState::EndOfFile;
    if (g_playbackStatus.isSeeking)                     return PlaybackState::Seeking;
    if (g_playbackStatus.isPaused)                      return PlaybackState::Paused;
    return PlaybackState::Playing;
}
MPVPlaybackStatus& GetMPVPlaybackStatus(){
    return g_playbackStatus;
}
VideoInfo& GetVideoInfo(){
    return g_videoInfo;
}
