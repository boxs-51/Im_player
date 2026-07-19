// WindowDefs.h
#pragma once
#include <windows.h>

enum class ResizeEdge1 {
    NONE, LEFT, RIGHT, TOP, BOTTOM,
    TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT
};

struct WindowState {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 1280;
    int height = 720;
    
    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    
    int restoreW = 0;
    int restoreH = 0;
    RECT fullscreenRestoreRect = {};
    WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };

    bool isFullscreen = false;
    bool isMaximized = false;
    bool isMinimized = false;
    bool hasFocus = false;
    bool isActive = true;
    bool isVisible = true;
    bool isShown = true;
    
    UINT dpiX = 96;
    UINT dpiY = 96;
    
    ResizeEdge1 resizeEdge = ResizeEdge1::NONE;
    LRESULT lastHit = 0;
    
    // Trạng thái chuột trên các nút custom titlebar
    bool mouseDownMin = false;
    bool mouseDownMax = false;
    bool mouseDownClose = false;
    bool mouseDownRestore = false;

    bool isClosedPending = false;
};

struct WindowStyle {
    bool borderless = false;
    bool rounded = false;
    bool titlebar = true;
    bool resizable = true;
    bool alwaysOnTop = false;
    bool transparent = false;
    bool snapEnabled = true;
    bool trayIconEnabled = false;
    bool hiden = false;
    bool fullscreen = false;
    bool allowhighdpi = false;
    bool minimized = false;

    int titleHeight = 28;
    int btnSize = 30;
    int resizeMargin = 8;

    bool isMainWindow = false;

    LONG Style = WS_OVERLAPPEDWINDOW;
    DWORD customStyleFlags = 0; 
};