#include "globals.h"

#include <popup/reusable_popup.h>
#include "windows/windows_borderless_state.h"
#include <gui/gui.h>
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

    // Thiết lập vị trí
    if (!positionInitialized) {
        if (lastPopupPos.x != 0 || lastPopupPos.y != 0) {
            ImGui::SetNextWindowPos(lastPopupPos, ImGuiCond_Appearing);
        } else {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        positionInitialized = true;
    }

    // Tăng kích thước mặc định nếu cần để trông cân đối hơn
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(1920, 1080));

    // --- BẮT ĐẦU STYLE HIỆN ĐẠI ---
    CSImGui::PushModernWindowStyle();

    // Loại bỏ thanh cuộn nếu không cần để UI mượt hơn
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse; 
    
    if (ImGui::Begin(title_.c_str(), &open, window_flags)) {
        
        // Vẽ nội dung bên trong
        // Lưu ý: Bên trong contentCallback, bạn nên gọi các hàm BeginModernChild đã hướng dẫn ở trên
        contentCallback(closeRequested);

        if (closeRequested)
            open = false;

        if (open)
            lastPopupPos = ImGui::GetWindowPos();

        ImGui::End();
    }
    
    // --- KẾT THÚC STYLE HIỆN ĐẠI ---
    CSImGui::PopModernWindowStyle();

    if (!open) {
        // Reset logic khi đóng hẳn
        positionInitialized = false; 
    }
}

bool ReusablePopup::IsOpen() const {
    return open;
}

void ReusablePopup::Close() {
    open = false;
}
