#pragma once

#include "../core/gui_types.h"

namespace CSImGui {
    void ModernTextEffect(const char* text, const TextEffectStyle& style);
    void ModernTextEffectFmt(const TextEffectStyle& style, const char* fmt, ...);
    void TextGradient(const char* text, ImU32 colStart, ImU32 colEnd);
    void TextWave(const char* text, float time);
    void TextScrolling(const char* text, float maxWidth, float speed = 40.0f, bool scrollOnlyOnHover = false);
}