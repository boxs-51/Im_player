// RuntimeFontController.cpp
#include "RuntimeFontController.h"
#include "WindowRuntime.h"
#include "WindowManager.h"
#include <iostream>

RuntimeFontController::RuntimeFontController(WindowRuntime* ownerRuntime)
    : m_ownerRuntime(ownerRuntime) {
    m_atlas = std::make_unique<ImFontAtlas>();
}

// Destructor: Dọn dẹp sạch sẽ ImFontAtlas khi Window bị đóng
RuntimeFontController::~RuntimeFontController() {
    Cleanup();
}

void RuntimeFontController::Cleanup() {
    if (m_atlas) {
        // Dọn dẹp GPU Textures / Font Data nội bộ của Atlas
        m_atlas->Clear();
        m_atlas.reset();
    }
    m_mainFont = nullptr;
}

bool RuntimeFontController::InitializeImGuiFontAtlas() {
    if (!m_ownerRuntime) return false;

    BuildAtlasInternal();
    return (m_mainFont != nullptr);
}

void RuntimeFontController::BuildAtlasInternal() {
    if (!m_atlas) {
        m_atlas = std::make_unique<ImFontAtlas>();
    }

    // 1. Dọn dẹp dữ liệu baked glyphs cũ trước khi thêm mới
    m_atlas->Clear();

#ifdef IMGUI_ENABLE_FREETYPE
    m_atlas->FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif

    float effectiveSize = m_settings.baseFontSize * m_settings.dpiScale;
    auto sysSet = FontManager::Instance().GetSystemFontSet(m_settings.mainFontId);

    // 2. Cấu hình Font chính
    ImFontConfig mainCfg{};
    mainCfg.OversampleH = 2;
    mainCfg.OversampleV = 2;
    mainCfg.PixelSnapH = true;
    
    // ⭐ CỰC CỲ QUAN TRỌNG ĐỂ TIẾT KIỆM RAM ⭐
    // Ép ImGui KHÔNG COPY thêm dữ liệu font vào RAM nữa, mà dùng chung pointer từ FontManager
    mainCfg.FontDataOwnedByAtlas = false; 

    if (sysSet.mainFont && sysSet.mainFont->fileData && !sysSet.mainFont->fileData->empty()) {
        m_mainFont = m_atlas->AddFontFromMemoryTTF(
            sysSet.mainFont->fileData->data(),
            static_cast<int>(sysSet.mainFont->fileData->size()),
            effectiveSize,
            &mainCfg,
            m_atlas->GetGlyphRangesDefault()
        );
    } else {
        m_mainFont = m_atlas->AddFontDefault();
    }

    // 3. Merge CJK, Icon, Emoji
    ImFontConfig mergeCfg = mainCfg;
    mergeCfg.MergeMode = true;

    if (sysSet.cjkFont && sysSet.cjkFont->fileData && !sysSet.cjkFont->fileData->empty()) {
        m_atlas->AddFontFromMemoryTTF(
            sysSet.cjkFont->fileData->data(),
            static_cast<int>(sysSet.cjkFont->fileData->size()),
            effectiveSize, &mergeCfg, m_atlas->GetGlyphRangesChineseFull()
        );
    }

    if (sysSet.iconFont && sysSet.iconFont->fileData && !sysSet.iconFont->fileData->empty()) {
        static const ImWchar icon_ranges[] = { 0xe005, 0xf8ff, 0 };
        m_atlas->AddFontFromMemoryTTF(
            sysSet.iconFont->fileData->data(),
            static_cast<int>(sysSet.iconFont->fileData->size()),
            effectiveSize, &mergeCfg, icon_ranges
        );
    }
    /*
    // 4. Build Atlas ngay tại đây
    m_atlas->Build();

    // In log kiểm tra kích thước Texture Atlas tạo ra trên RAM
    int width = 0, height = 0;
    m_atlas->GetTexDataAsAlpha8(nullptr, &width, &height);
    std::cout << "[FontController] Built Atlas for Window ID " 
              << (m_ownerRuntime ? m_ownerRuntime->info.id : 0) 
              << " | Texture Size: " << width << "x" << height 
              << " (" << (width * height / 1024) << " KB RAM)\n";*/
}

void RuntimeFontController::UpdateSettings(const FontSettings& newSettings, bool notifyChildren) {
    // Nếu thiết lập không đổi, không Rebuild làm gì để tránh lãng phí CPU/RAM
    if (m_settings.baseFontSize == newSettings.baseFontSize &&
        m_settings.dpiScale == newSettings.dpiScale &&
        m_settings.mainFontId == newSettings.mainFontId &&
        m_atlas && m_atlas->IsBuilt()) 
    {
        return; 
    }

    m_settings = newSettings;

    // Dựng lại Atlas
    BuildAtlasInternal();

    // Broadcast cho con
    if (notifyChildren && m_ownerRuntime) {
        auto children = WindowManager::GetInstance().GetChildrenOf(m_ownerRuntime->info.id);
        for (auto* child : children) {
            if (child && child->fontController) {
                FontSettings childSettings = child->fontController->GetSettings();
                childSettings.dpiScale = newSettings.dpiScale;
                childSettings.baseFontSize = newSettings.baseFontSize;
                
                child->fontController->UpdateSettings(childSettings, true);
            }
        }
    }
}