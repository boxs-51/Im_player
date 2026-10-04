#pragma once

#include <mpv/client.h>
#include <string>
#include <SDL_stdinc.h>
#include <vector>
#include <mutex>
#include <cstdint>

class Player; // Forward declaration
class PlayerStateSystem;

class PlaybackCommand {
public:
    PlaybackCommand(Player& player, PlayerStateSystem& state);

    // Raw Commands
    int Exec(const char** cmd);
    int Exec(const std::string& cmd);

    // Properties
    int SetPropertyString(const std::string& name, const std::string& value);
    int SetPropertyDouble(const std::string& name, double value);
    int SetPropertyFlag(const std::string& name, bool flag);

    // Basic Controls
    int LoadFile(const std::string& url, const std::string& extraFlags = "replace");
    int Play();
    int Pause();
    int SetMute(bool mute);
    int SetVolume(int volume);
    int SetSpeed(double speed);
    int SetAudioDelay(double delay);
    int Seek(float targetTime, float duration, const std::string& mode = "absolute");
    void Update(); // Called in main loop to handle pending commands
    int PlaylistNext();
    int PlaylistPrev();
    Uint64 GetStartupLoadId();
    bool RetryStartupLoadAfterEarlyFailure(Uint64 expectedLoadId);

private:
    int IssueLoadFile(const std::string& url, const std::string& extraFlags, bool retryAttempt);
    void ApplyPlaybackSettings();
    int DoSeek(float targetTime);
    
private:

    Player& m_player;
    PlayerStateSystem& m_state;

    std::mutex m_commandMutex;

    bool m_isSeekPending = false;
    float m_seekTargetTime = -1.0f;
    Uint64 m_lastSeekRequestTime = 0;

    std::string m_lastDirectLoadUrl;
    std::string m_lastDirectLoadFlags = "replace";
    Uint32 m_startupEarlyFailureRetries = 0;
    Uint64 m_startupKeepOpenOverrideLoadId = 0;
    bool m_startupKeepOpenOverrideActive = false;

    const Uint32 SEEK_DELAY_MS = 150;
    const Uint32 STARTUP_EARLY_FAILURE_RETRY_LIMIT = 1;
};