#pragma once

#include <mpv/client.h>
#include <vector>
#include <string>

class Player; // Forward declaration
class PlayerStateSystem;
class PlaybackCommandDispatcher;

struct VideoInfoResult;
struct ResolutionOption;
struct FormatGroup;
struct MediaFormatsModel;

class PlaybackObserver {
public:
    PlaybackObserver(Player& player, PlayerStateSystem& state, PlaybackCommandDispatcher& commander);

    void Init();
    void ProcessEvents();

private:
    void ObserveProps(const std::vector<std::pair<const char*, mpv_format>>& props, const char* groupName);
    void HandleMpvError(int err, const char* msgText);

    // --- YTDL Format Parsing & Building Helpers ---
    void HandleYTDLLog(const std::string& text);
    void BuildAllFormats(const VideoInfoResult& info);
    void BuildVideoOptions(const std::vector<ResolutionOption>& videoFormats, FormatGroup& videoGroup);
    void BuildAudioOptions(const std::vector<ResolutionOption>& audioFormats, FormatGroup& audioGroup);
    std::string BuildCombinedFormat(const MediaFormatsModel& allFormats);
    void UpdateVideoTypeInState(const VideoInfoResult& info);

    // Tách logic xử lý sự kiện thay đổi thuộc tính
    void HandlePropertyChange(mpv_event_property* prop);
    void HandleStringProperty(const char* name, const char* value);
    void HandleFlagProperty(const char* name, bool value);
    void HandleInt64Property(const char* name, int64_t value);
    void HandleDoubleProperty(const char* name, double value);
    void HandleNodeProperty(const char* name, const mpv_node* node);
    void HandlePlaybackState();

    // Helpers for property updates
    void UpdateVideoParams(const mpv_node* node);
    void UpdateAudioParams(const mpv_node* node);
    void UpdateTrackList(const mpv_node* node);
    void UpdateAudioDeviceList(const mpv_node* node);
    void UpdateChapterList(const mpv_node* node);
    void UpdatePlaylist(const mpv_node* node);
    void UpdateMetadata(const mpv_node* node);
    void UpdateLoudnessMetadata(const mpv_node* node);

private:

    Player& m_player;
    PlayerStateSystem& m_state;
    PlaybackCommandDispatcher& m_commander;
    mpv_handle* m_mpv;
};