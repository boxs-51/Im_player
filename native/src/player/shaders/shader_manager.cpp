
#include "player/shaders/shaders_manager.h"

#include <log.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

ShaderManager::ShaderManager() {}
ShaderManager::~ShaderManager() {}

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

void ShaderManager::NormalizeOrders() {
    // Group shader theo hook
    std::unordered_map<HookStage, std::vector<Shader*>> groups;

    for (Shader* s : activeShaders) {
        groups[s->hook].push_back(s);
    }

    // Xử lý từng group
    for (auto& [hook, vec] : groups) {

        // sort theo (order, loadIndex)
        std::sort(vec.begin(), vec.end(), [](Shader* a, Shader* b) {
            if (a->order != b->order)
                return a->order < b->order;
            return a->loadIndex < b->loadIndex; // 👈 cần có field này
        });

        int current = 1;
        int lastOrder = -1;

        for (Shader* s : vec) {
            int original = s->order;

            if (original <= 0 || original == lastOrder) {
                s->order = current;
            } else {
                current = std::max(current, original);
                s->order = current;
            }

            lastOrder = original;
            current++;
        }
    }
}
void ShaderManager::ApplyPipeline() {
    if (!mpv) return;
    std::lock_guard<std::recursive_mutex> lock(mtx);
    fs::path tempDir = fs::current_path() / "shader_cache";
    if (!fs::exists(tempDir)) fs::create_directories(tempDir);
    activeShaders.clear(); 
    // 1. Lọc và Sắp xếp (giữ nguyên logic Hook và Order)
    for (auto& [name, s] : shaders) {
        if (s.enabled) activeShaders.push_back(&s);
    }
    NormalizeOrders();

    std::sort(activeShaders.begin(), activeShaders.end(), [&](Shader* a, Shader* b) {
        int pA = HookPriority(a->hook);
        int pB = HookPriority(b->hook);

        if (pA != pB)
            return pA < pB;

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
    SaveState();
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
    bool collectingInfo = false;

    s.loadIndex = globalLoadCounter++;

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
                if (line.compare(0, 6, "@INFO:") == 0) {
                    currentParam->info = val;
                    collectingInfo = true; // Bắt đầu chế độ thu thập cho các dòng tiếp theo
                }
                else if (line.compare(0, 7, "@LABEL:") == 0)   currentParam->label = val;
                else if (line.compare(0, 5, "@MIN:") == 0)     currentParam->min = ToFloat(val, 0.0f);
                else if (line.compare(0, 5, "@MAX:") == 0)     currentParam->max = ToFloat(val, 1.0f);
                else if (line.compare(0, 9, "@DEFAULT:") == 0) {
                    float val_temp = ToFloat(val, 0.0f);
                    currentParam->value = val_temp;
                    currentParam->default_value = val_temp; 
                    currentParam->temp_value = val_temp;
                }
            }
        }
        else if (collectingInfo && currentParam) {
            // Nếu dòng không bắt đầu bằng @ và đang trong chế độ INFO
            // thì cộng dồn vào chuỗi info hiện tại
            currentParam->info += "\n" + line;
        }
        
    }
}



void ShaderManager::UpdateParams(const std::string& shaderName, const std::string& paramName, float value) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
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
/*
std::vector<Shader*> ShaderManager::GetActivePipeline() {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    std::vector<Shader*> activeShaders;

    // 1. Lọc ra các shader đang được enable
    for (auto& [name, s] : shaders) {
        if (s.enabled) {
            activeShaders.push_back(&s);
        }
    }

    // 2. Sắp xếp theo thứ tự ưu tiên của Hook và sau đó là Order
    std::sort(activeShaders.begin(), activeShaders.end(), [this](Shader* a, Shader* b) {
        int pA = HookPriority(a->hook);
        int pB = HookPriority(b->hook);
        
        if (pA != pB) return pA < pB; // Ưu tiên theo tầng (HookStage)
        return a->order < b->order;   // Nếu cùng tầng thì ưu tiên theo số order
    });

    return activeShaders;
}
*/
void ShaderManager::Reload() {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    
    // Lưu lại danh sách các shader đang bật để phục hồi sau khi reload
    std::map<std::string, bool> enabledStates;
    for (const auto& [name, s] : shaders) {
        if (s.enabled) enabledStates[name] = true;
    }

    shaders.clear();
    pipeline.clear();

    // Nạp lại từ đầu
    LoadShadersFromFolder(searchPaths);

    // Phục hồi trạng thái enabled
    for (auto& [name, s] : shaders) {
        if (enabledStates.count(name)) {
            s.enabled = true;
            pipeline.push_back(name);
        }
    }

    ApplyPipeline();
}
void ShaderManager::AddFolder(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    if (std::find(searchPaths.begin(), searchPaths.end(), path) == searchPaths.end()) {
        searchPaths.push_back(path);
        LoadShadersFromFolder({path}); // Chỉ nạp thêm từ folder mới
    }
}

void ShaderManager::RemoveFolder(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    searchPaths.erase(std::remove(searchPaths.begin(), searchPaths.end(), path), searchPaths.end());
    // Lưu ý: Tùy bạn chọn có muốn xóa các shader thuộc folder này khỏi danh sách hiện tại hay không
}
void ShaderManager::SaveState() {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    json j;

    j["folders"] = searchPaths;
    j["active_shaders"] = json::array(); // Đảm bảo là một mảng

    // Duyệt theo pipeline để giữ ĐÚNG THỨ TỰ sắp xếp
    for (const std::string& name : pipeline) {
        if (shaders.count(name) && shaders[name].enabled) {
            auto& s = shaders[name];
            json s_json;
            s_json["name"] = name;
            s_json["order"] = s.order;
            // Lưu các thông số của shader
            for (auto& p : s.params) {
                s_json["params"][p.name] = p.value;
            }
            j["active_shaders"].push_back(s_json);
        }
    }

    std::ofstream o(configPath);
    if (o.is_open()) {
        o << j.dump(4);
        o.close();
    }
}
void ShaderManager::LoadState() {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    if (!fs::exists(configPath)) return;

    try {
        std::ifstream i(configPath);
        json j = json::parse(i);

        // 1. Khôi phục danh sách folder
        if (j.contains("folders") && j["folders"].is_array()) {
            for (const auto& path : j["folders"]) {
                std::string p = path.get<std::string>();
                p = Trim(p); // Sử dụng hàm Trim bạn đã viết

                if (!p.empty()) {
                    // Kiểm tra trùng lặp trước khi thêm vào searchPaths
                    if (std::find(searchPaths.begin(), searchPaths.end(), p) == searchPaths.end()) {
                        searchPaths.push_back(p);
                    }
                }
            }
        }

        // 2. Nạp file vật lý từ ổ cứng trước
        // Quan trọng: Phải nạp đủ file thì mới có thông tin để map với dữ liệu Save
        if (!searchPaths.empty()) {
            shaders.clear(); 
            LoadShadersFromFolder(searchPaths); 
        }

        // 3. Reset pipeline hiện tại
        pipeline.clear();
        for (auto& [name, s] : shaders) s.enabled = false;

        // 4. Khôi phục trạng thái từ JSON
        if (j.contains("active_shaders") && j["active_shaders"].is_array()) {
            for (auto& s_json : j["active_shaders"]) {
                std::string name = s_json.value("name", "");
                
                // Kiểm tra shader có thực sự tồn tại trong folder không
                if (shaders.count(name)) {
                    Shader& s = shaders[name];
                    s.enabled = true;
                    pipeline.push_back(name); // Khôi phục đúng thứ tự mảng trong JSON
                    
                    if (s_json.contains("order")) {
                        s.order = s_json["order"].get<int>();
                    }

                    // Khôi phục giá trị tham số người dùng đã chỉnh
                    if (s_json.contains("params")) {
                        for (auto& p : s.params) {
                            if (s_json["params"].contains(p.name)) {
                                float val = s_json["params"][p.name].get<float>();
                                p.value = val;
                                p.temp_value = val;
                            }
                        }
                    }
                }
            }
        }
        
        // Cuối cùng mới Apply lên mpv
        ApplyPipeline();
        
    } catch (...) { /* LOG_ERROR("Load state failed"); */ }
}
void ShaderManager::ResetToDefault(const std::string& shaderName) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    auto it = shaders.find(shaderName);
    if (it != shaders.end()) {
        for (auto& p : it->second.params) {
            p.value = p.default_value;
            p.temp_value = p.default_value;
        }
        ApplyPipeline();
    }
}
void ShaderManager::ApplyChanges(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    if (shaders.count(name)) {
        for (auto& p : shaders[name].params) {
            p.value = p.temp_value;
        }
        ApplyPipeline();
    }
}
void ShaderManager::MoveUp(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    Shader& current = it->second;
    int currentOrder = current.order;
    HookStage currentHook = current.hook;

    Shader* target = nullptr;

    // 🔍 tìm shader cùng hook có order nhỏ hơn gần nhất
    for (auto& [k, s] : shaders) {
        if (s.hook != currentHook) continue;

        if (s.order < currentOrder) {
            if (!target || s.order > target->order) {
                target = &s;
            }
        }
    }

    if (target) {
        std::swap(current.order, target->order);
        ApplyPipeline();
    }
}
void ShaderManager::MoveDown(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    auto it = shaders.find(name);
    if (it == shaders.end()) return;

    Shader& current = it->second;
    int currentOrder = current.order;
    HookStage currentHook = current.hook;

    Shader* target = nullptr;

    // 🔍 tìm shader cùng hook có order lớn hơn gần nhất
    for (auto& [k, s] : shaders) {
        if (s.hook != currentHook) continue;

        if (s.order > currentOrder) {
            if (!target || s.order < target->order) {
                target = &s;
            }
        }
    }

    if (target) {
        std::swap(current.order, target->order);
        ApplyPipeline();
    }
}
void ShaderManager::EnableAll() {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    pipeline.clear(); // Xóa để nạp lại từ đầu theo đúng danh sách hiện có

    for (auto& [name, s] : shaders) {
        s.enabled = true;
        pipeline.push_back(name);
    }

    // Chỉ gọi Apply một lần sau khi đã bật tất cả để tránh gửi quá nhiều lệnh lên mpv
    ApplyPipeline();
}

void ShaderManager::Enable(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
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
    std::lock_guard<std::recursive_mutex> lock(mtx);
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
    std::lock_guard<std::recursive_mutex> lock(mtx);
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
    std::lock_guard<std::recursive_mutex> lock(mtx);
    for (auto& [k, s] : shaders)
        s.enabled = false;

    pipeline.clear();
    ApplyPipeline();
}

void ShaderManager::MoveShader(int from, int to)
{
    std::lock_guard<std::recursive_mutex> lock(mtx);

    if (from < 0 || to < 0 || 
        from >= (int)pipeline.size() || 
        to >= (int)pipeline.size() ||
        from == to)
        return;

    // Lấy shader name
    std::string item = pipeline[from];

    // Xóa khỏi vị trí cũ
    pipeline.erase(pipeline.begin() + from);

    // Chèn vào vị trí mới
    pipeline.insert(pipeline.begin() + to, item);

    // ===== UPDATE ORDER =====
    for (int i = 0; i < pipeline.size(); i++) {
        const std::string& shaderName = pipeline[i];

        if (shaders.count(shaderName)) {
            shaders[shaderName].order = i;
        }
    }

    // ===== APPLY =====
    ApplyPipeline();
}
void ShaderManager::DiscardChanges() {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    for (auto& [name, shader] : shaders) {
        for (auto& p : shader.params) {
            p.temp_value = p.value;
        }
    }
}
void ShaderManager::MoveShaderByName(const std::string& fromName, const std::string& toName)
{
    std::lock_guard<std::recursive_mutex> lock(mtx);

    auto fromIt = std::find(pipeline.begin(), pipeline.end(), fromName);
    auto toIt   = std::find(pipeline.begin(), pipeline.end(), toName);

    if (fromIt == pipeline.end() || toIt == pipeline.end())
        return;

    if (fromIt == toIt)
        return;

    // Lưu item
    std::string item = *fromIt;

    // Xóa khỏi vị trí cũ
    pipeline.erase(fromIt);

    // ⚠️ Quan trọng: sau erase, iterator có thể bị invalid → tìm lại
    toIt = std::find(pipeline.begin(), pipeline.end(), toName);

    // Chèn vào vị trí mới
    pipeline.insert(toIt, item);

    // ===== UPDATE ORDER =====
    for (int i = 0; i < (int)pipeline.size(); i++) {
        const std::string& shaderName = pipeline[i];

        if (shaders.count(shaderName)) {
            shaders[shaderName].order = i;
        }
    }

    // ===== APPLY =====
    ApplyPipeline();
}
