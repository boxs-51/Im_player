#include "UIStyleManager.h"
#include <cmath>

UIStyleManager g_UIStyle;

static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

void UIStyleManager::Init()
{
    presets["Dark"] = {
        ImVec4(0.10f, 0.11f, 0.13f, 1.0f),
        ImVec4(0.93f, 0.93f, 0.95f, 1.0f),
        ImVec4(0.25f, 0.50f, 0.90f, 1.0f),
        8.0f, 1.0f
    };

    presets["Light"] = {
        ImVec4(0.93f, 0.94f, 0.96f, 1.0f),
        ImVec4(0.10f, 0.10f, 0.12f, 1.0f),
        ImVec4(0.30f, 0.45f, 0.90f, 1.0f),
        6.0f, 1.0f
    };

    presets["Glass"] = {
        ImVec4(0.15f, 0.15f, 0.17f, 0.7f),
        ImVec4(0.95f, 0.95f, 0.95f, 1.0f),
        ImVec4(0.40f, 0.60f, 1.0f, 1.0f),
        12.0f, 0.85f
    };

    presets["Neon"] = {
        ImVec4(0.05f, 0.07f, 0.12f, 1.0f),
        ImVec4(0.90f, 1.00f, 1.00f, 1.0f),
        ImVec4(0.00f, 0.90f, 1.00f, 1.0f),
        10.0f, 1.0f
    };

    presets["Solarized"] = {
        ImVec4(0.00f, 0.17f, 0.21f, 1.0f),
        ImVec4(0.99f, 0.96f, 0.89f, 1.0f),
        ImVec4(0.55f, 0.68f, 0.00f, 1.0f),
        9.0f, 1.0f
    };

    presets["Cyberpunk"] = {
        ImVec4(0.10f, 0.00f, 0.18f, 1.0f),
        ImVec4(1.00f, 0.90f, 0.30f, 1.0f),
        ImVec4(1.00f, 0.00f, 0.80f, 1.0f),
        10.0f, 1.0f
    };

    currentPreset = "Dark";
    currentStyle = presets[currentPreset];
    ApplyGlobalStyle();
}

void UIStyleManager::ApplyGlobalStyle()
{
    const auto& p = currentStyle;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = p.rounding;
    style.FrameRounding  = p.rounding * 0.5f;
    style.GrabRounding   = p.rounding * 0.3f;
    style.Alpha          = p.alpha;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = p.bgColor;
    colors[ImGuiCol_Text] = p.textColor;
    colors[ImGuiCol_Button] = p.accentColor;
    colors[ImGuiCol_ButtonHovered] = LerpColor(p.accentColor, ImVec4(1,1,1,1), 0.2f);
}

void UIStyleManager::ApplyPreset(const std::string& name)
{
    if (HasPreset(name)) {
        currentPreset = name;
        currentStyle = presets[name];
        ApplyGlobalStyle();
    }
}

void UIStyleManager::PushStyle(const std::string& name)
{
    if (!HasPreset(name)) return;
    const auto& p = presets[name];
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, p.rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, p.alpha);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bgColor);
    ImGui::PushStyleColor(ImGuiCol_Text, p.textColor);
}

void UIStyleManager::PopStyle()
{
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void UIStyleManager::BeginStyledWindow(const std::string& title, bool* open, ImGuiWindowFlags flags)
{
    PushStyle(currentPreset);
    ImGui::Begin(title.c_str(), open, flags);
}
void UIStyleManager::EndStyledWindow()
{
    ImGui::End();
    PopStyle();
}

void UIStyleManager::BeginStyledChild(const std::string& id, ImVec2 size, bool border, ImGuiWindowFlags flags)
{
    PushStyle("Glass");
    ImGui::BeginChild(id.c_str(), size, border, flags);
}
void UIStyleManager::EndStyledChild()
{
    ImGui::EndChild();
    PopStyle();
}

void UIStyleManager::BeginStyledPopup(const std::string& id, ImGuiWindowFlags flags)
{
    PushStyle("Dark");
    ImGui::BeginPopup(id.c_str(), flags);
}
void UIStyleManager::EndStyledPopup()
{
    ImGui::EndPopup();
    PopStyle();
}

void UIStyleManager::BeginStyledTable(const std::string& id, int columns, ImGuiTableFlags flags)
{
    PushStyle("Light");
    ImGui::BeginTable(id.c_str(), columns, flags);
}
void UIStyleManager::EndStyledTable()
{
    ImGui::EndTable();
    PopStyle();
}

void UIStyleManager::TransitionTo(const std::string& name, float speed)
{
    if (!HasPreset(name) || name == currentPreset) return;
    transitioning = true;
    nextPreset = name;
    transitionProgress = 0.0f;
    transitionSpeed = speed;
    targetStyle = presets[name];
}

void UIStyleManager::UpdateTransition()
{
    if (!transitioning) return;
    transitionProgress += ImGui::GetIO().DeltaTime * transitionSpeed;
    float t = std::min(transitionProgress, 1.0f);

    currentStyle.bgColor   = LerpColor(currentStyle.bgColor, targetStyle.bgColor, t);
    currentStyle.textColor = LerpColor(currentStyle.textColor, targetStyle.textColor, t);
    currentStyle.accentColor = LerpColor(currentStyle.accentColor, targetStyle.accentColor, t);
    currentStyle.rounding = currentStyle.rounding + (targetStyle.rounding - currentStyle.rounding) * t;
    currentStyle.alpha = currentStyle.alpha + (targetStyle.alpha - currentStyle.alpha) * t;

    ApplyGlobalStyle();

    if (t >= 1.0f) {
        transitioning = false;
        currentPreset = nextPreset;
        currentStyle = targetStyle;
    }
}

void UIStyleManager::PushTemporaryColor(ImGuiCol target, const ImVec4& color)
{
    ImGui::PushStyleColor(target, color);
}
void UIStyleManager::PopTemporaryColor()
{
    ImGui::PopStyleColor();
}
