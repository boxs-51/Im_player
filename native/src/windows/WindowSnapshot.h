#pragma once
#include <mutex>
#include <cstdint>
#include "WindowDefs.h"

#ifdef IsMinimized
#undef IsMinimized
#endif

#ifdef IsMaximized
#undef IsMaximized
#endif


/**
 * @brief Class đại diện cho trạng thái của Window tại một frame cụ thể.
 * Thiết kế BẤT BIẾN (Immutable): Chỉ có thể đọc, không thể chỉnh sửa sau khi khởi tạo.
 */
class WindowSnapshot {
public:
    // Khởi tạo Snapshot từ live state (Constructor công khai nhận toàn bộ dữ liệu cần thiết)
    WindowSnapshot(
        const WindowState& state
    ) : 
        m_state(state)
    {}

    // Mặc định cho phép Move / Copy Construction để truyền Snapshot vào Render Thread
    WindowSnapshot(const WindowSnapshot&) = default;
    WindowSnapshot(WindowSnapshot&&) noexcept = default;

    // VÔ HIỆU HÓA toán tử gán để đảm bảo Snapshot không bị ghi đè dữ liệu sau khi tạo
    WindowSnapshot& operator=(const WindowSnapshot&) = delete;
    WindowSnapshot& operator=(WindowSnapshot&&) = delete;

    ~WindowSnapshot() = default;

    // --- READ-ONLY GETTERS ---

    // Tra về const reference để tránh chi phí copy dữ liệu lớn và ngăn chỉnh sửa
    const WindowState& GetState() const { return m_state; }
    const WindowLayout& GetLayout() const { return m_state.geometry.layout; }

    // Helper getters cho các trường thường dùng
    const WindowGeometryState& GetGeometry() const { return m_state.geometry; }
    const WindowDisplayState& GetDisplay() const { return m_state.display; }
    const WindowInputState& GetInput() const { return m_state.input; }

    bool IsVisible() const { return m_state.display.isVisible; }
    bool IsMinimized() const { return m_state.display.isMinimized; }
    bool IsMaximized() const { return m_state.display.isMaximized; }
    bool IsFullscreen() const { return m_state.display.isFullscreen; }
    bool IsRenderingEnabled() const { return m_state.runtime.renderingEnabled; }
    bool IsDirty() const { return m_state.runtime.is_dirty; }

private:
    WindowState m_state;
};