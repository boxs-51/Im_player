#pragma once

#include "PlayerSession.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

class WindowRuntime;
class PlayerSession;

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

    // ==========================================
    // Safe Chain Accessors (Bảo vệ khỏi nullptr)
    // ==========================================

    /**
     * @brief Thực thi an toàn trên một Session cụ thể theo ID.
     */
    template <typename Func>
    void WithSession(const std::string& id, Func&& func) {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(id);
        if (it != m_sessions.end() && it->second) {
            func(*it->second);
        }
    }

    Player* GetPlayer(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetPlayer() : nullptr;
    }

    PlayBackRender* GetRenderer(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetRenderer() : nullptr;
    }

    PlayBackRenderThread* GetRenderThread(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetRenderThread() : nullptr;
    }

    PlaybackObserver* GetObserver(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetObserver() : nullptr;
    }

    PlaybackCommand* GetCommander(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetCommander() : nullptr;
    }

    PlayBackProperty* GetProperty(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetProperty() : nullptr;
    }

    PlayerStateSystem* GetState(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetState() : nullptr;
    }

    AudioFilterManager* GetAudioFilterManager(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetAudioFilterManager() : nullptr;
    }

    VideoFilterManager* GetVideoFilterManager(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetVideoFilterManager() : nullptr;
    }
    ShaderManager* GetShaderManager(const std::string& id) {
        auto* session = GetSession(id);
        return session ? session->GetShaderManager() : nullptr;
    }



private:
    PlayerManager() = default;

    std::string GenerateUniqueSessionId();

    std::unordered_map<std::string, std::unique_ptr<PlayerSession>> m_sessions;
    std::string m_defaultSessionId;
    std::mutex m_sessionsMutex;
    uint64_t m_autoIdCounter = 0;
};