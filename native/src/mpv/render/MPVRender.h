#pragma once

#include <mpv/client.h>
#include <mpv/render.h>
#include <imgui.h>
#include <memory>

class MPVPlayer; // Forward declaration
class IGraphicsBackend;

class MPVRender {
public:
    MPVRender();
    ~MPVRender();

    bool Init(MPVPlayer& player, IGraphicsBackend* backend);
    void Shutdown();

    void Render(const ImVec2& size, IGraphicsBackend* backend);

    mpv_render_context* GetContext() const { return m_render_ctx; }

private:
    mpv_render_context* m_render_ctx = nullptr;
};