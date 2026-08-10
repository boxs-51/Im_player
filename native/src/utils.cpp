#pragma once
#include "utils.h"
#include "json.hpp"

#include "thread.h"
#include "notification.h"
#include "stb_image.h"
#include <popup/popup.h>

#include <player/mpv_basic_formats.h>
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
//#undef RATE_LIMITED_COUT
//#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
using json = nlohmann::json;


//TODO : chuyen logic sang he thong quan ly mpv rieng
void SetMPVOptions(mpv_handle* mpv, const std::unordered_map<std::string, std::string>& options, bool isProperty = false) {
    for (const auto& [key, value] : options) {
        if (isProperty)
            mpv_set_property_string(mpv, key.c_str(), value.c_str());
        else
            mpv_set_option_string(mpv, key.c_str(), value.c_str());
    }
}
void ApplyStaticMPVConfig(mpv_handle* mpv) {
    if (!mpv) return;
    
    SetMPVOptions(mpv, {
        {"log-level", "v"},
        //{"input-media-keys", "yes"},
        {"idle", "yes"},
        {"keep-open", "yes"},
       // {"stop-screensaver", "yes"},
        {"vo", "libmpv"},
        {"hwdec", "auto-safe"}, // Tự động chọn giải mã phần cứng ổn định nhất
        {"video-rotate", "no"},
        {"tls-verify", "no"},   // Hữu ích cho một số link stream https không chuẩn
        
        // Cấu hình âm thanh an toàn
        {"audio-buffer", "0.5"}, // Đơn vị giây, 0.2s là đủ mượt và không gây trễ
        {"audio-pitch-correction", "yes"}, // Giữ tone giọng khi thay đổi speed

        {"cookies", "yes"},
        {"ytdl-raw-options", "user-agent=Mozilla/5.0,referer=https://www.youtube.com/"},

    });
    
}
void ApplyDynamicMPVConfig(mpv_handle* mpv) {
    if (!mpv) return;
    VideoType type = GetVideoType();
    std::unordered_map<std::string, std::string> config;
    switch (type) {
        case VideoType::Live:
            config = {
                {"profile", "low-latency"},
                {"untimed", "yes"},
                {"cache", "yes"},
                {"cache-pause", "no"},         // Quan trọng: Không dừng hình khi mất kết nối tạm thời
                {"cache-secs", "5"},           // Giảm xuống 5s để giảm độ trễ thực tế
                {"demuxer-max-bytes", "50M"},
                {"demuxer-max-back-bytes", "0"},
                {"vd-lavc-fast", "yes"},       // Giải mã nhanh
                {"video-sync", "audio"},       // Khớp hình theo tiếng
                {"stream-buffer-size", "512k"},
            };
            break;
        case VideoType::Local:
            config = {
                {"cache", "auto"},
                {"cache-pause", "yes"},
                {"demuxer-max-bytes", "500M"},     // Tăng lên để đọc file 4K mượt hơn
                {"demuxer-max-back-bytes", "100M"}, // Cho phép tua ngược trong RAM mượt mà
                {"video-sync", "display-resample"},// Mượt hình trên màn hình (Sync theo tần số quét)
                {"interpolation", "yes"},          // Chống giật hình (judder)
                {"tscale", "oversample"},          // Đi kèm với interpolation
                
                // Bỏ ewa_lanczossharp nếu máy yếu, vì nó tốn tài nguyên GPU
                {"scale", "bilinear"},             // Mặc định an toàn, đổi thành lanczos nếu GPU mạnh
                {"cscale", "bilinear"},
            };
            break;
        case VideoType::Vio:
            config = {
                {"cache", "yes"},
                {"cache-pause", "yes"},
                // Tăng thời gian lưu trữ trong cache lên 5 phút (300 giây)
                {"cache-secs", "300"}, 
                // Quan trọng: Tăng giới hạn dung lượng RAM để chứa đủ 5 phút video đó
                // 1GB (1024M) là thoải mái cho video 4K bitrate cao
                {"demuxer-max-bytes", "1024M"}, 
                // Cho phép bộ giải mã đọc trước 300 giây
                {"demuxer-readahead-secs", "300"},
                // Cho phép lưu lại phần đã xem để tua ngược nhanh (nếu muốn)
                {"demuxer-max-back-bytes", "200M"},
                {"video-sync", "audio"},
                {"network-timeout", "60"}, // Tăng timeout lên 60s cho mạng cực chậm
            };
            break;
        default:
        {
            config = {
                {"cache", "auto"},
                {"cache-pause", "yes"},
                {"cache-secs", "10"},
                {"hr-seek", "yes"},
                {"hr-seek-framedrop", "yes"},
                {"initial-audio-sync", "no"},
                {"vd-lavc-skipframe", "default"},
                // Framedrop nên để mặc định là yes để tránh lệch tiếng khi máy lag
                {"framedrop", "vo"},
                // Kích hoạt tính năng deband
                {"deband", "yes"},
                // Tinh chỉnh thông số (tùy chọn để đạt chất lượng tốt hơn)
                {"deband-iterations", "2"}, // Số lần lặp, mặc định là 1
                {"deband-threshold", "48"}, // Độ nhạy (mặc định 32)
                {"deband-range", "16"},     // Phạm vi lấy mẫu (mặc định 16)
                {"deband-grain", "24"},     // Lượng nhiễu hạt để che vết rổ
            };
            break;
        }
    };

    SetMPVOptions(mpv, config, true);
}
void TerminateHandler() {
    RATE_LIMITED_COUT(terminate_handler, 1,std::cout << "[DEBUG] [WARNING] Terminate handler called. Cleaning up");
    StopService();
    std::abort();  // Kết thúc app
}
void SignalHandler(int signal) {
    RATE_LIMITED_COUT(signal_handler, 1,std::cout << "[DEBUG] [WARNING] Signal " << signal << " received. Cleaning up.");
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
