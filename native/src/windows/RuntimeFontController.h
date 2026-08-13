// RuntimeFontController.h
#pragma once
#include <imgui.h>
#include <memory>
#include <string>

//#define STB_TRUETYPE_IMPLEMENTATION
#define IMGUI_ENABLE_FREETYPE
#include "FontManager.h"
// include imstb_truetype WITHOUT STB implementation define here to avoid duplicate symbols.
// If you need the implementation, define STB_TRUETYPE_IMPLEMENTATION in exactly one .cpp in the project.
#if defined(STB_TRUETYPE_IMPLEMENTATION)
#include "imstb_truetype.h"
#endif
#if defined(IMGUI_ENABLE_FREETYPE)
#include "imgui_freetype.h" // from ImGui's misc/freetype folder
#endif

#include "imgui_internal.h"

struct WindowRuntime; // Forward declaration

struct FontSettings {
    float baseFontSize = 16.0f;
    float dpiScale = 1.0f;
    std::string mainFontId = "";
};

class RuntimeFontController {
public:
    explicit RuntimeFontController(WindowRuntime* ownerRuntime);
    ~RuntimeFontController();

    // Khai báo và khởi tạo Atlas độc lập cho Context của Window này
    bool InitializeImGuiFontAtlas();

    // Lắng nghe sự thay đổi kích thước/DPI hoặc thiết lập lại Font
    void UpdateSettings(const FontSettings& newSettings, bool notifyChildren = true);

    ImFont* GetMainFont() const { return m_mainFont; }
    ImFontAtlas* GetAtlas() const { return m_atlas.get(); }
    const FontSettings& GetSettings() const { return m_settings; }

    void PrepareNewFrame() {
        if (m_atlas) {
            // Cập nhật trạng thái atlas cho frame mới nếu tự quản lý Atlas
            // (Hàm nội bộ/public tùy phiên bản ImGui, hoặc ImGui tự gọi nếu gắn vào IO)
            ImGuiIO& io = ImGui::GetIO();
            
            // Đảm bảo Backend báo đã có Texture
            bool hasTexture = (io.BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0;
            
            // Đồng bộ trạng thái RendererHasTextures giữa Backend và Atlas
            m_atlas->RendererHasTextures = hasTexture;

            ImFontAtlasUpdateNewFrame(m_atlas.get(), ImGui::GetFrameCount(), m_atlas->RendererHasTextures);

        }
    }
    void Cleanup();

private:
    void BuildAtlasInternal();

    WindowRuntime* m_ownerRuntime = nullptr;
    std::unique_ptr<ImFontAtlas> m_atlas;
    ImFont* m_mainFont = nullptr;
    FontSettings m_settings;
};