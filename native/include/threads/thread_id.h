#pragma once
#include <functional>

enum class ThreadID {
    URLFetch,
    PlaylistLoader,
    ResolutionFetch,
    // ...
};

// Specialize std::hash<ThreadID> inline to avoid redefinition
namespace std {
    template <>
    struct hash<ThreadID> {
        std::size_t operator()(const ThreadID& id) const noexcept {
            return std::hash<std::underlying_type_t<ThreadID>>{}(static_cast<std::underlying_type_t<ThreadID>>(id));
        }
    };
}
