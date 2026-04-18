#pragma once
#include "globals.h"
#include "utils.h"
#include "json.hpp"
#include <windows/windows_borderless.h>
#include "thread.h"
#include "notification.h"

#include <popup/popup.h>

#include <mpv/mpv_basic_formats.h>
#include <mpv/mpv_custom_ui.h>
#include <mpv/mpv_render_video.h>
#include <services/services_services.h>

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
#undef RATE_LIMITED_COUT
#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>
using json = nlohmann::json;

WindowContext ctx;
WindowLayout Windowlayout;

void UpdateGlobalWindowLayout(SDL_Window* sdlWindow,  DragResizeState& state , WindowLayout& w)
{
    if (!sdlWindow) {w.titleBar = {0,0,0,0};w.videoArea = {0,0,0,0};return;}

    SDL_GetWindowSize(sdlWindow, &w.WinW, &w.WinH);
    SDL_GetWindowPosition(sdlWindow, &w.WinX, &w.WinY);
    // Fullscreen: video chiếm toàn bộ, title bar ẩn
    if (state.IsFullscreen_video ) {w.titleBar = {0,0,0,0};w.videoArea = {0,0,w.WinW,w.WinH};
    } else {
        // Windowed: title bar trên, video dưới
        w.titleBar = {w.WinX , w.WinY,w.WinW,(int)state.TitleHeight};
        w.videoArea = {w.WinX, w.WinY + (int)state.TitleHeight,w.WinW,w.WinH - (int)state.TitleHeight};
                                     
    }
    w.VideoPos = ToVec2_Pos(w.videoArea);
    w.VideoSize = ToVec2_Size(w.videoArea);
    #ifdef RENDER_MPV_THREAD
    int newW  = (int)w.VideoSize.x;
    int newH = (int)w.VideoSize.y;

    if (newW != renderThread.width || newH != renderThread.height) {
        renderThread.newW = newW;
        renderThread.newH = newH;
        renderThread.needResize = true;

        renderThread.cv.notify_one();
    }
    #endif
    w.WinDowPos = Vec2((float)w.WinX,(float)w.WinY);
    w.WinDowSize = Vec2((float)w.WinW,(float)w.WinH);
    w.TitlePos = ToVec2_Pos(w.titleBar);
    w.TitleSize = ToVec2_Size(w.titleBar);
}


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
        //{"stop-screensaver", "yes"},
        {"vo", "libmpv"},
        {"hwdec", "auto-safe"}, // Tự động chọn giải mã phần cứng ổn định nhất
        //{"video-rotate", "no"},
        {"tls-verify", "no"},   // Hữu ích cho một số link stream https không chuẩn
        
        // Cấu hình âm thanh an toàn
        {"audio-buffer", "0.2"}, // Đơn vị giây, 0.2s là đủ mượt và không gây trễ
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
void DrawCardWithHole(ImDrawList* dl,const ImVec2& cardMin,
    const ImVec2& cardMax,const ImVec2& holeMin,const ImVec2& holeMax,
    ImU32 fillCol,ImU32 borderCol,const CardHoleStyle& style) {
    const float r = style.rounding;
    // =========================
    // FILL
    // =========================
    // Phải
    dl->PushClipRect(ImVec2(holeMax.x, cardMin.y),cardMax,true);
    dl->AddRectFilled(cardMin,cardMax,fillCol,r, ImDrawFlags_RoundCornersRight);
    dl->PopClipRect();
    // Trên trái
    if (holeMin.y > cardMin.y){
        dl->PushClipRect(cardMin,ImVec2(holeMax.x, holeMin.y),true);
        dl->AddRectFilled(cardMin,cardMax,fillCol,r,ImDrawFlags_RoundCornersTopLeft);
        dl->PopClipRect();
    }
    // ⭐ LEFT – MIDDLE (BỔ SUNG)
    dl->PushClipRect(ImVec2(cardMin.x, holeMin.y),ImVec2(holeMin.x, holeMax.y),true);
    dl->AddRectFilled(cardMin,cardMax,fillCol,0.0f,ImDrawFlags_None);
    dl->PopClipRect();
    // Dưới trái
    if (holeMax.y < cardMax.y){
        dl->PushClipRect(ImVec2(cardMin.x, holeMax.y),ImVec2(holeMax.x, cardMax.y),true);
        dl->AddRectFilled(cardMin,cardMax,fillCol,r,ImDrawFlags_RoundCornersBottomLeft);
        dl->PopClipRect();
    }
    // =========================
    // BORDER
    // =========================
    if ((borderCol >> IM_COL32_A_SHIFT) > 0){
        dl->PushClipRect(ImVec2(holeMax.x, cardMin.y),cardMax,true);
        dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersRight,style.borderThickness);
        dl->PopClipRect();
        // Trên trái
        if (holeMin.y > cardMin.y){
            dl->PushClipRect(cardMin,ImVec2(holeMax.x, holeMin.y),true);
            dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersTopLeft,style.borderThickness);
            dl->PopClipRect();
        }
        // ⭐ LEFT – MIDDLE (BỔ SUNG)
        dl->PushClipRect(ImVec2(cardMin.x, holeMin.y),ImVec2(holeMin.x, holeMax.y),true);
        dl->AddRect(cardMin,cardMax,borderCol,0.0f, ImDrawFlags_None,style.borderThickness);
        dl->PopClipRect();
        // Dưới trái
        if (holeMax.y < cardMax.y){
            dl->PushClipRect(ImVec2(cardMin.x, holeMax.y),ImVec2(holeMax.x, cardMax.y),true);
            dl->AddRect(cardMin,cardMax,borderCol,r,ImDrawFlags_RoundCornersBottomLeft,style.borderThickness);
            dl->PopClipRect();
        }
    }
}


void UpdateUIState( bool& show_ui_video) {
    int mouseX, mouseY;
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 mousePos = io.MousePos; 
    //SDL_GetMouseState(&mouseX, &mouseY);
    //SDL_Point mousePos = { mouseX, mouseY };

    Uint64 currentTime = SDL_GetTicks64();
    
    // 1. Kiểm tra vị trí chuột
    
    bool isMouseInsideVideo = (mousePos.x >= Windowlayout.videoArea.x && 
                               mousePos.x <= (Windowlayout.videoArea.x + Windowlayout.videoArea.w) &&
                               mousePos.y >= Windowlayout.videoArea.y && 
                               mousePos.y <= (Windowlayout.videoArea.y + Windowlayout.videoArea.h));
    
    //bool isMouseInsideVideo = SDL_PointInRect(&mousePos, &Windowlayout.videoArea);

    // 2. Kiểm tra tương tác với UI (Hover nút, kéo slider, combo...)
    // io.WantCaptureMouse là cách nhanh nhất để biết chuột có đang đè lên bất kỳ cửa sổ ImGui nào không
    bool isInteractingWithUI = io.WantCaptureMouse && (ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered());

    
    // Lưu ý: Luôn reset timer nếu chuột đang di chuyển HOẶC đang tương tác với UI
    bool isMouseMoving = ((io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) && io.WantCaptureMouse);

    // 3. XÁC ĐỊNH TIMEOUT THEO 3 TRẠNG THÁI (Ưu tiên từ cao xuống thấp)
    Uint32 currentTimeout = 300; // Đã rời khỏi video: 0.5s
    if (isInteractingWithUI )    currentTimeout = 5000; // Đang tương tác UI: 5s
    else if (isMouseInsideVideo) currentTimeout = 1500; // Di chuột bình thường trong video: 1s
        
    // 4. RESET TIMER KHI CÓ HOẠT ĐỘNG
    
    if ((isMouseMoving  ) && isMouseInsideVideo) {
        lastInteractionTime = currentTime;
        
        // Tự động hiện lại UI nếu có hoạt động
        if (!show_ui_video) {
            show_ui_video = true;
            SDL_ShowCursor(SDL_ENABLE);
        }
    }

    // 5. LOGIC ẨN UI
    if(IsAnyPopupOpen()) {
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
            SDL_ShowCursor(SDL_ENABLE);
        }
    }

    if (show_ui_video) {
        // Chỉ ẩn khi hết thời gian chờ
        if (currentTime - lastInteractionTime > currentTimeout) {
            
            // CỰC KỲ QUAN TRỌNG: Không ẩn khi đang có Popup/Combo mở hoặc đang kéo Slider
            if (!ImGui::IsAnyItemActive() ) {
                show_ui_video = false;
                if(!IsAnyPopupOpen())
                    SDL_ShowCursor(SDL_DISABLE);
            }
        }
    }
}
void NotifyActivity(bool& show_ui_video) {
    // Chỉ gọi SDL_ShowCursor nếu nó đang bị ẩn để tiết kiệm tài nguyên
    if (!show_ui_video) {
        show_ui_video = true;
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
            SDL_ShowCursor(SDL_ENABLE);
        }
    }
    lastInteractionTime = SDL_GetTicks64();
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

bool SetDelayHover( bool isHovering, double delaySeconds ,const char* id) {
    // Dùng unordered_map để tốc độ tìm kiếm nhanh hơn (O(1))
    // Key là std::string để so sánh nội dung "text"
    static std::unordered_map<std::string, Uint32> hoverTimers;

    // Chuyển pointer thành string để làm key tìm kiếm
    std::string key(id); 

    if (isHovering) {
        // Nếu chưa tồn tại ID này trong danh sách đang hover
        if (hoverTimers.find(key) == hoverTimers.end()) {
            hoverTimers[key] = SDL_GetTicks64();
        }

        Uint64 elapsed = SDL_GetTicks64() - hoverTimers[key];
        if (elapsed >= (Uint64)(delaySeconds * 1000)) {
            return true;
        }
    } else {
        // Xóa khỏi map khi không còn hover
        hoverTimers.erase(key);
    }

    return false;
}
