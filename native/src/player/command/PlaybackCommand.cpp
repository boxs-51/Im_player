#include "player/command/PlaybackCommand.h"
#include "player/player/Player.h"

#include "settings_manager.h"
#include <SDL.h>
#include <algorithm>
#include <string>

PlaybackCommand::PlaybackCommand(Player& player) : m_player(player) {}

int PlaybackCommand::Exec(const char** cmd) {
    if (m_player.GetHandle()) {
        return mpv_command(m_player.GetHandle(), cmd);
    }
    return -1;
}

int PlaybackCommand::Exec(const std::string& cmd) {
    if (m_player.GetHandle()) {
        return mpv_command_string(m_player.GetHandle(), cmd.c_str());
    }
    return -1;
}

int  PlaybackCommand::SetPropertyString(const std::string& name, const std::string& value) {
    if (m_player.GetHandle()) {
        return mpv_set_property_string(m_player.GetHandle(), name.c_str(), value.c_str());
    }
    return -1;
}

int PlaybackCommand::SetPropertyDouble(const std::string& name, double value) {
    if (m_player.GetHandle()) {
        return mpv_set_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_DOUBLE, &value);
    }
    return -1;
}

int PlaybackCommand::SetPropertyFlag(const std::string& name, bool flag) {
    if (m_player.GetHandle()) {
        int val = flag ? 1 : 0;
        return mpv_set_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_FLAG, &val);
    }
    return -1;
}

int PlaybackCommand::LoadFile(const std::string& url, const std::string& extraFlags) {
    PlaybackCommand::ApplyPlaybackSettings();
    const char* cmd[] = { "loadfile", url.c_str(), extraFlags.c_str(), nullptr };
    return Exec(cmd);
}

int PlaybackCommand::Play() { return SetPropertyFlag("pause", false); }
int PlaybackCommand::Pause() { return SetPropertyFlag("pause", true); }
int PlaybackCommand::SetMute(bool mute) { return SetPropertyFlag("mute", mute); }
int PlaybackCommand::SetVolume(int volume) { return SetPropertyDouble("volume", std::clamp(volume, 0, 130)); }
int PlaybackCommand::SetSpeed(double speed) { return SetPropertyDouble("speed", speed); }
int PlaybackCommand::SetAudioDelay(double delay) { return SetPropertyDouble("audio-delay", delay); }

int PlaybackCommand::DoSeek(float targetTime) {
    if (!m_player.GetHandle()) return -1 ;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", targetTime);
    const char* cmd[] = { "seek", buffer, "absolute", nullptr };
    int ret = Exec(cmd);
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_lastSeekRequestTime = SDL_GetTicks64();
        m_isSeekPending = false;
    }
    return ret;
}

int PlaybackCommand::Seek(float targetTime, float duration, const std::string& mode) {
    if (!m_player.GetHandle() || duration <= 0.0f) return - 1;
    targetTime = std::clamp(targetTime, 0.0f, std::max(duration - 0.05f, 0.0f));
    
    Uint64 now = SDL_GetTicks64();
    Uint64 lastSeekRequestTime = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        lastSeekRequestTime = m_lastSeekRequestTime;
    }
    if (now - lastSeekRequestTime < SEEK_DELAY_MS) {
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_seekTargetTime = targetTime;
        m_isSeekPending = true;
    }

    } else {
        return DoSeek(targetTime);
    }

    return 0;
}

void PlaybackCommand::Update() {
    float seekTargetTime = 0.0f;
    bool isSeekPending = false;
    bool lastSeekRequestTime = false;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        seekTargetTime = m_seekTargetTime;
        isSeekPending = m_isSeekPending;
        lastSeekRequestTime = m_lastSeekRequestTime;
    }
    
    if (isSeekPending && (SDL_GetTicks64() - lastSeekRequestTime >= SEEK_DELAY_MS)) {
        DoSeek(seekTargetTime);
    }
}

int PlaybackCommand::PlaylistNext() {
    const char* cmd[] = {"playlist-next", nullptr};
    return Exec(cmd);
}
int PlaybackCommand::PlaylistPrev() {
    const char* cmd[] = {"playlist-prev", nullptr};
    return Exec(cmd);
}

void PlaybackCommand::ApplyPlaybackSettings() {

    auto& Cfg = ConfigManager::Instance();
    auto videoCfg = Cfg.GetVideoSettings();

    SetVolume(videoCfg.defaultVolume);

    double speed = (double)videoCfg.playbackSpeed;
    SetSpeed(speed);

    double audiodelay = (double)videoCfg.audiodelay;
    SetAudioDelay(audiodelay);

    const char* sub_vis = videoCfg.enableSubtitles ? "yes" : "no";
    SetPropertyString("sub-visibility", sub_vis);

    const char* repeat_mode = videoCfg.repeatVideo ? "inf" : "no";
    SetPropertyString("loop-file", repeat_mode);

    const char* auto_next_mode = videoCfg.autoPlayNext ? "yes" : "no";
    SetPropertyString("playlist-auto-advance", auto_next_mode);

    const char* loop_list = videoCfg.repeatlist ? "force" : "no";
    SetPropertyString("loop-playlist", loop_list);

    const char* chosenFormat = videoCfg.selectedResolution.c_str();
    SetPropertyString("ytdl-format", chosenFormat);
}