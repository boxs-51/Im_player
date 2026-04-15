#include "globals.h"
#include "utils.h"


#include <mpv/mpv_settings.h>
#include <mpv/mpv_ui_settings.h>
#include <mpv/mpv_data.h>


#include "sidebar_popup.h"
#include <imgui_internal.h>

#include <services/services_client_backend.h>

#include "FontManager.h"

#include <string>
#include <iostream>
#include <mutex>
#include <shellapi.h>

static int g_CurrentIndex = -1;
static std::function<void(int)> g_OnVideoSelected;
static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();

void OpenSidarBarPopup(ReusablePopup& popup) {
    popup.Open("Sidebar", [](bool& closePopup_siderbar) {
        ShowSidarBarPopup(closePopup_siderbar);
    });
}

void RenderSidarBarPopup(ReusablePopup& popup) {
    popup.Render();
}

void ShowSidarBarPopup(bool& closePopup_siderbar) {
    
    if (ImGui::BeginTabBar("##ListTabs")) {
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
        if (ImGui::BeginTabItem("Youtube")) {
            RenderVideoList();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ListMPV")) {
            RenderListVideoMPV(mpv.mpv );
            ImGui::EndTabItem();
        }
        ImGui::PopStyleColor();
        ImGui::EndTabBar();
        
    }
}
void RenderVideoItem(VideoItem& v, float listWidth) {
    ImGui::BeginChild(v.id.c_str(), ImVec2(listWidth, 140), false, ImGuiWindowFlags_NoScrollbar);

    bool canHover =  ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)||
                     ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)  ;

    float thumb_width = 200.0f;
    float thumb_height = 120.0f;
    float padding = 10.0f;
    float text_width = listWidth - thumb_width - padding - 10.0f; 

    // --- Thumbnail ---
    ImGui::BeginGroup();

    const float thumbInsetX = 10.0f;   // cách mép trái card
    const float thumbInsetY = 10.0f;  // cách mép trên card

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 thumbStart = ImVec2(
        cursor.x + thumbInsetX,
        cursor.y + thumbInsetY
    );
    ImVec2 thumbMin = thumbStart;
    ImVec2 thumbMax = ImVec2(thumbStart.x + thumb_width, thumbStart.y + thumb_height);
    ImVec2 thumbSize(thumb_width, thumb_height);

    ImGui::SetCursorScreenPos(thumbStart);

    if (g_thumbnailCache.count(v.id) && g_thumbnailCache[v.id].loaded)
    {
        auto& tex = g_thumbnailCache[v.id];
        ImGui::Image((void*)(intptr_t)tex.tex, thumbSize);
    }
    else
    {
        ImVec2 itemMin = ImGui::GetWindowPos();
        ImVec2 itemMax = itemMin + ImGui::GetWindowSize();

        ImVec2 viewMin = ImGui::GetWindowViewport()->Pos;
        ImVec2 viewMax = viewMin + ImGui::GetWindowViewport()->Size;

        bool visible =
            itemMax.y >= viewMin.y &&
            itemMin.y <= viewMax.y;
        // 1️⃣ Giữ layout đúng kích thước Image
        ImGui::Dummy(thumbSize);

        // 2️⃣ Nền placeholder
        ImGui::GetWindowDrawList()->AddRectFilled(
            thumbMin,
            thumbMax,
            IM_COL32(40, 40, 40, 180),
            6.0f
        );

        // 3️⃣ Text căn giữa
        const char* txt = "⏳ Loading...";
        ImVec2 txtSize = ImGui::CalcTextSize(txt);

        ImGui::GetWindowDrawList()->AddText(
            ImVec2(
                thumbMin.x + (thumbSize.x - txtSize.x) * 0.5f,
                thumbMin.y + (thumbSize.y - txtSize.y) * 0.5f
            ),
            IM_COL32(200, 200, 200, 220),
            txt
        );

        if (visible){
            LoadThumbnail(v);
            TrimThumbnailCache();
        }
    }

    // --- Overlay thời lượng trên thumbnail ---
    if (!v.duration.empty()) {
        ImVec2 durationSize = ImGui::CalcTextSize(v.duration.c_str());
        ImVec2 pos = ImVec2(
            thumbStart.x + thumb_width - durationSize.x - 8.0f,
            thumbStart.y + thumb_height - durationSize.y - 6.0f
        );
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x - 4, pos.y - 2),
            ImVec2(pos.x + durationSize.x + 4, pos.y + durationSize.y + 2),
            IM_COL32(0, 0, 0, 180),
            4.0f
        );
        ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(255, 255, 255, 255), v.duration.c_str());
    }

    ImGui::EndGroup();

    ImGui::SameLine(0,padding);

    // --- Phần chữ bên phải thumbnail ---
    float metaHeight = 45.0f; // chiều cao cố định cho vùng metadata (views, likes, channel, date)
    float titleHeight = 75.0f; // chiều cao vùng tiêu đề (wrap)

    // Vùng tiêu đề
    ImGui::BeginGroup();
    ImGui::BeginChild((v.id + "_title").c_str(), ImVec2(text_width, titleHeight), false, ImGuiWindowFlags_NoScrollbar ||
                                                                                         ImGuiWindowFlags_NoScrollWithMouse ||
                                                                                         ImGuiWindowFlags_NoBackground ||
                                                                                         ImGuiWindowFlags_NoInputs);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_width);
    ImGui::TextWrapped("%s", v.title.c_str());
    ImVec2 titleTextMin = ImGui::GetItemRectMin();
    ImVec2 titleTextMax = ImGui::GetItemRectMax();
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    // Vùng metadata cố định
    ImGui::BeginChild((v.id + "_meta").c_str(), ImVec2(text_width, metaHeight), false, ImGuiWindowFlags_NoScrollbar ||
                                                                                       ImGuiWindowFlags_NoScrollWithMouse ||
                                                                                       ImGuiWindowFlags_NoBackground ||
                                                                                       ImGuiWindowFlags_NoInputs);

    // --- Views & Likes (icon) ---
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180,180,180,255));
    ImGui::Text("View: %s  Like: %s", v.views, v.likes);
    ImGui::PopStyleColor();

    // --- Kênh & Ngày đăng ---
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150,150,150,255));
    float lineStartX = ImGui::GetCursorPosX();
    float lineY = ImGui::GetCursorPosY();
    ImGui::SetCursorPos(ImVec2(lineStartX, lineY + 5));
    
    // Kênh bên trái
    ImGui::Text("%s", v.channel.c_str());
    float channelWidth = ImGui::CalcTextSize(v.channel.c_str()).x;
    float dateWidth = ImGui::CalcTextSize(v.uploadDate.c_str()).x;
    float rightAlignX = text_width - dateWidth - channelWidth;
    ImGui::SameLine(0, rightAlignX);

    // Ngày đăng bên phải
    ImGui::Text("%s", v.uploadDate.c_str());
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::EndGroup();

    ImVec2 itemMin = ImGui::GetWindowPos();
    ImVec2 itemMax = ImVec2(
        itemMin.x + ImGui::GetWindowWidth(),
        itemMin.y + ImGui::GetWindowHeight()
    );

    bool isHovered = ImGui::IsMouseHoveringRect(itemMin, itemMax) && canHover;
    bool isClicked = isHovered && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool isDown    = isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool hoveringTitle = ImGui::IsMouseHoveringRect(titleTextMin, titleTextMax) && canHover;

    UpdateHoverAnim(v.titleHoverTime,hoveringTitle,12.0f);
    UpdateHoverAnim(v.hoverAnim,isHovered,12.0f);
    float hoverEase = v.hoverAnim * v.hoverAnim; 

    ImVec2 winMin = ImGui::GetWindowPos();
    ImVec2 winMax = ImVec2(winMin.x + ImGui::GetWindowWidth(), winMin.y + ImGui::GetWindowHeight());
    const float rounding = 6.0f;
    // Toàn item
    ImVec2 cardMin = winMin;
    ImVec2 cardMax = winMax;

    // Thumbnail
    ImVec2 holeMin = thumbMin;
    ImVec2 holeMax = thumbMax;

    ImU32 hoverFill = IM_COL32(255,255,255,(int)(25 * v.hoverAnim));
    ImU32 borderCol = IM_COL32(255,255,255,(int)(80 * hoverEase));
    ImU32 pressFill = IM_COL32(255,255,255,(int)(45 * v.hoverAnim));

    CardHoleStyle style;
    style.rounding = 6.0f;
    style.borderThickness = 1.5f;

    {
        ImU32 baseBg = IM_COL32(255, 255, 255, 10); // nền rất nhẹ
        ImGui::GetWindowDrawList()->AddRectFilled(
            cardMin,
            cardMax,
            baseBg,
            rounding
        );
    }

    if (v.titleHoverTime >= 1.0f) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextUnformatted(v.title.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    if (v.hoverAnim > 0.01f)
    {
        DrawCardWithHole(
            ImGui::GetWindowDrawList(),
            cardMin,
            cardMax,
            holeMin,
            holeMax,
            hoverFill,
            borderCol,
            style
        );
    }

    if (v.hoverAnim > 0.01f && isDown) 
    {
        DrawCardWithHole(
            ImGui::GetWindowDrawList(),
            cardMin,
            cardMax,
            holeMin,
            holeMax,
            pressFill,
            0,
            style
        );
    }
        
    if (v.hoverAnim > 0.01f && isClicked) {
        CallThread_URLFetch(v.link.c_str(), true);
        UpdateVideoData(VideoSource::Watched, v.link.c_str());
    }
    ImGui::SetCursorScreenPos(cardMin);
    ImGui::InvisibleButton(
        ("##card_btn_" + v.id).c_str(),
        ImVec2(cardMax.x - cardMin.x, cardMax.y - cardMin.y)
    );

    // --- Menu chuột phải ---
    if (ImGui::BeginPopupContextWindow(("##card_btn_" + v.id).c_str(),
            ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Copy URL"))
            ImGui::SetClipboardText(v.link.c_str());

        if (ImGui::MenuItem("Open Link"))
            ShellExecuteA(nullptr, "open", v.link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

        if (ImGui::MenuItem("App List MPV")) {
            CallThread_URLFetch(v.link.c_str(), false, v.title.c_str());
            UpdateVideoData(VideoSource::Watched, v.link.c_str());
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Separator();
}

static bool ContainsIgnoreCase(const std::string& src, const char* key)
{
    if (!key || !key[0]) return true;

    std::string a = src;
    std::string b = key;

    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);

    return a.find(b) != std::string::npos;
}

static std::string ToLower(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}
static std::vector<std::string> SplitWords(const std::string& s)
{
    std::stringstream ss(s);
    std::string word;
    std::vector<std::string> out;

    while (ss >> word)
        out.push_back(word);

    return out;
}
struct SearchInputCtx
{
    std::vector<int>* filtered;
    int* selected;

    bool requestApply = false;
    int  applyIndex   = -1;
};

static SearchInputCtx g_searchCtx;
static int SearchInputCallback(ImGuiInputTextCallbackData* data)
{
    SearchInputCtx* ctx = (SearchInputCtx*)data->UserData;

    // TAB / RIGHT
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        if (!ctx->filtered->empty())
        {
            int sel = (*ctx->selected >= 0) ? *ctx->selected : 0;
            int idx = (*ctx->filtered)[sel];
            const std::string& full = g_keywords[idx];

            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, full.c_str());

            *ctx->selected = -1;
        }
    }

    // ENTER chọn keyword
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && *ctx->selected >= 0)
        {
            int idx = (*ctx->filtered)[*ctx->selected];
            const std::string& full = g_keywords[idx];

            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, full.c_str());

            *ctx->selected = -1;

            // 🚫 chặn submit search
            return 1;
        }
    }
    // Click từ popup
    if (ctx->requestApply && ctx->applyIndex >= 0)
    {
        int kwIndex = (*ctx->filtered)[ctx->applyIndex];
        const std::string& kw = g_keywords[kwIndex];

        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, kw.c_str());

        ctx->requestApply = false;
        ctx->applyIndex = -1;
        *ctx->selected = -1;
    }

    return 0;
}
struct KeywordItemState
{
    float hoverAnim = 0.0f; // 0 -> 1
    bool selected = false;   // đang được chọn bằng ↑↓
    bool clicking = false;   // đang nhấn chuột
};
void RenderVideoList()
{
    bool canHover =
    ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
    ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    static bool keywordsLoaded = false;
    static int  g_keywordSelected = -1;
    static char search_buf[256] = "";
    static char last_buf[256] = "";
    static bool keywordPopupOpenPrev = false;
    static std::vector<int> g_filteredKeywordIndices;

    // --- Load keyword once ---
    if (!keywordsLoaded && !g_loading)
    {
        UpdateVideoData(VideoSource::Keywords, "", 20);
        keywordsLoaded = true;
    }

    // ================= TOP CONTROLS =================
    ImGui::BeginChild("top_controls", ImVec2(avail.x, 50), false);

    // MPV button
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5);
    if (ImGui::Button("MPV", ImVec2(80, 0)))
    {
        g_searchQuery.clear();
        search_buf[0] = 0;
        g_videoList.clear();
        g_nextPageToken.clear();
        UpdateVideoData(VideoSource::Trending);
    }
    ImGui::PopStyleVar();

    ImGui::SameLine();

    // Search box
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0.4f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f,0.3f,0.3f,1));

    g_searchCtx.filtered = &g_filteredKeywordIndices;
    g_searchCtx.selected = &g_keywordSelected;


    bool enterPressed = ImGui::InputText(
        "##Search",
        search_buf,
        IM_ARRAYSIZE(search_buf),
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackCompletion |
        ImGuiInputTextFlags_CallbackAlways |
        ImGuiInputTextFlags_NoUndoRedo,
        SearchInputCallback,
        &g_searchCtx
    );
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    bool searchFocused = ImGui::IsItemActive();

    // ================= FILTER KEYWORD =================
    bool textChanged = strcmp(last_buf, search_buf) != 0;
    if (textChanged)
    {
        strcpy(last_buf, search_buf);
        g_keywordSelected = -1;
        g_filteredKeywordIndices.clear();

        std::string key = ToLower(search_buf);
        auto tokens = SplitWords(key);

        std::lock_guard<std::mutex> lock(g_mutex);

        std::vector<int> starts;
        std::vector<int> contains;
        std::vector<int> related;

        for (int i = 0; i < (int)g_keywords.size(); i++)
        {
            std::string kw = ToLower(g_keywords[i]);

            if (!key.empty() && kw.rfind(key, 0) == 0)
            {
                starts.push_back(i);
            }
            else if (!key.empty() && kw.find(key) != std::string::npos)
            {
                contains.push_back(i);
            }
            else
            {
                // liên quan: chứa ít nhất 1 token
                for (const auto& t : tokens)
                {
                    if (t.size() >= 2 && kw.find(t) != std::string::npos)
                    {
                        related.push_back(i);
                        break;
                    }
                }
            }
        }

        // gộp theo độ ưu tiên
        g_filteredKeywordIndices.insert(
            g_filteredKeywordIndices.end(),
            starts.begin(), starts.end()
        );
        g_filteredKeywordIndices.insert(
            g_filteredKeywordIndices.end(),
            contains.begin(), contains.end()
        );
        g_filteredKeywordIndices.insert(
            g_filteredKeywordIndices.end(),
            related.begin(), related.end()
        );
    }
    const char* inlineSuggestion = nullptr;
    std::string typed = search_buf;
    std::string ghost;
    // 1. Nếu có item được chọn bằng keyboard
    if (g_keywordSelected >= 0 && g_keywordSelected < (int)g_filteredKeywordIndices.size())
    {
        int idx = g_filteredKeywordIndices[g_keywordSelected];
        inlineSuggestion = g_keywords[idx].c_str();
    }
    // 2. Nếu không có, lấy item đầu tiên
    else if (!g_filteredKeywordIndices.empty())
    {
        int idx = g_filteredKeywordIndices[0];
        inlineSuggestion = g_keywords[idx].c_str();
    }
   
    if (inlineSuggestion)
    {
        std::string s = inlineSuggestion;

        if (s.size() > typed.size() &&
            s.compare(0, typed.size(), typed) == 0)
        {
            ghost = s.substr(typed.size());
        }
    }

    if (searchFocused && !ghost.empty())
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec2 textPos = ImGui::GetItemRectMin();
        ImVec2 textSize = ImGui::CalcTextSize(search_buf);

        ImVec2 pos;
        pos.x = textPos.x + ImGui::GetStyle().FramePadding.x + textSize.x;
        pos.y = textPos.y + ImGui::GetStyle().FramePadding.y;

        dl->AddText(
            pos,
            IM_COL32(180, 180, 180, 120), // mờ
            ghost.c_str()
        );
    }
    Disabehotkey = searchFocused;

    ImVec2 inputPos  = ImGui::GetItemRectMin();
    ImVec2 inputSize = ImGui::GetItemRectSize();

    // ================= KEYBOARD NAV =================
    if (searchFocused && !g_filteredKeywordIndices.empty())
    {
        int count = (int)g_filteredKeywordIndices.size();

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            g_keywordSelected = (g_keywordSelected + 1) % count;

        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            g_keywordSelected = (g_keywordSelected - 1 + count) % count;

    }
    // ================= KEYWORD POPUP (InvisibleButton Version) =================
    static bool popupHovered = false;
    bool popupOpen = searchFocused && !g_filteredKeywordIndices.empty();

    // reset selection khi popup vừa mở
    if (popupOpen && !keywordPopupOpenPrev)
    {
        g_keywordSelected = -1;
    }
    keywordPopupOpenPrev = popupOpen;

    if ((popupOpen || popupHovered) && canHover)
    {
        const int MAX_VISIBLE = 5;
        const float ITEM_HEIGHT = 28.0f;

        int totalCount = (int)g_filteredKeywordIndices.size();
        int visibleCount = std::min(totalCount, MAX_VISIBLE);

        float popupHeight = visibleCount * ITEM_HEIGHT + ImGui::GetStyle().WindowPadding.y * 2;

        ImGui::SetNextWindowPos({ inputPos.x, inputPos.y + inputSize.y + 4 });
        ImGui::SetNextWindowSize({ inputSize.x, popupHeight + 10.0f});

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10,10});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f,0.05f,0.05f,0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f,0.25f,0.25f,1));

        ImGui::Begin("##keyword_popup", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoDecoration 
        );

        // 🔥 VÙNG CUỘN
        ImGui::BeginChild(
            "##keyword_scroll",
            ImVec2(0, popupHeight),
            false,
            ImGuiWindowFlags_AlwaysVerticalScrollbar |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoFocusOnAppearing
        );

        static int lastSelected = -1;
        static std::vector<KeywordItemState> g_itemStates;

        if (g_itemStates.size() != g_filteredKeywordIndices.size())
            g_itemStates.resize(g_filteredKeywordIndices.size());

        for (int i = 0; i < totalCount; i++)
        {
            int idx = g_filteredKeywordIndices[i];
            const std::string& k = g_keywords[idx];
            // Lấy trạng thái

            KeywordItemState& state = g_itemStates[i];

            state.selected = (i == g_keywordSelected);
            
            ImGui::PushID(idx);

            ImGui::BeginGroup(); 
            ImVec2 groupStart = ImGui::GetCursorScreenPos();

            // --- InvisibleButton phủ toàn bộ width item ---
            ImVec2 itemSize(ImGui::GetContentRegionAvail().x, ITEM_HEIGHT);
            ImGui::InvisibleButton("##item", itemSize,
                    ImGuiButtonFlags_None |
                    ImGuiViewportFlags_NoFocusOnClick
                );
            ImVec2 groupEnd = ImVec2(groupStart.x + itemSize.x, groupStart.y + itemSize.y);

            // --- Hover  ---
            bool hovered = ImGui::IsItemHovered();
            UpdateHoverAnim(state.hoverAnim, hovered, 12.0f);

            // Click
            state.clicking = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);

            // --- Tính màu theo trạng thái ---
            ImVec4 normalCol = ImVec4(0.15f,0.15f,0.15f,1.0f);
            ImVec4 hoverCol  = ImVec4(0.3f,0.3f,0.3f,1.0f);
            ImVec4 selectedCol = ImVec4(0.2f,0.45f,1.0f,0.8f);
            ImVec4 clickCol = ImVec4(0.1f,0.6f,1.0f,1.0f);

            ImVec4 finalCol = normalCol;

            if (state.selected)
                finalCol = selectedCol;
            else if (state.clicking)
                finalCol = clickCol;
            else if (state.hoverAnim > 0.0f)
            {
                float t = state.hoverAnim;
                finalCol.x = normalCol.x * (1.0f - t) + hoverCol.x * t;
                finalCol.y = normalCol.y * (1.0f - t) + hoverCol.y * t;
                finalCol.z = normalCol.z * (1.0f - t) + hoverCol.z * t;
                finalCol.w = normalCol.w * (1.0f - t) + hoverCol.w * t;
            }
            // --- Vẽ nền item ---
            ImGui::GetWindowDrawList()->AddRectFilled(groupStart, groupEnd, ImGui::ColorConvertFloat4ToU32(finalCol), 4.0f);

            // --- Khung border cho item ---
            ImGui::GetWindowDrawList()->AddRect(groupStart, groupEnd, IM_COL32(100,100,100,255), 4.0f);

            // --- Click chọn item ---
            if (ImGui::IsItemClicked())
            {
                g_keywordSelected = i;
                g_searchCtx.requestApply = true;
                g_searchCtx.applyIndex = i;

                // Không làm mất focus input text
                // Nếu cần, có thể gọi ImGui::SetKeyboardFocusHere(-1) để focus trở lại search box
                ImGui::SetKeyboardFocusHere(-1);
            }

            // --- Vẽ text + nút Del ---
            ImGui::SetCursorScreenPos(ImVec2(groupStart.x + 4, groupStart.y + 4));// padding left
            ImGui::TextUnformatted(k.c_str());

            // nút Del sát bên phải
            float buttonWidth = 40.0f;
            ImGui::SetCursorScreenPos(ImVec2(groupEnd.x - buttonWidth - 4, groupStart.y + 4)); // padding right
            if (ImGui::SmallButton("Del"))
            {
                // xử lý delete
                printf("Delete keyword: %s\n", k.c_str());
            }

            // --- Auto-scroll khi dùng ↑↓ ---
            if (state.selected && lastSelected != g_keywordSelected)
            {
                ImGui::SetScrollHereY(0.3f);
                lastSelected = g_keywordSelected;
            }
            ImGui::EndGroup();

            ImGui::PopID();
        }

        ImGui::EndChild(); // scroll
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                ImGui::ClearActiveID();
            }
        }
        popupHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);


        ImGui::End();      // popup

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    // ================= ENTER SUBMIT =================
    if (enterPressed)
    {
        g_searchQuery = search_buf;
        g_videoList.clear();
        g_nextPageToken.clear();

        UpdateVideoData(
            g_searchQuery.empty()
                ? VideoSource::Trending
                : VideoSource::Search
        );
    }

    ImGui::EndChild();
    ImGui::Separator();

    // ================= VIDEO LIST =================
    ProcessThumbnailQueue();

    ImGui::BeginChild("video_list",
        ImGui::GetContentRegionAvail(),
        false,
        ImGuiWindowFlags_HorizontalScrollbar
    );

    for (auto& v : g_videoList)
        RenderVideoItem(v, ImGui::GetContentRegionAvail().x - 20);

    if (ImGui::GetScrollY() + ImGui::GetWindowHeight() >=
        ImGui::GetScrollMaxY() - 50 && !g_loading)
    {
        UpdateVideoData(
            g_searchQuery.empty()
                ? VideoSource::Trending
                : VideoSource::Search
        );
    }

    ImGui::EndChild();
}


void RenderListVideoMPV(mpv_handle* mpv)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::Text("Playlist (%d):", (int)g_playbackStatus.g_playlist.size());
    ImGui::SameLine( avail.x - 20.0f , 0.0f);
    if (g_CurrentIndex >= 0 && g_CurrentIndex < (int)g_playbackStatus.g_playlist.size()) {
        if (ImGui::Button("Xóa")) {
            int i = g_CurrentIndex;

            g_playbackStatus.g_playlist.erase(g_playbackStatus.g_playlist.begin() + i);
            std::string indexStr = std::to_string(i);
            const char* cmd[] = { "playlist-remove", indexStr.c_str(), nullptr };
            int res = mpv_command(mpv, cmd);
            if(!(res<0)) {
                if (i >= g_playbackStatus.g_playlist.size())
                    g_CurrentIndex = (int)g_playbackStatus.g_playlist.size() - 1;
            } else {

            }
        }
    }
    ImGui::Separator();

    
    // --- Scrollable playlist ---
    ImVec2 avail_scroll = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("scroll", ImVec2(avail_scroll), true); // giữ chỗ cho footer nút xóa

    for (int i = 0 ; i < (int)g_playbackStatus.g_playlist.size(); ++i) {
        std::string label = 
                            (!g_playbackStatus.g_playlist[i].title.empty()) 
                                ? g_playbackStatus.g_playlist[i].title 
                                : ((!g_playbackStatus.g_playlist[i].filename.empty()) 
                                    ? g_playbackStatus.g_playlist[i].filename 
                                    : "No title");
        std::string displayLabel = std::to_string(i + 1) + ". " + label;

        ImGui::PushID(i);

        bool isSelected = (i == g_CurrentIndex);
        bool isPlaying  = (i == g_playbackStatus.g_PlayingIndex);

        // --- Highlight item ---
        if (isPlaying)
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255)); // xanh lá cho đang phát
        else if (isSelected)
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 0, 255)); // vàng cho item được chọn

        // Cho phép double-click
        if (ImGui::Selectable(displayLabel.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
            g_CurrentIndex = i; // Chọn item để có thể xóa

            // Nếu double-click → phát video
            if (ImGui::IsMouseDoubleClicked(0)) {
                std::string indexStr = std::to_string(i);
                const char* cmd[] = { "playlist-play-index", indexStr.c_str(), nullptr };
                int res = mpv_command(mpv, cmd);
                if(!(res <0))
                    g_playbackStatus.g_PlayingIndex = i; // Cập nhật trạng thái đang phát

                if (g_OnVideoSelected) g_OnVideoSelected(i);
            }
        }

        if (isPlaying || isSelected) ImGui::PopStyleColor();

        // --- Drag & Drop ---
        if (ImGui::BeginDragDropSource()) {
            int payloadIndex = i;
            ImGui::SetDragDropPayload("DND_PLAYLIST_ITEM", &payloadIndex, sizeof(int));
            ImGui::Text("Move: %s", label.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLAYLIST_ITEM")) {
                int srcIndex = *(const int*)payload->Data;
                if (srcIndex != i) {
                    // Lưu trạng thái video đang phát
                    int currentPlayingIndex = g_playbackStatus.g_PlayingIndex;

                    // Swap trong g_playlist
                    std::swap(g_playbackStatus.g_playlist[srcIndex], g_playbackStatus.g_playlist[i]);

                    // Thực hiện move trong MPV
                    std::string srcindexStr = std::to_string(srcIndex);
                    std::string indexStr = std::to_string(i);
                    const char* cmd[] = { "playlist-move", srcindexStr.c_str(), indexStr.c_str(), nullptr };
                    int res = mpv_command(mpv, cmd);
                    if(!(res < 0)) {
                        // --- Cập nhật g_CurrentIndex ---
                        if (g_CurrentIndex == srcIndex) g_CurrentIndex = i;
                        else if (g_CurrentIndex == i) g_CurrentIndex = srcIndex;

                        // --- Cập nhật g_PlayingIndex để video đang phát giữ đúng ---
                        if (currentPlayingIndex == srcIndex) g_playbackStatus.g_PlayingIndex = i;        // nếu item đang phát là item bị move
                        else if (currentPlayingIndex == i) g_playbackStatus.g_PlayingIndex = srcIndex;  // nếu item đang phát bị swap với item khác
                        // Nếu video đang phát nằm giữa srcIndex và i, adjust chỉ số
                        else if (currentPlayingIndex > srcIndex && currentPlayingIndex <= i) g_playbackStatus.g_PlayingIndex--;
                        else if (currentPlayingIndex < srcIndex && currentPlayingIndex >= i) g_playbackStatus.g_PlayingIndex++;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }
    ImGui::EndChild();
}