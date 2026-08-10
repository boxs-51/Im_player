#include "video_filter_manager.h"

VideoFilterManager& VideoFilterManager::Instance() {
    static VideoFilterManager inst;
    return inst;
}

void VideoFilterManager::Init(mpv_handle* h) { mpv = h; }

void VideoFilterManager::SetupDefaultFilters() {
    // 1. Color Control (Equalizer)
    AddFilter("ColorControl", "eq");
    AddFilterParam("ColorControl", "brightness", 0.0f, -1.0f, 1.0f, 0.0f);
    AddFilterParam("ColorControl", "contrast", 1.0f, -1.0f, 10.0f, 1.0f);
    AddFilterParam("ColorControl", "saturation", 1.0f, 0.0f, 3.0f, 1.0f);

    // 2. Rotation
    AddFilter("Rotation", "rotate");
    AddFilterParam("Rotation", "angle", 0.0f, 0.0f, 360.0f, 0.0f);

    // 3. Blur (Boxblur)
    AddFilter("Blur", "boxblur");
    AddFilterParam("Blur", "lr", 0.0f, 0.0f, 100.0f, 2.0f);
    AddFilterParam("Blur", "lh", 0.0f, 0.0f, 100.0f, 2.0f);

    // 4. Sharpen (Unsharp)
    AddFilter("Sharpen", "unsharp");
    AddFilterParam("Sharpen", "luma_msize_x", 5.0f, 1.0f, 50.0f, 5.0f);
    AddFilterParam("Sharpen", "luma_msize_y", 5.0f, 1.0f, 50.0f, 5.0f);
    AddFilterParam("Sharpen", "luma_amount", 1.0f, -5.0f, 5.0f, 1.0f);

    // 5. Denoise (TNLMEANS)
    AddFilter("Denoise", "tnlmeans");
    AddFilterParam("Denoise", "r", 2.0f, 1.0f, 10.0f, 2.0f);
    AddFilterParam("Denoise", "s", 7.0f, 1.0f, 50.0f, 7.0f);
    AddFilterParam("Denoise", "h", 0.02f, 0.001f, 0.5f, 0.02f);

    // 6. Deinterlace
    AddFilter("Deinterlace", "yadif");

    // 7. Scale/Resize
    AddFilter("Scale", "scale");
    AddFilterParam("Scale", "w", -1.0f, -1.0f, 4096.0f, -1.0f);
    AddFilterParam("Scale", "h", -1.0f, -1.0f, 4096.0f, -1.0f);

    // 8. Crop
    AddFilter("Crop", "crop");
    AddFilterParam("Crop", "x", 0.0f, 0.0f, 4096.0f, 0.0f);
    AddFilterParam("Crop", "y", 0.0f, 0.0f, 4096.0f, 0.0f);
    AddFilterParam("Crop", "w", -1.0f, -1.0f, 4096.0f, -1.0f);
    AddFilterParam("Crop", "h", -1.0f, -1.0f, 4096.0f, -1.0f);

    // 9. Flip (Transpose)
    AddFilter("Flip", "hflip");

    // 10. Color Space Conversion
    AddFilter("ColorSpace", "colorspace");
    AddFilterParam("ColorSpace", "space", 1.0f, 0.0f, 10.0f, 1.0f);
}

void VideoFilterManager::AddFilter(const std::string& label, const std::string& name) {
    VideoFilter vf;
    vf.label = label;
    vf.name = name;
    vf.enabled = false;
    filters[label] = vf;
    filterIndex[label] = 1; // Just a marker
}

void VideoFilterManager::AddFilterParam(const std::string& label, const std::string& paramName,
                                        float value, float min, float max, float defaultVal) {
    if (filters.count(label)) {
        VFParam param;
        param.name = paramName;
        param.value = value;
        param.min = min;
        param.max = max;
        param.default_value = defaultVal;
        param.label = paramName;
        filters[label].params.push_back(param);
    }
}

void VideoFilterManager::UpdateParam(std::string label, std::string pName, float val) {
    if (filters.count(label)) {
        for (auto& p : filters[label].params) {
            if (p.name == pName) {
                p.value = (val < p.min) ? p.min : (val > p.max) ? p.max : val;
                break;
            }
        }
        Apply();
    }
}

void VideoFilterManager::BatchUpdateParams(const std::vector<std::tuple<std::string, std::string, float>>& updates) {
    for (const auto& [label, pName, val] : updates) {
        if (filters.count(label)) {
            for (auto& p : filters[label].params) {
                if (p.name == pName) {
                    p.value = (val < p.min) ? p.min : (val > p.max) ? p.max : val;
                    break;
                }
            }
        }
    }
    Apply();
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

VideoFilter* VideoFilterManager::FindFilter(const std::string& label) {
    auto it = filters.find(label);
    if (it != filters.end()) {
        return &it->second;
    }
    return nullptr;
}

