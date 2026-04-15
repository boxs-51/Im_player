#pragma once
#include <SDL.h>
#include <mpv/client.h>
#include <mpv/mpv_settings.h>

bool HandleHotkeys(const SDL_Event* e , mpv_handle* mpv ,AppSettings * v);
