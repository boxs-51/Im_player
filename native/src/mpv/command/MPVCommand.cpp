#include "MPVCommand.h"
#include "mpv/player/MPVPlayer.h"
#include <SDL.h>
#include <algorithm>
#include <string>

MPVCommandDispatcher::MPVCommandDispatcher(MPVPlayer& player) : m_player(player) {}

void MPVCommandDispatcher::Exec(const char** cmd) {
    if (m_player.GetHandle()) {
        mpv_command(m_player.GetHandle(), cmd);
    }
}

void MPVCommandDispatcher::Exec(const std::string& cmd) {
    if (m_player.GetHandle()) {
        mpv_command_string(m_player.GetHandle(), cmd.c_str());
    }
}

void MPVCommandDispatcher::SetPropertyString(const std::string& name, const std::string& value) {
    if (m_player.GetHandle()) {
        mpv_set_property_string(m_player.GetHandle(), name.c_str(), value.c_str());
    }
}

void MPVCommandDispatcher::SetPropertyDouble(const std::string& name, double value) {
    if (m_player.GetHandle()) {
        mpv_set_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_DOUBLE, &value);
    }
}

void MPVCommandDispatcher::SetPropertyFlag(const std::string& name, bool flag) {
    if (m_player.GetHandle()) {
        int val = flag ? 1 : 0;
        mpv_set_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_FLAG, &val);
    }
}

void MPVCommandDispatcher::LoadFile(const std::string& url, const std::string& extraFlags) {
    const char* cmd[] = { "loadfile", url.c_str(), extraFlags.c_str(), nullptr };
    Exec(cmd);
}

void MPVCommandDispatcher::Play() { SetPropertyFlag("pause", false); }
void MPVCommandDispatcher::Pause() { SetPropertyFlag("pause", true); }
void MPVCommandDispatcher::SetMute(bool mute) { SetPropertyFlag("mute", mute); }
void MPVCommandDispatcher::SetVolume(int volume) { SetPropertyDouble("volume", std::clamp(volume, 0, 130)); }
void MPVCommandDispatcher::SetSpeed(double speed) { SetPropertyDouble("speed", speed); }
void MPVCommandDispatcher::SetAudioDelay(double delay) { SetPropertyDouble("audio-delay", delay); }

void MPVCommandDispatcher::DoSeek(float targetTime) {
    if (!m_player.GetHandle()) return;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", targetTime);
    const char* cmd[] = { "seek", buffer, "absolute", nullptr };
    Exec(cmd);
    m_lastSeekRequestTime = SDL_GetTicks64();
    m_isSeekPending = false;
}

void MPVCommandDispatcher::Seek(float targetTime, float duration, const std::string& mode) {
    if (!m_player.GetHandle() || duration <= 0.0f) return;
    targetTime = std::clamp(targetTime, 0.0f, std::max(duration - 0.05f, 0.0f));
    
    if (SDL_GetTicks64() - m_lastSeekRequestTime < SEEK_DELAY_MS) {
        m_seekTargetTime = targetTime;
        m_isSeekPending = true;
    } else {
        DoSeek(targetTime);
    }
}

void MPVCommandDispatcher::Update() {
    if (m_isSeekPending && (SDL_GetTicks64() - m_lastSeekRequestTime >= SEEK_DELAY_MS)) {
        DoSeek(m_seekTargetTime);
    }
}

void MPVCommandDispatcher::PlaylistNext() {
    const char* cmd[] = {"playlist-next", nullptr};
    Exec(cmd);
}
void MPVCommandDispatcher::PlaylistPrev() {
    const char* cmd[] = {"playlist-prev", nullptr};
    Exec(cmd);
}