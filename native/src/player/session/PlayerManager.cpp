#include "PlayerManager.h"

#include <chrono>
PlayerManager &PlayerManager::GetInstance()
{
    static PlayerManager instance;
    return instance;
}

std::string PlayerManager::GenerateUniqueSessionId() {
    // Không tự khóa mutex ở đây vì hàm này được gọi bên trong các hàm đã khóa m_sessionsMutex
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mpv_session_" + std::to_string(now) + "_" + std::to_string(++m_autoIdCounter);
}

PlayerSession *PlayerManager::CreateSession(const std::string &id, WindowRuntime *runtime)
{   
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    std::string sessionId = id.empty() ? GenerateUniqueSessionId() : id;

    if (m_sessions.count(id))
    {
        return nullptr; // Session đã tồn tại
    }

    auto session = std::make_unique<PlayerSession>(id);
    if (session->Init(runtime))
    {
        if (m_defaultSessionId.empty())
        {
            m_defaultSessionId = id;
        }
        auto *ptr = session.get();
        m_sessions[id] = std::move(session);
        return ptr;
    }
    return nullptr;
}

PlayerSession *PlayerManager::RegisterSession(std::unique_ptr<PlayerSession> session)
{
    if (!session)
    {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    const std::string &id = session->GetId();

    if (m_sessions.count(id))
    {
        return nullptr; // Session đã tồn tại
    }
    if (m_defaultSessionId.empty())
    {
        m_defaultSessionId = id;
    }
    auto *ptr = session.get();
    m_sessions[id] = std::move(session);
    return ptr;
}

PlayerSession *PlayerManager::GetSession(const std::string &id)
{
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(id);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}

void PlayerManager::DestroySession(const std::string &id)
{
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions.erase(id);
    if (m_defaultSessionId == id)
    {
        m_defaultSessionId.clear();
    }
}

PlayerSession *PlayerManager::GetDefaultSession()
{
    return GetSession(m_defaultSessionId);
}