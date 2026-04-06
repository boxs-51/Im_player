#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <filesystem> // Cần cho path handling
#include <mutex>

// Forward declaration cho mpv
struct mpv_handle;

struct ShaderParam {
    std::string name;   // Tên biến để #define (ví dụ: SHARPEN_STR)
    std::string label;  // Tên hiển thị trên UI
    std::string info;   

    
    float value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
};

enum class ShaderType {
    OTHER,
    UPSCALE,
    DOWNSCALE,
    DENOISE
};

enum class HookStage {
    NATIVE,      // Chạy trên độ phân giải gốc (LUMA_NATIVE)
    PREKERNEL,   // Trước khi scale (LUMA_UP_PREKERNEL)
    POSTKERNEL,  // Sau khi scale (LUMA_UP_POSTKERNEL)
    LINEAR,      // Sau khi khử gamma (RGB_LINEAR)
    MAIN,        // Giai đoạn trộn (MAIN)
    OUTPUT,      // Giai đoạn xuất (OUTPUT)
    UNKNOWN
};

struct Shader {
    std::string name = "";
    std::string path = "";
    std::string description ="";

    ShaderType type = ShaderType::OTHER;
    HookStage hook = HookStage::UNKNOWN;

    int order = 0;           
    bool enabled = false;
    std::string last_generated_code = "";
    
    std::vector<ShaderParam> params = {}; 
};

class ShaderManager {
public:
    static ShaderManager& Instance();

    // Ngăn chặn copy singleton
    ShaderManager(const ShaderManager&) = delete;
    void operator=(const ShaderManager&) = delete;

    void Init(mpv_handle* mpvHandle);
    
    // Quản lý file
    void LoadShadersFromFolder(const std::vector<std::string>& folders);
    void Register(const std::string& name, const std::string& path);

    // Điều khiển trạng thái
    void Enable(const std::string& name);
    void Disable(const std::string& name);
    void Toggle(const std::string& name);
    void EnableAll();
    void DisableAll();

    // Cập nhật thông số từ UI
    void UpdateParams(const std::string& shaderName, const std::string& paramName, float value);
    
    // Thay đổi thứ tự ưu tiên (Quan trọng cho pipeline)
    void MoveUp(const std::string& name);
    void MoveDown(const std::string& name);

    // Logic xử lý pipeline & mpv communication
    void ApplyPipeline();

    // Getters
    std::lock_guard<std::mutex> Lock() { return std::lock_guard<std::mutex>(mtx); }
    std::unordered_map<std::string, Shader>& GetShaders() { return shaders; }
    const std::vector<std::string>& GetPipeline() { return pipeline; }
    
    Shader* GetShaderByName(const std::string& name) {
        auto it = shaders.find(name);
        return (it != shaders.end()) ? &it->second : nullptr;
    }
    std::string HookStageToString(HookStage h);
private:
    ShaderManager() = default; 

    // Helper nội bộ
    std::string Quote(const std::string& s);
    int HookPriority(HookStage h);
    void LoadMeta(const std::string& path, Shader& s);
    void ParseShaderFile(const std::string& path, Shader& s);
    std::string GenerateTempShader(const Shader& s);

    // Dọn dẹp file tạm khi thoát
    void CleanupTempShaders();

    mpv_handle* mpv = nullptr;
    std::unordered_map<std::string, Shader> shaders;
    
    // Danh sách lưu đúng THỨ TỰ các shader đang được bật
    std::vector<std::string> pipeline; 

    std::mutex mtx;
};