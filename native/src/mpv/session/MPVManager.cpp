#include "MPVManager.h"

MPVManager& MPVManager::GetInstance() {
    static MPVManager instance;
    return instance;
}

MPVSession* MPVManager::CreateSession(const std::string& id, WindowRuntime* runtime) {
    if (m_sessions.count(id)) {
        return nullptr; // Session đã tồn tại
    }

    auto session = std::make_unique<MPVSession>(id);
    if (session->Init(runtime)) {
        if (m_defaultSessionId.empty()) {
            m_defaultSessionId = id;
        }
        auto* ptr = session.get();
        m_sessions[id] = std::move(session);
        return ptr;
    }
    return nullptr;
}

MPVSession* MPVManager::RegisterSession(std::unique_ptr<MPVSession> session) {
    if (!session) {
        return nullptr;
    }
    const std::string& id = session->GetId();
    if (m_sessions.count(id)) {
        return nullptr; // Session đã tồn tại
    }
    if (m_defaultSessionId.empty()) {
        m_defaultSessionId = id;
    }
    auto* ptr = session.get();
    m_sessions[id] = std::move(session);
    return ptr;
}

MPVSession* MPVManager::GetSession(const std::string& id) {
    auto it = m_sessions.find(id);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}

void MPVManager::DestroySession(const std::string& id) {
    m_sessions.erase(id);
    if (m_defaultSessionId == id) {
        m_defaultSessionId.clear();
    }
}

MPVSession* MPVManager::GetDefaultSession() {
    return GetSession(m_defaultSessionId);
}