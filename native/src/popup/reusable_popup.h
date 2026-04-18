#pragma once
#include <string>
#include <functional>
#include <imgui.h>

class ReusablePopup {
public:
    ReusablePopup() = default;

    // Mở popup với title và callback
    void Open(const std::string& title, std::function<void(bool&)> contentFunc);

    // Render popup mỗi frame
    void Render();

    // Kiểm tra popup đang mở
    bool IsOpen() const;

    // Đóng popup
    void Close();

private:
    std::string title_;
    std::function<void(bool&)> contentCallback;
    bool open = false;

    // Vị trí popup
    ImVec2 lastPopupPos = {0,0};
    bool positionInitialized = false;

    
};
