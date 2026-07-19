#pragma once

#include <mpv/client.h>
#include <vector>
#include <string>

class MPVPlayer; // Forward declaration

class MPVObserver {
public:
    MPVObserver(MPVPlayer& player);

    void Init();
    void ProcessEvents();

private:
    void ObserveProps(const std::vector<std::pair<const char*, mpv_format>>& props, const char* groupName);
    void HandleMpvError(int err, const char* msgText);
    void HandlePropertyChange(mpv_event_property* prop);

    // Helpers for property updates
    void UpdateVideoParams(const mpv_node* node);
    void UpdateAudioParams(const mpv_node* node);
    void UpdateTrackList(const mpv_node* node);
    void UpdateAudioDeviceList(const mpv_node* node);
    void UpdateChapterList(const mpv_node* node);
    void UpdatePlaylist(const mpv_node* node);
    void UpdateMetadata(const mpv_node* node);
    void UpdateLoudnessMetadata(const mpv_node* node);

    MPVPlayer& m_player;
    mpv_handle* m_mpv;
};