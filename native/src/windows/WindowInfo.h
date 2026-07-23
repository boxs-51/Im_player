// WindowInfo.h
#pragma once
#include <string>
#include <cstdint>

using WindowId = uint32_t;

struct WindowInfo {
    std::string templateName;
    WindowId id = 0;
};