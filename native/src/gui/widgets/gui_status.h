#pragma once

#include <imgui.h>

// ============================================================
// Status Type
// ============================================================

enum class StatusType
{
    None = 0,

    Success,
    Info,
    Warning,
    Error,

    Disabled,

    Active,
    Inactive,

    Processing,
    Loading
};

typedef int BadgeFlags;
enum BadgeFlags_ {
    BadgeFlags_None        = 0,
    BadgeFlags_Filled      = 1 << 0,   // Có background
    BadgeFlags_Border      = 1 << 1,   // Có border
    BadgeFlags_Indicator   = 1 << 2,   // Hiển thị icon/dấu hiệu trạng thái
    BadgeFlags_Compact     = 1 << 3,   // Không thêm padding ngang
    BadgeFlags_NoRounding  = 1 << 4,   // Không rounding

    BadgeFlags_Default     = BadgeFlags_Filled | BadgeFlags_Indicator
};


struct StatusColors
{
    ImVec4 text;
    ImVec4 background;
    ImVec4 border;
    ImVec4 indicator;
};


namespace CSImGui {

StatusColors GetStatusColors(StatusType type);

void TextStatus(
    const char* text,
    StatusType type = StatusType::None);

void TextStatusFmt(
    StatusType type,
    const char* fmt,
    ...);

void Badge(
    const char* text,
    StatusType type = StatusType::None,
    BadgeFlags flags = BadgeFlags_Default);

void BadgeFmt(
    const char* fmt,
    StatusType type = StatusType::None,
    BadgeFlags flags = BadgeFlags_Default
    , ...);
    

void SuccessBadge(
    const char* text);

void InfoBadge(
    const char* text);

void WarningBadge(
    const char* text);

void ErrorBadge(
    const char* text);

void DisabledBadge(
    const char* text);

void ActiveBadge(
    const char* text);

void InactiveBadge(
    const char* text);

}