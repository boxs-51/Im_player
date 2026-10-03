#pragma once

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

namespace LifecycleEvidence {

inline std::mutex& EvidenceMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline void Emit(
    std::string_view component,
    std::string_view phase,
    std::string_view identity = {})
{
    char line[768]{};
    const DWORD pid = GetCurrentProcessId();
    const DWORD tid = GetCurrentThreadId();

    const char* componentData = component.empty() ? "" : component.data();
    const char* phaseData = phase.empty() ? "" : phase.data();
    const char* identityData = identity.empty() ? "" : identity.data();

    const int written = std::snprintf(
        line,
        sizeof(line),
        "[Lifecycle] phase=%.*s component=%.*s id=%.*s pid=%lu tid=%lu\n",
        static_cast<int>(phase.size()), phaseData,
        static_cast<int>(component.size()), componentData,
        static_cast<int>(identity.size()), identityData,
        static_cast<unsigned long>(pid),
        static_cast<unsigned long>(tid));

    if (written <= 0)
        return;

    OutputDebugStringA(line);

    const char* path = std::getenv("IM_PLAYER_LIFECYCLE_LOG");
    if (!path || !*path)
        return;

    std::lock_guard<std::mutex> lock(EvidenceMutex());

    FILE* file = nullptr;
    if (fopen_s(&file, path, "ab") != 0 || !file)
        return;

    const size_t length = static_cast<size_t>(
        written < static_cast<int>(sizeof(line)) ? written : sizeof(line) - 1);
    std::fwrite(line, 1, length, file);
    std::fclose(file);
}

inline void EmitDiagnostic(
    std::string_view category,
    std::string_view message)
{
    char line[1024]{};
    const DWORD pid = GetCurrentProcessId();
    const DWORD tid = GetCurrentThreadId();

    const int written = std::snprintf(
        line,
        sizeof(line),
        "[BRG5-DIAG] category=%.*s pid=%lu tid=%lu %.*s\n",
        static_cast<int>(category.size()), category.data(),
        static_cast<unsigned long>(pid),
        static_cast<unsigned long>(tid),
        static_cast<int>(message.size()), message.data());

    if (written <= 0)
        return;

    OutputDebugStringA(line);

    const char* path = std::getenv("IM_PLAYER_LIFECYCLE_LOG");
    if (!path || !*path)
        return;

    std::lock_guard<std::mutex> lock(EvidenceMutex());

    FILE* file = nullptr;
    if (fopen_s(&file, path, "ab") != 0 || !file)
        return;

    const size_t length = static_cast<size_t>(
        written < static_cast<int>(sizeof(line)) ? written : sizeof(line) - 1);
    std::fwrite(line, 1, length, file);
    std::fclose(file);
}

inline std::string PointerIdentity(const void* value)
{
    char buffer[2 + sizeof(void*) * 2 + 1]{};
    std::snprintf(buffer, sizeof(buffer), "%p", value);
    return std::string(buffer);
}

} // namespace LifecycleEvidence
