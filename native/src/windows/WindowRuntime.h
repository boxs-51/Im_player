// WindowRuntime.h (Cập nhật)
#pragma once
#include <SDL.h>
#include <windows.h>
#include <memory>
#include "WindowDefs.h"
#include "WindowPropertyBag.h"
#include "WindowRenderer.h"
#include "WindowController.h"
#include "WindowInfo.h"
#include "WindowResource.h"
#include "WindowRelation.h"


#include "utils.h" 

class FrameTimer;

/**
 * @brief 
 * 
 */
class WindowRuntime {
public:

    WindowInfo info;
    WindowState state;
    WindowStyle style;
    PropertyBag properties;
    
    WindowRelation relation;
    WindowResource resource;
 
    std::unique_ptr<WindowRenderer> renderer;
    std::unique_ptr<WindowController> controller;

    std::unique_ptr<FrameTimer> windowloop;

    //bool isTemporarilyHidden = false;


    WindowRuntime(WindowId id = 0, SDL_Window* sdlWindow = nullptr, HWND hwnd = nullptr);
    ~WindowRuntime();

    // --- Các hàm tiện ích truy vấn ---
    WindowRuntime* GetParent();
    std::vector<WindowRuntime*> GetChildren();
    bool HasVisibleChildren();
};