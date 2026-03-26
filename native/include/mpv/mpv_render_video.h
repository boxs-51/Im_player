#pragma once

#include <mpv/client.h>
#include <imgui.h>

/// Khởi tạo mpv và thiết lập các tuỳ chọn cơ bản
bool InitMPV(mpv_handle*& mpv);

/// Tạo render context OpenGL cho mpv (render_ctx)
bool InitMPVRenderContext(mpv_handle* mpv);

/// Render video mpv ra FBO đang được ImGui/OpenGL sử dụng
void RenderMPVVideo(const ImVec2& size);

/// Dọn dẹp mpv + render context khi thoát
void CleanupMPV();
