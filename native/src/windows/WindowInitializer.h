// WindowInitializer.h
#pragma once
#include <string>

#include "WindowRuntime.h"
#include "WindowTemplate.h"

// Forward declarations
class WindowRuntime;
class WindowTemplate;
class IGraphicsBackend;

class WindowInitializer {
public:
    bool Initialize(WindowRuntime* runtime, const WindowTemplate* tpl, WindowRuntime* parent);

private:
    bool InitializeSDL(WindowRuntime* runtime, const WindowTemplate* tpl, IGraphicsBackend* backend);
    bool InitializeSharedGroup(WindowRuntime* runtime, WindowRuntime* parent);
    bool InitializeImGui(WindowRuntime* runtime);
    bool InitializeBackend(WindowRuntime* runtime);
    void InitializeNative(WindowRuntime* runtime);
    bool InitializeThread(WindowRuntime* runtime);
    void InitializeRenderer(WindowRuntime* runtime);
    void AttachMPV(WindowRuntime* runtime, WindowRuntime* parent);
};