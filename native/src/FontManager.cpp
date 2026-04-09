

#define STB_TRUETYPE_IMPLEMENTATION
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
// To use FreeType, define USE_FREETYPE and make sure FT_Init_FreeType/FT_Done_FreeType are enabled below.
// For now we disable FreeType to use stb validation only:
//#define USE_FREETYPE
#include "utils.h"
#undef RATE_LIMITED_COUT
#define RATE_LIMITED_COUT(key, interval_ms, expr) do {} while(0)
#include <log.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <cstring>

 // ----------------- helpers -----------------
std::string FontManager::ToLower(const std::string& s) const {
    std::string out = s;
    for (auto& c : out) c = (char)tolower((unsigned char)c);
    return out;
}

bool FontManager::HasExtension(const std::string& path, const std::vector<std::string>& exts) const {
    std::string ext = ToLower(fs::path(path).extension().string());
    for (auto& e : exts) if (ext == e) return true;
    return false;
}

bool FontManager::IsIconFont(const std::string& family, const std::string& filename) const {
    std::string lower = ToLower(filename + family);
    return (//lower.find("fa-") != std::string::npos ||
            lower.find("fontawesome") != std::string::npos ||
            //lower.find("material") != std::string::npos ||
            lower.find("icons") != std::string::npos);
}

bool FontManager::IsEmojiFont(const std::string& family, const std::string& filename) const {
    std::string lower = ToLower(filename + family);
    return (lower.find("emoji") != std::string::npos ||
            lower.find("twemoji") != std::string::npos );
            //lower.find("noto") != std::string::npos);
}

std::string FontManager::FixPath(const std::string& path) const {
    std::string s = path;
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

// ----------------- singleton -----------------
FontManager& FontManager::Instance() {
    static FontManager inst;
    return inst;
}

// ----------------- Clear -----------------
void FontManager::Clear() {
    // Delete GPU textures
    for (auto& a : m_atlases) {
        if (a.texID) {
            glDeleteTextures(1, &a.texID);
            a.texID = 0;
        }
        if (a.atlas) {
            a.atlas->ClearTexData();
            a.atlas->ClearFonts();
            delete a.atlas;
            a.atlas = nullptr;
        }
    }
    m_atlases.clear();
    m_fonts.clear();
    m_activeFont = nullptr;
    m_currentFontIndex = -1;
}

// ----------------- Load multi-atlas -----------------
// synchronized with header: returns bool, accepts texWidth
bool FontManager::LoadFontsSmartMultiAtlas(
    float size,
    int maxFonts,
    int maxFontsPerAtlas,
    int texWidth,
    const std::string& defaultFamily,
    const std::string& defaultStyle,
    std::vector<std::string> dirs
)
{

    std::vector<std::string> exts = { ".ttf", ".otf" }; // skip .ttc to avoid stb parse issues

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    Clear();
    #ifdef USE_FREETYPE
    FT_Library ft = nullptr;
    if (FT_Init_FreeType(&ft)) {
        RATE_LIMITED_COUT(font_manager_freetype_init_failed_MultiAtlas, 1,std::cout << "[DEBUG] [ERROR] [FontManager] FreeType initialization failed. Falling back to stb_truetype.");
        return false;
    }
    #endif

    // clamp desired texture width to GPU limit if possible (we don't set TexDesiredWidth on older ImGui)
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    int desiredWidth = texWidth > 0 ? texWidth : 4096;
    if (maxTex > 0 && desiredWidth > maxTex) desiredWidth = maxTex;

    int loadedCount = 0;
    int defaultIndex = -1;
    std::set<std::pair<std::string, std::string>> loadedSet;

    ImFontAtlas* currentAtlas = new ImFontAtlas();
    // Note: some ImGui versions don't expose TexDesiredWidth; skip setting to maintain compatibility.
    m_atlases.push_back({ currentAtlas, 0, false });

    // reserve to avoid frequent reallocations
    m_fonts.reserve(std::min(maxFonts, 4096));

    for (const auto& dir : dirs) {
        if (loadedCount >= maxFonts) break;
        if (!fs::exists(dir)) continue;

        for (auto& entry : fs::directory_iterator(dir)) {
            if (loadedCount >= maxFonts) break;
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();
            if (!HasExtension(path, exts)) continue;

            std::error_code ec;
            auto fsize = fs::file_size(entry.path(), ec);
            if (ec || fsize < 1024) continue;

            #ifdef USE_FREETYPE
            FT_Face face = nullptr;
            if (FT_New_Face(ft, path.c_str(), 0, &face)) continue;

            std::string family = face->family_name ? face->family_name : fs::path(path).stem().string();
            std::string style  = face->style_name ? face->style_name : "Regular";
            #else
            // -- FreeType disabled: validate with stb_truetype and derive family/style from filename --
            std::string family = fs::path(path).stem().string();
            std::string style = "Regular";
            #endif

            auto key = std::make_pair(family, style);
            if (loadedSet.find(key) != loadedSet.end()) {
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }

            // read file into memory
            std::ifstream fi(path, std::ios::binary);
            if (!fi.good()) {
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }

            fi.seekg(0, std::ios::end);
            size_t sz = (size_t)fi.tellg();
            fi.seekg(0, std::ios::beg);
            auto buf = std::make_shared<std::vector<unsigned char>>();
            buf->resize(sz);
            if (sz > 0) fi.read((char*)buf->data(), sz);
            fi.close();

            if (buf->empty()) {
                RATE_LIMITED_COUT(font_manager_font_load_failed_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] Failed to read font file: " << path << "\n");
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }

            // Quick header checks: skip TTC and OpenType-CFF ("OTTO") which stb_truetype may not handle.
            // Accept only likely-TrueType fonts (sfnt version 0x00010000 or "true") to avoid stb parse failures.
            const unsigned char* hdr = buf->data();
            bool header_ok = false;
            if (buf->size() >= 4) {
                // 'ttcf' => font collection (skip)
                if (memcmp(hdr, "ttcf", 4) == 0) {
                    RATE_LIMITED_COUT(font_manager_skip_ttc_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] Skip font collection (TTC): " << path << "\n");
                    #ifdef USE_FREETYPE
                    FT_Done_Face(face);
                    #endif
                    continue;
                }
                // 'OTTO' => OpenType/CFF (skip for stb_truetype backend)
                if (memcmp(hdr, "OTTO", 4) == 0) {
                    RATE_LIMITED_COUT(font_manager_skip_otf_cff_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] Skip OpenType/CFF font (not supported by stb_truetype): " << path << "\n");
                    #ifdef USE_FREETYPE
                    FT_Done_Face(face);
                    #endif
                    continue;
                }
                // 0x00010000 (big-endian) or "true" indicate TrueType sfnt - acceptable
                if (hdr[0] == 0x00 && hdr[1] == 0x01 && hdr[2] == 0x00 && hdr[3] == 0x00) header_ok = true;
                if (memcmp(hdr, "true", 4) == 0) header_ok = true;
                // Some fonts may also start with '\0\0\0\0' or other; be conservative
            }
            if (!header_ok) {
                std::cerr << "[FontManager] Skip (unsupported header) : " << path << " size=" << buf->size() << "\n";
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }


            int currentCount = 0;
            // Attempt to read Fonts.Size safely (older ImFontAtlas internals may differ)
            // Use fallback of 0 if not accessible
            #if defined(IMGUI_VERSION_NUM)
            // try to access Fonts.Size where available
            currentCount = (int)currentAtlas->Fonts.Size;
            #endif

            if (currentCount >= maxFontsPerAtlas) {
                currentAtlas = new ImFontAtlas();
                // Note: don't set TexDesiredWidth to stay compatible with ImGui versions without it
                m_atlases.push_back({ currentAtlas, 0, false });
            }

            ImFontConfig cfg{}; // zero-init for safety
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            cfg.MergeMode = true;
            cfg.PixelSnapH = true;
            cfg.RasterizerMultiply = 1.0f;
            cfg.FontDataOwnedByAtlas = false; // will adjust when we hand memCopy to ImGui

            ImFont* font = nullptr;

            const int fontSizeInt = (int)buf->size();
            if (fontSizeInt <= 0) {
                RATE_LIMITED_COUT(font_manager_invalid_font_size_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] Invalid font size (0) for: " << path << "\n");
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }
            // Make a copy of the font bytes and hand ownership to ImGui (safe lifetime)
            void* memCopy = ImGui::MemAlloc(fontSizeInt);
            if (!memCopy) {
                RATE_LIMITED_COUT(font_manager_mem_alloc_failed_failed_MultiAtlas, 1,std::cout << "[DEBUG] [ERROR] [FontManager] ImGui MemAlloc failed for font: " << path << " size=" << fontSizeInt << "\n");
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }
            memcpy(memCopy, buf->data(), fontSizeInt);
            
            // Validate font bytes with stb_truetype before handing to ImGui.
            // This catches cases where FreeType accepted the file but stb_truetype (ImGui backend) cannot parse it,
            // preventing ImGui assertion/crash.
            #ifdef STB_TRUETYPE_IMPLEMENTATION
            {
                const unsigned char* data_u = reinterpret_cast<const unsigned char*>(memCopy);
                int font_index = stbtt_GetFontOffsetForIndex(data_u, 0);
                stbtt_fontinfo finfo;
                if (!stbtt_InitFont(&finfo, data_u, font_index)) {
                    RATE_LIMITED_COUT(font_manager_stb_validation_failed_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] stb_truetype validation failed (skipping): " << path << "\n");
                    ImGui::MemFree(memCopy);
                    #ifdef USE_FREETYPE
                    FT_Done_Face(face);
                    #endif
                    continue;
                }
            }
            #endif

            cfg.FontDataOwnedByAtlas = true; // ImGui will free memCopy later
            try {
                if (IsIconFont(family, path)) {
                    cfg.MergeMode = true;
                    static const ImWchar icon_ranges[] = { 
                        (ImWchar)0xF000, 
                        (ImWchar)0xFAFF, 
                        0 };
                    font = currentAtlas->AddFontFromMemoryTTF(memCopy, fontSizeInt, size - 2.0f, &cfg, icon_ranges);
                } else if (IsEmojiFont(family, path)) {
                    cfg.MergeMode = true;
                    static const ImWchar emoji_ranges[] = { 
                        (ImWchar)0x1F300, 
                        (ImWchar)0x1F6FF, 
                        0 };
                    font = currentAtlas->AddFontFromMemoryTTF(memCopy, fontSizeInt, size, &cfg, emoji_ranges);
                } else {
                    static const ImWchar default_ranges[] = {
                        0x0020, 0x00FF,
                        0x0100, 0x024F,
                        0x1EA0, 0x1EFF,
                        0
                    };
                    font = currentAtlas->AddFontFromMemoryTTF(memCopy, fontSizeInt, size, &cfg, default_ranges);
                }
            } catch (...) {
                font = nullptr;
            }
            if (!font) {
                // AddFontFromMemoryTTF failed → free copy and skip file
                ImGui::MemFree(memCopy);
                RATE_LIMITED_COUT(font_manager_add_font_failed_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] ImGui AddFontFromMemoryTTF failed (skipping): " << path << "\n");
                #ifdef USE_FREETYPE
                FT_Done_Face(face);
                #endif
                continue;
            }

            FontEntry fe;
            fe.file = path;
            fe.family = family;
            fe.style = style;
            fe.imFont = font;
            fe.atlas = currentAtlas;
            fe.baseScale = 1.0f;
            // We still keep original buf in FontEntry.rawData if needed, but memCopy is owned by ImGui
            fe.rawData = buf;
            m_fonts.push_back(std::move(fe));
            loadedSet.insert(key);
            loadedCount++;

            if (!defaultFamily.empty() && !defaultStyle.empty() &&
                family == defaultFamily && style == defaultStyle)
                defaultIndex = loadedCount - 1;

            #ifdef USE_FREETYPE
            FT_Done_Face(face);
            #endif

            RATE_LIMITED_COUT(font_manager_font_loaded_failed_MultiAtlas, 1,std::cout << "[DEBUG] [INFO] [FontManager] Loaded font: " << family << " - " << style << " from " << path << "\n");
        }
    }

    #ifdef USE_FREETYPE
    FT_Done_FreeType(ft);
    #endif

    // upload (requires GL context!). Caller must ensure context is current.

    UploadAtlasTextures_OpenGL3();
    if (m_fonts.empty()) {
        RATE_LIMITED_COUT(font_manager_no_fonts_loaded_failed_MultiAtlas, 1,std::cout << "[DEBUG] [WARNING] [FontManager] No fonts loaded. Using ImGui default font as fallback.\n");
        ImFont* def = io.Fonts->AddFontDefault();
        UploadAtlasTextures_OpenGL3();
        m_fonts.push_back({ "builtin", "Default", "Regular", def, io.Fonts, 1.0f });
        m_activeFont = def;
        m_currentFontIndex = 0;
        RATE_LIMITED_COUT(font_manager_loaded_builtin_font_failed_MultiAtlas, 1,std::cout << "[DEBUG] [INFO] [FontManager] Loaded ImGui built-in default font as fallback.\n");
        return false;
    } else {
        if (defaultIndex < 0) defaultIndex = 0;
        m_currentFontIndex = defaultIndex;
        m_activeFont = m_fonts[defaultIndex].imFont;
        RATE_LIMITED_COUT(font_manager_fonts_loaded_successfully_MultiAtlas, 1,std::cout << "[DEBUG] [INFO] [FontManager] Loaded " 
                            << m_fonts.size() << " fonts successfully. Current font: " 
                            << m_fonts[defaultIndex].family << " - " 
                            << m_fonts[defaultIndex].style << "\n");
        return true;
    }
}


// ----------------- Upload atlas textures -----------------
void FontManager::UploadAtlasTextures_OpenGL3() {
    for (auto& a : m_atlases) {
        if (!a.atlas) continue;

        if (!a.atlas->IsBuilt()){
#if defined(IMGUI_ENABLE_FREETYPE)
            const ImFontLoader* ft_loader = ImGuiFreeType::GetFontLoader();
            if (ft_loader) {
                a.atlas->SetFontLoader(ft_loader);
            }
            // Build regardless (FreeType loader will be used if set)
            a.atlas->Build();
#else
            a.atlas->Build(); // build internal font texture
#endif
        }

        // Try RGBA32 first
        unsigned char* pixelsRGBA = nullptr;
        int width = 0, height = 0;
        a.atlas->GetTexDataAsRGBA32(&pixelsRGBA, &width, &height);

        if (pixelsRGBA && width > 0 && height > 0) {
            // upload RGBA texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);
            a.texID = tex;
            a.atlas->SetTexID((ImTextureID)(intptr_t)tex);
            a.built = true;
            a.atlas->ClearTexData();
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            std::cout << "Uploaded RGBA atlas: " << width << "x" << height << " TexID=" << tex << "\n";
            continue;
        }

        // Fallback: try alpha8 (1 byte per pixel)
        unsigned char* pixelsA = nullptr;
        a.atlas->GetTexDataAsAlpha8(&pixelsA, &width, &height);
        if (pixelsA && width > 0 && height > 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            // Upload single channel; GL_RED preferred on modern GL
#if defined(GL_RED)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0,
                         GL_RED, GL_UNSIGNED_BYTE, pixelsA);
            // Ensure red channel is used as alpha for shader that expects alpha mask
            GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
#else
            // Older GL: use GL_ALPHA if available
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, width, height, 0,
                         GL_ALPHA, GL_UNSIGNED_BYTE, pixelsA);
#endif
            a.texID = tex;
            a.atlas->SetTexID((ImTextureID)(intptr_t)tex);
            a.built = true;
            a.atlas->ClearTexData();
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            std::cout << "Uploaded ALPHA atlas: " << width << "x" << height << " TexID=" << tex << "\n";
            continue;
        }

        std::cerr << "Atlas has no texture data, skip\n";
    }
}
// ----------------- accessors -----------------
void FontManager::SetCurrentFont(int index) {
    if (index >= 0 && index < (int)m_fonts.size()) {
        m_currentFontIndex = index;
        m_activeFont = m_fonts[index].imFont;
    }
}
ImFont* FontManager::GetCurrentFont() const {
    return m_activeFont;
}
ImFont* FontManager::GetFontByName(const std::string& family, const std::string& style) {
    for (auto& f : m_fonts) {
        if (ToLower(f.family) == ToLower(family) &&
            ToLower(f.style) == ToLower(style)) {
            return f.imFont;
        }
    }
    return nullptr;
}
ImFont* FontManager::GetFontByFamily(const std::string& family) {
    for (auto& f : m_fonts) {
        if (ToLower(f.family) == ToLower(family)) {
            return f.imFont;
        }
    }
    return nullptr;
}
ImFont* FontManager::GetFont(const std::string& family, const std::string& style, float scale) {
    for (auto& f : m_fonts) {
        if (ToLower(f.family) == ToLower(family) &&
            ToLower(f.style) == ToLower(style)) {
            f.imFont->Scale = scale;
            return f.imFont;
        }
    }
    return nullptr;
}
void FontManager::ResetFontScale(ImFont* font) {
    for (auto& f : m_fonts) {
        if (f.imFont == font) {
            f.imFont->Scale = f.baseScale;
            return;
        }
    }
}
const std::vector<FontEntry>& FontManager::GetFonts() const {
    return m_fonts;
}
void FontManager::PushFont(int index) {
    if (index < 0 || index >= (int)m_fonts.size()) return;
    ImGui::PushFont(m_fonts[index].imFont);
}
void FontManager::PopFont() {
    ImGui::PopFont();
}
bool FontManager::LoadFontsSmartAuto(
    float size,
    int maxFonts,
    const std::string& defaultFamily,
    const std::string& defaultStyle,
    std::vector<std::string> dirs)
{
    ImGuiIO& io = ImGui::GetIO();
#ifdef USE_FREETYPE
    io.Fonts->FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif
    io.Fonts->Clear(); // reset toàn bộ font
    Clear();

    std::vector<std::string> exts = { ".ttf", ".otf" ,"ttc"};

#ifdef USE_FREETYPE
    FT_Library ft = nullptr;
    if (FT_Init_FreeType(&ft)) {
        RATE_LIMITED_COUT(font_manager_freetype_init_failed_failed_SmartAuto, 1,
            std::cout << "[DEBUG] [ERROR] FreeType initialization failed. Falling back to stb_truetype.\n");
        return false;
    }
#endif

    int loadedCount = 0;
    int defaultIndex = -1;
    std::set<std::pair<std::string, std::string>> loadedSet;
    std::vector<std::string> emojiIconFonts;

    for (const auto& dir : dirs) {
        if (loadedCount >= maxFonts) break;
        if (!fs::exists(dir)) continue;

        for (auto& entry : fs::directory_iterator(dir)) {
            if (loadedCount >= maxFonts) break;
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();
            if (!HasExtension(path, exts)) continue;

            std::ifstream fi(path, std::ios::binary);
            if (!fi.good()) continue;
            fi.seekg(0, std::ios::end);
            size_t sz = (size_t)fi.tellg();
            fi.seekg(0, std::ios::beg);
            if (sz < 1024) continue;

            std::vector<unsigned char> data(sz);
            fi.read((char*)data.data(), sz);
            fi.close();

#ifdef STB_TRUETYPE_IMPLEMENTATION
            unsigned char* data_u = data.data();
            int font_index = stbtt_GetFontOffsetForIndex(data_u, 0);
            stbtt_fontinfo finfo;
            if (font_index < 0 || !stbtt_InitFont(&finfo, data_u, font_index)) {
                continue;
            }
#endif

#ifdef USE_FREETYPE
            FT_Face face = nullptr;
            if (FT_New_Face(ft, path.c_str(), 0, &face)) continue;
            std::string family = face->family_name ? face->family_name : fs::path(path).stem().string();
            std::string style  = face->style_name ? face->style_name : "Regular";
#else
            std::string family = fs::path(path).stem().string();
            std::string style = "Regular";
#endif

            if (IsEmojiFont(family, path) || IsIconFont(family, path)) {
#ifdef USE_FREETYPE
                FT_Done_Face(face);
#endif
                emojiIconFonts.push_back(path);
                continue;
            }

            auto key = std::make_pair(family, style);
            if (loadedSet.count(key)) {
#ifdef USE_FREETYPE
                FT_Done_Face(face);
#endif
                continue;
            }

            ImFontConfig cfg{};
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            cfg.PixelSnapH = true;
            cfg.RasterizerMultiply = 1.0f;

            ImFont* font = io.Fonts->AddFontFromFileTTF(
                path.c_str(),
                size,
                &cfg,
                io.Fonts->GetGlyphRangesDefault()
            );

            if (!font) {
                RATE_LIMITED_COUT(font_manager_add_font_failed_failed_SmartAuto, 1,
                    std::cout << "[DEBUG] [WARNING] AddFontFromFileTTF failed: " << path << "\n");
#ifdef USE_FREETYPE
                FT_Done_Face(face);
#endif
                continue;
            }

            FontEntry fe;
            fe.file = path;
            fe.family = family;
            fe.style = style;
            fe.imFont = font;
            fe.atlas = io.Fonts;
            fe.baseScale = 1.0f;
            m_fonts.push_back(std::move(fe));

            loadedSet.insert(key);
            loadedCount++;

            if (!defaultFamily.empty() && !defaultStyle.empty() &&
                family == defaultFamily && style == defaultStyle)
                defaultIndex = loadedCount - 1;

#ifdef USE_FREETYPE
            FT_Done_Face(face);
#endif
            RATE_LIMITED_COUT(font_manager_font_loaded_SmartAuto, 1,
                std::cout << "[DEBUG] Loaded font: " << family << " - " << style << "\n");
        }
    }

    // Merge emoji/icon fonts
    if (!emojiIconFonts.empty() && !m_fonts.empty()) {
        for (const auto& emojiPath : emojiIconFonts) {
            if (!fs::exists(emojiPath)) continue;

            ImFontConfig mergeCfg{};
            mergeCfg.MergeMode = true;
            mergeCfg.PixelSnapH = true;
            mergeCfg.OversampleH = 1;
            mergeCfg.OversampleV = 1;
#ifdef USE_FREETYPE
            mergeCfg.FontLoaderFlags = ImGuiFreeTypeBuilderFlags_LoadColor;
#endif

            static const ImWchar emoji_ranges[] = {
                (ImWchar)0x2190, (ImWchar)0x21FF,
                (ImWchar)0x2300, (ImWchar)0x23FF,
                (ImWchar)0x2600, (ImWchar)0x27BF,
                (ImWchar)0x2900, (ImWchar)0x297F,
                (ImWchar)0x1F000, (ImWchar)0x1FAFF,
                0
            };

            ImFont* merged = io.Fonts->AddFontFromFileTTF(
                emojiPath.c_str(),
                size * 0.9f,
                &mergeCfg,
                emoji_ranges
            );

            if (merged) {
                RATE_LIMITED_COUT(font_manager_emoji_merged_SmartAuto, 1,
                    std::cout << "[DEBUG] Merged emoji/icon font: " << emojiPath << "\n");
            } else {
                RATE_LIMITED_COUT(font_manager_emoji_merge_failed_SmartAuto, 1,
                    std::cout << "[DEBUG] Failed to merge emoji/icon font: " << emojiPath << "\n");
            }
        }
    }

#ifdef USE_FREETYPE
    if (ft) FT_Done_FreeType(ft);
#endif

    if (m_fonts.empty()) {
        RATE_LIMITED_COUT(font_manager_no_fonts_loaded_SmartAuto, 1,
            std::cout << "[DEBUG] No fonts loaded. Using ImGui default font.\n");
        ImFont* def = io.Fonts->AddFontDefault();
        m_fonts.push_back({ "builtin", "Default", "Regular", def, io.Fonts, 1.0f });
        m_activeFont = def;
        m_currentFontIndex = 0;
        return false;
    }

    if (defaultIndex < 0) defaultIndex = 0;
    m_currentFontIndex = defaultIndex;
    m_activeFont = m_fonts[defaultIndex].imFont;

    // Font atlas info
    unsigned char* pixels = nullptr;
    int width = 0, height = 0, bpp = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);

    std::cout << "[DEBUG] Font atlas size: " << width << "x" << height << ", Bpp=" << bpp << "\n";
    return true;
}

bool FontManager::LoadFontsSpecific(float size, const std::string& fontDir) {
    ImGuiIO& io = ImGui::GetIO();
    
    // Hàm trợ giúp kiểm tra file "sống" hay không
    auto IsValidFontFile = [](const std::string& path) -> bool {
        try {
            if (!fs::exists(path)) return false;
            if (!fs::is_regular_file(path)) return false;
            size_t fileSize = fs::file_size(path);
            if (fileSize < 1024) return false; // File nhỏ hơn 1KB chắc chắn không phải font hợp lệ
            return true;
        } catch (...) { return false; }
    };

#ifdef USE_FREETYPE
    io.Fonts->FontLoaderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
#endif

    io.Fonts->Clear();
    this->Clear();

    std::string pathMain  = fontDir + "/Notosans-Regular.ttf";
    std::string pathCJK   = fontDir + "/NotoSansCJK-Regular.ttc";
    std::string pathEmoji = fontDir + "/NotoColorEmoji.ttf";
    std::string pathIcon  = fontDir + "/fa-solid-900.ttf";

    // 1. Cấu hình Font chính
    ImFontConfig mainCfg{};
    mainCfg.OversampleH = 2;
    mainCfg.OversampleV = 2;
    mainCfg.PixelSnapH = true;

    // --- BƯỚC 1: LOAD FONT CHÍNH ---
    if (!IsValidFontFile(pathMain)) {
        std::cout << "[ERROR] Main font missing or invalid: " << pathMain << "\n";
        // Nếu font chính lỗi, dùng font mặc định của ImGui để tránh crash
        ImFont* defFont = io.Fonts->AddFontDefault();
        m_activeFont = defFont;
        return false;
    }

    ImFont* mainFont = io.Fonts->AddFontFromFileTTF(pathMain.c_str(), size, &mainCfg, io.Fonts->GetGlyphRangesDefault());
    if (!mainFont) return false;

    // --- CẤU HÌNH MERGE ---
    ImFontConfig mergeCfg{};
    mergeCfg.MergeMode = true;
    mergeCfg.PixelSnapH = true;

    // --- BƯỚC 2: MERGE CJK ---
    if (IsValidFontFile(pathCJK)) {
        io.Fonts->AddFontFromFileTTF(pathCJK.c_str(), size, &mergeCfg, io.Fonts->GetGlyphRangesChineseFull());
    }

    // --- BƯỚC 3: MERGE ICONS ---
    if (IsValidFontFile(pathIcon)) {
        static const ImWchar icon_ranges[] = { 0xe005, 0xf8ff, 0 }; 
        io.Fonts->AddFontFromFileTTF(pathIcon.c_str(), size, &mergeCfg, icon_ranges);
    }
    // --- BƯỚC 4: MERGE EMOJI ---
    #ifdef USE_FREETYPE 
        // Chỉ thử load Emoji nếu có FreeType
        if (IsValidFontFile(pathEmoji)) {
            ImFontConfig emojiCfg = mergeCfg;
            emojiCfg.FontLoaderFlags = ImGuiFreeTypeBuilderFlags_LoadColor; // Bắt buộc
            
            static const ImWchar emoji_ranges[] = { 0x2000, 0x206F, 0x2190, 0x21FF, 0x1F000, 0x1FAFF, 0 };
            
            // Dùng con trỏ tạm để kiểm tra
            ImFont* res = io.Fonts->AddFontFromFileTTF(pathEmoji.c_str(), size, &emojiCfg, emoji_ranges);
            if (!res) {
                RATE_LIMITED_COUT(font_manager_emoji_merge_failed_specific, 1,
                    std::cout << "[DEBUG] Failed to merge emoji font: " << pathEmoji << "\n");  
            }
        }
    #else
        RATE_LIMITED_COUT(font_manager_freetype_required_for_emoji_specific, 1,
            std::cout << "[DEBUG] FreeType required to load emoji fonts. Skipping: " << pathEmoji << "\n");
    #endif
    // Cập nhật trạng thái
    FontEntry fe;
    fe.family = "Default";
    fe.style  = "Default";
    fe.imFont = mainFont;
    m_fonts.push_back(fe);
    m_activeFont = mainFont;

    return true;
}