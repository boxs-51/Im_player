#pragma once
#include <string>
#include <memory>
#include <Windows.h>

class WindowRuntime;

class WindowController {
private:
    WindowRuntime* runtime;

public:
    explicit WindowController(WindowRuntime* rt) : runtime(rt) {}

    // --- Các hàm cơ bản cũ ---
    void Move(int x, int y);
    void Resize(int w, int h);
    void ToggleFullscreen();
    void Close();
    void SetTitle(const std::string& title);
    void Maximize();
    void Minimize();
    void Restore();
    void SetOpacity(float opacity);

    // --- CÁC LỆNH ĐIỀU KHIỂN NÂNG CẤP MỚI ---
    
    /**
     * @brief Ghim hoặc bỏ ghim cửa sổ luôn hiển thị trên cùng (Always-On-Top).
     */
    void SetAlwaysOnTop(bool enable);
    void ToggleAlwaysOnTop();

    /**
     * @brief Đặt tiêu điểm Focus vào cửa sổ hiện tại.
     */
    void Focus();

    /**
     * @brief Ẩn / Hiện cửa sổ.
     */
    void Show();
    void Hide();

    /**
     * @brief Căn giữa cửa sổ ra giữa màn hình Monitor chứa nó.
     */
    void CenterOnScreen();

    /**
     * @brief Bật/Tắt chế độ Borderless (Khung viền cửa sổ).
     */
    void SetBordered(bool bordered);

    /**
     * @brief Nhấp nháy thanh Taskbar để gây sự chú ý tới người dùng (Notification Flash).
     */
    void Flash(bool start = true);
};