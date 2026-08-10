// WindowDefs.h
#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <memory>
#include "backends/IGraphicsBackend.h"

enum class ResizeEdge {
    NONE, LEFT, RIGHT, TOP, BOTTOM,
    TOPLEFT, TOPRIGHT, BOTTOMLEFT, BOTTOMRIGHT
};

struct WindowGeometryState {
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

    HMONITOR currentMonitor = nullptr;
};

struct WindowInputState {
    bool hasFocus = false;
    bool isActive = true;
    bool inputEnabled = true;

    ResizeEdge resizeEdge = ResizeEdge::NONE;
    LRESULT lastHit = 0;
    std::string hittestname;

    bool mouseDownMin = false;
    bool mouseDownMax = false;
    bool mouseDownClose = false;
    bool mouseDownRestore = false;

    bool mouseCaptured = false;

    bool moving = false;
    bool resizing = false;
    bool dragging = false;

    HCURSOR currentCursor = nullptr;
};

struct WindowDisplayState {
    bool isFullscreen = false;
    bool isMaximized = false;
    bool isMinimized = false;
    bool isVisible = true;
    bool isShown = true;

    UINT dpiX = 96;
    UINT dpiY = 96;
    float dpiScale = 1.0f;

    ImVec4 closeColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    ImVec4 maxColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);
    ImVec4 minColor = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);

    bool mouseHoverMin = false;
    bool mouseHoverMax = false;
    bool mouseHoverClose = false;
    bool mouseHoverRestore = false;
};

struct WindowRuntimeState {
    bool modal = false;
    bool created = false;
    bool destroyed = false;
    bool rendererReady = false;
    bool swapchainReady = false;
    bool surfaceLost = false;
    bool eventLoopAttached = false;
    bool renderingEnabled = true;
    bool is_dirty = false;
    bool isClosedPending = false;
};

struct WindowState {
    WindowGeometryState geometry;
    WindowInputState input;
    WindowDisplayState display;
    WindowRuntimeState runtime;
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
    bool allowhighdpi = false;

    int titleHeight = 28;
    int btnSize = 30;
    int resizeMargin = 8;

    bool isMainWindow = false;

    LONG Style = WS_OVERLAPPEDWINDOW;
    DWORD customStyleFlags = 0;

};