#pragma once
#include <shared_mutex>
#include <mutex>
#include "mpv/MPVDataModels.h" // Thay thế mpv_data.h bằng models mới

struct FullMPVState {
    PlaybackModel playback;
    MediaModel media;
    VideoModel video;
    AudioModel audio;
    SubtitleModel subtitle;
    TrackModel track;
    PlaylistModel playlist;
    NetworkModel network;
};

// Lớp quản lý trạng thái MPV, thay thế cho các biến global trong mpv_data.h
class MPVStateSystem {
public:
     MPVStateSystem() = default;
    ~MPVStateSystem() = default;
    // Cung cấp truy cập chỉ đọc (read-only) vào dữ liệu.
    // Sử dụng shared_lock để cho phép nhiều luồng đọc cùng lúc.
    template<typename Func>
    void ReadPlayback(Func&& func) const {
        std::shared_lock lock(m_playbackMutex);
        func(m_playbackStatus);
    }

    template<typename Func>
    void ReadMedia(Func&& func) const {
        std::shared_lock lock(m_mediaMutex);
        func(m_media);
    }

    template<typename Func>
    void ReadVideo(Func&& func) const {
        std::shared_lock lock(m_videoMutex);
        func(m_video);
    }

    template<typename Func>
    void ReadAudio(Func&& func) const {
        std::shared_lock lock(m_audioMutex);
        func(m_audio);
    }

    template<typename Func>
    void ReadSubtitle(Func&& func) const {
        std::shared_lock lock(m_subtitleMutex);
        func(m_subtitle);
    }

    template<typename Func>
    void ReadTrack(Func&& func) const {
        std::shared_lock lock(m_trackMutex);
        func(m_track);
    }

    template<typename Func>
    void ReadPlaylist(Func&& func) const {
        std::shared_lock lock(m_playlistMutex);
        func(m_playlist);
    }

    template<typename Func>
    void ReadNetwork(Func&& func) const {
        std::shared_lock lock(m_networkMutex);
        func(m_network);
    }


    // Cung cấp truy cập ghi (write) vào dữ liệu.
    // Sử dụng unique_lock để đảm bảo chỉ một luồng được ghi tại một thời điểm.
    template<typename Func>
    void WritePlayback(Func&& func) {
        std::unique_lock lock(m_playbackMutex);
        func(m_playbackStatus);
    }

    template<typename Func>
    void WriteMedia(Func&& func) {
        std::unique_lock lock(m_mediaMutex);
        func(m_media);
    }

    template<typename Func>
    void WriteVideo(Func&& func) {
        std::unique_lock lock(m_videoMutex);
        func(m_video);
    }

    template<typename Func>
    void WriteAudio(Func&& func) {
        std::unique_lock lock(m_audioMutex);
        func(m_audio);
    }

    template<typename Func>
    void WriteSubtitle(Func&& func) {
        std::unique_lock lock(m_subtitleMutex);
        func(m_subtitle);
    }

    template<typename Func>
    void WriteTrack(Func&& func) {
        std::unique_lock lock(m_trackMutex);
        func(m_track);
    }

    template<typename Func>
    void WritePlaylist(Func&& func) {
        std::unique_lock lock(m_playlistMutex);
        func(m_playlist);
    }

    template<typename Func>
    void WriteNetwork(Func&& func) {
        std::unique_lock lock(m_networkMutex);
        func(m_network);
    }


    // --- CÁC HÀM TRUY CẬP RÚT GỌN ---

    // Lấy một bản sao của trạng thái playback
    MPVPlaybackStatus GetPlaybackStatus() const {
        std::shared_lock lock(m_playbackMutex);
        return m_playbackStatus;
    }

    // Lấy một bản sao của thông tin video
    VideoModel GetVideoInfo() const {
        std::shared_lock lock(m_videoMutex);
        return m_video;
    }

    // Lấy một bản sao của TOÀN BỘ trạng thái
    FullMPVState GetFullState() const {
        std::scoped_lock lock(m_playbackMutex, m_mediaMutex, m_videoMutex, m_audioMutex, 
                              m_subtitleMutex, m_trackMutex, m_playlistMutex, m_networkMutex);
        return {m_playbackStatus, m_media, m_video, m_audio, m_subtitle, m_track, m_playlist, m_network};
    }

private:
    MPVStateSystem(const MPVStateSystem&) = delete;
    MPVStateSystem& operator=(const MPVStateSystem&) = delete;

    // Dữ liệu được chia nhỏ và bảo vệ bởi các mutex riêng
    mutable std::shared_mutex m_playbackMutex;
    PlaybackModel m_playbackStatus;

    mutable std::shared_mutex m_mediaMutex;
    MediaModel m_media;

    mutable std::shared_mutex m_videoMutex;
    VideoModel m_video;

    mutable std::shared_mutex m_audioMutex;
    AudioModel m_audio;

    mutable std::shared_mutex m_subtitleMutex;
    SubtitleModel m_subtitle;

    mutable std::shared_mutex m_trackMutex;
    TrackModel m_track;

    mutable std::shared_mutex m_playlistMutex;
    PlaylistModel m_playlist;

    mutable std::shared_mutex m_networkMutex;
    NetworkModel m_network;

    // ... (sẽ thêm các subsystem khác như Audio, Track... ở đây)
};
