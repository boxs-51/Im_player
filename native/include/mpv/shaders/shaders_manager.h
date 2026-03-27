
#include <string>
#include <unordered_map>

#include <mpv/client.h>

struct Shader {
    std::string name;
    std::string path;
    bool enabled = false;
    std::string type;
};

class ShaderManager {
public:
    static ShaderManager& Instance();

    void Init(mpv_handle* mpvHandle);

    void Register(const std::string& name, const std::string& path);

    void Enable(const std::string& name);

    void Disable(const std::string& name);

    void Toggle(const std::string& name);

    void DisableAll() ;
private:
    std::vector<std::string> pipeline;
    mpv_handle* mpv = nullptr;
    std::unordered_map<std::string, Shader> shaders;
};