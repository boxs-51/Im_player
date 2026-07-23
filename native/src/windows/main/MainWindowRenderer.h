// MainWindowRenderer.h
#pragma once
#include "WindowRenderer.h"
#include "WindowRuntime.h"
#include "utils.h"

#include <SDL.h>

#define SDL_CURSOR_EVENT (SDL_USEREVENT + 3)

class MainWindowRenderer : public WindowRenderer {
private:
    Uint64 lastInteractionTime = 0;

    void UpdateUIState(WindowRuntime* runtime);
    void ShowSubWindows();

public:
    void Initialize(WindowRuntime* runtime) override;
    void RenderUI(WindowRuntime* runtime) override;
    void Shutdown() override;

    // Giải phóng interface cũ không còn cần thiết vì Backend đã quản lý Frame Lifecycle
    void BeginFrame() override {}
    void EndFrame() override {}
};