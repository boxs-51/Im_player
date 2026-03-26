#pragma once
#include "imgui.h"
#include <string>
#include <unordered_map>
#include <algorithm>

struct UIStylePreset {
    ImVec4 bgColor;
    ImVec4 textColor;
    ImVec4 accentColor;
    float rounding;
    float alpha;
};

class UIStyleManager {
public:
    void Init();
    void ApplyGlobalStyle();
    void ApplyPreset(const std::string& name);
    void PushStyle(const std::string& name);
    void PopStyle();

    void BeginStyledWindow(const std::string& title, bool* open = nullptr, ImGuiWindowFlags flags = 0);
    void EndStyledWindow();

    void BeginStyledChild(const std::string& id, ImVec2 size = ImVec2(0,0), bool border = false, ImGuiWindowFlags flags = 0);
    void EndStyledChild();

    void BeginStyledPopup(const std::string& id, ImGuiWindowFlags flags = 0);
    void EndStyledPopup();

    void BeginStyledTable(const std::string& id, int columns, ImGuiTableFlags flags = 0);
    void EndStyledTable();

    bool HasPreset(const std::string& name) const { return presets.find(name) != presets.end(); }
    const std::string& GetCurrentPreset() const { return currentPreset; }

    // 🌈 Chuyển preset mượt (fade / interpolate)
    void TransitionTo(const std::string& name, float speed = 2.0f);
    void UpdateTransition(); // gọi mỗi frame để cập nhật màu

    // 🔆 Style tạm cho 1 widget riêng
    void PushTemporaryColor(ImGuiCol target, const ImVec4& color);
    void PopTemporaryColor();

private:
    std::unordered_map<std::string, UIStylePreset> presets;
    std::string currentPreset;

    // Cho hiệu ứng chuyển theme
    bool transitioning = false;
    std::string nextPreset;
    float transitionProgress = 0.0f;
    float transitionSpeed = 2.0f;

    UIStylePreset currentStyle; // dùng khi blending
    UIStylePreset targetStyle;
};

extern UIStyleManager g_UIStyle;
