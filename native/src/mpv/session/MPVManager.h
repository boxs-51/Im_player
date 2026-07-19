#pragma once

#include "MPVSession.h"
#include <memory>
#include <string>
#include <unordered_map>

class MPVManager {
public:
    static MPVManager& GetInstance();

    MPVSession* CreateSession(const std::string& id, WindowRuntime* runtime);
    MPVSession* GetSession(const std::string& id);
    MPVSession* RegisterSession(std::unique_ptr<MPVSession> session);
    void DestroySession(const std::string& id);

    MPVSession* GetDefaultSession();

private:
    MPVManager() = default;
    std::unordered_map<std::string, std::unique_ptr<MPVSession>> m_sessions;
    std::string m_defaultSessionId;
};