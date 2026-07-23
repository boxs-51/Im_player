#pragma once
#include "WindowRenderer.h"

class MockSubWindowRenderer : public WindowRenderer {
public:

    void Initialize(WindowRuntime* runtime) override;
    void RenderUI(WindowRuntime* runtime) override;
    void Shutdown() override;

    // Giải phóng interface cũ không còn cần thiết vì Backend đã quản lý Frame Lifecycle
    void BeginFrame() override {}
    void EndFrame() override {}
};