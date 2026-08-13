// WindowRenderer.h
#pragma once

class WindowRuntime; // Forward declaration

class WindowRenderer {
public:
    virtual ~WindowRenderer() = default;
    virtual void Initialize(WindowRuntime* runtime) = 0;
    virtual void BeginFrame() = 0;
    virtual void RenderUI(WindowRuntime* runtime, const class WindowSnapshot& snapshot) = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
};