#include "Player.h"
#include "player/mpv_data.h"
#include "player/shaders/shaders_manager.h"
#include "player/scripts/script_manager.h"
#include "player/audio/filter/af_m.h"

#include "utils.h"
#include <SDL.h>
#include <algorithm>

#include "windows/WindowUtils.h"

Player::Player() : m_mpv(nullptr) {}

Player::~Player() {
    Shutdown();
}

bool Player::Init() {
    if (m_mpv) return true;

    m_mpv = mpv_create();
    if (!m_mpv) return false;

    ApplyStaticMPVConfig(m_mpv);

    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    ScriptManager::Instance().Init(m_mpv);
    ScriptManager::Instance().LoadScriptFromFolder({ AutoPath<std::string>("%ROOT%","scripts") });

    ShaderManager::Instance().Init(m_mpv);
    ShaderManager::Instance().LoadState();

    AudioFilterManager::Instance().Init(m_mpv);
    AudioFilterManager::Instance().LoadFromFile();

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