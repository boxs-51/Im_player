#include "PlayerManager.h"

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

PlayerManager &PlayerManager::GetInstance()
{
    static PlayerManager instance;
    return instance;
}

std::string PlayerManager::GenerateUniqueSessionId() {
    // Called while m_sessionsMutex is held.
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mpv_session_" + std::to_string(now) + "_" + std::to_string(++m_autoIdCounter);
}

PlayerSession *PlayerManager::CreateSession(const std::string &id, WindowRuntime *runtime)
{
    std::string sessionId;

    // Reserve the ID under the manager lock, but do not perform heavyweight
    // PlayerSession initialization while holding the lock.
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        sessionId = id.empty() ? GenerateUniqueSessionId() : id;

        if (m_sessions.count(sessionId) || m_pendingSessionIds.count(sessionId))
            return nullptr;

        m_pendingSessionIds.insert(sessionId);
    }

    std::unique_ptr<PlayerSession> session;
    bool initialized = false;

    try {
        session = std::make_unique<PlayerSession>(sessionId);
        initialized = session->Init(runtime);
    }
    catch (...) {
        // A failed constructor/external Init must never leave the ID reserved.
        // Cleanup is manager-state only and happens before propagating the
        // original exception to the caller.
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_pendingSessionIds.erase(sessionId);
        throw;
    }

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_pendingSessionIds.erase(sessionId);

        if (!initialized)
            return nullptr;

        // Defensive duplicate check in case another registration path claimed
        // the ID while initialization was running.
        if (m_sessions.count(sessionId))
            return nullptr;

        if (m_defaultSessionId.empty())
            m_defaultSessionId = sessionId;

        auto *ptr = session.get();
        m_sessions.emplace(sessionId, std::move(session));
        return ptr;
    }
}

PlayerSession *PlayerManager::RegisterSession(std::unique_ptr<PlayerSession> session)
{
    if (!session)
        return nullptr;

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    const std::string id = session->GetId();

    if (m_sessions.count(id) || m_pendingSessionIds.count(id))
        return nullptr;

    if (m_defaultSessionId.empty())
        m_defaultSessionId = id;

    auto *ptr = session.get();
    m_sessions.emplace(id, std::move(session));
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
    std::unique_ptr<PlayerSession> detached;

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(id);
        if (it == m_sessions.end())
            return;

        detached = std::move(it->second);
        m_sessions.erase(it);

        if (m_defaultSessionId == id)
            m_defaultSessionId.clear();
    }

    // Heavy Shutdown/destructor work occurs after releasing m_sessionsMutex.
    detached.reset();
}

void PlayerManager::DestroySession(PlayerSession *session)
{
    if (!session)
        return;

    const std::string id = session->GetId();
    DestroySession(id);
}

void PlayerManager::DestroyAllSessions()
{
    std::unordered_map<std::string, std::unique_ptr<PlayerSession>> detached;

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        detached.swap(m_sessions);
        m_defaultSessionId.clear();
    }

    // Destroy all sessions outside the manager lock while process/runtime
    // services are still alive and normal shutdown ordering is controlled.
    detached.clear();
}

PlayerSession *PlayerManager::GetDefaultSession()
{
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(m_defaultSessionId);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> PlayerManager::GetAllSessionIds()
{
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    std::vector<std::string> result;
    result.reserve(m_sessions.size());
    for (const auto &[id, session] : m_sessions)
        result.push_back(id);
    return result;
}

std::vector<PlayerSession*> PlayerManager::GetAllSessions()
{
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    std::vector<PlayerSession*> result;
    result.reserve(m_sessions.size());
    for (auto &[id, session] : m_sessions) {
        if (session)
            result.push_back(session.get());
    }
    return result;
}
