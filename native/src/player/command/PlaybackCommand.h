#pragma once

#include <mpv/client.h>
#include <string>
#include <SDL_stdinc.h>
#include <vector>

class Player; // Forward declaration

class PlaybackCommandDispatcher {
public:
    PlaybackCommandDispatcher(Player& player);

    // Raw Commands
    void Exec(const char** cmd);
    void Exec(const std::string& cmd);

    // Properties
    void SetPropertyString(const std::string& name, const std::string& value);
    void SetPropertyDouble(const std::string& name, double value);
    void SetPropertyFlag(const std::string& name, bool flag);

    // Basic Controls
    void LoadFile(const std::string& url, const std::string& extraFlags = "append-play");
    void Play();
    void Pause();
    void SetMute(bool mute);
    void SetVolume(int volume);
    void SetSpeed(double speed);
    void SetAudioDelay(double delay);
    void Seek(float targetTime, float duration, const std::string& mode = "absolute");
    void Update(); // Called in main loop to handle pending commands
    void PlaylistNext();
    void PlaylistPrev();

private:
    void DoSeek(float targetTime);

    Player& m_player;

    bool m_isSeekPending = false;
    float m_seekTargetTime = -1.0f;
    Uint64 m_lastSeekRequestTime = 0;
    const Uint32 SEEK_DELAY_MS = 150;
};