#pragma once
#include <string>
#include <functional>
#include <imgui.h>
class WindowRuntime;
class ReusablePopup {
public:
    ReusablePopup() = default;

    // Mở popup với title và callback
    using ContentCallback = std::function<void(WindowRuntime*, bool&)>;

    void Open(const std::string& title, ContentCallback contentFunc);

    // Render popup mỗi frame
    void Render(WindowRuntime* window);

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
