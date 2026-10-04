#include "Player.h"


#include "player/scripts/script_manager.h"
#include "player/PlayerUtils.h"
#include "YtDlpManager.h"
#include "common/LifecycleEvidence.h"

#include "windows/WindowUtils.h"

#include "utils.h"
#include <SDL.h>
#include <algorithm>

Player::Player() : m_mpv(nullptr) {}

Player::~Player() {
    Shutdown();
}

bool Player::Init() {
    if (m_mpv) return true;

    m_mpv = mpv_create();
    if (!m_mpv) return false;

    ApplyStaticMPVConfig(m_mpv);

    const std::string ytdlpPath =
        YtDlpManager::GetInstance().ResolveExecutableForPlayback();
    if (!ytdlpPath.empty()) {
        const std::string ytdlScriptOpt =
            "ytdl_hook-ytdl_path=" + ytdlpPath;
        const int ytdlOptResult = mpv_set_option_string(
            m_mpv,
            "script-opts-append",
            ytdlScriptOpt.c_str());

        LifecycleEvidence::EmitDiagnostic(
            "STARTUP",
            FormatString(
                "event=YTDL_PATH_RESOLVED result=%d path=%s",
                ytdlOptResult,
                ytdlpPath.c_str()));
    } else {
        LifecycleEvidence::EmitDiagnostic(
            "STARTUP",
            "event=YTDL_PATH_MISSING");
    }

    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    ScriptManager::Instance().Init(m_mpv);
    ScriptManager::Instance().LoadScriptFromFolder({ AutoPath<std::string>("%ROOT%","scripts") });

    mpv_request_log_messages(m_mpv, "v");

    mpv_set_wakeup_callback(m_mpv, [](void*) {
        SDL_Event ev;
        ev.type = SDL_MPV_EVENT;
        SDLUtils::SDLX_PushUniqueEvent(ev);
    }, nullptr);

    return true;
}

void Player::Shutdown() {
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}