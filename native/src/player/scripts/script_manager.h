#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <map>
#include "mpv/client.h"


struct ScriptInfo {
    std::string path;      // Đường dẫn đầy đủ
    std::string name;      // Tên file (không có đuôi .lua)
    bool enabled = true;   // Trạng thái bật/tắt
    bool isLoaded = false; // Đã load thành công vào MPV chưa
};

class ScriptManager
{
private:
    ScriptManager() = default; // Private constructor cho Singleton
    mpv_handle* mpv = nullptr;
    std::map<std::string, ScriptInfo> m_scripts; // Key là path để đảm bảo duy nhất

public:
    static ScriptManager& Instance();

    // Ngăn chặn copy singleton
    ScriptManager(const ScriptManager&) = delete;
    void operator=(const ScriptManager&) = delete;

    void Init(mpv_handle* mpvHandle);

    void LoadScriptFromFolder(const std::vector<std::string>& scriptPaths);
    void LoadScript(const std::vector<std::string>& scriptPaths);

    void ToggleScript(const std::string& path, bool enable);
    void RemoveScript(const std::string& path);

    std::vector<ScriptInfo> GetAllScripts();

};



