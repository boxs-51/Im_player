#pragma once

#include <imgui.h>
#include <imgui_internal.h> // Để truy cập ImGui::GetActiveID() và GetHoveredID()

struct UIState {
    // =========================================================
    // 1. TRẠNG THÁI TOÀN CỤC (GLOBAL MOUSE & WINDOW)
    // =========================================================
    bool isGlobalHovered = false;       // Chuột đang nằm trên bất kỳ Window/Widget nào
    bool isGlobalClicked = false;       // Click chuột trái bất kỳ đâu
    bool isGlobalRightClicked = false;  // Click chuột phải bất kỳ đâu
    bool isGlobalDoubleClicked = false; // Double click chuột trái bất kỳ đâu
    
    ImVec2 mousePos = ImVec2(0, 0);     // Tọa độ chuột
    ImVec2 mouseDelta = ImVec2(0, 0);   // Khoảng dịch chuyển chuột (Hover Delta toàn cục)
    
    bool isWindowFocused = false;       // Window hiện tại có Focus
    bool isWindowUnfocused = false;     // Window VỪA MẤT Focus (Kích hoạt đúng 1 frame)

    // =========================================================
    // 2. TRẠNG THÁI ITEM BẤM KỲ (ANY/ACTIVE ITEM STATES - TỰ ĐỘNG)
    // =========================================================
    bool isAnyItemHovered = false;      // Đang di chuột lên BẤM KỲ item nào
    bool isAnyItemActive = false;       // BẤM KỲ item nào đang được giữ/kéo/tương tác (Drag slider, gõ text...)
    bool isAnyItemFocused = false;      // BẤM KỲ item nào đang giữ Focus
    bool isAnyItemEdited = false;       // BẤM KỲ item nào vừa bị sửa đổi giá trị trong frame này
    
    ImGuiID activeItemID = 0;           // ID của item đang Active/Tương tác
    ImGuiID hoveredItemID = 0;          // ID của item đang được Hover
    
    bool isAnyItemClicked = false;      // Click chuột trái vào BẤM KỲ item nào
    bool isAnyItemRightClicked = false; // Click chuột phải vào BẤM KỲ item nào
    bool isAnyItemDoubleClicked = false;// Double click vào BẤM KỲ item nào
    
    ImVec2 activeItemMouseDelta = ImVec2(0, 0); // Delta chuyển động chuột khi đang giữ/kéo 1 item bất kỳ

    // =========================================================
    // 3. CỜ HÀNH ĐỘNG TỔNG HỢP (ACTION STATE)
    // =========================================================
    bool isAnyAction = false;           // Có BẤM KỲ hành động tương tác nào ở frame này không

    // =========================================================
    // 4. TRẠNG THÁI ITEM CỤ THỂ (Dùng khi cần soi riêng 1 widget)
    // =========================================================
    bool isSpecificItemHovered = false;
    bool isSpecificItemClicked = false;
    bool isSpecificItemActive = false;

private:
    bool lastWindowFocused = false;

public:
    /**
     * @brief Cập nhật TỰ ĐỘNG toàn bộ trạng thái (Toàn cục + Bất kỳ Item nào).
     * Gọi duy nhất 1 lần ở ĐẦU FRAME RENDER.
     */
    void UpdateGlobal() {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiContext* g = ImGui::GetCurrentContext();

        // 1. Chuột & Delta Toàn cục
        mousePos = io.MousePos;
        mouseDelta = io.MouseDelta;

        isGlobalClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        isGlobalRightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        isGlobalDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        // 2. Bắt trạng thái Item BẤM KỲ (Any Item) qua ImGui Context
        isAnyItemHovered = ImGui::IsAnyItemHovered();
        isAnyItemActive = ImGui::IsAnyItemActive();
        isAnyItemFocused = ImGui::IsAnyItemFocused();
        isAnyItemEdited = ImGui::IsItemEdited(); // Trạng thái edit vừa xảy ra

        // Trích xuất ID hệ thống của Item đang Hover/Active
        hoveredItemID = ImGui::GetHoveredID();
        activeItemID = ImGui::GetActiveID();

        // Kiểm tra Click lên BẤM KỲ Item nào
        isAnyItemClicked = isAnyItemHovered && isGlobalClicked;
        isAnyItemRightClicked = isAnyItemHovered && isGlobalRightClicked;
        isAnyItemDoubleClicked = isAnyItemHovered && isGlobalDoubleClicked;

        // Delta di chuyển chuột khi đang thao tác trên Item
        if (isAnyItemActive || isAnyItemHovered) {
            activeItemMouseDelta = mouseDelta;
        } else {
            activeItemMouseDelta = ImVec2(0.0f, 0.0f);
        }

        // 3. Hover toàn cục (Nằm trên Window hoặc Item)
        isGlobalHovered = isAnyItemHovered || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);

        // 4. Focus / Unfocus Window
        bool currentWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        isWindowUnfocused = (lastWindowFocused && !currentWindowFocused);
        isWindowFocused = currentWindowFocused;
        lastWindowFocused = currentWindowFocused;

        // 5. Cờ Action tổng hợp (True nếu có bất kỳ di chuyển, click, hay sửa đổi nào)
        isAnyAction = isGlobalClicked || isGlobalRightClicked || isGlobalDoubleClicked ||
                      isAnyItemActive || isAnyItemEdited ||
                      (isGlobalHovered && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f));
    }

    /**
     * @brief Optional: Chỉ dùng nếu bạn muốn soi riêng trạng thái của MỘT WIDGET CỤ THỂ vừa render.
     */
    void UpdateSpecificItem() {
        isSpecificItemHovered = ImGui::IsItemHovered();
        isSpecificItemClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        isSpecificItemActive = ImGui::IsItemActive();
    }
};