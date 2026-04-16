#include "hotkey_handler.h"
#include "mpv_controller.h"
#include "popup.h"
#include "globals.h"
#include "thread.h"
#include "sidebar_popup.h"
#include <mpv/mpv_data.h>

#include "windows/windows_borderless_state.h"
#include "windows/windows_borderless.h"

#include <log.h>
#include <SDL.h>

static DragResizeState& g_DragResizeState = GetDragResizeState();
static MPVPlaybackStatus& g_playback = GetMPVPlaybackStatus();
static VideoInfo& g_videoinfo = GetVideoInfo();
// --- Playback Hotkeys --- //
bool HandleBasicHotkeys(const SDL_Event* e, mpv_handle* mpv, AppSettings * v) {
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();
    // Lấy trạng thái phát lại
    PlaybackState state = GetPlaybackState();
    bool isPlayable = (state == PlaybackState::Playing || state == PlaybackState::Paused) && state != PlaybackState::Loading;

    // --- ƯU TIÊN HOTKEY CÓ CTRL --- //
    if (mod & KMOD_CTRL) {
        const Uint8* state = SDL_GetKeyboardState(NULL);

        if (state[SDL_SCANCODE_UP] && state[SDL_SCANCODE_DOWN]) {
            // 👉 Thực hiện hành động đặc biệt, ví dụ reset audio delay:
            double resetDelay = 0.0;
            mpv_command_set_audio_delay(mpv,resetDelay);
            v->audiodelay = resetDelay;
            SaveSettings_Video();
            return true;
        }
        if (state[SDL_SCANCODE_LEFT] && state[SDL_SCANCODE_RIGHT]) {
            // 👉 Thực hiện hành động đặc biệt, ví dụ reset speed:
            double resetspeed = 1.0;
            mpv_command_set_speed(mpv, resetspeed);
            v->playbackSpeed = resetspeed;
            SaveSettings_Video();
            return true;
        }

        switch (key) {
            case SDLK_UP:
            case SDLK_DOWN:
            {   
                double step = (mod & KMOD_SHIFT) ? 0.5 : 0.1;
                if (key == SDLK_DOWN) step = -step;
                double audio_delay = std::clamp(g_videoinfo.audio_delay + step , -10.0, 10.0);
                mpv_command_set_audio_delay(mpv, audio_delay);
                v->audiodelay = audio_delay;
                SaveSettings_Video();
                return true;
            }
            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 1.0 : 0.1;
                if (key == SDLK_LEFT) step = -step;
                double speed = std::clamp(g_playback.speed + step, 0.2, 3.0);
                mpv_command_set_speed(mpv, speed);
                v->playbackSpeed = speed;
                SaveSettings_Video();
                return true;
            }
            default:
                return false;
        }
    }

    // --- HOTKEY THƯỜNG (KHÔNG CÓ CTRL) --- //
    if (isPlayable) {
        switch (key) {
            case SDLK_SPACE:
                if (g_playback.isPaused) {
                    mpv_command_play(mpv);
                } else {
                    mpv_command_pause(mpv);
                    lastInteractionTime = SDL_GetTicks64();
                }
                return true;

            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 20.0f : 10.0f;
                if (key == SDLK_LEFT) step = -step;
                float targetthime = (float)g_playback.playbackTime + step;
                mpv_command_seek_abs(mpv, targetthime, (float)g_playback.duration);
                return true;
            }
            case SDLK_DOWN: 
            case SDLK_UP: 
            {
                float step = (mod & KMOD_SHIFT) ? 15.0f : 5.0f;
                if (key == SDLK_DOWN) step = -step;
                float newVol = std::clamp(g_playback.volume + step,0.0f,130.0f);
                mpv_command_set_volume(mpv, newVol);
                v->defaultVolume = newVol;
                SaveSettings_Video();
                return true;
            }

            case SDLK_m:
                mpv_command_set_mute(mpv, !g_playback.isMuted);
                return true;

            default:
                break;
        }
    }

    // --- OTHER HOTKEYS (KHÔNG LIÊN QUAN PLAYBACK) --- //
    if (key == SDLK_F11) {
        g_DragResizeState.ToggleFullscreen = true;
        return true;
    }

    return false;
}

// --- Popup Hotkeys --- //
bool HandlePopupHotkeys(const SDL_Event* e) {
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();

    // ESC: đóng tất cả popup đang mở
    if (key == SDLK_ESCAPE) {

        if (Popup_Url.IsOpen()) {
            Popup_Url.Close();
        }
        else if (videoInfoPopup.IsOpen()) {
            videoInfoPopup.Close();
        }
        else if (SettingPopup.IsOpen()) {
            SettingPopup.Close();
        }
        else if (SidarBarPopup.IsOpen()) {
            SidarBarPopup.Close();
        }
    }

    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_u:
            {
                if (Popup_Url.IsOpen()) {Popup_Url.Close();
                } else {OpenURLPopup(Popup_Url);}
                return true;
            }
            case SDLK_a:
            {
                if (videoInfoPopup.IsOpen()) {videoInfoPopup.Close();
                } else {OpenVideoInfoPopup(videoInfoPopup);}    
                return true;
            }
            case SDLK_l:
            {
                if (SidarBarPopup.IsOpen()){SidarBarPopup.Close();
                } else {OpenSidarBarPopup(SidarBarPopup);}  
                return true; 
            }
            case SDLK_s:
            {
                if (SettingPopup.IsOpen()) {SettingPopup.Close();
                } else {OpenSettingPopup(SettingPopup);}    
                return true; 
            }
            default:
                break;
        }
    }
    return false;
}

bool HandleExtersionHotkeys(const SDL_Event* e){
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();
    switch (key) {
        case SDLK_F12:
        {
            uiState.show_settings = !uiState.show_settings;
            return true;
        }
        case SDLK_F1:
        {
            if (IsConsoleVisible()) 
                CloseConsoleWindow();
            else
                OpenConsoleWindow();
            return true;
        }
        default:
            break;
    }
    return false;
}
// Hàm tổng gộp xử lý hotkey
bool HandleHotkeys(const SDL_Event* e, mpv_handle* mpv ,AppSettings * v) {
    if (Disabehotkey) return false;
    return HandleBasicHotkeys(e, mpv ,v) ||
           HandlePopupHotkeys(e) ||
           HandleExtersionHotkeys(e);
}
