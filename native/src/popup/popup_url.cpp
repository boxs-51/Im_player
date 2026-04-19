
#include "globals.h"
#include "utils.h"
#include "json.hpp"

#include <popup/popup_url.h>

#include <mpv/mpv_basic_formats.h>
#include <mpv/mpv_custom_ui.h>
#include <mpv/mpv_ui.h>
#include <mpv/mpv_ui_settings.h>

#include <commdlg.h>  
#include <vector>
#include <string>
#include <imgui.h>
#include "reusable_popup.h"
#include <filesystem>

using json = nlohmann::json;

static bool urlConfirmed = false;
static bool bufferInitialized = false;
// --- State tồn tại suốt vòng đời popup ---
static PopupData data;
static std::wstring urlInput;
static std::wstring lastURLCopy;
static std::vector<std::wstring> pendingLocalFilesLocal;
static std::wstring outResultURL;


const std::string SETTINGS_PATH_POPUP_URL = AutoPath<std::string>("%ROOT%", "data","popup_data.json");

static int StringResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = (std::string*)data->UserData;
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }
    return 0;
}
// --- Lịch sử URL ---
PopupData LoadPopupData() {
    std::filesystem::path path = SETTINGS_PATH_POPUP_URL;
    PopupData data;

    // 2️⃣ Nếu file chưa tồn tại, tạo mặc định
    if(!std::filesystem::exists(path)) {
        std::ofstream o(path);
        json j;
        j["config"]["AutoLoadLastURL"] = data.autoLoadLastURL;
        j["config"]["SaveHistory"] = data.saveHistory;
        j["lastURL"] = WideToUTF8(data.lastURL);
        j["urlHistory"] = {};
        j["localHistory"] = {};
        o << j.dump(4);
        return data;
    }

    // 3️⃣ Load dữ liệu JSON
    std::ifstream f(path);
    if(f.is_open()){
        try {
            json j; f >> j;
            data.autoLoadLastURL = j["config"].value("AutoLoadLastURL", data.autoLoadLastURL);
            data.saveHistory = j["config"].value("SaveHistory", data.saveHistory);
            data.lastURL = UTF8ToWide(j.value("lastURL", WideToUTF8(data.lastURL)));

            for(auto &u : j["urlHistory"])
                data.urlHistory.push_back(UTF8ToWide(u.get<std::string>()));

            for(auto &lf : j["localHistory"])
                data.localHistory.push_back(UTF8ToWide(lf.get<std::string>()));
        } catch(const std::exception& e) {
            std::cerr << "Failed to load popup JSON: " << e.what() << std::endl;
        }
    }

    return data;
}


void SavePopupData( const PopupData& data){
    std::filesystem::path path = SETTINGS_PATH_POPUP_URL;
    json j;
    j["config"]["AutoLoadLastURL"] = data.autoLoadLastURL;
    j["config"]["SaveHistory"] = data.saveHistory;
    j["lastURL"] = WideToUTF8(data.lastURL);
    j["urlHistory"] = {};
    for(auto &u : data.urlHistory) j["urlHistory"].push_back(WideToUTF8(u));
    j["localHistory"] = {};
    for(auto &f : data.localHistory) j["localHistory"].push_back(WideToUTF8(f));
    
    std::ofstream o(path);
    o << j.dump(4); // indent 4 spaces
}

bool IsValidURLOrPath(const wchar_t* text) {
    if (!text || !*text) return false;

    // URL
    if (wcsncmp(text, L"http://", 7) == 0 ||
        wcsncmp(text, L"https://", 8) == 0) {
        return true;
    }

    // File URI
    if (wcsncmp(text, L"file://", 7) == 0) {
        return true;
    }

    // Windows absolute path: C:\...
    if (wcslen(text) > 2 &&
        iswalpha(text[0]) &&
        text[1] == L':' &&
        (text[2] == L'\\' || text[2] == L'/')) {
        return true;
    }

    // UNC path: \\server\share
    if (wcsncmp(text, L"\\\\", 2) == 0) {
        return true;
    }

    return false;
}

std::vector<std::wstring> OpenFilePickerW() {
    std::vector<std::wstring> files;

    OPENFILENAMEW ofn;
    wchar_t szFile[32768] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile)/sizeof(wchar_t);
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.mov\0All Files\0*.*\0";
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        wchar_t* p = ofn.lpstrFile;
        std::wstring folder = p;
        p += folder.size() + 1;

        if (*p == 0) { // chỉ chọn 1 file
            files.push_back(folder);
        } else {       // nhiều file
            while (*p) {
                files.push_back(folder + L"\\" + std::wstring(p));
                p += wcslen(p) + 1;
            }
        }
    }

    return files;
}
void AddURLToHistory(PopupData& data, const std::wstring& newUrl){
    data.urlHistory.erase(std::remove(data.urlHistory.begin(), data.urlHistory.end(), newUrl), data.urlHistory.end());
    data.urlHistory.insert(data.urlHistory.begin(), newUrl);
    if(data.urlHistory.size() > 50) data.urlHistory.resize(50);
    data.lastURL = newUrl;
}




void AddLocalToHistory(PopupData& data, const std::wstring& filePath){
    data.localHistory.erase(std::remove(data.localHistory.begin(), data.localHistory.end(), filePath), data.localHistory.end());
    data.localHistory.insert(data.localHistory.begin(), filePath);
    if(data.localHistory.size() > 50) data.localHistory.resize(50);
    data.lastURL = filePath;
}

// --- Kiểm tra hợp lệ ---
bool IsLikelyVideoURL(const std::string& url) {
    return (url.rfind("http://",0) == 0 || url.rfind("https://",0) == 0) && url.length() > 10 && url.find('.') != std::string::npos && url.find(' ') == std::string::npos;
}

bool IsValidLocalFile(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES) && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}
void RebuildUrlCache(PopupData& data, float maxWidth)
{
    data.urlCache.clear();
    data.urlCache.reserve(data.urlHistory.size());

    for (auto& w : data.urlHistory)
    {
        UrlCacheItem item;
        item.utf8 = WideToUTF8(w);
        item.truncated = TextUtils::TruncateTextByPixels(item.utf8.c_str(), maxWidth - 10.0f);
        data.urlCache.push_back(std::move(item));
    }
}
void RebuildLocalCache(PopupData& data, float maxWidth)
{
    data.localCache.clear();
    data.localCache.reserve(data.localHistory.size());

    for (auto& w : data.localHistory)
    {
        FileCacheItem item;
        item.utf8 = WideToUTF8(w);
        item.truncated = TextUtils::TruncateTextByPixels(item.utf8.c_str(), maxWidth - 10.0f);
        data.localCache.push_back(std::move(item));
    }
}
// --- Popup chính ---
void HistoryUrlPage(PopupData& data, std::wstring& urlInput, ImVec2 size)
{
    if (CSImGui::BeginModernChild(
        "##URLHistoryTable",
        size,
        true,
        ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        static int selectedUrlIndex = -1;
        static float lastWidth = 0.0f;

        auto& urlHistory = data.urlHistory;
        auto& cache = data.urlCache;

        float maxWidth = ImGui::GetContentRegionAvail().x;

        // 👉 Rebuild cache khi cần
        if (cache.size() != urlHistory.size() || fabs(lastWidth - maxWidth) > 1.0f)
        {
            RebuildUrlCache(data, maxWidth);
            lastWidth = maxWidth;
        }

        // 👉 Clip list (QUAN TRỌNG NHẤT)
        ImGuiListClipper clipper;
        clipper.Begin((int)urlHistory.size());

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto& item = cache[i];
                bool isSelected = (selectedUrlIndex == i);

                if (CSImGui::ModernSelectable(item.truncated.c_str(), isSelected))
                {
                    selectedUrlIndex = i;

                    urlInput = urlHistory[i];
                }

                // 👉 Tooltip chỉ khi hover

                CSImGui::ShowTooltipDelayed(item.utf8.c_str(),ImGui::IsItemHovered(),3.0f,
                                                ("url_tooltip_" + std::to_string(i)).c_str());
 
                ImGui::Separator();
            }
        }

        CSImGui::EndModernChild();
    }
}
void HistoryFileLocalPage(PopupData& data, std::wstring& urlInput, ImVec2 size)
{
    if (CSImGui::BeginModernChild(
        "##LocalFilesTable",
        size,
        true,
        ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        static int selectedFileIndex = -1;
        static float lastWidth = 0.0f;

        auto& localHistory = data.localHistory;
        auto& cache = data.localCache;

        float maxWidth = ImGui::GetContentRegionAvail().x;

        // 👉 Rebuild cache khi cần
        if (cache.size() != localHistory.size() || fabs(lastWidth - maxWidth) > 1.0f)
        {
            RebuildLocalCache(data, maxWidth);
            lastWidth = maxWidth;
        }

        // 👉 Clip list (QUAN TRỌNG)
        ImGuiListClipper clipper;
        clipper.Begin((int)localHistory.size());

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto& item = cache[i];
                bool isSelected = (selectedFileIndex == i);

                if (CSImGui::ModernSelectable(item.truncated.c_str(), isSelected))
                {
                    selectedFileIndex = i;

                    urlInput = localHistory[i];
                }

                // 👉 Tooltip chỉ khi hover

                CSImGui::ShowTooltipDelayed(item.utf8.c_str(),ImGui::IsItemHovered(),3.0f,
                                                ("file_tooltip_" + std::to_string(i)).c_str());

            }
        }

        CSImGui::EndModernChild();
    }
}
void ShowURLPopupContent(bool& closePopup, std::wstring& outResultURL,
                         PopupData& data, std::wstring& lastURL,std::wstring& urlInput ,
                         std::vector<std::wstring>& pendingLocalFilesLocal) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float popupWidth = avail.x;
    float lineHeight = ImGui::GetTextLineHeight();
    float maxInputHeight = lineHeight*2;
    float historyHeight = lineHeight*10;
    static bool invalidUrl = false; // local

    ImGui::TextDisabled("NHẬP NGUỒN VIDEO");
    ImGui::Separator();

    ImGui::Text("Vui lòng nhập URL hoặc chọn file local:");
    
    std::string urlInpututf8 = WideToUTF8(urlInput);
    if(CSImGui::ModernInputTextMultiline("##URLInput", urlInpututf8,
                            ImVec2(popupWidth, maxInputHeight),
                            ImGuiInputTextFlags_AllowTabInput))
  
    Disabehotkey = ImGui::IsItemActive();

    if(CSImGui::ModernCheckbox("Tự động lấy URL trước đó",&data.autoLoadLastURL))SavePopupData(data);
    ImGui::Separator();

    // --- History tabs ---
    if (CSImGui::BeginModernTabBar("##HistoryTabs")) {
        // URL Tab
        ImVec2 size = ImVec2(popupWidth,historyHeight);
        if(CSImGui::ModernTabItem("URL")){
            HistoryUrlPage(data , urlInput ,size);
            CSImGui::EndModernTabItem();
        }

        // Local Tab
        if(CSImGui::ModernTabItem("File local")){
            HistoryFileLocalPage(data , urlInput ,size);
            CSImGui::EndModernTabItem();
        }

        CSImGui::EndModernTabBar();
    }

    ImGui::Separator();
    if(CSImGui::ModernCheckbox("Tự động lưu lich sử ",&data.saveHistory))SavePopupData(data);

    if( CSImGui::ModernButton("Lấy từ Clipboard")){
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
            return;
        if(OpenClipboard(NULL)){
            HANDLE hData=GetClipboardData(CF_UNICODETEXT);
            if(hData){
                LPCWSTR clipText=(LPCWSTR)GlobalLock(hData);
                if (clipText && *clipText) {

                    // Check URL hợp lệ cơ bản
                    if (IsValidURLOrPath(clipText)) {
                        urlInput = clipText;
                    }
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }


    ImGui::SameLine(0.0f,30.0f);
    if( CSImGui::ModernButton("Thêm từ file local")){
        auto files=OpenFilePickerW();
        if(!files.empty()){
            std::wstring firstFileUtf8=files[0];
            urlInput = firstFileUtf8;
            pendingLocalFilesLocal.clear();
            for(auto &f:files) pendingLocalFilesLocal.push_back(f);
        }
    }

    ImGui::Separator();
    ImGui::Text("URL trước đó:");
    
    std::string lasturltrum = TextUtils::TruncateTextByPixels(WideToUTF8(lastURL).c_str(),popupWidth - 20.0f);
    ImGui::Text("%s",lasturltrum.c_str());


    // --- Buttons OK/Cancel ---
    if( CSImGui::ModernButton("OK")){
        if( IsValidLocalFile(urlInput)){
            outResultURL = urlInput;
            if(data.saveHistory) AddLocalToHistory(data, urlInput);
            pendingLocalFilesLocal.clear();
            SetVideoTypeLocal();
            urlConfirmed = true;
            closePopup = true;
            invalidUrl = false;
        }else if(IsLikelyVideoURL(WideToUTF8(urlInput))){
            outResultURL =urlInput;
            if(data.saveHistory) AddURLToHistory(data, urlInput);
            urlConfirmed = true;
            closePopup = true;
            invalidUrl = false;
        }else{
            invalidUrl = true;
        }
        SavePopupData(data);
    }

    ImGui::SameLine();
    if( CSImGui::ModernButton("Hủy")){
        closePopup=true;
        pendingLocalFilesLocal.clear();
    }

    if(invalidUrl)
        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1),"⚠ URL hoặc file không hợp lệ.");
}

void OpenURLPopup(ReusablePopup& popup ) {

    // Chỉ load dữ liệu khi popup chưa mở
    if(!popup.IsOpen()) {
        data = LoadPopupData();
        lastURLCopy = data.lastURL;
        urlInput = data.autoLoadLastURL ? data.lastURL : L"https://";
        pendingLocalFilesLocal.clear();
        bufferInitialized = false;
    }

    popup.Open("Popup Url", [] (bool& closePopup) {
        ShowURLPopupContent(closePopup,outResultURL,
                            data,lastURLCopy,
                            urlInput,
                            pendingLocalFilesLocal); 
    });
}


void RenderPopupOverlay_Url(ReusablePopup& popup) {
    // Hiển thị popup nhập URL
    popup.Render();

    // Nếu đã xác nhận URL từ popup
    if (urlConfirmed && !outResultURL.empty()) {

        CallThread_URLFetch(WideToUTF8(outResultURL),true); // Giao luôn việc cho thread
        urlConfirmed = false;
        outResultURL.clear();
    }

}
