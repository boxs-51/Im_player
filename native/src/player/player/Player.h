#pragma once

#include <mpv/client.h>
#include <SDL_events.h>

#define SDL_MPV_EVENT (SDL_USEREVENT + 1)
#define SDL_MPV_RENDER_UPDATE (SDL_USEREVENT + 2)
class Player {
public:
    Player();
    ~Player();

    bool Init(); // Giữ lại Init để khởi tạo mpv_handle
    void Shutdown();

    mpv_handle* GetHandle() const { return m_mpv; }

private:
    mpv_handle* m_mpv = nullptr;
};