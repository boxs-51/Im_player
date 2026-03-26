#include "lang_strings.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <locale>

static std::map<std::wstring, std::wstring> g_StringTable;
static std::map<std::wstring, std::string> g_CacheUTF8; // cache cho __(...)


void LoadStrings(const std::wstring& languageCode) {
    std::wstring filePath = L"D:\\MPVPLAYER\\language\\" + languageCode + L".ini";
    const wchar_t* section = L"UI";
    wchar_t buffer[1024];

    g_StringTable.clear();
    g_CacheUTF8.clear();

    for (int i = 1; i <= 9999; ++i) {
        std::wstring key = L"1001_" + std::to_wstring(i);
        if (GetPrivateProfileStringW(section, key.c_str(), L"", buffer, 1024, filePath.c_str()) == 0)
            break;

        g_StringTable[key] = buffer;
    }
}

std::wstring _(const std::wstring& key) {
    auto it = g_StringTable.find(key);
    return (it != g_StringTable.end()) ? it->second : key;
}

const char* __(const std::wstring& key) {
    auto it = g_CacheUTF8.find(key);
    if (it != g_CacheUTF8.end()) return it->second.c_str();

    std::string utf8 = WideToUTF8(_(key));
    g_CacheUTF8[key] = utf8;
    return g_CacheUTF8[key].c_str();
}
