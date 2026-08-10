#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

#include "PlayerSession.h"

class PlayerManager {
public:
    static PlayerManager& GetInstance();

    // Tự động sinh ID nếu id == ""
    PlayerSession* CreateSession(const std::string& id, WindowRuntime* runtime);
    PlayerSession* CreateSession(WindowRuntime* runtime) { return CreateSession("", runtime); }

    PlayerSession* GetSession(const std::string& id);

    PlayerSession* RegisterSession(std::unique_ptr<PlayerSession> session);

    // Hủy Session theo ID hoặc theo con trỏ
    void DestroySession(const std::string& id);
    void DestroySession(PlayerSession* session);

    PlayerSession* GetDefaultSession();

    // Lấy danh sách ID của toàn bộ Session
    std::vector<std::string> GetAllSessionIds();

    // Lấy danh sách con trỏ tất cả Session
    std::vector<PlayerSession*> GetAllSessions();

private:
    PlayerManager() = default;

    std::string GenerateUniqueSessionId();

    std::unordered_map<std::string, std::unique_ptr<PlayerSession>> m_sessions;
    std::string m_defaultSessionId;
    std::mutex m_sessionsMutex;
    uint64_t m_autoIdCounter = 0;
};