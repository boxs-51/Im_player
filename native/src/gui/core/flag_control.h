#include <type_traits>

// ============================================================
// Generic Bitwise Flag Helpers (Tái sử dụng cho mọi loại Flags)
// ============================================================

namespace CSImGui {

// Helper lấy kiểu số nguyên nền (underlying integer type) cho cả enum lẫn integer type
template <typename T>
struct flag_underlying {
    using type = std::conditional_t<std::is_enum_v<T>, std::underlying_type_t<T>, T>;
};

template <typename T>
using flag_underlying_t = typename flag_underlying<T>::type;


// 1. Check: Kiểm tra xem value có chứa TẤT CẢ các bit trong flag không
template <typename TFlags, typename TFlag>
constexpr bool HasFlag(TFlags value, TFlag flag)
{
    using U = flag_underlying_t<TFlags>;
    U flag_val = static_cast<U>(flag);
    return flag_val != 0 && (static_cast<U>(value) & flag_val) == flag_val;
}

// 2. Check Any: Kiểm tra xem value có chứa BẤT KỲ bit nào trong flag không (dành cho composite flags)
template <typename TFlags, typename TFlag>
constexpr bool HasAnyFlag(TFlags value, TFlag flag)
{
    using U = flag_underlying_t<TFlags>;
    return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
}

// 3. Set: Bật một hoặc nhiều flags
template <typename TFlags, typename TFlag>
constexpr TFlags SetFlag(TFlags value, TFlag flag, bool enabled = true)
{
    using U = flag_underlying_t<TFlags>;
    if (enabled) {
        return static_cast<TFlags>(static_cast<U>(value) | static_cast<U>(flag));
    } else {
        return static_cast<TFlags>(static_cast<U>(value) & ~static_cast<U>(flag));
    }
}

// 4. Unset/Clear: Tắt một hoặc nhiều flags
template <typename TFlags, typename TFlag>
constexpr TFlags ClearFlag(TFlags value, TFlag flag)
{
    using U = flag_underlying_t<TFlags>;
    return static_cast<TFlags>(static_cast<U>(value) & ~static_cast<U>(flag));
}

// 5. Toggle: Đảo trạng thái một hoặc nhiều flags (Bật -> Tắt, Tắt -> Bật)
template <typename TFlags, typename TFlag>
constexpr TFlags ToggleFlag(TFlags value, TFlag flag)
{
    using U = flag_underlying_t<TFlags>;
    return static_cast<TFlags>(static_cast<U>(value) ^ static_cast<U>(flag));
}

} // namespace CSImGui