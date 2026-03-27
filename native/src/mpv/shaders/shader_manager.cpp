#include "mpv/shaders/shaders_manager.h"

ShaderManager& ShaderManager::Instance() {
    static ShaderManager inst;
    return inst;
}

void ShaderManager::Init(mpv_handle* mpvHandle) {

    mpv = mpvHandle;
}

void ShaderManager::Register(const std::string& name, const std::string& path) {
    shaders[name] = { name, path, false };
}
void ShaderManager::ApplyPipeline() {
    if (!mpv) return;

    std::string list;

    for (auto& name : pipeline) {
        auto& s = shaders[name];
        if (!s.enabled) continue;

        list += "\"" + s.path + "\";";
    }

    if (!list.empty())
        list.pop_back(); // remove ;

    std::string cmd = "set glsl-shaders \"" + list + "\"";
    mpv_command_string(mpv, cmd.c_str());
}
void ShaderManager::BuildPipeline(const std::vector<std::string>& order) {
    pipeline.clear();

    for (const auto& name : order) {
        auto it = shaders.find(name);
        if (it != shaders.end()) {
            pipeline.push_back(it->second.path);
            it->second.enabled = true;
        }
    }

    ApplyPipeline();
}
void ShaderManager::Enable(const std::string& name) {
    if (!mpv) return;
    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    auto& s = it->second;
    if (s.enabled) return;

    std::string cmd = "change-list glsl-shaders append " + s.path;
    mpv_command_string(mpv, cmd.c_str());

    s.enabled = true;
}

void ShaderManager::Disable(const std::string& name) {
    if (!mpv) return;
    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    auto& s = it->second;
    if (!s.enabled) return;

    std::string cmd = "change-list glsl-shaders remove " + s.path;
    mpv_command_string(mpv, cmd.c_str());

    s.enabled = false;
}

void ShaderManager::Toggle(const std::string& name) {
    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    if (it->second.enabled)
        Disable(name);
    else
        Enable(name);
}

void ShaderManager::DisableAll() {
    for (auto& [k, s] : shaders)
        Disable(k);
}

