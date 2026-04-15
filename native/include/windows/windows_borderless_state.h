#pragma once

#include "globals.h"
#include "utils.h"
#include "imgui.h"

#include <string>
#include <windows.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
using namespace Microsoft::WRL;


// ================== Enums ==================
enum class ResizeEdge {
    NONE,
    LEFT, RIGHT, TOP, BOTTOM,
    TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT
};

// ================== State ==================

struct DragResizeState {

    int resizeMargin  = 8;
    #ifdef CUSTOM_TITLEBAR
    int TitleHeight = 28;
    #else
    int TitleHeight = 0;
    #endif
    int sdlMinW = 0;
    int sdlMinH = 0;
    int sdlMaxW = 0;
    int sdlMaxH = 0;
    int restoreW = 0;
    int restoreH  = 0;

    bool IsMax = false;
    bool showDebug  = true;
    bool IsFullscreen_video = false;

    bool  snapEnabled = true;

    WINDOWPLACEMENT placement  ={};

    LONG style  ={};

    HWND hwnd_windown_main = nullptr;

    WNDPROC g_OldWndProc = nullptr;


    ResizeEdge resizeEdge = ResizeEdge::NONE;
        
    UINT dpiX = 96;
    UINT dpiY = 96;             
    
    std::string debugInfo = "None";

    LRESULT lastHit = 0;

    bool mouseDownMin = false;
    bool mouseDownMax = false;
    bool mouseDownClose = false;
    bool mouseDownRestore = false;

    bool trayiconAdded = true;
    bool ToggleFullscreen = false;

    int btnSize = 30;

    RECT fullscreenRestoreRect;

};

DragResizeState& GetDragResizeState();


