// WindowTemplate.h
#pragma once
#include <functional>
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "IGraphicsBackend.h"
#include <string>
#include <functional>
#include <memory>

class WindowStyle;
class PropertyBag;
class WindowState;

class WindowRenderer;
class IGraphicsBackend;
class FrameTimer;

struct WindowTemplate {
    std::string name;
    WindowStyle style;
    WindowState state;
    PropertyBag defaultProperties;

    std::function<std::unique_ptr<IGraphicsBackend>()> graphicsBackendFactory;
    std::function<std::unique_ptr<WindowRenderer>()> rendererFactory;
    std::function<std::unique_ptr<FrameTimer>()> windowloopFactory;
};