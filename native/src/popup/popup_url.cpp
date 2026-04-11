
#include "globals.h"
#include "utils.h"
#include "popup_url.h"
#include "json.hpp"

#include "mpv/mpv_basic_formats.h"
#include "mpv/mpv_custom_ui.h"

#include "thread.h"

#include <commdlg.h>  
#include <vector>
#include <string>
#include <imgui.h>
#include "reusable_popup.h"
#include <filesystem>

using json = nlohmann::json;

static bool urlConfirmed = false;
static bool bufferInitialized = false;
static char buffer[32768] = {};
// --- State tồn tại suốt vòng đời popup ---
static PopupData data;
static std::wstring urlInput;
static std::wstring lastURLCopy;
static std::vector<std::wstring> pendingLocalFilesLocal;
static std::wstring outResultURL;

const std::string SETTINGS_PATH_POPUP_URL = AutoPath<std::string>("%ROOT%", "data","popup_data.json");
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

// --- Popup chính ---

void HistoryUrlPage(PopupData& data, std::wstring& urlInput ,ImVec2 size )
{
    if(CusTomImGui::BeginModernChild("##URLHistoryTable",size,true,ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        static int selectedUrlIndex=-1;
        auto& urlHistory = data.urlHistory;
        for(size_t i=0; i<urlHistory.size(); ++i){
            std::string labelUtf8 = WideToUTF8(urlHistory[i]);
            if (CusTomImGui::ModernSelectable(labelUtf8.c_str(), selectedUrlIndex==(int)i)) {
            //if(ImGui::Selectable(labelUtf8.c_str(), selectedUrlIndex==(int)i)){
                selectedUrlIndex = (int)i;
                std::wstring wLabel = UTF8ToWide(labelUtf8);
                strncpy(buffer, WideToUTF8(wLabel).c_str(), sizeof(buffer)-1);
                buffer[sizeof(buffer) - 1] = '\0';
                urlInput = wLabel;
            }
            ImGui::Separator();
        }
        CusTomImGui::EndModernChild();
    }
}
void HistoryFileLocalPage(PopupData& data, std::wstring& urlInput ,ImVec2 size )
{
    if(CusTomImGui::BeginModernChild("##LocalFilesTable",size,true,ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        static int selectedFileIndex=-1;
        auto& localHistory = data.localHistory;
        for(size_t i=0; i<localHistory.size(); ++i){
            std::string labelUtf8 = WideToUTF8(localHistory[i]);
            if(ImGui::Selectable(labelUtf8.c_str(), selectedFileIndex==(int)i)){
                selectedFileIndex = (int)i;
                std::wstring wLabel = UTF8ToWide(labelUtf8);
                strncpy(buffer, WideToUTF8(wLabel).c_str(), sizeof(buffer)-1);
                buffer[sizeof(buffer) - 1] = '\0';
                urlInput = wLabel;
            }
        }
        CusTomImGui::EndModernChild();
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


    if (!bufferInitialized) {
        std::string utf8 = WideToUTF8(urlInput);
        strncpy(buffer, utf8.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        bufferInitialized = true;
    }      
    CusTomImGui::ModernInputTextMultiline("##URLInput", buffer, sizeof(buffer),
                            ImVec2(popupWidth-1, maxInputHeight-1),
                            ImGuiInputTextFlags_AllowTabInput);
 
    Disabehotkey = ImGui::IsItemActive();

    urlInput = UTF8ToWide(buffer);

    if(CusTomImGui::ModernCheckbox("Tự động lấy URL trước đó",&data.autoLoadLastURL))SavePopupData(data);
    ImGui::Separator();

    // --- History tabs ---
    if (CusTomImGui::BeginModernTabBar("##HistoryTabs")) {
        // URL Tab
        ImVec2 size = ImVec2(popupWidth,historyHeight);
        if(ImGui::BeginTabItem("URL")){
            HistoryUrlPage(data , urlInput ,size);
            ImGui::EndTabItem();
        }

        // Local Tab
        if(ImGui::BeginTabItem("File local")){
            HistoryFileLocalPage(data , urlInput ,size);
            ImGui::EndTabItem();
        }

        CusTomImGui::EndModernTabBar();
    }

    ImGui::Separator();
    if(CusTomImGui::ModernCheckbox("Tự động lưu lich sử ",&data.saveHistory))SavePopupData(data);

    if( CusTomImGui::ModernButton("Lấy từ Clipboard")){
        if(OpenClipboard(NULL)){
            HANDLE hData=GetClipboardData(CF_UNICODETEXT);
            if(hData){
                LPCWSTR clipText=(LPCWSTR)GlobalLock(hData);
                if(clipText && *clipText && wcsstr(clipText,L"http")){
                    urlInput = clipText;
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }

    ImGui::SameLine(0.0f,30.0f);
    if( CusTomImGui::ModernButton("Thêm từ file local")){
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
    if( CusTomImGui::ModernButton("OK")){
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
    if( CusTomImGui::ModernButton("Hủy")){
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
