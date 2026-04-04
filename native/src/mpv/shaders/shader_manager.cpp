
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
        case HookStage::NATIVE:     return 0;   // Khử nhiễu, xử lý gốc
        case HookStage::PREKERNEL:  return 100; // Upscale (như FSRCNNX)
        case HookStage::POSTKERNEL: return 200; // Làm nét sau scale
        case HookStage::LINEAR:     return 300; // Chỉnh màu vật lý
        case HookStage::MAIN:       return 400; // Blend, hậu kỳ
        case HookStage::OUTPUT:     return 500; // Deband, Color mapping
        default:                    return 1000;
    }
}

void ShaderManager::Register(const std::string& name, const std::string& path) {
    shaders[name] = { name, path, false };
}

void ShaderManager::ApplyPipeline() {
    if (!mpv) return;

    fs::path tempDir = fs::current_path() / "shader_cache";
    if (!fs::exists(tempDir)) fs::create_directories(tempDir);

    // 1. Lọc và Sắp xếp (giữ nguyên logic Hook và Order)
    std::vector<Shader*> activeShaders;
    for (auto& [name, s] : shaders) {
        if (s.enabled) activeShaders.push_back(&s);
    }

    std::sort(activeShaders.begin(), activeShaders.end(), [this](Shader* a, Shader* b) {
        int pA = HookPriority(a->hook);
        int pB = HookPriority(b->hook);
        if (pA != pB) return pA < pB;
        return a->order < b->order;
    });

    // 2. Xử lý nội dung và Ghi file có chọn lọc
    std::string list;
    bool needsMpvUpdate = false; // Biến kiểm soát việc gửi lệnh cho mpv
    
    #ifdef _WIN32
        char sep = ';';
    #else
        char sep = ':';
    #endif

    for (Shader* s : activeShaders) {
        std::string currentCode = GenerateTempShader(*s);
        fs::path tempFile = tempDir / (s->name + "_active.glsl");
        std::string tempFilePath = tempFile.generic_string();

        // CHỈ GHI FILE NẾU NỘI DUNG THAY ĐỔI
        if (currentCode != s->last_generated_code || !fs::exists(tempFile)) {
            std::ofstream out(tempFile);
            if (out.is_open()) {
                out << currentCode;
                out.close();
                s->last_generated_code = currentCode; // Cập nhật cache
                needsMpvUpdate = true; // Đánh dấu cần reload mpv
            }
        }

        list += tempFilePath + sep;
    }

    if (!list.empty()) list.pop_back();

    // 3. Chỉ gửi lệnh cho mpv nếu có ít nhất 1 shader thay đổi nội dung 
    // hoặc danh sách shader (pipeline) bị thay đổi số lượng/thứ tự
    static std::string lastFullList; 
    if (needsMpvUpdate || list != lastFullList) {
        std::string currentList = list; // Copy ra một bản tạm
        const char* cmd[] = {"set", "glsl-shaders", currentList.c_str(), nullptr};
        int status = mpv_command(mpv, cmd);
        if (status < 0) {
            // Log lỗi từ mpv: mpv_error_string(status)
        }
        lastFullList = currentList;
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
std::string ShaderManager::HookStageToString(HookStage hook) {
    switch (hook) {
        case HookStage::NATIVE:     return "NATIVE";
        case HookStage::PREKERNEL:  return "PREKERNEL";
        case HookStage::POSTKERNEL: return "POSTKERNEL";
        case HookStage::LINEAR:     return "LINEAR";
        case HookStage::MAIN:       return "MAIN";
        case HookStage::OUTPUT:     return "OUTPUT";
        default:                    return "UNKNOWN";
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
// Loại bỏ khoảng trắng và ký tự điều hướng (\r, \n) ở hai đầu chuỗi
static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Lấy giá trị sau dấu ':' một cách an toàn
static std::string GetValue(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos || colonPos + 1 >= line.length()) {
        return "";
    }
    return Trim(line.substr(colonPos + 1));
}

// Chuyển đổi số an toàn với giá trị mặc định (Fallback)
static float ToFloat(const std::string& val, float fallback = 0.0f) {
    if (val.empty()) return fallback;
    try {
        return std::stof(val);
    } catch (...) {
        return fallback;
    }
}

static int ToInt(const std::string& val, int fallback = 0) {
    if (val.empty()) return fallback;
    try {
        return std::stoi(val);
    } catch (...) {
        return fallback;
    }
}
void ShaderManager::ParseShaderFile(const std::string& path, Shader& s) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    ShaderParam* currentParam = nullptr;

    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty()) continue;

        // Dừng khi hết khối comment meta (thường dùng trong mpv shader)
        if (line.find("*/") != std::string::npos) break;

        // Chỉ xử lý các dòng bắt đầu bằng @
        if (line[0] == '@') {
            std::string val = GetValue(line);

            if (line.compare(0, 6, "@DESC:") == 0) {
                s.description = val.empty() ? "No description" : val;
            }
            else if (line.compare(0, 7, "@ORDER:") == 0) {
                s.order = ToInt(val, 0);
            }
            else if (line.compare(0, 6, "@HOOK:") == 0) {
                if (val.find("PREKERNEL") != std::string::npos) s.hook = HookStage::PREKERNEL;
                else if (val.find("POSTKERNEL") != std::string::npos) s.hook = HookStage::POSTKERNEL;
                else if (val.find("NATIVE") != std::string::npos) s.hook = HookStage::NATIVE;
                else if (val.find("LINEAR") != std::string::npos) s.hook = HookStage::LINEAR;
                else if (val.find("OUTPUT") != std::string::npos) s.hook = HookStage::OUTPUT;
                else if (val.find("MAIN") != std::string::npos) s.hook = HookStage::MAIN;
                else s.hook = HookStage::UNKNOWN;
            }
            else if (line.compare(0, 7, "@PARAM:") == 0) {
                ShaderParam p;
                p.name = val;
                // Xóa khoảng trắng trong tên biến kỹ thuật
                p.name.erase(std::remove_if(p.name.begin(), p.name.end(), ::isspace), p.name.end());
                
                if (!p.name.empty()) {
                    s.params.push_back(p);
                    currentParam = &s.params.back();
                }
            }
            else if (currentParam) {
                if (line.compare(0, 7, "@LABEL:") == 0)      currentParam->label = val;
                else if (line.compare(0, 5, "@MIN:") == 0)   currentParam->min = ToFloat(val, 0.0f);
                else if (line.compare(0, 5, "@MAX:") == 0)   currentParam->max = ToFloat(val, 1.0f);
                else if (line.compare(0, 9, "@DEFAULT:") == 0) currentParam->value = ToFloat(val, 0.0f);
            }
        }
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

