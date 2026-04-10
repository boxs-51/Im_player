#pragma once
#include <mpv/client.h>
#include <string>
#include <map>

struct VFParam {
    std::string name;
    float value;
    float min;
    float max;
    float default_value;
    std::string label;
};

struct VideoFilter {
    std::string name;    // Tên gốc của mpv (ví dụ: "eq", "rotate", "yadif")
    std::string label;   // Tên hiển thị (ví dụ: "Equalizer")
    bool enabled = false;
    std::vector<VFParam> params;

    // Chuyển đổi thành chuỗi mpv: @label:name=p1=v1:p2=v2
    std::string toString() const {
        if (params.empty()) return "@" + label + ":" + name;
        
        std::string p_str = "";
        for (size_t i = 0; i < params.size(); ++i) {
            p_str += params[i].name + "=" + std::to_string(params[i].value);
            if (i < params.size() - 1) p_str += ":";
        }
        return "@" + label + ":" + name + "=" + p_str;
    }
};
class VideoFilterManager {
public:
    static VideoFilterManager& Instance();
    void Init(mpv_handle* h);
    // Đăng ký các filter phổ biến
    void SetupDefaultFilters();

    void UpdateParam(std::string label, std::string pName, float val);

    void Toggle(std::string label);

    void Apply();

private:
    mpv_handle* mpv;
    std::map<std::string, VideoFilter> filters;
    VideoFilterManager() = default;
};