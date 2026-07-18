// WindowRuntime.h
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "WindowController.h"

using WindowId = uint32_t;


class WindowRuntime {
public:
    WindowId id = 0;
    SDL_Window* sdlWindow = nullptr;
    SDL_GLContext mainGLContext = nullptr;
    HWND hwnd = nullptr;

    WindowState state;
    WindowStyle style;
    PropertyBag properties;
    
    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;

    WindowRuntime();
    ~WindowRuntime();
};