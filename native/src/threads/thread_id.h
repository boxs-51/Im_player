#pragma once
#include <string>
#include <type_traits>
#include <iostream>

/**
 * @brief ThreadID Linh Hoạt (Dynamic Thread ID)
 * Hỗ trợ nhận tự động từ: const char*, std::string, hoặc BẤT KỲ Enum nào bạn tự định nghĩa.
 */
class ThreadID {
public:
    // 1. Nhận chuỗi hằng (C-string)
    ThreadID(const char* name) : id_(name) {}

    // 2. Nhận std::string
    ThreadID(std::string name) : id_(std::move(name)) {}

    // 3. Nhận BẤT KỲ enum nào (C++ Enum class hoặc Enum thường)
    template <typename EnumType, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
    ThreadID(EnumType enumVal) {
        // Tự động chuyển giá trị Enum thành chuỗi "Enum_<số_thứ_tự>"
        id_ = "Enum_" + std::to_string(static_cast<std::underlying_type_t<EnumType>>(enumVal));
    }

    // Lấy chuỗi định danh
    const std::string& ToString() const { return id_; }

    // Toán tử so sánh để dùng làm Key cho unordered_map/set
    bool operator==(const ThreadID& other) const { return id_ == other.id_; }
    bool operator<(const ThreadID& other) const { return id_ < other.id_; }

private:
    std::string id_;
};

// Đăng ký std::hash cho ThreadID
namespace std {
    template <>
    struct hash<ThreadID> {
        std::size_t operator()(const ThreadID& id) const noexcept {
            return std::hash<std::string>{}(id.ToString());
        }
    };
}