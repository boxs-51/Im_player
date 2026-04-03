
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
    if (!mpv) return;

    // Tạo thư mục tạm nếu chưa có (ví dụ trong thư mục shader_cache)
    fs::path tempDir = fs::current_path() / "shader_cache";
    if (!fs::exists(tempDir)) fs::create_directory(tempDir);

    std::string list;
    #ifdef _WIN32
        char sep = ';';
    #else
        char sep = ':';
    #endif

    for (auto& [name, s] : shaders) {
        if (s.enabled) {
            // 1. Tạo nội dung shader đã chèn thông số (Dùng hàm GenerateTempShader bạn đã có)
            std::string processedCode = GenerateTempShader(s);

            // 2. Ghi ra file tạm (Ví dụ: shader_cache/bloom_active.glsl)
            fs::path tempFile = tempDir / (s.name + "_active.glsl");
            std::ofstream out(tempFile);
            out << processedCode;
            out.close();

            // 3. Thêm đường dẫn file TẠM vào danh sách nạp của mpv
            list += tempFile.string() + sep;
        }
    }

    if (!list.empty()) list.pop_back();

    // 4. Gửi danh sách file tạm cho mpv
    const char* cmd[] = {"set", "glsl-shaders", list.c_str(), nullptr};
    mpv_command(mpv, cmd);
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
std::string ShaderManager::HookStageToString(HookStage hook){
    switch(hook){
        case HookStage::MAIN:        return "MAIN";
        case HookStage::POSTKERNEL:  return "POSTKERNEL";
        case HookStage::PREKERNEL:   return "PREKERNEL";
        case HookStage::OUTPUT:      return "OUTPUT";
        default:                     return "MAIN";
    }
}
std::string ShaderManager::GenerateTempShader(const Shader& s) {
    std::stringstream ss;

    // 1. Chèn các chỉ thị bắt buộc cho mpv (Mpv Header)
    ss << "//!HOOK " << HookStageToString(s.hook) << "\n";
    ss << "//!BIND HOOKED\n";
    ss << "//!DESC " << s.name << " (Generated)\n\n";

    // 2. Chèn các giá trị Runtime dưới dạng #define
    // Điều này thay thế hoàn toàn việc truyền glsl-shader-opts
    for (const auto& p : s.params) {
        ss << "#define " << p.name << " " << std::fixed << std::setprecision(4) << p.value << "\n";
    }
    ss << "\n";

    // 3. Đọc và chèn phần code gốc (bỏ qua phần meta comment)
    std::ifstream f(s.path);
    std::string line;
    bool inMeta = false;
    while (std::getline(f, line)) {
        if (line.find("/*") != std::string::npos) { inMeta = true; continue; }
        if (line.find("*/") != std::string::npos) { inMeta = false; continue; }
        if (!inMeta) {
            ss << line << "\n";
        }
    }

    return ss.str();
}
void ShaderManager::ParseShaderFile(const std::string& path, Shader& s) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    ShaderParam* currentParam = nullptr;

    while (std::getline(f, line)) {
        // Xóa khoảng trắng
        line.erase(0, line.find_first_not_of(" \t"));
        
        if (line.find("@DESC:") == 0) s.description = line.substr(6);
        else if (line.find("@HOOK:") == 0) {
            std::string h = line.substr(6);
            if (h.find("MAIN") != std::string::npos) s.hook = HookStage::MAIN;
            else if (line.find("POSTKERNEL") != std::string::npos) s.hook = HookStage::POSTKERNEL;
            else if (line.find("PREKERNEL") != std::string::npos) s.hook = HookStage::PREKERNEL;
            else if (line.find("OUTPUT") != std::string::npos) s.hook = HookStage::OUTPUT;

        }
        else if (line.find("@PARAM:") == 0) {
            ShaderParam p;
            p.name = line.substr(7);
            // Xóa khoảng trắng thừa trong tên biến
            p.name.erase(std::remove(p.name.begin(), p.name.end(), ' '), p.name.end());
            s.params.push_back(p);
            currentParam = &s.params.back();
        }
        else if (currentParam) {
            if (line.find("@LABEL:") == 0) currentParam->label = line.substr(7);
            else if (line.find("@MIN:") == 0) currentParam->min = std::stof(line.substr(5));
            else if (line.find("@MAX:") == 0) currentParam->max = std::stof(line.substr(5));
            else if (line.find("@DEFAULT:") == 0) currentParam->value = std::stof(line.substr(9));
        }
        
        // Nếu gặp dấu kết thúc comment khối và đã parse xong meta thì dừng để tiết kiệm CPU
        if (line.find("*/") != std::string::npos) break;
    }
}



void ShaderManager::UpdateParams(const std::string& shaderName, const std::string& paramName, float value) {
    auto it = shaders.find(shaderName);
    if (it != shaders.end()) {
        for (auto& p : it->second.params) {
            if (p.name == paramName) {
                p.value = value;
                break;
            }
        }
        // Sau khi đổi thông số, render lại file tạm và bắt mpv load lại
        ApplyPipeline(); 
    }
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

