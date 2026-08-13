// MainWindowRenderer.h
#pragma once
#include "WindowRenderer.h"

#include <SDL.h>

#define SDL_CURSOR_EVENT (SDL_USEREVENT + 3)

class MainWindowRenderer : public WindowRenderer {
private:
    Uint64 lastInteractionTime = 0;

    void UpdateUIState(class WindowRuntime* runtime, const struct WindowLayout& layout);
    void ShowSubWindows();

public:
    void Initialize(class WindowRuntime* runtime) override;
    void RenderUI(class WindowRuntime* runtime, const class WindowSnapshot& snapshot) override;
    void Shutdown() override;

    // Giải phóng interface cũ không còn cần thiết vì Backend đã quản lý Frame Lifecycle
    void BeginFrame() override {}
    void EndFrame() override {}
};