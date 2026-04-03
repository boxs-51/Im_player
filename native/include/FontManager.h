// FontManager.h
#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <memory>

#include "imgui.h"
#include <GL/gl3w.h>
#include <SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace fs = std::filesystem;

struct LoadedFont {
    std::string path;
    std::string name;
    ImFont* font;
};
struct FontEntry {
    std::string file;
    std::string family;
    std::string style;
    ImFont* imFont = nullptr;
    ImFontAtlas* atlas = nullptr;
    float baseScale = 1.0f;

    // giữ bộ nhớ font để đảm bảo pointer còn sống
    std::shared_ptr<std::vector<unsigned char>> rawData;
};

struct AtlasEntry {
    ImFontAtlas* atlas = nullptr;
    GLuint texID = 0;
    bool built = false;
};

class FontManager {
public:
    static FontManager& Instance();

    // clear all (fonts + atlas + GPU textures)
    void Clear();

    bool LoadFontsSpecific(float size, const std::string& fontDir);

    bool LoadFontsSmartAuto(
        float size = 16.0f,
        int maxFonts = 100,
        const std::string& defaultFamily = "",
        const std::string& defaultStyle = "",
        std::vector<std::string> dirs = {
            "C:/Windows/Fonts"
        } 
    );

    // Multi-atlas smart loader
    bool LoadFontsSmartMultiAtlas(
        float size = 16.0f,
        int maxFonts = 100,
        int maxFontsPerAtlas = 120,
        int texWidth = 4080 ,
        const std::string& defaultFamily = "",
        const std::string& defaultStyle = "",
        std::vector<std::string> dirs = {
            "C:/Windows/Fonts"
        }
    );

    // Upload atlas textures to GPU
    void UploadAtlasTextures_OpenGL3();

    // Accessors
    void SetCurrentFont(int index);
    ImFont* GetCurrentFont() const;
    ImFont* GetFontByName(const std::string& family, const std::string& style);
    ImFont* GetFontByFamily(const std::string& family);
    
    ImFont* GetFont(const std::string& family, const std::string& style, float scale = 1.0f);

    int GetCurrentFontIndex() const { return m_currentFontIndex; };
    const std::vector<FontEntry>& GetFonts() const;
    template<typename Func>
    void WithFont(const std::string& family,const std::string& style,float scale,Func func)          
    {
        ImFont* font = GetFont(family, style, scale);
        if (font) {
            ImGui::PushFont(font);
            func();
            ImGui::PopFont();
            // reset lại font scale gốc để không ảnh hưởng chỗ khác
            ResetFontScale(font);
        } else {
            func(); // fallback nếu không tìm thấy font
        }
    }
    // ImGui helpers
    void PushFont(int index);
    void PopFont();

private:
    FontManager() = default;
    ~FontManager() { Clear(); }

    // utilities
    void ResetFontScale(ImFont* font);
    std::string ToLower(const std::string& s) const;
    bool HasExtension(const std::string& path, const std::vector<std::string>& exts) const;
    bool IsIconFont(const std::string& family, const std::string& filename) const;
    bool IsEmojiFont(const std::string& family, const std::string& filename) const;
    std::string FixPath(const std::string& path) const;

private:
    std::vector<AtlasEntry> m_atlases;
    std::vector<FontEntry> m_fonts;
    std::vector<LoadedFont> loadedFonts;
    ImFont* m_activeFont = nullptr;
    int m_currentFontIndex = -1;
};
