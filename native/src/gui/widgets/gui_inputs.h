#pragma once

#include "../core/gui_types.h"
#include <string>
#include <vector>
#include <functional>

namespace CSImGui {
    bool ModernInputTextMultiline(const char *label, char *buf, size_t buf_size, const ImVec2 &size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0);
    bool ModernInputTextMultiline(const char *label, std::string &str, const ImVec2 &size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0);
    bool ModernInputText(const char *label, char *buf, size_t buf_size, float width, ImGuiInputTextFlags flags = 0);
    bool ModernInputText(const char *label, std::string &buffer, float width, ImGuiInputTextFlags flags = 0);
    bool ModernInputText(const char *label, char *buf, size_t buf_size, ImGuiInputTextFlags flags = 0);

    bool ModernSearchCombo(const char *label,
                           std::string &current_value,
                           const std::vector<std::string> &options,
                           float custom_width = 200.0f,
                           int max_items_visible = 6,
                           std::function<bool(std::string &)> on_validate_confirm = nullptr,
                           std::function<std::string(const std::string &)> on_get_dynamic_opt = nullptr);

    bool NormalCombo(const char *label, std::string &current_item, const std::vector<std::string> &options, float custom_width = 200.0f, int max_items_visible = 5);
}