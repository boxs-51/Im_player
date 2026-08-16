#pragma once
#include "utils.h"
#include "json.hpp"

#include "thread.h"
#include "notification.h"
#include "stb_image.h"
#include <popup/popup.h>

#include <gui/gui.h>
#include <player/render/PlayBackRenderThread.h>
#include <player/session/PlayerManager.h>
#include <backends/backend.h>

#include <filesystem>
#include <vector>
#include <commdlg.h>  
#include <string>
#include <array>
#include <stdexcept>
#include <exception>
#include <mutex>
#include <set>
#include <unordered_set>
#include <unordered_map>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
//#undef LOG
//#define LOG(key, interval_ms, expr) do {} while(0)
#include <log.h>
using json = nlohmann::json;

void TerminateHandler() {
    LOG(terminate_handler, 1, LogLevel::Warning,LogCategory::System, "Terminate handler called. Cleaning up");
    StopService();
    std::abort();  // Kết thúc app
}
void SignalHandler(int signal) {
    LOG(signal_handler, 1, LogLevel::Warning,LogCategory::System,  "Signal %d received. Cleaning up.", signal);
    StopService();
    std::_Exit(signal);  // Kết thúc app ngay, tránh gọi các destructor
}
std::wstring UTF8ToWide(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}
std::string WideToUTF8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
    return str;
}


void UpdateHoverAnim(float& animValue, bool isHovering, float speed) {
    float animSpeed = speed * ImGui::GetIO().DeltaTime;
    if (isHovering)
        animValue = std::min(1.0f, animValue + animSpeed);
    else
        animValue = std::max(0.0f, animValue - animSpeed);
}

static std::unordered_map<std::string, GLuint> iconCache;

GLuint GetIcon(const std::string& path)
{
    // Nếu đã load trước đó, trả luôn
    auto it = iconCache.find(path);
    if(it != iconCache.end())
        return it->second;

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if(!data)
    {
        std::string reason = stbi_failure_reason();
        return 0;
    }

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    iconCache[path] = textureID;


    return textureID;
}

bool SetDelayHover(bool hovering, double delaySeconds, ImGuiID id)
{
    if (id == 0)
        id = ImGui::GetItemID();
    ImGuiStorage* storage = ImGui::GetStateStorage();

    float* start_time = storage->GetFloatRef(id, -1.0f);
    float now = (float)ImGui::GetTime();

    if (hovering)
    {
        if (*start_time < 0.0f)
            *start_time = now;

        return (now - *start_time) >= delaySeconds;
    }
    else
    {
        *start_time = -1.0f;
    }

    return false;
}
