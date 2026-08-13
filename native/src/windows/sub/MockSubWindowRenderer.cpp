#include "MockSubWindowRenderer.h"
#include "windows/WindowRuntime.h"
#include "WindowSnapshot.h"
#include "main/ui/ui.h"

#include "gui.h"
#include <imgui.h>

void MockSubWindowRenderer::Initialize(WindowRuntime* runtime) {
    // Không cần khởi tạo gì cho renderer đơn giản này
}

void MockSubWindowRenderer::RenderUI(WindowRuntime* runtime, const WindowSnapshot& snapshot) {
    // UIRenderThread đã gọi BeginFrame/NewFrame.
    // Chúng ta chỉ cần vẽ nội dung của mình.

    // Giờ đây tất cả các cửa sổ đều dùng chung WindowLayout
    //auto layout = runtime->properties.GetValue<WindowLayout>("Layout");
    const auto layout = snapshot.GetLayout();

    ImGui::SetNextWindowPos(ImVec2(layout.WinX, layout.WinY));
    ImGui::SetNextWindowSize(ImVec2(layout.WinW, layout.WinH));
    //ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    
    ImGui::Begin("SubWindowCanvas", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove
    );
    RenderTitleBarWindowObject(runtime, "SubWindowCanvas", "", layout.TitlePos, layout.TitleSize);

    ImGui::Text("Đây là một cửa sổ phụ (sub-window) mô phỏng.");
    ImGui::Text("Bạn có thể vẽ bất cứ thứ gì ở đây bằng ImGui.");
    ImGui::Separator();
    if (ImGui::Button("Đóng cửa sổ này")) {
        if (runtime->controller) {
            runtime->controller->Close(); // Gửi sự kiện đóng một cách an toàn
        }
    }

    ImGui::End();
}

void MockSubWindowRenderer::Shutdown() {
    // Không cần dọn dẹp gì
}