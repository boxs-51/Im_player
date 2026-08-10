#include "globals.h"
#include "utils.h"
#include "json.hpp"

#include "player/mpv_basic_formats.h"

#include "popup/popup_url.h"
#include "gui/gui.h"
#include "windows/WindowManager.h"

#include <commdlg.h>  
#include <vector>
#include <string>
#include <algorithm> 
#include <imgui.h>
#include "reusable_popup.h"
#include <filesystem>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

// --- State tồn tại suốt vòng đời popup ---
static bool urlConfirmed = false;
static bool bufferInitialized = false;
static bool isEditMode = false; 
static bool openConfirmDeletePopup = false;

static PopupData data;
static std::wstring urlInput;
static std::string urlInputBufUtf8;       
static std::wstring lastURLCopy;
static std::string lastURLTruncatedCache; 
static std::vector<std::wstring> pendingLocalFilesLocal;
static std::wstring outResultURL;

// Cờ hiệu ra lệnh cuộn tới vị trí dòng được chọn (Chỉ kích hoạt khi vừa mở Popup)
static bool needScrollToSelected = false; 

const std::string SETTINGS_PATH_POPUP_URL = AutoPath<std::string>("%ROOT%", "data", "popup_data.json");

static int ImGuiStringResizeCallback(ImGuiInputTextCallbackData* cb_data) {
    if (cb_data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = (std::string*)cb_data->UserData;
        str->resize(cb_data->BufTextLen);
        cb_data->Buf = (char*)str->data();
    }
    return 0;
}

PopupData LoadPopupData() {
    std::filesystem::path path = SETTINGS_PATH_POPUP_URL;
    PopupData localData;

    if (!std::filesystem::exists(path)) {
        std::ofstream o(path);
        if (o.is_open()) {
            json j;
            j["config"]["AutoLoadLastURL"] = localData.autoLoadLastURL;
            j["config"]["SaveHistory"] = localData.saveHistory;
            j["lastURL"] = WideToUTF8(localData.lastURL);
            j["urlHistory"] = json::array();
            j["localHistory"] = json::array();
            o << j.dump(4);
        }
        return localData;
    }

    std::ifstream f(path);
    if (f.is_open()) {
        try {
            json j; f >> j;
            localData.autoLoadLastURL = j["config"].value("AutoLoadLastURL", localData.autoLoadLastURL);
            localData.saveHistory = j["config"].value("SaveHistory", localData.saveHistory);
            localData.lastURL = UTF8ToWide(j.value("lastURL", WideToUTF8(localData.lastURL)));

            if (j.contains("urlHistory") && j["urlHistory"].is_array()) {
                for (auto& u : j["urlHistory"])
                    localData.urlHistory.push_back(UTF8ToWide(u.get<std::string>()));
            }

            if (j.contains("localHistory") && j["localHistory"].is_array()) {
                for (auto& lf : j["localHistory"])
                    localData.localHistory.push_back(UTF8ToWide(lf.get<std::string>()));
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load popup JSON: " << e.what() << std::endl;
        }
    }
    return localData;
}

void SavePopupData(const PopupData& saveData) {
    std::filesystem::path path = SETTINGS_PATH_POPUP_URL;
    json j;
    j["config"]["AutoLoadLastURL"] = saveData.autoLoadLastURL;
    j["config"]["SaveHistory"] = saveData.saveHistory;
    j["lastURL"] = WideToUTF8(saveData.lastURL);
    j["urlHistory"] = json::array();
    for (auto& u : saveData.urlHistory) j["urlHistory"].push_back(WideToUTF8(u));
    j["localHistory"] = json::array();
    for (auto& f : saveData.localHistory) j["localHistory"].push_back(WideToUTF8(f));

    std::ofstream o(path);
    if (o.is_open()) {
        o << j.dump(4);
    }
}

bool IsValidURLOrPath(const wchar_t* text) {
    if (!text || !*text) return false;
    if (wcsncmp(text, L"http://", 7) == 0 || wcsncmp(text, L"https://", 8) == 0) return true;
    if (wcsncmp(text, L"file://", 7) == 0) return true;
    if (wcslen(text) > 2 && iswalpha(text[0]) && text[1] == L':' && (text[2] == L'\\' || text[2] == L'/')) return true;
    if (wcsncmp(text, L"\\\\", 2) == 0) return true;
    return false;
}

std::vector<std::wstring> OpenFilePickerW() {
    std::vector<std::wstring> files;
    OPENFILENAMEW ofn;
    wchar_t szFile[32768] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.mov\0All Files\0*.*\0";
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        wchar_t* p = ofn.lpstrFile;
        std::wstring folder = p;
        p += folder.size() + 1;

        if (*p == 0) { 
            files.push_back(folder);
        } else {       
            while (*p) {
                files.push_back(folder + L"\\" + std::wstring(p));
                p += wcslen(p) + 1;
            }
        }
    }
    return files;
}

void AddURLToHistory(PopupData& histData, const std::wstring& newUrl) {
    histData.urlHistory.erase(std::remove(histData.urlHistory.begin(), histData.urlHistory.end(), newUrl), histData.urlHistory.end());
    histData.urlHistory.insert(histData.urlHistory.begin(), newUrl);
    if (histData.urlHistory.size() > 50) histData.urlHistory.resize(50);
    histData.lastURL = newUrl;
}

void AddLocalToHistory(PopupData& histData, const std::wstring& filePath) {
    histData.localHistory.erase(std::remove(histData.localHistory.begin(), histData.localHistory.end(), filePath), histData.localHistory.end());
    histData.localHistory.insert(histData.localHistory.begin(), filePath);
    if (histData.localHistory.size() > 50) histData.localHistory.resize(50);
    histData.lastURL = filePath;
}

bool IsLikelyVideoURL(const std::string& url) {
    return (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) && url.length() > 10 && url.find('.') != std::string::npos && url.find(' ') == std::string::npos;
}

bool IsValidLocalFile(const std::wstring& path) {
    DWORD attrib = GetFileAttributesW(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES) && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

void RebuildUrlCache(PopupData& cacheData, float maxWidth) {
    cacheData.urlCache.clear();
    cacheData.urlCache.reserve(cacheData.urlHistory.size());
    
    // Khoảng trống an toàn: Nếu chế độ Edit cần chừa thêm ~35px cho checkbox + spacing
    float padding = isEditMode ? 45.0f : 15.0f; 
    float targetWidth = (maxWidth > padding) ? (maxWidth - padding) : 10.0f;

    for (auto& w : cacheData.urlHistory) {
        UrlCacheItem item;
        item.utf8 = WideToUTF8(w);
        item.truncated = TextUtils::TruncateTextByPixels(item.utf8.c_str(), targetWidth);
        item.selectedForDelete = false;
        cacheData.urlCache.push_back(std::move(item));
    }
}

void RebuildLocalCache(PopupData& cacheData, float maxWidth) {
    cacheData.localCache.clear();
    cacheData.localCache.reserve(cacheData.localHistory.size());
    
    float padding = isEditMode ? 45.0f : 15.0f;
    float targetWidth = (maxWidth > padding) ? (maxWidth - padding) : 10.0f;

    for (auto& w : cacheData.localHistory) {
        FileCacheItem item;
        item.utf8 = WideToUTF8(w);
        item.truncated = TextUtils::TruncateTextByPixels(item.utf8.c_str(), targetWidth);
        item.selectedForDelete = false;
        cacheData.localCache.push_back(std::move(item));
    }
}

template <typename CacheType, typename HistoryType>
void RenderHistoryPageGeneric(PopupData& pageData, 
                              std::string& inputBufUtf8, 
                              ImVec2 size, 
                              CacheType& cache, 
                              const HistoryType& history, 
                              const std::string& tooltipSuffix,
                              const char* childId,
                              void (*rebuildCacheFunc)(PopupData&, float)) 
{
    ImGuiStyle& style = ImGui::GetStyle();
    float item_text_height = ImGui::GetTextLineHeight(); 

    // --- 1. THANH TIỆN ÍCH SUB-BAR (ĐẨY PHẢI ĐỒNG BỘ) ---
    if (isEditMode && !cache.empty()) {
        int selectedCount = 0;
        for (const auto& item : cache) {
            if (item.selectedForDelete) selectedCount++;
        }

        bool allSelected = (selectedCount == (int)cache.size());
        
        ImGui::Dummy(ImVec2(0, 4.0f)); 

        ImGui::BeginGroup();
        if (CSImGui::ModernCheckbox("Chọn tất cả", &allSelected)) {
            for (auto& item : cache) {
                item.selectedForDelete = allSelected;
            }
        }
        ImGui::SameLine();

        ImGui::TextDisabled("|  Đã chọn: %d/%d mục", selectedCount, (int)cache.size());
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0, 4.0f));
        float subBarHeight = ImGui::GetItemRectSize().y;
        size.y -= subBarHeight;
    }

    // --- 2. BẢNG DANH SÁCH LỊCH SỬ ---
    if (CSImGui::BeginModernChild(childId, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        static float lastWidth = 0.0f;
        static bool lastEditMode = false; // <--- THÊM BIẾN NÀY ĐỂ THEO DÕI TRẠNG THÁI EDIT
        
        float maxWidth = ImGui::GetContentRegionAvail().x;
        
        // NẾU THAY ĐỔI KÍCH THƯỚC HOẶC THAY ĐỔI CHẾ ĐỘ EDIT -> ÉP REBUILD LẠI TEXT CẮT
        if (cache.size() != history.size() || fabs(lastWidth - maxWidth) > 1.0f || lastEditMode != isEditMode) {
            rebuildCacheFunc(pageData, maxWidth);
            lastWidth = maxWidth;
            lastEditMode = isEditMode; // Cập nhật lại trạng thái mới
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)history.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto& item = cache[i];
                ImGui::PushID(i); 

                if (isEditMode) {
                    float start_cursor_y = ImGui::GetCursorPosY();
                    float chk_size = item_text_height;
                    // Trừ bớt khoảng trống để text đẹp mắt, không chạm sát rạt vào checkbox
                    float selectable_width = maxWidth - chk_size - style.ItemSpacing.x;

                    if (CSImGui::ModernSelectable(item.truncated.c_str(), item.selectedForDelete, 0, ImVec2(selectable_width, 0))) {
                        item.selectedForDelete = !item.selectedForDelete;
                    }

                    float end_cursor_y = ImGui::GetCursorPosY(); 
                    float selectable_height = end_cursor_y - start_cursor_y;

                    ImGui::SameLine();

                    float align_offset_y = (selectable_height - chk_size) * 0.5f;
                    ImGui::SetCursorPosY(start_cursor_y + align_offset_y);

                    CSImGui::ModernCheckbox("##delete_chk", &item.selectedForDelete, CheckboxStyle::Tick, ImVec2(chk_size, chk_size));
                    
                    ImGui::SetCursorPosY(end_cursor_y);
                } else {
                    bool isSelected = (inputBufUtf8 == item.utf8);
                    
                    if (isSelected && needScrollToSelected) {
                        ImGui::SetScrollHereY(0.5f);
                        needScrollToSelected = false;
                    }

                    if (CSImGui::ModernSelectable(item.truncated.c_str(), isSelected)) {
                        inputBufUtf8 = item.utf8; 
                    }
                }

                std::string label = item.utf8 + tooltipSuffix;
                CSImGui::ToolTip(label.c_str(), 3.0f, ToolTipFlags_Animation);
                ImGui::Separator();
                
                ImGui::PopID();
            }
        }
        CSImGui::EndModernChild();
    }
}
void HistoryUrlPage(PopupData& pageData, std::string& inputBufUtf8, ImVec2 size) {
    RenderHistoryPageGeneric(
        pageData, inputBufUtf8, size, 
        pageData.urlCache, pageData.urlHistory, 
        "##url_tooltip", "##URLHistoryTable", RebuildUrlCache
    );
}

void HistoryFileLocalPage(PopupData& pageData, std::string& inputBufUtf8, ImVec2 size) {
    RenderHistoryPageGeneric(
        pageData, inputBufUtf8, size, 
        pageData.localCache, pageData.localHistory, 
        "##local_tooltip", "##LocalFilesTable", RebuildLocalCache
    );
}

void ShowURLPopupContent(bool& closePopup, PopupData& popupData, std::string& lastUrlTruncated, 
                         std::string& inputBufUtf8, std::vector<std::wstring>& pendingFiles) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float popupWidth = avail.x;
    float lineHeight = ImGui::GetTextLineHeight();
    float maxInputHeight = lineHeight * 2;
    float historyHeight = lineHeight * 10;
    static bool invalidUrl = false;
    static int currentTab = 0; 

    ImGui::TextDisabled("NHẬP NGUỒN VIDEO");
    ImGui::Separator();
    ImGui::Text("Vui lòng nhập URL hoặc chọn file local:");

    ImGui::BeginDisabled(isEditMode);
    if (CSImGui::ModernInputTextMultiline("##URLInput", inputBufUtf8,
                                         ImVec2(popupWidth, maxInputHeight),
                                         ImGuiInputTextFlags_AllowTabInput)) {
    }
    ImGui::EndDisabled();
    
    Disabehotkey = ImGui::IsItemActive();

    if (CSImGui::ModernCheckbox("Tự động lấy URL trước đó", &popupData.autoLoadLastURL)) 
        SavePopupData(popupData);
    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Lịch sử sử dụng:");

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 btnPadding = ImVec2(15, 8); 

    if (!isEditMode) {
        ImVec2 labelSize = ImGui::CalcTextSize("Quản lý xóa", NULL, true);
        float buttonWidth_Manage = labelSize.x + btnPadding.x * 2.0f;

        float targetPosX = popupWidth - buttonWidth_Manage;
        ImGui::SameLine(targetPosX);

        if (CSImGui::ModernButton("Quản lý xóa", ImVec2(0, 0), false)) {
            isEditMode = true;
            for (auto& item : popupData.urlCache) item.selectedForDelete = false;
            for (auto& item : popupData.localCache) item.selectedForDelete = false;
            
            // Mẹo nhỏ: Clear cache tạm thời để ép hàm dựng lại ngay frame tiếp theo với IsEditMode = true
            popupData.urlCache.clear();
            popupData.localCache.clear();
        }
    } else {
        ImVec2 sizeDelete = ImGui::CalcTextSize("Xóa mục chọn", NULL, true);
        float buttonWidth_Delete = sizeDelete.x + btnPadding.x * 2.0f;

        ImVec2 sizeCancel = ImGui::CalcTextSize("Hủy", NULL, true);
        float buttonWidth_Cancel = sizeCancel.x + btnPadding.x * 2.0f;

        float totalButtonsWidth = buttonWidth_Delete + style.ItemSpacing.x + buttonWidth_Cancel;
        float targetPosX = popupWidth - totalButtonsWidth;
        ImGui::SameLine(targetPosX);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        
        if (CSImGui::ModernButton("Xóa mục chọn", ImVec2(0, 0), true)) {
            // 🌟 KIỂM TRA: Đếm xem người dùng có thực sự tích chọn mục nào không
            int itemsToDeleteCount = 0;
            if (currentTab == 0) {
                for (const auto& item : popupData.urlCache) if (item.selectedForDelete) itemsToDeleteCount++;
            } else {
                for (const auto& item : popupData.localCache) if (item.selectedForDelete) itemsToDeleteCount++;
            }

            // Nếu có ít nhất 1 mục được chọn, bật cờ ra lệnh mở Popup xác nhận
            if (itemsToDeleteCount > 0) {
                openConfirmDeletePopup = true; 
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (CSImGui::ModernButton("Hủy##Hủy1", ImVec2(0, 0), false)) {
            isEditMode = false;
        }
    }

    // ==========================================
    // 🌟 LUỒNG POPUP XÁC NHẬN XÓA (CONFIRMATION MODAL)
    // ==========================================
    if (openConfirmDeletePopup) {
        ImGui::OpenPopup("Xác nhận xóa dòng lịch sử");
        openConfirmDeletePopup = false; // Reset cờ ngay lập tức sau khi ra lệnh mở
    }

    // Đặt kích thước cố định hoặc cấu hình vị trí căn giữa cho modal popup
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350.0f, 0.0f)); // Chiều rộng 350px, chiều cao tự động co giãn theo text

    if (ImGui::BeginPopupModal("Xác nhận xóa dòng lịch sử", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("Bạn có chắc chắn muốn xóa các mục đã chọn khỏi danh sách lịch sử không? Hành động này không thể hoàn tác.");
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));

        // Nút ĐỒNG Ý XÓA (Xử lý toàn bộ logic xóa cũ tại đây)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Đồng ý", ImVec2(120, 30))) {
            if (currentTab == 0) {
                std::vector<std::wstring> newHistory;
                for (size_t i = 0; i < popupData.urlHistory.size(); i++) {
                    if (i < popupData.urlCache.size() && !popupData.urlCache[i].selectedForDelete) {
                        newHistory.push_back(popupData.urlHistory[i]);
                    }
                }
                popupData.urlHistory = std::move(newHistory);
                popupData.urlCache.clear();
            } 
            else if (currentTab == 1) {
                std::vector<std::wstring> newLocal;
                for (size_t i = 0; i < popupData.localHistory.size(); i++) {
                    if (i < popupData.localCache.size() && !popupData.localCache[i].selectedForDelete) {
                        newLocal.push_back(popupData.localHistory[i]);
                    }
                }
                popupData.localHistory = std::move(newLocal);
                popupData.localCache.clear();
            }
            SavePopupData(popupData);
            isEditMode = false;         // Thoát chế độ edit
            ImGui::CloseCurrentPopup(); // Đóng popup modal xác nhận
        }
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetWindowWidth() - 140.0f); // Đẩy nút hủy sang bên phải cách lề 140px

        // Nút HỦY BỎ KHÔNG XÓA
        if (ImGui::Button("Không", ImVec2(120, 30))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // --- History Tabs Container ---
    // Sử dụng ImGuiTabBarFlags_NoTooltip để tab chạy mượt
    // --- Inside ShowURLPopupContent ---
    if (CSImGui::BeginModernTabBar("##HistoryTabs")) {
        ImVec2 size = ImVec2(popupWidth, historyHeight);
        
        // 1️⃣ Khởi tạo các flag mặc định cho hệ thống ImGui
        ImGuiTabItemFlags urlTabFlags = ImGuiTabItemFlags_None;
        ImGuiTabItemFlags localTabFlags = ImGuiTabItemFlags_None;

        // 2️⃣ Chỉ gán flag ép chuyển tab một lần duy nhất tại frame đầu tiên khi mở popup
        if (needScrollToSelected) {
            if (IsValidLocalFile(UTF8ToWide(inputBufUtf8))) {
                localTabFlags |= ImGuiTabItemFlags_SetSelected;
            } else {
                urlTabFlags |= ImGuiTabItemFlags_SetSelected;
            }
        }
        
        // 3️⃣ Gọi hàm đúng signature: truyền NULL cho p_open (không có nút X đóng tab) 
        // và truyền 0 (hoặc flag ModernTabFlags mặc định của bạn) vào tham số cuối.
        if (CSImGui::ModernTabItem("URL", NULL, urlTabFlags, 0)) {
            currentTab = 0;
            HistoryUrlPage(popupData, inputBufUtf8, size);
            CSImGui::EndModernTabItem();
        }

        if (CSImGui::ModernTabItem("File local", NULL, localTabFlags, 0)) {
            currentTab = 1;
            HistoryFileLocalPage(popupData, inputBufUtf8, size);
            CSImGui::EndModernTabItem();
        }
        
        CSImGui::EndModernTabBar();
    }

    ImGui::Separator();
    if (CSImGui::ModernCheckbox("Tự động lưu lịch sử", &popupData.saveHistory)) 
        SavePopupData(popupData);

    ImGui::BeginDisabled(isEditMode);
    if (CSImGui::ModernButton("Lấy từ Clipboard")) {
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            if (OpenClipboard(NULL)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    LPCWSTR clipText = (LPCWSTR)GlobalLock(hData);
                    if (clipText && *clipText && IsValidURLOrPath(clipText)) {
                        inputBufUtf8 = WideToUTF8(clipText);
                        needScrollToSelected = true; // Kích hoạt lại tính năng cuộn khi lấy dữ liệu mới
                    }
                    GlobalUnlock(hData);
                }
                CloseClipboard();
            }
        }
    }

    ImGui::SameLine(0.0f, 30.0f);
    if (CSImGui::ModernButton("Thêm từ file local")) {
        auto files = OpenFilePickerW();
        if (!files.empty()) {
            inputBufUtf8 = WideToUTF8(files[0]);
            pendingFiles.clear();
            for (auto& f : files) pendingFiles.push_back(f);
            needScrollToSelected = true;
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::Text("URL trước đó:");
    ImGui::TextUnformatted(lastUrlTruncated.c_str()); 

    ImGui::BeginDisabled(isEditMode); 
    if (CSImGui::ModernButton("OK")) {
        urlInput = UTF8ToWide(inputBufUtf8); 

        if (IsValidLocalFile(urlInput)) {
            outResultURL = urlInput;
            if (popupData.saveHistory) AddLocalToHistory(popupData, urlInput);
            pendingFiles.clear();
            SetVideoTypeLocal();
            urlConfirmed = true;
            closePopup = true;
            invalidUrl = false;
            SavePopupData(popupData);
        } 
        else if (IsLikelyVideoURL(inputBufUtf8)) {
            outResultURL = urlInput;
            if (popupData.saveHistory) AddURLToHistory(popupData, urlInput);
            urlConfirmed = true;
            closePopup = true;
            invalidUrl = false;
            SavePopupData(popupData);
        } 
        else {
            invalidUrl = true;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (CSImGui::ModernButton("Hủy")) {
        if (isEditMode) {
            isEditMode = false; 
        } else {
            closePopup = true;  
            pendingFiles.clear();
        }
        invalidUrl = false;
    }

    if (invalidUrl)
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "⚠ URL hoặc file không hợp lệ.");
}

void OpenURLPopup(ReusablePopup& popup) {
    if (!popup.IsOpen()) {
        data = LoadPopupData();
        lastURLCopy = data.lastURL;
        isEditMode = false; 
        
        lastURLTruncatedCache = TextUtils::TruncateTextByPixels(WideToUTF8(data.lastURL).c_str(), 500.0f);
        
        std::wstring rawInput = data.autoLoadLastURL ? data.lastURL : L"https://";
        urlInputBufUtf8 = WideToUTF8(rawInput);
        
        if (urlInputBufUtf8.empty()) {
            urlInputBufUtf8.resize(1, '\0');
        }
        
        pendingLocalFilesLocal.clear();
        bufferInitialized = false;

        // 🌟 BẬT CỜ HIỆU AUTO-SCROLL MỖI KHI POPUP ĐƯỢC KÍCH HOẠT MỞ LÊN
        needScrollToSelected = true; 
    }

    popup.Open("Popup Url", [](bool& closePopup) {
        ShowURLPopupContent(closePopup, data, lastURLTruncatedCache, urlInputBufUtf8, pendingLocalFilesLocal);
    });
}

void RenderPopupOverlay_Url(ReusablePopup& popup) {
    if(popup.IsOpen()) {
        popup.Render();
    }
    
    auto* runtime = WindowManager::GetInstance().GetMainWindow();
    if (urlConfirmed && !outResultURL.empty()) {
        if(runtime)runtime->resource.GetPlayerSession()->GetCommander()->LoadFile(WideToUTF8(outResultURL));
        urlConfirmed = false;
        outResultURL.clear();
    }
}