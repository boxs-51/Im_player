#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

struct FontDescriptor {
    std::string id;
    std::string family;
    std::string style;
    std::string filePath;
    bool isIcon = false;
    bool isEmoji = false;
    bool isCJK = false;
    std::shared_ptr<std::vector<unsigned char>> fileData = nullptr; // Mặc định nullptr để Lazy Load
};

class FontManager {
public:
    static FontManager& Instance();

    void ScanDirectories(const std::vector<std::string>& dirs);
    bool EnsureFontDataLoaded(std::shared_ptr<FontDescriptor> desc); // Lazy load binary font
    
    std::vector<FontDescriptor> GetAvailableFonts() const;
    std::shared_ptr<FontDescriptor> GetFontById(const std::string& id) const;
    std::shared_ptr<FontDescriptor> GetDefaultFont() const;

    struct SystemFontSet {
        std::shared_ptr<FontDescriptor> mainFont;
        std::shared_ptr<FontDescriptor> cjkFont;
        std::shared_ptr<FontDescriptor> iconFont;
        std::shared_ptr<FontDescriptor> emojiFont;
    };
    SystemFontSet GetSystemFontSet(const std::string& preferredFamily = "") const;

    void Shutdown();

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    std::string ToLower(const std::string& s) const;
    bool IsIconFont(const std::string& family, const std::string& filename) const;
    bool IsEmojiFont(const std::string& family, const std::string& filename) const;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<FontDescriptor>> m_fontRegistry;
    std::vector<std::shared_ptr<FontDescriptor>> m_fontList;
};