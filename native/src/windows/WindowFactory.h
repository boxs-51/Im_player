// WindowFactory.h
#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "WindowTemplate.h"
#include "WindowRuntime.h"
#include <SDL_syswm.h>
#include <gl3w.h>

class WindowTemplateRegistry {
private:
    std::unordered_map<std::string, WindowTemplate> templates;

public:
    void RegisterTemplate(const std::string& name, const WindowTemplate& tpl) {
        templates[name] = tpl;
    }

    const WindowTemplate* GetTemplate(const std::string& name) const {
        auto it = templates.find(name);
        return (it != templates.end()) ? &it->second : nullptr;
    }
};

class WindowFactory {
private:
    WindowTemplateRegistry* registry;
    uint32_t nextId = 1;

public:
    WindowFactory(WindowTemplateRegistry* reg) : registry(reg) {}

    WindowRuntime* Create(const std::string& templateName) {
        const auto* tpl = registry->GetTemplate(templateName);
        if (!tpl) return nullptr;

        auto* runtime = new WindowRuntime();
        runtime->id = nextId++;
        runtime->style = tpl->style;
        runtime->properties = tpl->defaultProperties; // Bản sao sâu (deep copy) thuộc tính
        if (tpl->rendererFactory) {
            runtime->renderer = tpl->rendererFactory();
        }

        // Tạo cửa sổ vật lý thông qua SDL2
        Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
        if (runtime->style.borderless) flags |= SDL_WINDOW_BORDERLESS;
        if (runtime->style.resizable)  flags |= SDL_WINDOW_RESIZABLE;

        runtime->sdlWindow = SDL_CreateWindow(
            tpl->name.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            runtime->state.width, runtime->state.height, flags
        );

        if (!runtime->sdlWindow) {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }
        runtime->mainGLContext = SDL_GL_CreateContext(runtime->sdlWindow);

        if (!runtime->mainGLContext) {
            SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
            SDL_DestroyWindow(runtime->sdlWindow);
            return false;
        }

        // Make current before gl loader init
        if (SDL_GL_MakeCurrent(runtime->sdlWindow, runtime->mainGLContext) != 0) {
            SDL_Log("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
            SDL_GL_DeleteContext(runtime->mainGLContext);
            SDL_DestroyWindow(runtime->sdlWindow);
            return false;
        }

        if (gl3wInit() != 0) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenGL Error", "Can't init gl3w!", nullptr);
            SDL_GL_DeleteContext(runtime->mainGLContext);
            SDL_DestroyWindow(runtime->sdlWindow);
            return false;
        }

        SDL_GL_SetSwapInterval(1);

        // Trích xuất HWND WinAPI và thực hiện Hook WndProc đa luồng / đa cửa sổ[cite: 4]
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(runtime->sdlWindow, &wmInfo);
        runtime->hwnd = wmInfo.info.win.window;

        // Lưu con trỏ runtime vào HWND của WinAPI[cite: 4]
        SetPropW(runtime->hwnd, L"WINDOW_RUNTIME_PTR", (HANDLE)runtime);

        // Thực hiện gài đè WndProc[cite: 4]
        extern LRESULT CALLBACK MultiWindowWndProc(HWND, UINT, WPARAM, LPARAM);
        WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(runtime->hwnd, GWLP_WNDPROC, (LONG_PTR)MultiWindowWndProc);
        runtime->properties.Set<WNDPROC>("OldWndProc", oldProc);

        // Áp dụng các Style nâng cao của WinAPI
        LONG winStyle = GetWindowLong(runtime->hwnd, GWL_STYLE);
        winStyle |= (  WS_MINIMIZEBOX  |
                        WS_THICKFRAME |
                        WS_CAPTION 
                    );
        if (runtime->style.snapEnabled) winStyle |= WS_MAXIMIZEBOX;
        SetWindowLong(runtime->hwnd, GWL_STYLE, winStyle);
        SetWindowPos(runtime->hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        if (runtime->renderer) {
            runtime->renderer->Initialize(runtime);
        }

        return runtime;
    }
};