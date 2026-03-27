#include "hotkey_handler.h"
#include "mpv_controller.h"
#include "popup.h"
#include "globals.h"
#include "thread.h"
#include "sidebar_popup.h"

#include "windows/windows_borderless_state.h"
#include "windows/windows_borderless.h"

#include <log.h>
#include <SDL.h>

// --- Playback Hotkeys --- //
bool HandleBasicHotkeys(const SDL_Event& e, mpv_handle* mpv, bool& isFullscreen, SDL_Window* window) {
    if (e.type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e.key.keysym.sym;
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
            v_Settings.audiodelay = resetDelay;
            SaveSettings_Video();
            return true;
        }
        if (state[SDL_SCANCODE_LEFT] && state[SDL_SCANCODE_RIGHT]) {
            // 👉 Thực hiện hành động đặc biệt, ví dụ reset speed:
            double resetspeed = 1.0;
            mpv_command_set_speed(mpv, resetspeed);
            v_Settings.playbackSpeed = resetspeed;
            SaveSettings_Video();
            return true;
        }

        switch (key) {
            case SDLK_UP:
            case SDLK_DOWN:
            {   
                double step = (mod & KMOD_SHIFT) ? 0.5 : 0.1;
                if (key == SDLK_DOWN) step = -step;
                double audio_delay = std::clamp(mpv_get_audio_delay(mpv) + step , -10.0, 10.0);
                mpv_command_set_audio_delay(mpv, audio_delay);
                v_Settings.audiodelay = audio_delay;
                SaveSettings_Video();
                return true;
            }
            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 1.0 : 0.1;
                if (key == SDLK_LEFT) step = -step;
                double speed = std::clamp(mpv_get_speed(mpv) + step, 0.2, 3.0);
                mpv_command_set_speed(mpv, speed);
                v_Settings.playbackSpeed = speed;
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
                if (mpv_is_paused(mpv)) {
                    mpv_command_play(mpv);
                } else {
                    mpv_command_pause(mpv);
                    lastInteractionTime = SDL_GetTicks();
                }
                return true;

            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 20.0f : 10.0f;
                if (key == SDLK_LEFT) step = -step;
                
                mpv_command_seek_clamped(mpv, step, (float)g_playbackStatus.playbackTime, g_playbackStatus.duration);
                return true;
            }
            case SDLK_DOWN: 
            case SDLK_UP: 
            {
                float step = (mod & KMOD_SHIFT) ? 15.0f : 5.0f;
                if (key == SDLK_DOWN) step = -step;
                float newVol = std::clamp(mpv_get_volume(mpv) + step,0.0f,100.0f);
                mpv_command_set_volume(mpv, newVol);
                v_Settings.defaultVolume = newVol;
                SaveSettings_Video();
                return true;
            }

            case SDLK_m:
                mpv_command_set_mute(mpv, !mpv_is_muted(mpv));
                return true;

            default:
                break;
        }
    }

    // --- OTHER HOTKEYS (KHÔNG LIÊN QUAN PLAYBACK) --- //
    if (key == SDLK_F11) {
        //g_RequestToggleFullscreen = true;
        BW.isFullscreen_video = SDLX_ToggleFullscreen(ctx.mainWindow, !g_DragResizeState.IsFullscreen_video);
        return true;
    }

    return false;
}

// --- Popup Hotkeys --- //
bool HandlePopupHotkeys(const SDL_Event& e) {
    if (e.type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e.key.keysym.sym;
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
                } else {OpenURLPopup(Popup_Url, Url);}
                return true;
            }
            case SDLK_a:
            {
                if (videoInfoPopup.IsOpen()) {videoInfoPopup.Close();
                } else {OpenVideoInfoPopup();}    
                return true;
            }
            case SDLK_l:
            {
                if (SidarBarPopup.IsOpen()){SidarBarPopup.Close();
                } else {OpenSidarBarPopup();}  
                return true; 
            }
            case SDLK_s:
            {
                if (SettingPopup.IsOpen()) {SettingPopup.Close();
                } else {OpenSettingPopup();}    
                return true; 
            }
            default:
                break;
        }
    }
    return false;
}

bool HandleExtersionHotkeys(const SDL_Event& e){
    if (e.type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e.key.keysym.sym;
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
bool HandleHotkeys(const SDL_Event& e, mpv_handle* mpv, bool& isFullscreen_video, SDL_Window* window) {
    if (Disabehotkey) return false;
    return HandleBasicHotkeys(e, mpv, isFullscreen_video, window ) ||
           HandlePopupHotkeys(e) ||
           HandleExtersionHotkeys(e);
}
