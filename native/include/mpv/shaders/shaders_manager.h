#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>



// forward declaration cho mpv
struct mpv_handle;
struct ShaderParam {
    std::string name;   // Tên biến trong shader (ví dụ: SHARPEN_STR)
    std::string label;  // Tên hiển thị trên UI (ví dụ: Độ sắc nét)
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
    PREKERNEL,
    MAIN,
    POSTKERNEL,
    OUTPUT,
    UNKNOWN
};

struct Shader {
    std::string name;
    std::string path;
    std::string description;

    ShaderType type = ShaderType::OTHER;
    HookStage hook = HookStage::UNKNOWN;

    int order = 0;           
    bool enabled = false;
    
    std::vector<ShaderParam> params; 
};

class ShaderManager {
public:
    static ShaderManager& Instance();

    // Ngăn chặn copy singleton
    ShaderManager(const ShaderManager&) = delete;
    void operator=(const ShaderManager&) = delete;

    void Init(mpv_handle* mpvHandle);
    
    // Load shader từ folder hoặc file đơn lẻ
    void LoadShadersFromFolder(const std::vector<std::string>& folder);
    void Register(const std::string& name, const std::string& path);

    // Điều khiển trạng thái
    void Enable(const std::string& name);
    void EnableAll();
    void Disable(const std::string& name);
    void Toggle(const std::string& name);

    void UpdateParams(const std::string& shaderName, const std::string& paramName, float value) ;
    void DisableAll();

    // Logic xử lý pipeline
    void ApplyPipeline();

    std::unordered_map<std::string, Shader>& GetShaders() { return shaders; }

    const std::vector<std::string>& GetPipeline() { return pipeline; }

    Shader* GetShaderByName(const std::string& name) {
        if (shaders.find(name) != shaders.end()) {
            return &shaders[name];
        }
        return nullptr;
    }

private:
    ShaderManager() = default; // Private constructor cho Singleton

    std::string Quote(const std::string& s);
    int HookPriority(HookStage h);
    void LoadMeta(const std::string& path, Shader& s);


    std::string HookStageToString(HookStage h);
    void ParseShaderFile(const std::string& path, Shader& s) ;
    std::string GenerateTempShader(const Shader& s);


    mpv_handle* mpv = nullptr;
    std::unordered_map<std::string, Shader> shaders;
    
    // Danh sách tên shader theo thứ tự người dùng mong muốn (nếu có)
    std::vector<std::string> pipeline; 
};