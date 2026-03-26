#include "globals.h"
#include "reusable_popup.h"
#include "windows/windows_borderless_state.h"
#include <imgui.h>

void ReusablePopup::Open(const std::string& title, std::function<void(bool&)> contentFunc) {
    title_ = title;
    contentCallback = contentFunc;
    open = true;
    positionInitialized = false; // đặt lại vị trí lần đầu
}

void ReusablePopup::Render() {
    if (!open || !contentCallback)
        return;

    bool closeRequested = false;

    // Đặt vị trí lần đầu ở giữa màn hình
    if (!positionInitialized) {
        if (lastPopupPos.x != 0 || lastPopupPos.y != 0) {
            ImGui::SetNextWindowPos(lastPopupPos, ImGuiCond_Appearing); // mở lại vị trí cũ
        } else {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
        }
        positionInitialized = true;
    }

    // Mở window thay cho popup
    if( g_DragResizeState.IsFullscreen_video || g_DragResizeState.IsMax || !g_WindowVisible)ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); 
    if (ImGui::Begin(title_.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
        contentCallback(closeRequested);

        // Nếu callback yêu cầu đóng
        if (closeRequested)
            open = false;

        // Lưu vị trí hiện tại
        if (open)
            lastPopupPos = ImGui::GetWindowPos();
        

        ImGui::End();
    } else {
        // Nếu window bị đóng bất ngờ
        open = false;
    }
}

bool ReusablePopup::IsOpen() const {
    return open;
}

void ReusablePopup::Close() {
    open = false;
}
