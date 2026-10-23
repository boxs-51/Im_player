#pragma once

// Issue #42 diagnostic-only main-demuxer cache state decoder.
// The pinned libmpv e470f8986e exports demuxer-cache-state/ts-per-stream;
// independent external-audio demuxers may not be represented here.
// Missing, duplicate, malformed or non-finite values stay UNKNOWN.
// No URL/metadata is retained and no source/HTTP reachability is inferred.

#include <mpv/client.h>
#include <cmath>
#include <cstring>

constexpr int MAX_CACHE_NODE_ENTRIES = 64;

struct MpvCacheStreamStatus {
    bool present = false;
    bool durationKnown = false;
    bool readerPtsKnown = false;
    bool cacheEndKnown = false;
    double durationSeconds = -1.0;
    double readerPtsSeconds = -1.0;
    double cacheEndSeconds = -1.0;
};

struct MpvCacheStreams {
    bool mainDemuxerCacheValid = false;
    MpvCacheStreamStatus audio;
    MpvCacheStreamStatus video;
};

inline const mpv_node* FindMpvCacheField(const mpv_node* node,
                                         const char* key) noexcept {
    if (!node || node->format != MPV_FORMAT_NODE_MAP || !node->u.list)
        return nullptr;
    const mpv_node_list* map = node->u.list;
    if (map->num < 0 || map->num > MAX_CACHE_NODE_ENTRIES ||
        (map->num && (!map->values || !map->keys)))
        return nullptr;
    const mpv_node* found = nullptr;
    for (int i = 0; i < map->num; ++i) {
        if (!map->keys[i])
            return nullptr;
        if (std::strcmp(map->keys[i], key) == 0) {
            // Duplicate keys make the typed value ambiguous.
            if (found)
                return nullptr;
            found = &map->values[i];
        }
    }
    return found;
}

inline void ReadMpvCacheNumber(const mpv_node* entry, const char* key,
                               bool requireNonnegative, bool& known,
                               double& value) noexcept {
    const mpv_node* field = FindMpvCacheField(entry, key);
    if (!field || field->format != MPV_FORMAT_DOUBLE ||
        !std::isfinite(field->u.double_) ||
        (requireNonnegative && field->u.double_ < 0))
        return;
    known = true;
    value = field->u.double_;
}

inline MpvCacheStreams ParseMpvCacheStreams(const mpv_node* node) noexcept {
    MpvCacheStreams out{};
    const mpv_node* streams = FindMpvCacheField(node, "ts-per-stream");
    if (!streams || streams->format != MPV_FORMAT_NODE_ARRAY ||
        !streams->u.list)
        return out;
    const mpv_node_list* array = streams->u.list;
    if (array->num < 0 || array->num > MAX_CACHE_NODE_ENTRIES ||
        (array->num && !array->values))
        return out;
    out.mainDemuxerCacheValid = true;
    bool seenAudio = false, seenVideo = false;
    bool ambiguousAudio = false, ambiguousVideo = false;
    for (int i = 0; i < array->num; ++i) {
        const mpv_node* entry = &array->values[i];
        const mpv_node* type = FindMpvCacheField(entry, "type");
        if (!type || type->format != MPV_FORMAT_STRING || !type->u.string)
            continue;
        const bool isAudio = std::strcmp(type->u.string, "audio") == 0;
        const bool isVideo = std::strcmp(type->u.string, "video") == 0;
        if (!isAudio && !isVideo)
            continue;
        bool& seen = isAudio ? seenAudio : seenVideo;
        bool& ambiguous = isAudio ? ambiguousAudio : ambiguousVideo;
        MpvCacheStreamStatus& target = isAudio ? out.audio : out.video;
        if (seen) {
            ambiguous = true;
            target = MpvCacheStreamStatus{};
            continue;
        }
        seen = true;
        if (ambiguous)
            continue;
        target.present = true;
        ReadMpvCacheNumber(entry, "cache-duration", true,
                           target.durationKnown, target.durationSeconds);
        ReadMpvCacheNumber(entry, "reader-pts", false,
                           target.readerPtsKnown, target.readerPtsSeconds);
        ReadMpvCacheNumber(entry, "cache-end", false,
                           target.cacheEndKnown, target.cacheEndSeconds);
    }
    return out;
}
