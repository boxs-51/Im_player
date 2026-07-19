#pragma once
#include <SDL.h>
#include <mpv/client.h>
#include "WindowRuntime.h"
bool HandleHotkeys(const SDL_Event* e , mpv_handle* mpv);
bool HandleHotkeys(const SDL_Event* e, mpv_handle* mpv, WindowRuntime* runtime);
