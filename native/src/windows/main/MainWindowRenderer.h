// MainWindowRenderer.h
#pragma once
#include "WindowRenderer.h"
#include "WindowRuntime.h"
#include <SDL.h>

class MainWindowRenderer : public WindowRenderer {
private:
    Uint64 lastInteractionTime = 0;
    bool show_ui_video = true;

    void UpdateUIState(WindowRuntime* runtime);
    void ShowSubWindows();

public:
    void Initialize(WindowRuntime* runtime) override;
    void BeginFrame() override;
    void RenderUI(WindowRuntime* runtime) override;
    void EndFrame() override;
    void Shutdown() override;
};