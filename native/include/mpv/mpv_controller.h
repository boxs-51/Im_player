#pragma once
#include <string>
#include <mpv/client.h>

// Điều khiển
void mpv_command_play(mpv_handle* mpv);
void mpv_command_pause(mpv_handle* mpv);
void mpv_command_seek_clamped(mpv_handle* mpv, float deltaSeconds, float currentTime, float duration);
void mpv_command_seek_abs(mpv_handle* mpv, float targetTime, float duration);
void mpv_command_set_volume(mpv_handle* mpv, int volume);
void mpv_command_set_mute(mpv_handle* mpv, bool mute);
void mpv_update_seek_pending(mpv_handle* mpv);
void mpv_command_set_audio_delay(mpv_handle* mpv, double delaySeconds);
void mpv_command_set_speed(mpv_handle* mpv, double speed);
// Truy vấn trạng thái
double mpv_get_speed(mpv_handle* mpv);
double mpv_get_audio_delay(mpv_handle* mpv);
bool mpv_is_paused(mpv_handle* mpv);
bool mpv_is_muted(mpv_handle* mpv);
int  mpv_get_volume(mpv_handle* mpv);
double mpv_get_playback_time(mpv_handle* mpv);
double mpv_get_duration(mpv_handle* mpv);
void mpv_disable_video(mpv_handle* mpv);
void mpv_enable_video(mpv_handle* mpv);

void mpv_command_next_video(mpv_handle* mpv);
void mpv_command_prev_video(mpv_handle* mpv);
void mpv_command_set_shader(mpv_handle* mpv, const std::string& path  = "");