#include "mpv/mpv_controller.h"

#include <Windows.h>
#include <string>
#include <codecvt>
#include <utils.h>
#include <algorithm>
#include "globals.h"


static bool g_isSeekPending = false;
static float g_seekTargetTime = -1.0;

static Uint32 g_lastSeekRequestTime = 0;

bool mpv_is_muted(mpv_handle* mpv) {
    int muteFlag = 0;
    if (mpv_get_property(mpv, "mute", MPV_FORMAT_FLAG, &muteFlag) == 0)
        return muteFlag != 0;
    return false;
}
int mpv_get_volume(mpv_handle* mpv) {
    int64_t vol = 50;
    if (mpv_get_property(mpv, "volume", MPV_FORMAT_INT64, &vol) == 0)
        return static_cast<int>(vol);
    return 50; // fallback nếu lỗi
}

void mpv_command_set_audio_delay(mpv_handle* mpv, double delaySeconds) {
    mpv_set_property(mpv, "audio-delay", MPV_FORMAT_DOUBLE, &delaySeconds);
}

double mpv_get_audio_delay(mpv_handle* mpv) {
    double delay = 0.0;
    if (mpv_get_property(mpv, "audio-delay", MPV_FORMAT_DOUBLE, &delay)==0)
        return delay;
    return 0.0;
}
void mpv_command_set_speed(mpv_handle* mpv, double speed){
    mpv_set_property(mpv, "speed", MPV_FORMAT_DOUBLE, &speed);
}
double mpv_get_speed(mpv_handle* mpv) {
    double speed = 0.0;
    if (mpv_get_property(mpv, "speed", MPV_FORMAT_DOUBLE, &speed)==0)
        return speed;
    return 0.0;
}
// Phát video
void mpv_command_play(mpv_handle *mpv) {
    const char *cmd[] = { "set", "pause", "no", nullptr };
    mpv_command(mpv, cmd);
}

// Tạm dừng
void mpv_command_pause(mpv_handle *mpv) {
    const char *cmd[] = { "set", "pause", "yes", nullptr };
    mpv_command(mpv, cmd);
}


const Uint32 SEEK_DELAY_MS = 150; // Delay tối thiểu giữa các lần gọi mpv_command


void mpv_command_seek_abs(mpv_handle* mpv, float targetTime, float duration) {
    if (!mpv || duration <= 0.0f || targetTime < 0.0f)
        return;

    // Clamp targetTime vào [0, duration]
    targetTime = std::clamp(targetTime, 0.0f, std::max(duration - 0.05f, 0.0f));


    dataseek.forward = (targetTime > (float)g_playbackStatus.playbackTime);
    dataseek.pulse = 1.0f;

    Uint64 now = SDL_GetTicks64();

    // Nếu chưa đủ delay -> chỉ cập nhật target, đánh dấu pending
    if (now - g_lastSeekRequestTime < SEEK_DELAY_MS) {
        g_seekTargetTime = targetTime;
        g_isSeekPending = true;
        return;
    }

    // Nếu đủ delay -> thực hiện seek ngay
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", targetTime);
    const char* cmd[] = { "seek", buffer, "absolute", nullptr };
    mpv_command(mpv, cmd);

    g_seekTargetTime = targetTime;
    g_lastSeekRequestTime = now;
    g_isSeekPending = false;
}


// Seek tới vị trí tuyệt đối (seconds)
void mpv_command_seek_clamped(mpv_handle* mpv, float targetTime, float playbackTime, float duration) {
    if (!mpv || duration <= 0.0f || playbackTime < 0.0f)
        return;

    // Nếu targetTime là delta thì cộng với playbackTime
    // => trường hợp bạn truyền delta thay vì absolute
    if (targetTime < 0 || targetTime < duration + 1.0f) {
        targetTime = playbackTime + targetTime;
    }

    // Clamp targetTime vào [0, duration]
    targetTime = std::clamp(targetTime, 0.0f, std::max(duration - 0.05f, 0.0f));

    dataseek.forward = (targetTime > (float)g_playbackStatus.playbackTime);
    dataseek.pulse = 1.0f;

    Uint64 now = SDL_GetTicks64();

    // Nếu chưa đủ delay -> lưu pending
    if (now - g_lastSeekRequestTime < SEEK_DELAY_MS) {
        g_seekTargetTime = targetTime;
        g_isSeekPending = true;
        return;
    }

    // Thực hiện seek tuyệt đối
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", targetTime);
    const char* cmd[] = { "seek", buffer, "absolute", nullptr };
    mpv_command(mpv, cmd);

    g_seekTargetTime = targetTime;
    g_lastSeekRequestTime = now;
    g_isSeekPending = false;
}

// Gọi trong update loop để xử lý seek pending
void mpv_update_seek_pending(mpv_handle* mpv) {
    if (!g_isSeekPending)
        return;

    Uint64 now = SDL_GetTicks64();
    if (now - g_lastSeekRequestTime >= SEEK_DELAY_MS) {
        // Thực hiện seek
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", g_seekTargetTime);
        const char* cmd[] = { "seek", buffer, "absolute", nullptr };
        mpv_command(mpv, cmd);

        g_lastSeekRequestTime = now;
        g_isSeekPending = false;
    }
}

// Chỉnh âm lượng (0 - 100)
void mpv_command_set_volume(mpv_handle *mpv, int volume) {
    volume = std::clamp(volume, 0, 130);  // Đảm bảo không vượt quá 100
    std::string value = std::to_string(volume);
    const char *cmd[] = { "set", "volume", value.c_str(), nullptr };
    mpv_command(mpv, cmd);
}

void mpv_command_next_video(mpv_handle* mpv) {
    const char* cmd[] = { "playlist-next", nullptr };
    mpv_command(mpv, cmd);
}
void mpv_command_prev_video(mpv_handle* mpv) {
    const char* cmd[] = { "playlist-prev", nullptr };
    mpv_command(mpv, cmd);
}
// Bật / tắt tiếng
void mpv_command_set_mute(mpv_handle *mpv, bool mute) {
    const char *cmd[] = { "set", "mute", mute ? "yes" : "no", nullptr };
    mpv_command(mpv, cmd);
}
// Lấy thời gian đang phát (giây)
double mpv_get_playback_time(mpv_handle *mpv) {
    double time = 0;
    if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &time) == 0)
        return time;
    return 0.0;
}

// Lấy tổng thời lượng video (giây)
double mpv_get_duration(mpv_handle *mpv) {
    double dur = 0;
    if (mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &dur) == 0)
        return dur;
    return 0.0;
}

// Kiểm tra đang pause hay không
bool mpv_is_paused(mpv_handle *mpv) {
    int pauseFlag = 1;
    if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &pauseFlag) == 0)
        return pauseFlag != 0;
    return true;
}
void mpv_disable_video(mpv_handle* mpv) {
    if (!mpv) return;
    //mpv_set_property_string(mpv, "video-aspect-override", "0");
    mpv_set_property_string(mpv, "video", "no");
}
void mpv_enable_video(mpv_handle* mpv) {
    if (!mpv) return;
    //mpv_set_property_string(mpv, "video-aspect-override", "-2");
    mpv_set_property_string(mpv, "video", "auto");
}
void mpv_command_set_shader(mpv_handle* mpv, const std::string& path) {
    if (!mpv || path.empty()) return;

    // Sử dụng dấu nháy đơn xung quanh path để tránh lỗi đường dẫn có dấu cách
    std::string cmd = "change-list glsl-shaders append '" + path + "'";
    mpv_command_string(mpv, "change-list glsl-shaders clr ''");
    mpv_command_string(mpv, cmd.c_str());
}

