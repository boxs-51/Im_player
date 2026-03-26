#pragma once
#include <string>
#include <functional>

class ReusableWindow {
public:
    void Open(const std::string& id, const std::string& title, std::function<void(bool&)> contentFunc);
    void Render();
    void Close();
    bool IsOpen() const;

private:
    std::string id_;         // ID cố định để tránh trùng lặp
    std::string title_;      // Tên hiển thị
    bool open = false;
    std::function<void(bool&)> contentCallback;
};
