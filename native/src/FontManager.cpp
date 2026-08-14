#include "FontManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include "log.h"

FontManager& FontManager::Instance() {
    static FontManager inst;
    return inst;
}

std::string FontManager::ToLower(const std::string& s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

bool FontManager::IsIconFont(const std::string& family, const std::string& filename) const {
    std::string lower = ToLower(filename + family);
    return (lower.find("fontawesome") != std::string::npos ||
            lower.find("fa-") != std::string::npos ||
            lower.find("icons") != std::string::npos);
}

bool FontManager::IsEmojiFont(const std::string& family, const std::string& filename) const {
    std::string lower = ToLower(filename + family);
    return (lower.find("emoji") != std::string::npos || lower.find("twemoji") != std::string::npos);
}

void FontManager::ScanDirectories(const std::vector<std::string>& dirs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fontRegistry.clear();
    m_fontList.clear();

    std::vector<std::string> validExts = { ".ttf", ".otf", ".ttc" };

    for (const auto& dir : dirs) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();
            std::string ext = ToLower(entry.path().extension().string());

            if (std::find(validExts.begin(), validExts.end(), ext) == validExts.end()) continue;

            std::error_code ec;
            auto sz = fs::file_size(entry.path(), ec);
            if (ec || sz < 1024) continue; // Bỏ qua file lỗi/nhỏ hơn 1KB

            auto desc = std::make_shared<FontDescriptor>();
            desc->filePath = path;
            desc->family = entry.path().stem().string();
            desc->style = "Regular";
            desc->id = desc->family;

            desc->isIcon = IsIconFont(desc->family, entry.path().filename().string());
            desc->isEmoji = IsEmojiFont(desc->family, entry.path().filename().string());
            desc->isCJK = (ToLower(desc->family).find("cjk") != std::string::npos);

            // ⚠️ TIẾT KIỆM RAM BẰNG LAZY LOADING:
            // KHÔNG nạp fileData ở đây! Chỉ lưu metadata (đường dẫn, kích thước)
            desc->fileData = nullptr; 

            m_fontRegistry[desc->id] = desc;
            m_fontList.push_back(desc);
        }
    }
    LOG_NO_KEY(1, LogLevel::Info, LogCategory::System, 
        std::cout << "[FontManager] Scanned " << m_fontList.size() << " fonts metadata (0 MB RAM used).\n";
    );
}

// Hàm bổ sung: Lazy load dữ liệu nhị phân khi thực sự cần dùng
bool FontManager::EnsureFontDataLoaded(std::shared_ptr<FontDescriptor> desc) {
    if (!desc) return false;
    
    //std::lock_guard<std::mutex> lock(m_mutex);
    
    // Nếu đã nạp rồi thì dùng lại
    if (desc->fileData && !desc->fileData->empty()) {
        return true;
    }

    // Đọc file từ đĩa
    std::ifstream fi(desc->filePath, std::ios::binary);
    if (!fi.good()) return false;

    fi.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(fi.tellg());
    fi.seekg(0, std::ios::beg);

    desc->fileData = std::make_shared<std::vector<unsigned char>>(sz);
    fi.read(reinterpret_cast<char*>(desc->fileData->data()), sz);
    fi.close();

    LOG_NO_KEY(1, LogLevel::Info, LogCategory::System, 
        std::cout << "[FontManager] Lazy-loaded font data: " << desc->id << " (" << (sz / 1024) << " KB)\n";
    );
    return true;
}

std::vector<FontDescriptor> FontManager::GetAvailableFonts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FontDescriptor> result;
    result.reserve(m_fontList.size());
    for (const auto& item : m_fontList) {
        result.push_back(*item);
    }
    return result;
}

std::shared_ptr<FontDescriptor> FontManager::GetFontById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_fontRegistry.find(id);
    if (it != m_fontRegistry.end()) return it->second;
    return nullptr;
}

std::shared_ptr<FontDescriptor> FontManager::GetDefaultFont() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_fontList.empty()) return m_fontList.front();
    return nullptr;
}

FontManager::SystemFontSet FontManager::GetSystemFontSet(const std::string& preferredFamily) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    SystemFontSet set;
    
    for (const auto& font : m_fontList) {
        if (font->isIcon && !set.iconFont) set.iconFont = font;
        else if (font->isEmoji && !set.emojiFont) set.emojiFont = font;
        else if (font->isCJK && !set.cjkFont) set.cjkFont = font;
        else if (!set.mainFont || font->family == preferredFamily) set.mainFont = font;
    }

    // Đảm bảo các font được chọn đã được nạp binary vào RAM
    const_cast<FontManager*>(this)->EnsureFontDataLoaded(set.mainFont);
    const_cast<FontManager*>(this)->EnsureFontDataLoaded(set.cjkFont);
    const_cast<FontManager*>(this)->EnsureFontDataLoaded(set.iconFont);
    const_cast<FontManager*>(this)->EnsureFontDataLoaded(set.emojiFont);

    return set;
}

void FontManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, desc] : m_fontRegistry) {
        if (desc && desc->fileData) {
            desc->fileData->clear();
            desc->fileData->shrink_to_fit();
            desc->fileData.reset();
        }
    }
    m_fontRegistry.clear();
    m_fontList.clear();
}