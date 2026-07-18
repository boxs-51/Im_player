// WindowTemplate.h
#pragma once
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include <string>
#include <functional>
#include <memory>

struct WindowTemplate {
    std::string name;
    WindowStyle style;
    PropertyBag defaultProperties;
    std::function<std::unique_ptr<WindowRenderer>()> rendererFactory;
};