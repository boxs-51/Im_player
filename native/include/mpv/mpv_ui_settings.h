//ui_ioch_settings.h
#pragma once

#include "globals.h"
#include <string>
#include <vector>
#include <SDL.h>  
#include <mutex>   
#include <regex>

//
// ===========================
// CẤU HÌNH VIDEO PLAYER (IOCH)
// ===========================
//


// ==== PLAYBACK ====

void ApplyPlaybackSettings();   // Áp dụng settings đang có lên mpv

int  PlayVideo(mpv_handle * mpv, const std::string& Url, const std::string& resolutionFormat, const std::string& title = "" );  // Phát video kèm độ phân giải

bool RenderToggleCombo(const char* label, bool& state);
// ==== UI SIDEBAR ====
void RenderIOCHSidebar(mpv_handle * mpv ,ImVec2 videoPos ,ImVec2 videoSize, bool open);  // Giao diện chọn độ phân giải

void StartResolutionFetchInBackground(const std::string& Url);




