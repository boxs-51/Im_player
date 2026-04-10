#include <mpv/fillter/video_fillter_manager.h>

static VideoFilterManager::VideoFilterManager& Instance() {
    static VideoFilterManager inst;
    return inst;
}
void VideoFilterManager::Init(mpv_handle* h) { mpv = h; }

// Đăng ký các filter phổ biến
void VideoFilterManager::SetupDefaultFilters() {
    // Filter chỉnh màu (Equalizer)
    VideoFilter eq = {"eq", "ColorControl", false};
    eq.params = {
        {"brightness", 0.0f, -1.0f, 1.0f, 0.0f, "Brightness"},
        {"contrast", 1.0f, -1.0f, 10.0f, 1.0f, "Contrast"},
        {"saturation", 1.0f, 0.0f, 3.0f, 1.0f, "Saturation"}
    };
    filters["ColorControl"] = eq;

    // Filter xoay hình
    VideoFilter rot = {"rotate", "Rotation", false};
    rot.params = {{"angle", 0.0f, 0.0f, 360.0f, 0.0f, "Angle"}};
    filters["Rotation"] = rot;
}

void VideoFilterManager::UpdateParam(std::string label, std::string pName, float val) {
    if (filters.count(label)) {
        for (auto& p : filters[label].params) {
            if (p.name == pName) { p.value = val; break; }
        }
        Apply();
    }
}

void VideoFilterManager::Toggle(std::string label) {
    if (filters.count(label)) {
        filters[label].enabled = !filters[label].enabled;
        Apply();
    }
}

void VideoFilterManager::Apply() {
    std::string vf_string = "";
    bool first = true;
    for (auto const& [label, f] : filters) {
        if (f.enabled) {
            if (!first) vf_string += ",";
            vf_string += f.toString();
            first = false;
        }
    }
    mpv_set_property_string(mpv, "vf", vf_string.c_str());
}
