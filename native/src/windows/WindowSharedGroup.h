// WindowSharedGroup.h
#pragma once

#include <memory>
#include "imgui.h"

class WindowSharedGroup {
public:
    std::shared_ptr<ImFontAtlas> m_sharedFontAtlas;

    WindowSharedGroup();
};