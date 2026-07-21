#include "mpv_data.h"
#include "mpv_basic_formats.h"
#include "mpv/MPVStateSystem.h"
#include "mpv/session/MPVManager.h"

#include <globals.h>
#include <log.h>
#include <mutex>
#include <memory>

static MPVPlaybackStatus g_playbackStatus;
static VideoInfo g_videoInfo;

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
   // MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
    // Sử dụng GetMPVPlaybackStatus() để đảm bảo g_playbackStatus được đồng bộ trước khi kiểm tra
    MPVPlaybackStatus& status = GetMPVPlaybackStatus();
    if (g_playbackStatus.isLoadingMedia )               return PlaybackState::Loading;
    if (g_playbackStatus.idle_active)                   return PlaybackState::Idle;
    if (g_playbackStatus.eofReached && 
        !g_playbackStatus.isSeeking)                    return PlaybackState::EndOfFile;
    if (g_playbackStatus.isSeeking)                     return PlaybackState::Seeking;
    if (g_playbackStatus.isPaused)                      return PlaybackState::Paused;
    return PlaybackState::Playing;
}
MPVPlaybackStatus& GetMPVPlaybackStatus(){
    
    // Lấy con trỏ tới MPVStateSystem từ session mặc định
    auto* session = MPVManager::GetInstance().GetDefaultSession();
    if (session) {
        auto& state = *session->GetState();

        // Đọc và đồng bộ dữ liệu từ các Model vào g_playbackStatus
        state.ReadPlayback([&](const PlaybackModel& m) {
            g_playbackStatus.isPaused = m.flags.isPaused;
            g_playbackStatus.seeking = m.flags.isSeeking; // Cập nhật cả trường seeking cũ
            g_playbackStatus.isSeeking = m.flags.isSeeking;
            g_playbackStatus.isLoadingMedia = m.isLoadingMedia;
            g_playbackStatus.eofReached = m.flags.eofReached;
            g_playbackStatus.isCoreIdle = m.flags.isCoreIdle;
            g_playbackStatus.ilde = m.flags.isIdleActive; // Cập nhật cả trường ilde cũ
            g_playbackStatus.idle_active = m.flags.isIdleActive;
            g_playbackStatus.hasFile = m.flags.hasFile;
            g_playbackStatus.seekable = m.flags.seekable;
            g_playbackStatus.hasTime = m.timing.timePos > 0.0;
            g_playbackStatus.timePos = m.timing.timePos;
            g_playbackStatus.duration = m.timing.duration;
            g_playbackStatus.playbackTime = m.timing.playbackTime;
            g_playbackStatus.percent_pos = m.timing.percent_pos;
            g_playbackStatus.time_remaining = m.timing.time_remaining;
            g_playbackStatus.speed = m.config.speed;
            g_playbackStatus.loopMode = m.config.loopMode;
        });

        state.ReadMedia([&](const MediaModel& m) {
            g_playbackStatus.Title = m.title;
            g_playbackStatus.mediaTitle = m.mediaTitle;
            g_playbackStatus.filename = m.filename;
            g_playbackStatus.fileFormat = m.fileFormat;
            g_playbackStatus.working_directory = m.working_directory;
            g_playbackStatus.streamUrl = m.streamUrl;
            g_playbackStatus.stream_path = m.stream_path;
        });

        state.ReadAudio([&](const AudioModel& m) {
            g_playbackStatus.volume = m.volume.volume;
            g_playbackStatus.isMuted = m.volume.isMuted;
            g_playbackStatus.audio_client_name = m.device.audio_client_name;
            g_playbackStatus.g_audioDevices = m.device.audioDevices;
        });

        state.ReadSubtitle([&](const SubtitleModel& m) {
            g_playbackStatus.g_subinfo.sub_Visible = m.sub_Visible;
            g_playbackStatus.g_subinfo.sub_scale = m.sub_scale;
            g_playbackStatus.g_subinfo.sub_Delay = m.sub_Delay;
            g_playbackStatus.g_subinfo.sub_codec = m.sub_codec;
            g_playbackStatus.g_subinfo.sub_ass_override = m.sub_ass_override;
        });

        state.ReadPlaylist([&](const PlaylistModel& m) {
            g_playbackStatus.g_PlayingIndex = m.g_PlayingIndex;
            g_playbackStatus.g_PlayingIndex_1 = m.g_PlayingIndex_1;
            g_playbackStatus.g_playlist_count = m.g_playlist_count;
            g_playbackStatus.g_playlist = m.playlist;
        });

        state.ReadNetwork([&](const NetworkModel& m) {
            g_playbackStatus.demuxer_via_network = m.demuxer_via_network;
            g_playbackStatus.stream_pos = (int)m.stream_pos;
            g_playbackStatus.cache_buffering_state = m.cache_buffering_state;
            g_playbackStatus.stream_pos = m.stream_pos;
            g_playbackStatus.demuxer_bitrate = m.demuxer_bitrate;
            g_playbackStatus.demuxer_cache_duration = m.demuxer_cache_duration;
            g_playbackStatus.demuxer_cache_time = m.demuxer_cache_time;
            g_playbackStatus.audio_buffer = m.audio_buffer;
        });
    }
    return g_playbackStatus;
}

VideoInfo& GetVideoInfo(){
    
    // Lấy con trỏ tới MPVStateSystem từ session mặc định
    auto* session = MPVManager::GetInstance().GetDefaultSession();
    if (session) {
        auto& state = *session->GetState();

        // Đọc và đồng bộ dữ liệu từ các Model vào g_videoInfo
        state.ReadVideo([&](const VideoModel& m) {
            g_videoInfo.width = m.dimensions.width;
            g_videoInfo.height = m.dimensions.height;
            g_videoInfo.rotate = m.dimensions.rotate; // 'video-rotate' đã được đổi tên từ 'video-rotation'
            g_videoInfo.vbitrate = m.stats.vbitrate;
            g_videoInfo.aspect = m.dimensions.aspect;
            g_videoInfo.estimated_vf_fps_mpv = m.stats.estimated_vf_fps_mpv;
            g_videoInfo.currentFPS = m.stats.currentFPS;
            g_videoInfo.video_format = m.codec.video_format;
            g_videoInfo.vcodec = m.codec.vcodec;
            g_videoInfo.v_out = m.codec.v_out;
            g_videoInfo.hwdec = m.codec.hwdec;

            // Đồng bộ VideoParams
            g_videoInfo.g_videoparams.vwidth = m.dimensions.width;
            g_videoInfo.g_videoparams.vheight = m.dimensions.height;
            g_videoInfo.g_videoparams.vrotate = m.dimensions.rotate;
            g_videoInfo.g_videoparams.vaspect = m.dimensions.aspect;
            g_videoInfo.g_videoparams.vpixfmt = m.params.vpixfmt;
            g_videoInfo.g_videoparams.vprimaries = m.params.vprimaries;
            g_videoInfo.g_videoparams.vgamma = m.params.vgamma;
            g_videoInfo.g_videoparams.vcolormatrix = m.params.vcolormatrix;
            g_videoInfo.g_videoparams.vcolorlevels = m.params.vcolorlevels;
            g_videoInfo.g_videoparams.vstereo_in = m.params.vstereo_in;
            g_videoInfo.g_videoparams.vchroma_location = m.params.vchroma_location;
            g_videoInfo.g_videoparams.vaspect_name = m.params.vaspect_name;
            g_videoInfo.g_videoparams.vlight = m.params.vlight;
            g_videoInfo.g_videoparams.vsar_name = m.params.vsar_name;
            g_videoInfo.g_videoparams.average_bpp = m.params.average_bpp;
            g_videoInfo.g_videoparams.vpar = m.params.vpar;
            g_videoInfo.g_videoparams.vsar = m.params.vsar;
            g_videoInfo.g_videoparams.vsig_peak = m.params.vsig_peak;
        });

        state.ReadAudio([&](const AudioModel& m) {
            g_videoInfo.acodec = m.codec.acodec;
            g_videoInfo.audio_delay = m.codec.audio_delay;
            g_videoInfo.abitrate = m.codec.abitrate;
            g_videoInfo.a_out = m.codec.a_out;
            g_videoInfo.achannels = m.params.channel_count;
            g_videoInfo.asamplerate = m.params.asamplerate;
            g_videoInfo.a_filter = m.codec.a_filter;
            g_videoInfo.audio_device = m.device.audio_device;

            // Đồng bộ AudioParams
            g_videoInfo.g_audioarams.aformat = m.params.aformat;
            g_videoInfo.g_audioarams.ahr_channels = m.params.ahr_channels;
            g_videoInfo.g_audioarams.achannels_str = m.params.achannels_str;
            g_videoInfo.g_audioarams.channel_count = m.params.channel_count;
            g_videoInfo.g_audioarams.asamplerate = m.params.asamplerate;
            g_videoInfo.g_audioarams.loudness_momentary = m.loudness.loudness_momentary;
            g_videoInfo.g_audioarams.loudness_shortterm = m.loudness.loudness_shortterm;
            g_videoInfo.g_audioarams.loudness_integrated = m.loudness.loudness_integrated;
            g_videoInfo.g_audioarams.loudness_range = m.loudness.loudness_range;
            g_videoInfo.g_audioarams.loudness_lra_low = m.loudness.loudness_lra_low;
            g_videoInfo.g_audioarams.loudness_lra_high = m.loudness.loudness_lra_high;
            g_videoInfo.g_audioarams.true_peak = m.loudness.true_peak;
            g_videoInfo.g_audioarams.true_peak_ch0 = m.loudness.true_peak_ch0;
            g_videoInfo.g_audioarams.true_peak_ch1 = m.loudness.true_peak_ch1;
            g_videoInfo.g_audioarams.sample_peak = m.loudness.sample_peak;
            g_videoInfo.g_audioarams.sample_peak_ch0 = m.loudness.sample_peak_ch0;
            g_videoInfo.g_audioarams.sample_peak_ch1 = m.loudness.sample_peak_ch1;
        });

        state.ReadTrack([&](const TrackModel& m) {
            g_videoInfo.g_current_chapter = m.g_current_chapter;
            g_videoInfo.g_tracks = m.tracks;
            g_videoInfo.g_chapters = m.chapters;
        });

        state.ReadMedia([&](const MediaModel& m) {
            g_videoInfo.metadata = m.metadata;
        });

        state.ReadSubtitle([&](const SubtitleModel& m) {
            g_videoInfo.hasSubtitles = m.hasSubtitles;
        });
    }
    return g_videoInfo;
}
