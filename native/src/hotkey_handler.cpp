#include "hotkey_handler.h"
#include "popup.h"
#include "sidebar_popup.h"
#include "player/session/PlayerSession.h"

#include "settings_manager.h"

#include "WindowRuntime.h"
#include <log.h>
#include <SDL.h>

#include "WindowManager.h"

void OpenMockSubWindow()
{
    auto &winManager = WindowManager::GetInstance();
    const std::string templateName = "MockSubWindow";

    // 1. Tìm xem có cửa sổ nào cùng loại đã bị ẩn không
    WindowRuntime *Win = winManager.FindHiddenWindowByTemplate(templateName);

    if (Win)
    {
        // 2. Nếu có, chỉ cần hiện nó lên
        if (Win->state.display.isShown)
            winManager.HideWindow(Win->info.id);
        else
            winManager.ShowWindow(Win->info.id);
    }
    else
    {
        // 3. Nếu không, tạo mới như bình thường
        // Lấy cửa sổ chính làm cha
        WindowRuntime *mainWin = winManager.GetMainWindow();
        if (mainWin)
        {
            winManager.QueueCreateWindow(templateName, mainWin);
        }
    }
}

// --- Playback Hotkeys --- //
bool HandleBasicHotkeys(const SDL_Event *e, WindowRuntime *runtime)
{
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();

    auto &config = ConfigManager::Instance();

    // --- ƯU TIÊN HOTKEY CÓ CTRL --- //
    if (auto *player = runtime->resource.GetPlayerSession())
    {

        if (mod & KMOD_CTRL)
        {
            const Uint8 *state = SDL_GetKeyboardState(NULL);

            if (state[SDL_SCANCODE_UP] && state[SDL_SCANCODE_DOWN])
            {
                // 👉 Thực hiện hành động đặc biệt, ví dụ reset audio delay:
                double resetDelay = 0.0;
                if (auto *commander = player->GetCommander())
                    commander->SetAudioDelay(resetDelay);
                else
                    return false;
                config.UpdateVideoSettings([resetDelay](AppSettings &settings)
                                           { settings.audiodelay = resetDelay; });
                config.SaveVideo();
                return true;
            }
            if (state[SDL_SCANCODE_LEFT] && state[SDL_SCANCODE_RIGHT])
            {
                // 👉 Thực hiện hành động đặc biệt, ví dụ reset speed:
                double resetspeed = 1.0;
                if (auto *commander = player->GetCommander())
                    commander->SetSpeed(resetspeed);
                else
                    return false;
                config.UpdateVideoSettings([resetspeed](AppSettings &settings)
                                           { settings.playbackSpeed = resetspeed; });
                config.SaveVideo();
                return true;
            }

            switch (key)
            {
            case SDLK_UP:
            case SDLK_DOWN:
            {
                double step = (mod & KMOD_SHIFT) ? 0.5 : 0.1;
                if (key == SDLK_DOWN)
                    step = -step;

                double audio_delay = 0.0;
                if (auto *state = player->GetState())
                    state->ReadAudio([&audio_delay](auto const &m)
                                     { audio_delay = m.codec.audio_delay; });
                else
                    return false;

                double new_audio_delay = std::clamp(audio_delay + step, -10.0, 10.0);
                if (auto *playercommand = player->GetCommander())
                    playercommand->SetAudioDelay(new_audio_delay);
                else
                    return false;

                config.UpdateVideoSettings([new_audio_delay](AppSettings &settings)
                                           { settings.audiodelay = new_audio_delay; });
                config.SaveVideo();

                return true;
            }
            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 1.0 : 0.1;
                if (key == SDLK_LEFT)
                    step = -step;

                double speed = 1.0;
                if (auto *state = player->GetState())
                    state->ReadPlayback([&speed](PlaybackModel const &m)
                                        { speed = m.config.speed; });
                else
                    return false;

                double newspeed = std::clamp(speed + step, 0.2, 3.0);
                if (auto *playercommand = player->GetCommander())
                    playercommand->SetSpeed(newspeed);
                else
                    return false;

                config.UpdateVideoSettings([newspeed](AppSettings &settings)
                                           { settings.playbackSpeed = newspeed; });
                config.SaveVideo();

                return true;
            }
            default:
                return false;
            }
        }

        // --- HOTKEY THƯỜNG (KHÔNG CÓ CTRL) --- //
        bool isPlayable = true;
        if (auto *state = player->GetState())
        {
            state->ReadPlayback([&isPlayable](PlaybackModel const &m)
                                { isPlayable = m.state == PlaybackState::Playing || m.state == PlaybackState::Paused; });
        }

        if (isPlayable)
        {
            switch (key)
            {
            case SDLK_SPACE:
            {

                auto *playercommand = player->GetCommander();

                bool isPaused = false;
                if (auto *state = player->GetState())
                {
                    state->ReadPlayback([&isPaused](PlaybackModel const &m)
                                        { isPaused = m.flags.isPaused; });
                }
                else
                {
                    return false;
                }

                if (isPaused)
                {
                    if (playercommand)
                        playercommand->Play();
                    else
                        return false;
                }
                else
                {
                    if (playercommand)
                        playercommand->Pause();
                    else
                        return false;
                }

                return true;
            }

            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                double step = (mod & KMOD_SHIFT) ? 20.0f : 10.0f;
                if (key == SDLK_LEFT)
                    step = -step;

                double playbackTime = 0.0;
                double duration = 0.0;
                if (auto *state = player->GetState())
                {
                    state->ReadPlayback([&playbackTime, &duration](PlaybackModel const &m)
                                        {
                    playbackTime = m.timing.playbackTime;
                    duration = m.timing.duration; });
                }
                else
                {
                    return false;
                }

                float targetthime = (float)playbackTime + step;
                if (auto *playercommand = player->GetCommander())
                {
                    playercommand->Seek(targetthime, (float)duration);
                }
                else
                {
                    return false;
                }

                return true;
            }
            case SDLK_DOWN:
            case SDLK_UP:
            {
                float step = (mod & KMOD_SHIFT) ? 15.0f : 5.0f;

                if (key == SDLK_DOWN)
                    step = -step;

                int volume = 100;
                if (auto *state = player->GetState())
                {
                    state->ReadAudio([&volume](auto const &m)
                                     { volume = m.volume.volume; });
                }
                else
                {
                    return false;
                }

                int newVol = (int)std::clamp(volume + step, 0.0f, 130.0f);
                if (auto *playercommand = player->GetCommander())
                {
                    playercommand->SetVolume(newVol);
                }
                else
                {
                    return false;
                }

                config.UpdateVideoSettings([newVol](AppSettings &settings)
                                           { settings.defaultVolume = newVol; });
                config.SaveVideo();

                return true;
            }

            case SDLK_m:
            {
                bool isMuted = false;
                if (auto *state = player->GetState())
                {
                    state->ReadAudio([&isMuted](auto const &m)
                                     { isMuted = m.volume.isMuted; });
                }
                else
                {
                    return false;
                };

                if (auto *commander = player->GetCommander())
                {
                    commander->SetMute(!isMuted);
                }
                else
                {
                    return false;
                }

                return true;
            }
            default:
                break;
            }
        }
    }

    // --- OTHER HOTKEYS (KHÔNG LIÊN QUAN PLAYBACK) --- //
    if (key == SDLK_F11)
    {
        runtime->properties.Set<bool>("TriggerToggleFullscreen", true);
        return true;
    }
    return false;
}

// --- Popup Hotkeys --- //
bool HandlePopupHotkeys(const SDL_Event *e)
{
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    // Use the modifier snapshot carried by this SDL_KEYDOWN event. Reading
    // SDL_GetModState() here can observe a newer global keyboard state after
    // the event was queued, which makes Ctrl+hotkeys timing-dependent.
    SDL_Keymod mod = static_cast<SDL_Keymod>(e->key.keysym.mod);

    // ESC: đóng tất cả popup đang mở
    if (key == SDLK_ESCAPE)
    {

        if (Popup_Url.IsOpen())
        {
            Popup_Url.Close();
        }
        else if (videoInfoPopup.IsOpen())
        {
            videoInfoPopup.Close();
        }
        else if (SettingPopup.IsOpen())
        {
            SettingPopup.Close();
        }
        else if (SidarBarPopup.IsOpen())
        {
            SidarBarPopup.Close();
        }
    }

    if (mod & KMOD_CTRL)
    {
        switch (key)
        {
        case SDLK_u:
        {
            if (Popup_Url.IsOpen())
            {
                Popup_Url.Close();
            }
            else
            {
                OpenURLPopup(Popup_Url);
            }
            return true;
        }
        case SDLK_a:
        {
            if (videoInfoPopup.IsOpen())
            {
                videoInfoPopup.Close();
            }
            else
            {
                OpenVideoInfoPopup(videoInfoPopup);
            }
            return true;
        }
        case SDLK_l:
        {
            if (SidarBarPopup.IsOpen())
            {
                SidarBarPopup.Close();
            }
            else
            {
                OpenSidarBarPopup(SidarBarPopup);
            }
            return true;
        }
        case SDLK_s:
        {
            if (SettingPopup.IsOpen())
            {
                SettingPopup.Close();
            }
            else
            {
                OpenSettingPopup(SettingPopup);
            }
            return true;
        }
        case SDLK_t:
        {
            if (TestPopup.IsOpen())
            {
                TestPopup.Close();
            }
            else
            {
                OpenTestPopup(TestPopup);
            }
            return true;
        }
        case SDLK_p: // Hotkey mới: Ctrl + P
        {
            // Gọi hàm logic để mở cửa sổ phụ
            OpenMockSubWindow();
            return true;
        }
        default:
            break;
        }
    }
    return false;
}

bool HandleExtersionHotkeys(const SDL_Event *e)
{
    if (e->type != SDL_KEYDOWN)
        return false;

    SDL_Keycode key = e->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();
    switch (key)
    {
    case SDLK_F12:
    {
        uiState.show_settings = !uiState.show_settings;
        return true;
    }

    default:
        break;
    }
    return false;
}
// Hàm tổng gộp xử lý hotkey
bool HandleHotkeys(const SDL_Event *e, WindowRuntime *runtime)
{
    if (Disabehotkey)
        return false;
    return HandleBasicHotkeys(e, runtime) ||
           HandlePopupHotkeys(e) ||
           HandleExtersionHotkeys(e);
}
