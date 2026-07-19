//ui_ioch_settings.h
#pragma once
#include <string>
#include <imgui.h>
#include <mpv/client.h>
#include "windows/WindowManager.h"

//
// ===========================
// CẤU HÌNH VIDEO PLAYER (IOCH)
// ===========================
//


// ==== PLAYBACK ====

void ApplyPlaybackSettings();   // Áp dụng settings đang có lên mpv

int  PlayVideo(WindowRuntime* runtime, const std::string& Url, const std::string& resolutionFormat, const std::string& title = "" );  // Phát video kèm độ phân giải

bool RenderToggleCombo(const char* label, bool& state);
// ==== UI SIDEBAR ====
void RenderIOCHSidebar(WindowRuntime* runtime ,ImVec2 videoPos ,ImVec2 videoSize, bool open, bool& show_ui_video ,ImVec2 iconPos);  // Giao diện chọn độ phân giải

void StartResolutionFetchInBackground(const std::string& Url);

void CallThread_URLFetch(WindowRuntime* runtime,const std::string& Url , bool playNow = true , const std::string& title = "" ,const std::string& format_id = "") ;




