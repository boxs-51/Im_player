#include "player/scripts/script_manager.h"

#include <log.h>

namespace fs = std::filesystem;

ScriptManager& ScriptManager::Instance() {
    static ScriptManager inst;
    return inst;
}
void ScriptManager::Init(mpv_handle* mpvHandle) {
    mpv = mpvHandle;
}
void ScriptManager::LoadScriptFromFolder(const std::vector<std::string>& folders) {
    for (const auto& folderPath : folders) {
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) continue;

        for (const auto& entry : fs::directory_iterator(folderPath)) {
            if (!entry.is_regular_file()) continue;

            fs::path p = entry.path();
            if (p.extension() == ".lua") {
                std::string fullPath = p.string();
                
                // Nếu chưa có trong danh sách quản lý thì mới thêm vào
                if (m_scripts.find(fullPath) == m_scripts.end()) {
                    ScriptInfo info;
                    info.path = fullPath;
                    info.name = p.stem().string();
                    
                    // Thử load vào MPV
                    const char* args[] = { "load-script", fullPath.c_str(), nullptr };
                    int res = mpv_command(mpv, args);
                    
                    info.isLoaded = (res >= 0);
                    info.enabled = info.isLoaded;

                    m_scripts[fullPath] = info;
                    
                    if (info.isLoaded) {
                        LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] Loaded script: %s (Command result: %d)", fullPath, res);
                    } else {
                        LOG(1, LogLevel::Error, LogCategory::System, "[ERROR] [MPV ERROR] Failed to load script: %s (Error code: %d)", fullPath, res);
                    }
                }
            }
        }
    }
}

void ScriptManager::LoadScript(const std::vector<std::string>& scriptPaths) {
    for (const auto& path : scriptPaths) {
        if (m_scripts.find(path) != m_scripts.end()) continue; // Đã có rồi

        fs::path p(path);
        if (p.extension() == ".lua" && fs::exists(p) && fs::is_regular_file(p)) {
            ScriptInfo info;
            info.path = p.string();
            info.name = p.stem().string();

            const char* args[] = { "load-script", info.path.c_str(), nullptr };
            int res = mpv_command(mpv, args);
            
            info.isLoaded = (res >= 0);
            info.enabled = info.isLoaded;

            m_scripts[info.path] = info;

            if (info.isLoaded) {
                LOG(1,  LogLevel::Info, LogCategory::System, "[DEBUG] [INFO] Loaded script: %s (Command result: %d)", info.path, res);
            } else {
                LOG(1,  LogLevel::Error, LogCategory::System, "[ERROR] [MPV ERROR] Failed to load script: %s (Error code: %d)", info.path, res);
            }
        }
    }
}

// Trả về toàn bộ struct để biết script nào đang bật/tắt
std::vector<ScriptInfo> ScriptManager::GetAllScripts() {
    std::vector<ScriptInfo> list;
    for (auto const& [path, info] : m_scripts) {
        list.push_back(info);
    }
    return list;
}

void ScriptManager::ToggleScript(const std::string& path, bool enable) {
    auto it = m_scripts.find(path);
    if (it == m_scripts.end() || !mpv) return;

    if (enable && !it->second.enabled) {
        // Bật: Load lại vào MPV
        const char* args[] = { "load-script", path.c_str(), nullptr };
        if (mpv_command(mpv, args) >= 0) {
            it->second.enabled = true;
            it->second.isLoaded = true;
        }
    } 
    else if (!enable && it->second.enabled) {
        // Tắt: Gỡ khỏi MPV dựa vào tên (stem)
        const char* args[] = { "script-remove", it->second.path.c_str(), nullptr };
        if (mpv_command(mpv, args) >= 0) {
            it->second.enabled = false;
            it->second.isLoaded = false;
        }
    }
}

void ScriptManager::RemoveScript(const std::string& path) {
    auto it = m_scripts.find(path);
    if (it != m_scripts.end()) {
        // Gỡ khỏi MPV trước
        if (it->second.isLoaded) {
            const char* args[] = { "script-remove", it->second.path.c_str(), nullptr };
            if(mpv_command(mpv, args) >= 0){
                m_scripts.erase(it);
            }
        }
        // Sau đó xóa khỏi map
        
    }
}