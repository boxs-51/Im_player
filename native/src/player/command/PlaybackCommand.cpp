#include "player/command/PlaybackCommand.h"
#include "player/player/Player.h"
#include "player/PlayerStateSystem.h"
#include "player/PlayerDataModels.h"

#include "settings_manager.h"
#include <SDL.h>
#include <algorithm>
#include <string>
#include <log.h>
#include "common/LifecycleEvidence.h"

PlaybackCommand::PlaybackCommand(Player& player, PlayerStateSystem& state) : m_player(player), m_state(state) {}

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
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_lastDirectLoadUrl = url;
        m_lastDirectLoadFlags = extraFlags;
        m_startupEarlyFailureRetries = 0;
    }
    return IssueLoadFile(url, extraFlags, false);
}

int PlaybackCommand::IssueLoadFile(
    const std::string& url,
    const std::string& extraFlags,
    bool retryAttempt) {
    Uint64 loadId = 0;
    Uint32 retryCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        retryCount = m_startupEarlyFailureRetries;
    }

    m_state.WritePlayback([&](PlaybackModel& m) {
        // A direct new-media transaction must never inherit a format-switch
        // seek armed for the previous media item.
        m.pendingseektime = -1.0;
        // Source classification belongs to this load transaction. Never
        // inherit Vio/Live/Local from the previous media item.
        m.videoType = VideoType::None;
        m.startupRestartCount = 0;
        if (!retryAttempt) {
            m.startupLoadId += 1;
        }
        m.startupLoadedLoadId = 0;
        m.startupVideoEvidenceLoadId = 0;
        m.startupVideoEvidenceArmed = false;
        m.startupMediaStartedLoadId = 0;
        loadId = m.startupLoadId;
        m.isLoadingMedia = true;
    });

    const Uint64 now = SDL_GetTicks64();
    if (retryAttempt) {
        LifecycleEvidence::EmitDiagnostic(
            "STARTUP",
            FormatString(
                "event=LOAD_RETRY_REQUEST load_id=%llu ts_ms=%llu attempt=%u reason=early_terminal_before_first_frame flags=%s url=%s",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(now),
                static_cast<unsigned int>(retryCount + 1),
                extraFlags.c_str(),
                url.c_str()));
    } else {
        LOG(1, LogLevel::Info, LogCategory::System,
            "[STARTUP] event=LOAD_REQUEST load_id=%llu ts_ms=%llu flags=%s url=%s",
            static_cast<unsigned long long>(loadId),
            static_cast<unsigned long long>(now),
            extraFlags.c_str(),
            url.c_str());

        LifecycleEvidence::EmitDiagnostic(
            "STARTUP",
            FormatString(
                "event=LOAD_REQUEST load_id=%llu ts_ms=%llu flags=%s url=%s",
                static_cast<unsigned long long>(loadId),
                static_cast<unsigned long long>(now),
                extraFlags.c_str(),
                url.c_str()));
    }

    PlaybackCommand::ApplyPlaybackSettings();

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_startupKeepOpenOverrideLoadId = loadId;
        m_startupKeepOpenOverrideActive = true;
    }

    // #33: keep the historical global keep-open=yes contract, but disable it
    // only for the startup window of this file so an early terminal condition
    // cannot be turned into an implicit last-frame seek. mpv restores per-file
    // options automatically when playback ends.
    const char* cmd[] = {
        "loadfile",
        url.c_str(),
        extraFlags.c_str(),
        "-1",
        "keep-open=no",
        nullptr
    };
    const int ret = Exec(cmd);

    LifecycleEvidence::EmitDiagnostic(
        "STARTUP",
        FormatString(
            "event=LOAD_COMMAND_RESULT load_id=%llu ts_ms=%llu attempt=%u retry=%d result=%d",
            static_cast<unsigned long long>(loadId),
            static_cast<unsigned long long>(SDL_GetTicks64()),
            static_cast<unsigned int>(retryCount + 1),
            retryAttempt ? 1 : 0,
            ret));

    if (ret < 0) {
        m_state.WritePlayback([](PlaybackModel& m) {
            m.isLoadingMedia = false;
        });
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (m_startupKeepOpenOverrideLoadId == loadId) {
            m_startupKeepOpenOverrideActive = false;
        }
    }
    return ret;
}

bool PlaybackCommand::RetryStartupLoadAfterEarlyFailure(Uint64 expectedLoadId) {
    if (GetStartupLoadId() != expectedLoadId) {
        return false;
    }

    std::string retryUrl;
    std::string retryFlags;
    Uint32 retryCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (m_lastDirectLoadUrl.empty()
            || m_startupEarlyFailureRetries >= STARTUP_EARLY_FAILURE_RETRY_LIMIT) {
            return false;
        }
        ++m_startupEarlyFailureRetries;
        retryCount = m_startupEarlyFailureRetries;
        retryUrl = m_lastDirectLoadUrl;
        retryFlags = m_lastDirectLoadFlags;
    }

    LifecycleEvidence::EmitDiagnostic(
        "STARTUP",
        FormatString(
            "event=EARLY_TERMINAL_RETRY_DISPATCH load_id=%llu ts_ms=%llu retry=%u limit=%u",
            static_cast<unsigned long long>(expectedLoadId),
            static_cast<unsigned long long>(SDL_GetTicks64()),
            static_cast<unsigned int>(retryCount),
            static_cast<unsigned int>(STARTUP_EARLY_FAILURE_RETRY_LIMIT)));

    return IssueLoadFile(retryUrl, retryFlags, true) >= 0;
}

Uint64 PlaybackCommand::GetStartupLoadId() {
    Uint64 loadId = 0;
    m_state.ReadPlayback([&](PlaybackModel const& m) {
        loadId = static_cast<Uint64>(m.startupLoadId);
    });
    return loadId;
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

    const Uint64 loadId = GetStartupLoadId();
    LifecycleEvidence::EmitDiagnostic(
        "STARTUP",
        FormatString(
            "event=APP_SEEK_COMMAND load_id=%llu ts_ms=%llu target=%.3f mode=absolute",
            static_cast<unsigned long long>(loadId),
            static_cast<unsigned long long>(SDL_GetTicks64()),
            static_cast<double>(targetTime)));

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
    Uint64 lastSeekRequestTime = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        seekTargetTime = m_seekTargetTime;
        isSeekPending = m_isSeekPending;
        lastSeekRequestTime = m_lastSeekRequestTime;
    }
    
    if (isSeekPending && (SDL_GetTicks64() - lastSeekRequestTime >= SEEK_DELAY_MS)) {
        DoSeek(seekTargetTime);
    }

    Uint64 keepOpenLoadId = 0;
    bool keepOpenOverrideActive = false;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        keepOpenLoadId = m_startupKeepOpenOverrideLoadId;
        keepOpenOverrideActive = m_startupKeepOpenOverrideActive;
    }

    if (keepOpenOverrideActive && keepOpenLoadId > 0) {
        bool mediaStarted = false;
        m_state.ReadPlayback([&](PlaybackModel const& m) {
            mediaStarted =
                m.startupLoadId == keepOpenLoadId &&
                m.startupMediaStartedLoadId == keepOpenLoadId;
        });

        if (mediaStarted) {
            const int restoreResult = SetPropertyString("keep-open", "yes");
            LifecycleEvidence::EmitDiagnostic(
                "STARTUP",
                FormatString(
                    "event=STARTUP_KEEP_OPEN_RESTORE load_id=%llu ts_ms=%llu result=%d error_text=%s",
                    static_cast<unsigned long long>(keepOpenLoadId),
                    static_cast<unsigned long long>(SDL_GetTicks64()),
                    restoreResult,
                    mpv_error_string(restoreResult)));

            if (restoreResult >= 0) {
                std::lock_guard<std::mutex> lock(m_commandMutex);
                if (m_startupKeepOpenOverrideLoadId == keepOpenLoadId) {
                    m_startupKeepOpenOverrideActive = false;
                }
            }
        }
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