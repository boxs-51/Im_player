
#define LOG_SHADER

#include "mpv/shaders/shaders_manager.h"

#include <log.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

ShaderManager& ShaderManager::Instance() {
    static ShaderManager inst;
    return inst;
}
std::string ShaderManager::Quote(const std::string& s) {
    return "\"" + s + "\"";
}

void ShaderManager::Init(mpv_handle* mpvHandle) {
    mpv = mpvHandle;
}
int ShaderManager::HookPriority(HookStage h) {
    switch (h) {
        case HookStage::PREKERNEL: return 0;
        case HookStage::MAIN:      return 100;
        case HookStage::POSTKERNEL:return 200;
        case HookStage::OUTPUT:    return 300;
        default:                  return 1000;
    }
}

void ShaderManager::Register(const std::string& name, const std::string& path) {
    shaders[name] = { name, path, false };
}

void ShaderManager::ApplyPipeline() {
    if (!mpv) {

        return;
    }

    // 1. Tạo danh sách các shader đang bật
    std::vector<Shader*> activeShaders;
    for (auto& [name, s] : shaders) {
        if (s.enabled) {
            activeShaders.push_back(&s);
        }
    }

    // 2. Sắp xếp theo ưu tiên (Hook -> Order -> Name)
    std::sort(activeShaders.begin(), activeShaders.end(),
    [this](Shader* a, Shader* b) {
        int ha = HookPriority(a->hook);
        int hb = HookPriority(b->hook);
        if (ha != hb) return ha < hb;
        if (a->order != b->order) return a->order < b->order;
        return a->name < b->name;
    });

    // 3. Xây dựng chuỗi command cho mpv
    std::string list;
    #ifdef _WIN32
        char sep = ';';
    #else
        char sep = ':';
    #endif
    for (Shader* s : activeShaders) {
        list += s->path + sep; // mpv dùng ':' hoặc ';' tùy OS, thường ':' là chuẩn
    }

    if (!list.empty()) list.pop_back();

    // Dùng mpv_command thay vì string để an toàn hơn với khoảng trắng
    const char* cmd[] = {"set", "glsl-shaders", list.c_str(), nullptr};
    int res = mpv_command(mpv, cmd);

    if (res < 0) {
    }
}

void ShaderManager::LoadMeta(const std::string& path, Shader& s) {
    try {
        std::ifstream f(path);
        json data = json::parse(f);

        if (data.contains("description")) s.description = data["description"];
        if (data.contains("order")) s.order = data["order"];
        // Có thể thêm các field khác như type, v.v.
    } catch (...) {
        // Log error hoặc bỏ qua nếu file meta lỗi format
    }
}

void ShaderManager::ParseShaderFile(const std::string& path, Shader& s) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    ShaderParam currentParam;
    bool hasParam = false;

    while (std::getline(f, line)) {
        // Xóa khoảng trắng thừa ở đầu dòng (trim)
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line.find("//!") != 0) continue;

        // 1. Lấy mô tả: //!DESC <text>
        if (line.find("//!DESC") == 0) {
            s.description = line.substr(7); 
            s.description.erase(0, s.description.find_first_not_of(" ")); // trim left
        }
        
        // 2. Lấy Hook Stage: //!HOOK <STAGE>
        else if (line.find("//!HOOK") == 0) {
            if (line.find("PREKERNEL") != std::string::npos) s.hook = HookStage::PREKERNEL;
            else if (line.find("POSTKERNEL") != std::string::npos) s.hook = HookStage::POSTKERNEL;
            else if (line.find("OUTPUT") != std::string::npos) s.hook = HookStage::OUTPUT;
            else if (line.find("MAIN") != std::string::npos) s.hook = HookStage::MAIN;
        }

        // 3. Lấy Parameter: //!PARAM <name> <desc>
        else if (line.find("//!PARAM") == 0) {
            // Lưu param cũ nếu có
            if (hasParam) s.params.push_back(currentParam);
            
            currentParam = {}; 
            // Cắt chuỗi để lấy tên biến và mô tả
            std::string content = line.substr(8);
            content.erase(0, content.find_first_not_of(" "));
            
            size_t spacePos = content.find_first_of(" \t");
            if (spacePos != std::string::npos) {
                currentParam.name = content.substr(0, spacePos);
                // Phần còn lại là mô tả để hiển thị UI
                currentParam.label = content.substr(spacePos + 1); 
            } else {
                currentParam.name = content;
            }
            hasParam = true;
        }

        // 4. Lấy các thuộc tính của Param (Min, Max, Default)
        else if (hasParam) {
            if (line.find("//!MINIMUM") == 0) currentParam.min = std::stof(line.substr(10));
            else if (line.find("//!MAXIMUM") == 0) currentParam.max = std::stof(line.substr(10));
            else if (line.find("//!DEFAULT") == 0) currentParam.value = std::stof(line.substr(11));
        }
    }

    // Đẩy param cuối cùng vào list
    if (hasParam) s.params.push_back(currentParam);
}



void ShaderManager::UpdateParams(const Shader& s) {
    if (!mpv || s.params.empty()) return;

    // Định dạng: param1=value1,param2=value2
    std::string opts;
    for (size_t i = 0; i < s.params.size(); ++i) {
        opts += s.params[i].name + "=" + std::to_string(s.params[i].value);
        if (i < s.params.size() - 1) opts += ",";
    }

    // Gửi toàn bộ options của shader hiện tại vào mpv
    mpv_set_property_string(mpv, "glsl-shader-opts", opts.c_str());
}
void ShaderManager::LoadShadersFromFolder(const std::vector<std::string>& folder) {
    for (const auto& folderPath : folder) {
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
            continue; 
        }

        for (auto& entry : fs::directory_iterator(folderPath)) {
            if (!entry.is_regular_file()) continue;

            auto path = entry.path();
            std::string ext = path.extension().string();
            
            // Hỗ trợ cả .glsl và .hook
            if (ext != ".glsl" && ext != ".hook") continue;

            std::string name = path.stem().string();
            Shader s;
            s.name = name;
            s.path = path.string();

            // Sử dụng một hàm Parse tổng hợp duy nhất cho hiệu suất cao
            ParseShaderFile(s.path, s);

            // Tìm file meta đi kèm (ví dụ: anime4k.hook -> anime4k.meta)
            fs::path metaPath = path;
            metaPath.replace_extension(".meta");
            if (fs::exists(metaPath)) {
                LoadMeta(metaPath.string(), s);
            }

            shaders[name] = s;
        }
        
    }
}

void ShaderManager::EnableAll() {
    pipeline.clear(); // Xóa để nạp lại từ đầu theo đúng danh sách hiện có

    for (auto& [name, s] : shaders) {
        s.enabled = true;
        pipeline.push_back(name);
    }

    // Chỉ gọi Apply một lần sau khi đã bật tất cả để tránh gửi quá nhiều lệnh lên mpv
    ApplyPipeline();
}

void ShaderManager::Enable(const std::string& name) {
    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    auto& s = it->second;
    if (s.enabled) return;

    s.enabled = true;

    // thêm vào pipeline cuối
    pipeline.push_back(name);

    ApplyPipeline(); // rebuild toàn bộ để giữ thứ tự đúng
}

void ShaderManager::Disable(const std::string& name) {
    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    auto& s = it->second;
    if (!s.enabled) return;

    s.enabled = false;

    // remove khỏi pipeline
    pipeline.erase(std::remove(pipeline.begin(), pipeline.end(), name), pipeline.end());

    ApplyPipeline();
}

void ShaderManager::Toggle(const std::string& name) {
    auto it = shaders.find(name);
    if (it == shaders.end()){

        return;
    } 

    if (it->second.enabled)
        Disable(name);
    else
        Enable(name);

    ApplyPipeline();
}

void ShaderManager::DisableAll() {
    for (auto& [k, s] : shaders)
        s.enabled = false;

    pipeline.clear();
    ApplyPipeline();
}

