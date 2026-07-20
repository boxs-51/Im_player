// WindowTemplate.h
#pragma once
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "IGraphicsBackend.h"
#include <string>
#include <functional>
#include <memory>
class FrameTimer;
struct WindowTemplate {
    std::string name;
    WindowStyle style;
    PropertyBag defaultProperties;
    
    std::function<std::unique_ptr<IGraphicsBackend>()> graphicsBackendFactory;
    std::function<std::unique_ptr<WindowRenderer>()> rendererFactory;
    std::function<std::unique_ptr<FrameTimer>()> windowloopFactory;
};