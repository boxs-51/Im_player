#pragma once
#include <SDL.h>
#include <mpv/client.h>
#include "WindowRuntime.h"
bool HandleHotkeys(const SDL_Event* e, WindowRuntime* runtime);
