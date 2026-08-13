#include "globals.h"

#include "popup/reusable_popup.h"
#include "windows/WindowRuntime.h"
#include "gui/gui.h"
#include <imgui.h>

void ReusablePopup::Open(const std::string& title, ContentCallback contentFunc) {
    title_ = title;
    contentCallback = contentFunc;
    open = true;
    positionInitialized = false; // đặt lại vị trí lần đầu
}

PopupState ReusablePopup::Render(WindowRuntime* runtime) {
    PopupState state{};

    if (!open || !contentCallback)
        return state;

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
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 350), ImVec2(FLT_MAX, FLT_MAX));

    // --- BẮT ĐẦU STYLE HIỆN ĐẠI ---
    CSImGui::PushModernWindowStyle();

    // Loại bỏ thanh cuộn nếu không cần để UI mượt hơn
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse; 
    
    if (ImGui::Begin(title_.c_str(), &open, window_flags)) {

        // --- BẮT TRẠNG THÁI TƯƠNG TÁC CỦA POPUP ---
        // 1. Kiểm tra con trỏ chuột có đang Hover trên Popup (kể cả item con)
        state.isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows);
        
        // 2. Kiểm tra Popup có đang được Focus hay không
        state.isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        // 3. Kiểm tra Click vào bất kỳ vùng nào thuộc Popup (chuột trái hoặc chuột phải)
        state.isClicked = state.isHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right));

        // 4. Kiểm tra có Widget con nào bên trong Popup đang Active (ví dụ: đang gõ input, kéo slider)
        state.isAnyItemActive = ImGui::IsAnyItemActive();

        state.isAnyItemHover = ImGui::IsAnyItemHovered();

        if( state.isClicked || state.isAnyItemActive || state.isAnyItemHover) {
            std::lock_guard<std::mutex> lock(runtime->stateMutex);
            runtime->state.runtime.is_dirty = true;
        }

        // Vẽ nội dung bên trong
        // Lưu ý: Bên trong contentCallback, bạn nên gọi các hàm BeginModernChild đã hướng dẫn ở trên
        contentCallback(runtime, closeRequested);

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
    return state;
}

bool ReusablePopup::IsOpen() const {
    return open;
}

void ReusablePopup::Close() {
    open = false;
}
