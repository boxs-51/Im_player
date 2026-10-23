// Issue #42 real typed mpv_node parsing, no network/media/SDL.
#include "player/event/MpvCacheStreamTelemetry.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
void str(mpv_node& n, const char* value) {
    n.format = MPV_FORMAT_STRING;
    n.u.string = const_cast<char*>(value);
}
void num(mpv_node& n, double value) {
    n.format = MPV_FORMAT_DOUBLE;
    n.u.double_ = value;
}
struct Fixture {
    mpv_node audioFields[4]{};
    mpv_node videoFields[4]{};
    char* audioKeys[4]{const_cast<char*>("type"),
        const_cast<char*>("cache-duration"),
        const_cast<char*>("reader-pts"),
        const_cast<char*>("cache-end")};
    char* videoKeys[4]{const_cast<char*>("type"),
        const_cast<char*>("cache-duration"),
        const_cast<char*>("reader-pts"),
        const_cast<char*>("cache-end")};
    mpv_node_list audioMap{};
    mpv_node_list videoMap{};
    mpv_node streamValues[2]{};
    mpv_node_list streamArray{};
    mpv_node rootFields[1]{};
    char* rootKeys[1]{const_cast<char*>("ts-per-stream")};
    mpv_node_list rootMap{};
    mpv_node root{};
    Fixture() {
        str(audioFields[0], "audio");
        num(audioFields[1], 0.0);
        num(audioFields[2], -1.5); // negative PTS can be valid.
        num(audioFields[3], 5.0);
        str(videoFields[0], "video");
        num(videoFields[1], 4.5);
        num(videoFields[2], 15.0);
        num(videoFields[3], 19.5);
        audioMap = {4, audioFields, audioKeys};
        videoMap = {4, videoFields, videoKeys};
        streamValues[0].format = MPV_FORMAT_NODE_MAP;
        streamValues[0].u.list = &audioMap;
        streamValues[1].format = MPV_FORMAT_NODE_MAP;
        streamValues[1].u.list = &videoMap;
        streamArray = {2, streamValues, nullptr};
        rootFields[0].format = MPV_FORMAT_NODE_ARRAY;
        rootFields[0].u.list = &streamArray;
        rootMap = {1, rootFields, rootKeys};
        root.format = MPV_FORMAT_NODE_MAP;
        root.u.list = &rootMap;
    }
};
bool verify(bool success, const char* label) {
    if (!success) std::fprintf(stderr, "[F02] FAILED %s\n", label);
    return success;
}
}

int main() {
    bool all = true;
    {
        Fixture f;
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(r.mainDemuxerCacheValid, "root recognized");
        all &= verify(r.audio.present && r.video.present, "named audio/video");
        all &= verify(r.audio.durationKnown &&
                      r.audio.durationSeconds == 0.0, "zero duration is known");
        all &= verify(r.video.durationKnown &&
                      r.video.durationSeconds == 4.5, "video duration independent");
        all &= verify(r.audio.readerPtsKnown &&
                      r.audio.readerPtsSeconds == -1.5, "negative valid PTS");
        all &= verify(r.audio.cacheEndKnown &&
                      r.audio.cacheEndSeconds == 5.0, "audio end parsed");
    }
    {
        Fixture f;
        f.audioFields[1].format = MPV_FORMAT_NONE;
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(r.audio.present && !r.audio.durationKnown,
                      "missing duration is UNKNOWN");
        all &= verify(r.video.durationKnown, "video survives audio missing field");
    }
    {
        Fixture f;
        num(f.audioFields[1], std::numeric_limits<double>::quiet_NaN());
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.audio.durationKnown, "NaN duration rejected");
    }
    {
        Fixture f;
        num(f.audioFields[1], -5.0);
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.audio.durationKnown, "negative duration rejected");
    }
    {
        Fixture f;
        f.videoFields[0].u.string = const_cast<char*>("audio");
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.audio.present && !r.video.present,
                      "duplicate audio type is ambiguous");
    }
    {
        Fixture f;
        f.rootMap.num = MAX_CACHE_NODE_ENTRIES + 1;
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.mainDemuxerCacheValid, "oversize root map rejected");
    }
    {
        Fixture f;
        f.rootFields[0].u.list = nullptr;
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.mainDemuxerCacheValid, "null stream array rejected");
    }
    {
        Fixture f;
        f.audioFields[0].format = MPV_FORMAT_INT64;
        const auto r = ParseMpvCacheStreams(&f.root);
        all &= verify(!r.audio.present && r.video.present,
                      "wrong type field ignored");
    }
    {
        const auto r = ParseMpvCacheStreams(nullptr);
        all &= verify(!r.mainDemuxerCacheValid &&
                      !r.audio.durationKnown && !r.video.durationKnown,
                      "null node is unknown");
    }
    if (all) std::puts("ISSUE42_MPV_CACHE_STREAMS_NATIVE PASS; media=NOT_RUN");
    return all ? 0 : 1;
}
