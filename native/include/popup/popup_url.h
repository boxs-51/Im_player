#pragma once

#include "globals.h"
#include "utils.h"
#include <string>
#include "reusable_popup.h"

struct PopupData {
    bool autoLoadLastURL = true;
    bool saveHistory = true;
    std::wstring lastURL =  L"https://";
    std::vector<std::wstring> urlHistory {};
    std::vector<std::wstring> localHistory {};
};



// --- Lịch sử URL ---
PopupData LoadPopupData();
void SavePopupData( const PopupData& data);


// --- Popup chính ---
void ShowURLPopupContent(bool& closePopup_url,
                         std::wstring& outResultURL,
                         PopupData& data,
                         std::wstring& lastURL,
                         std::wstring& urlInput,
                         std::vector<std::wstring>& pendingLocalFilesLocal);

void OpenURLPopup(class ReusablePopup& popup );
void RenderPopupOverlay_Url() ;
