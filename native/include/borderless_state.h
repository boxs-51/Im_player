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
enum class SnapState {
    NONE,
    MAXIMIZED,
    LEFT,
    RIGHT
};
enum class ResizeEdge {
    NONE,
    LEFT, RIGHT, TOP, BOTTOM,
    TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT
};

// ================== State ==================

struct DragResizeState {

    int             resizeMargin            = 0;
    int             TitleHeight             = 0;
    int             sdlMinW                 = 0,                    sdlMinH     =0,        sdlMaxW      =0,     sdlMaxH     =0;
    int             restoreW                = 0;
    int             restoreH                = 0;
    int             aspectNum               = 0,                    aspectDen   = 0;    
    int             snapThreshold           = 10;  

    float           ControlWidth            = 0;
    float           dragRatioX              = 0.5f;  // tỉ lệ chuột trong cửa sổ (X)
    float           dragRatioY              = 0.0f;  // nếu cần Y

    bool            inTitle                 = false;
    bool            IsMax                   = false;
    bool            showDebug               = true;
    bool            IsFullscreen_video      = false;
    bool            isResizing              = false;
    bool            isDragging              = false;
    bool            aspectLock              = false;
    bool            actionSnap              = false;
    bool            snapEnabled             = true;
    bool            hasRestore              = false;

    WINDOWPLACEMENT placement               ={};

    LONG            style                   ={};

    HWND            hwnd_windown_main       = nullptr;
    HWND            hwndOverlay             = nullptr;

    WNDPROC         g_OldWndProc            = nullptr;

    ImVec2          dragStart               = ImVec2(0.0f, 0.0f);

    ResizeEdge      resizeEdge              = ResizeEdge::NONE;

    SnapState       snapState               = SnapState::NONE;

    RECT            winStart                = {};
    RECT            restoreRect             = {};
    RECT            fullscreenRestoreRect   = BW.fullscreenRestoreRect_temp;
    RECT            workArea                = {};            
    RECT            rcMin                   = {},                  rcMax        = {},         rcClose           = {};

    POINT           dragStartCursor         = {};

    UINT            dpiX                    = 96,                   dpiY        = 96;             
    
    std::string     debugInfo               = "None";

    LRESULT         lastHit                 = 0;
};

extern DragResizeState g_DragResizeState;


