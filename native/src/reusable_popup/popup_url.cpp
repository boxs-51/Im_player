
#include "globals.h"
#include "utils.h"
#include "popup_url.h"
#include "json.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <Windows.h>
#include <imgui.h>
#include "reusable_popup.h"
#include <filesystem>
#include <commdlg.h>  
#include "thread.h"

using json = nlohmann::json;

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
    wchar_t szFile[1024] = {0};
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


void ShowURLPopupContent(bool& closePopup_url, std::wstring& outResultURL,
                         PopupData& data, std::wstring& lastURL,std::wstring& urlInput ,
                         std::vector<std::wstring>& pendingLocalFilesLocal) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float popupWidth = avail.x;
    float lineHeight = ImGui::GetTextLineHeight();
    float maxInputHeight = lineHeight*3;
    float historyHeight = lineHeight*10;
    static bool invalidUrl = false; // local

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.85f,0.85f,0.85f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f,0.8f,0.8f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f,0.9f,0.9f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f,0.7f,0.7f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.9f,0.9f,0.9f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.2f,0.2f,0.8f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.8f,0.8f,0.8f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.9f,0.9f,0.9f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.7f,0.7f,0.7f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f,0.6f,0.6f,1.0f));

    ImGui::BeginChild("##PopupUrl",avail,true);
    ImGui::Text("Vui lòng nhập URL hoặc chọn file local:");
    char buffer[2048] = {};
    if (!urlInput.empty())strncpy(buffer, WideToUTF8(urlInput).c_str(), sizeof(buffer)-1);
        
    ImGui::InputTextMultiline("##URLInput", buffer, sizeof(buffer),
                              ImVec2(popupWidth-1, maxInputHeight-1),
                              ImGuiInputTextFlags_AllowTabInput);

    Disabehotkey = ImGui::IsItemActive()||ImGui::IsItemFocused();

    urlInput = UTF8ToWide(buffer);
    if(ImGui::Checkbox("Tự động lấy URL trước đó",&data.autoLoadLastURL))SavePopupData(data);;
    ImGui::Separator();

    // --- History tabs ---
    if(ImGui::BeginTabBar("##HistoryTabs")){
        // URL Tab
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
        if(ImGui::BeginTabItem("URL")){
            static int selectedUrlIndex=-1;
            ImGui::BeginChild("##URLHistoryTable",ImVec2(popupWidth,historyHeight),true,ImGuiWindowFlags_AlwaysVerticalScrollbar);
            auto& urlHistory = data.urlHistory;
            for(size_t i=0; i<urlHistory.size(); ++i){
                std::string labelUtf8 = WideToUTF8(urlHistory[i]);
                if(ImGui::Selectable(labelUtf8.c_str(), selectedUrlIndex==(int)i)){
                    selectedUrlIndex = (int)i;
                    std::wstring wLabel = UTF8ToWide(labelUtf8);
                    strncpy(buffer, WideToUTF8(wLabel).c_str(), sizeof(buffer)-1);
                    urlInput = wLabel;
                    buffer[2047] = 0;
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // Local Tab
        if(ImGui::BeginTabItem("File local")){
            static int selectedFileIndex=-1;
            ImGui::BeginChild("##LocalFilesTable",ImVec2(popupWidth,historyHeight),true,ImGuiWindowFlags_AlwaysVerticalScrollbar);
            auto& localHistory = data.localHistory;
            for(size_t i=0; i<localHistory.size(); ++i){
                std::string labelUtf8 = WideToUTF8(localHistory[i]);
                if(ImGui::Selectable(labelUtf8.c_str(), selectedFileIndex==(int)i)){
                    selectedFileIndex = (int)i;
                    std::wstring wLabel = UTF8ToWide(labelUtf8);
                    strncpy(buffer, WideToUTF8(wLabel).c_str(), sizeof(buffer)-1);
                    urlInput = wLabel;
                    buffer[2047] = 0;
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::PopStyleColor();
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if(ImGui::Checkbox("Tự động lưu lich sử ",&data.saveHistory))SavePopupData(data);;

    if( ImGui::Button("Lấy từ Clipboard")){
        if(OpenClipboard(NULL)){
            HANDLE hData=GetClipboardData(CF_UNICODETEXT);
            if(hData){
                LPCWSTR clipText=(LPCWSTR)GlobalLock(hData);
                if(clipText && *clipText && wcsstr(clipText,L"http")){
                    //strncpy(urlInput,clipUtf8.c_str(),sizeof(urlInput)-1);
                    urlInput = clipText;
                    //urlInput[sizeof(urlInput)-1]=0;
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }

    ImGui::SameLine(0.0f,30.0f);
    if(ImGui::Button("Thêm từ file local")){
        auto files=OpenFilePickerW();
        if(!files.empty()){
            std::wstring firstFileUtf8=files[0];
            //strncpy(urlInput,firstFileUtf8.c_str(),sizeof(urlInput)-1);
            //urlInput[sizeof(urlInput)-1]=0;
            urlInput = firstFileUtf8;
            pendingLocalFilesLocal.clear();
            for(auto &f:files) pendingLocalFilesLocal.push_back(f);
        }
    }

    ImGui::Separator();
    ImGui::Text("URL trước đó:");
    ImGui::TextWrapped("%ls",lastURL.c_str());

    // --- Buttons OK/Cancel ---
    if(ImGui::Button("OK")){

       if( IsValidLocalFile(urlInput)){
            outResultURL = urlInput;
            if(data.saveHistory) AddLocalToHistory(data, urlInput);
            pendingLocalFilesLocal.clear();
            urlConfirmed = true;
            closePopup_url = true;
            invalidUrl = false;
            file_local = true;
        }else if(IsLikelyVideoURL(WideToUTF8(urlInput))){
            outResultURL =urlInput;
            if(data.saveHistory) AddURLToHistory(data, urlInput);
            urlConfirmed = true;
            closePopup_url = true;
            invalidUrl = false;
            file_local = false;
        }else{
            invalidUrl = true;
        }
        SavePopupData(data);
    }

    ImGui::SameLine();
    if(ImGui::Button("Hủy")){
        closePopup_url=true;
        pendingLocalFilesLocal.clear();
    }

    if(invalidUrl)
        
        ImGui::TextColored(ImVec4(1,0.4f,0.4f,1),"⚠ URL hoặc file không hợp lệ.");

    ImGui::EndChild();
    ImGui::PopStyleColor(11);
    ImGui::PopStyleVar(2);
}

void OpenURLPopup(ReusablePopup& popup, std::wstring& outResultURL) {
    // --- State tồn tại suốt vòng đời popup ---
    static PopupData data;
    static std::wstring urlInput;
    static std::wstring lastURLCopy;
    static std::vector<std::wstring> pendingLocalFilesLocal;

    // Chỉ load dữ liệu khi popup chưa mở
    if(!popup.IsOpen()) {
        data = LoadPopupData();
        lastURLCopy = data.lastURL;
        urlInput = data.autoLoadLastURL ? data.lastURL : L"https://";
        pendingLocalFilesLocal.clear();
    }

    // Lambda capture **copy các static** để ImGui safe và không warning
    popup.Open("Popup Url",
        [dataCopy = data,
         urlInputCopy = urlInput,
         lastURLCopyCopy = lastURLCopy,
         pendingCopy = pendingLocalFilesLocal,
         &outResultURL](bool& closePopup) mutable {
            ShowURLPopupContent(closePopup,
                                outResultURL,
                                dataCopy,
                                lastURLCopyCopy,
                                urlInputCopy,
                                pendingCopy);
        });
}


void RenderPopupOverlay_Url(mpv_handle* mpv) {
    // Hiển thị popup nhập URL
    Popup_Url.Render();

    // Nếu đã xác nhận URL từ popup
    if (urlConfirmed && !Url.empty()) {

        CallThread_URLFetch(WideToUTF8(Url),true); // Giao luôn việc cho thread
        urlConfirmed = false;
        Url.clear();
    }

}
