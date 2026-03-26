#pragma once
#include <SDL.h>
#include <mpv/client.h>

bool HandleHotkeys(const SDL_Event& e,  bool& render_popup , mpv_handle* mpv, bool& isFullscreen_video, SDL_Window* window);
