#pragma once

#include <string>
#include <popup/reusable_popup.h>

struct UrlCacheItem {
    std::string utf8;
    std::string truncated;
    bool selectedForDelete = false;
};
struct FileCacheItem {
    std::string utf8;
    std::string truncated;
    bool selectedForDelete = false;
};
struct PopupData {
    
    bool autoLoadLastURL = true;
    bool saveHistory = true;
    std::wstring lastURL =  L"https://";
    std::vector<std::wstring> urlHistory {};
    std::vector<std::wstring> localHistory {};
    std::vector<UrlCacheItem> urlCache;
    std::vector<FileCacheItem>localCache;
};


void OpenURLPopup(class ReusablePopup& popup );
void RenderPopupOverlay_Url(class ReusablePopup& popup, class WindowRuntime* window);
