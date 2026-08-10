// WindowFlagBuilder.h
#pragma once
#include <SDL.h>
#include "WindowDefs.h"
#include "backends/IGraphicsBackend.h"

enum WinDownFlags{
    CREATE_MPV = 0
};

class WindowFlagBuilder {
private:
    Uint32 flags = 0;

public:
    WindowFlagBuilder& CreateMPV(bool condition) {
        if (condition) flags |= CREATE_MPV;
        return *this;
    }

    Uint32 Build() const {
        return flags;
    }

};

class SDLFlagBuilder {
private:
    Uint32 flags = 0;

public:
    SDLFlagBuilder& Hidden(bool condition) {
        if (condition) flags |= SDL_WINDOW_HIDDEN;
        else flags |= SDL_WINDOW_SHOWN;
        return *this;
    }

    SDLFlagBuilder& Fullscreen(bool condition) {
        if (condition) flags |= SDL_WINDOW_FULLSCREEN;
        return *this;
    }

    SDLFlagBuilder& Borderless(bool condition) {
        if (condition) flags |= SDL_WINDOW_BORDERLESS;
        return *this;
    }

    SDLFlagBuilder& Resizable(bool condition) {
        if (condition) flags |= SDL_WINDOW_RESIZABLE;
        return *this;
    }

    SDLFlagBuilder& Minimized(bool condition) {
        if (condition) flags |= SDL_WINDOW_MINIMIZED;
        return *this;
    }

    SDLFlagBuilder& HighDPI(bool condition) {
        if (condition) flags |= SDL_WINDOW_ALLOW_HIGHDPI;
        return *this;
    }

    SDLFlagBuilder& WithBackend(IGraphicsBackend* backend) {
        if (backend) flags |= backend->GetWindowFlags();
        return *this;
    }

    Uint32 Build() const {
        return flags;
    }
};