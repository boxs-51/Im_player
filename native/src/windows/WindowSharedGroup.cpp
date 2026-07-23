// WindowSharedGroup.cpp
#include "WindowSharedGroup.h"

WindowSharedGroup::WindowSharedGroup() {
    m_sharedFontAtlas = std::make_shared<ImFontAtlas>();
}