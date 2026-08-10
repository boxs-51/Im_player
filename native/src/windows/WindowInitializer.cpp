// WindowInitializer.cpp
#include "WindowInitializer.h"
#include "WindowRuntime.h"
#include "WindowTemplate.h"
#include "WindowFlagBuilder.h"
#include "NativeWindowRegistrar.h"
#include "WindowSharedGroup.h"
#include "UIRenderThread.h"
#include <SDL.h>
#include <imgui.h>

bool WindowInitializer::Initialize(WindowRuntime* runtime, const WindowTemplate* tpl, WindowRuntime* parent) {
    if (!runtime || !tpl) return false;

    // Create backend first to get its flags
    std::unique_ptr<IGraphicsBackend> backend = nullptr;
    if (tpl->graphicsBackendFactory) {
        backend = tpl->graphicsBackendFactory();
    }

    if (!InitializeSDL(runtime, tpl, backend.get())) {
        return false;
    }

    if (!InitializeSharedGroup(runtime, parent)) {
        SDL_DestroyWindow(runtime->resource.sdlWindow);
        return false;
    }

    if (!InitializeImGui(runtime)) {
        SDL_DestroyWindow(runtime->resource.sdlWindow);
        return false;
    }

    runtime->resource.graphicsBackend = std::move(backend);
    if (!InitializeBackend(runtime)) {
        ImGui::DestroyContext(runtime->resource.imguiCtx);
        SDL_DestroyWindow(runtime->resource.sdlWindow);
        return false;
    }

    if (!InitializeThread(runtime)) {
        // dtor of runtime will handle cleanup
        return false;
    }

    InitializeNative(runtime);
    InitializeRenderer(runtime);
    AttachMPV(runtime, parent);

    runtime->resource.uiRenderThread->Start();

    return true;
}

bool WindowInitializer::InitializeSDL(WindowRuntime* runtime, const WindowTemplate* tpl, IGraphicsBackend* backend) {
    Uint32 flags = SDLFlagBuilder()
        .Hidden(!runtime->state.display.isShown)
        .Fullscreen(runtime->state.display.isFullscreen)
        .Borderless(runtime->style.borderless)
        .Resizable(runtime->style.resizable)
        .Minimized(runtime->state.display.isMinimized)
        .HighDPI(runtime->style.allowhighdpi)
        .WithBackend(backend)
        .Build();

    runtime->resource.sdlWindow = SDL_CreateWindow(
        tpl->name.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        runtime->state.geometry.width, runtime->state.geometry.height, flags
    );

    if (!runtime->resource.sdlWindow) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool WindowInitializer::InitializeSharedGroup(WindowRuntime* runtime, WindowRuntime* parent) {
    if (parent) {
        runtime->resource.sharedGroup = parent->resource.sharedGroup;
        runtime->relation.sharedGroup = parent->resource.sharedGroup;
    } else {
        runtime->resource.sharedGroup = std::make_shared<WindowSharedGroup>();
        runtime->relation.sharedGroup = runtime->resource.sharedGroup;
    }

    if (!runtime->relation.sharedGroup.lock()) {
        SDL_Log("Failed to create or inherit WindowSharedGroup.");
        return false;
    }
    return true;
}

bool WindowInitializer::InitializeImGui(WindowRuntime* runtime) {
    auto shared_atlas_ptr = runtime->relation.sharedGroup.lock();
    if (!shared_atlas_ptr || !shared_atlas_ptr->m_sharedFontAtlas) {
        SDL_Log("Failed to get shared font atlas from WindowSharedGroup.");
        return false;
    }
    runtime->resource.imguiCtx = ImGui::CreateContext(shared_atlas_ptr->m_sharedFontAtlas.get());
    ImGui::SetCurrentContext(runtime->resource.imguiCtx);
    return true;
}

bool WindowInitializer::InitializeBackend(WindowRuntime* runtime) {
    if (!runtime->resource.graphicsBackend) {
        SDL_Log("Window '%s' requires a graphics backend.", runtime->info.templateName.c_str());
        return false;
    }
    if (!runtime->resource.graphicsBackend->InitContext(runtime->resource.sdlWindow) || !runtime->resource.graphicsBackend->InitImGuiBackend(runtime->resource.sdlWindow)) {
        SDL_Log("Backend initialization failed for window '%s'.", runtime->info.templateName.c_str());
        return false;
    }
    return true;
}

bool WindowInitializer::InitializeThread(WindowRuntime* runtime) {
    try {
        runtime->resource.uiRenderThread = std::make_unique<UIRenderThread>(runtime, runtime->resource.graphicsBackend.get());
    } catch (const std::exception& e) {
        SDL_Log("Failed to create UIRenderThread: %s", e.what());
        return false;
    }
    return true;
}

void WindowInitializer::InitializeNative(WindowRuntime* runtime) {
    NativeWindowRegistrar::Register(runtime);
}

void WindowInitializer::InitializeRenderer(WindowRuntime* runtime) {
    if (runtime->renderer) {
        runtime->renderer->Initialize(runtime);
    }
}

void WindowInitializer::AttachMPV(WindowRuntime* runtime, WindowRuntime* parent) {
    if (!runtime) return;

    if (parent) {
        // Cửa sổ con: Phân định Semantic rõ ràng
        // 1. Sao chép ID phiên phát từ cha để có thể truy cập Commander/Property/Observer
        runtime->resource.playersessionid = parent->resource.playersessionid;
        runtime->resource.playersession = parent->resource.playersession;

        // 2. ĐÁNH DẤU CỬA SỔ CON: Không cho phép tự mở render loop video riêng lên FBO 
        // để tránh 2 window cùng render vào 1 mpv_render_context gây crash/xé hình
        runtime->properties.Set<bool>("IsSecondaryMpvOutput", true);
        runtime->properties.Set<bool>("RenderVideoFlag", false);
    } else {
        if(runtime->style.create_mpv && !runtime->resource.playersession) {
            // Cửa sổ độc lập / Root Window: Tự tạo một PlayerSession mới
            auto* session = PlayerManager::GetInstance().CreateSession(runtime);
            if (session) {
                runtime->resource.playersessionid = session->GetId();
                runtime->resource.playersession = session;
                runtime->properties.Set<bool>("IsSecondaryMpvOutput", false);
            }
        }
    }
}