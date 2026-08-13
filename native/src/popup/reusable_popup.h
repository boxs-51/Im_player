#pragma once
#include <string>
#include <functional>
#include <imgui.h>
class WindowRuntime;

// Cấu trúc chứa toàn bộ trạng thái tương tác của Popup
struct PopupState {
    bool isHovered = false;         // Con trỏ chuột đang nằm trên Popup
    bool isFocused = false;         // Popup đang nhận Focus
    bool isClicked = false;         // Người dùng nhấp chuột trái/phải vào Popup
    bool isAnyItemActive = false;   // Người dùng đang tương tác với 1 widget bên trong (Drag, Type, Combo...)
    bool isAnyItemHover = false;    //
    
    // Toán tử chuyển đổi ngầm định sang bool (Trả về true nếu có BẤM CỨ TƯƠNG TÁC NÀO)
    operator bool() const {
        return isHovered || isFocused || isClicked || isAnyItemActive || isAnyItemHover;
    }
};

class ReusablePopup {
public:
    ReusablePopup() = default;

    // Mở popup với title và callback
    using ContentCallback = std::function<void(WindowRuntime*, bool&)>;

    void Open(const std::string& title, ContentCallback contentFunc);

    // Render popup mỗi frame
    /**
     * @brief Render Popup và trả về trạng thái tương tác
     * @return PopupState chứa thông tin chi tiết (hoặc ép kiểu về bool)
     */
    PopupState Render(WindowRuntime* runtime);

    // Kiểm tra popup đang mở
    bool IsOpen() const;

    // Đóng popup
    void Close();

private:
    std::string title_;
    ContentCallback contentCallback;
    bool open = false;

    // Vị trí popup
    ImVec2 lastPopupPos = {0,0};
    bool positionInitialized = false;

    
};
