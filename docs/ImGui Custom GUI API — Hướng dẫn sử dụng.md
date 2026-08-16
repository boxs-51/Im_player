# ImGui Custom GUI API
## Hướng dẫn sử dụng thư viện GUI trong `gui/`

> Phiên bản tài liệu: 1.0  
> Nguồn: `gui.zip`  
> Kiến trúc: C++ + Dear ImGui  
> Namespace chính: `CSImGui`

---

# 1. Tổng quan

Module `gui/` là một lớp wrapper/custom widget trên Dear ImGui, cung cấp:

- Theme system.
- Button.
- Icon button.
- Checkbox.
- Toggle.
- Input text.
- Multiline input.
- Combo box.
- Search combo.
- Slider seek.
- Tooltip.
- Text effect.
- Card.
- Child window.
- Tab bar/tab item.
- Popup.
- Table.
- Selectable.
- Tree node.
- Header.
- Custom draw helper.
- Media/system/volume icons.

Cấu trúc chính:

```text
gui/
├── gui.h
├── core/
│   ├── gui_types.h
│   ├── gui_theme.h
│   └── gui_theme.cpp
│
├── widgets/
│   ├── gui_buttons.h
│   ├── gui_inputs.h
│   ├── gui_slider.h
│   ├── gui_tooltip.h
│   ├── gui_text.h
│   └── gui_containers.h
│
└── draw/
    ├── gui_draw_helpers.h
    └── icons/
        ├── icon_media.h
        ├── icon_volume.h
        └── icon_system.h
```

Header tổng:

```cpp
#include "gui/gui.h"
```

Sau khi include `gui/gui.h`, phần lớn API public của module có thể sử dụng trực tiếp.

---

# 2. Nguyên tắc sử dụng quan trọng

Các API trong module này **không phải retained-mode GUI độc lập**.

Chúng vẫn phụ thuộc vào Dear ImGui context hiện tại.

Do đó phải gọi chúng bên trong:

```cpp
ImGui::NewFrame();

...

// GUI code
CSImGui::ModernButton("Play");

...

ImGui::Render();
```

Không được gọi widget khi chưa có ImGui context/current frame hợp lệ.

Đặc biệt:

```cpp
CSImGui::GetColors(...)
ImGui::GetCurrentWindow()
ImGui::GetStateStorage()
ImGui::GetIO()
GImGui
```

đều phụ thuộc vào `ImGuiContext` hiện tại.

---

# 3. Include tổng

Khuyến nghị sử dụng:

```cpp
#include "gui/gui.h"
```

Header này export:

```cpp
#include "core/gui_types.h"
#include "core/gui_theme.h"

#include "draw/gui_draw_helpers.h"
#include "draw/icons/icon_media.h"
#include "draw/icons/icon_volume.h"
#include "draw/icons/icon_system.h"

#include "widgets/gui_buttons.h"
#include "widgets/gui_inputs.h"
#include "widgets/gui_slider.h"
#include "widgets/gui_tooltip.h"
#include "widgets/gui_text.h"
#include "widgets/gui_containers.h"
```

---

# 4. Theme API

## 4.1. ThemeType

Có 4 theme:

```cpp
enum class ThemeType : uint8_t {
    DarkMode,
    LightMode,
    MidnightMode,
    RetroMode
};
```

Sử dụng:

```cpp
CSImGui::InitThemeLibrary(ThemeType::DarkMode);
```

hoặc:

```cpp
CSImGui::InitThemeLibrary(ThemeType::MidnightMode);
```

Các theme hiện có:

| Theme | Ý nghĩa |
|---|---|
| `DarkMode` | Dark theme mặc định |
| `LightMode` | Light theme |
| `MidnightMode` | Dark xanh/xám |
| `RetroMode` | Retro |

---

## 4.2. InitThemeLibrary()

```cpp
void CSImGui::InitThemeLibrary(ThemeType themetype);
```

Khởi tạo toàn bộ theme library và áp dụng theme được chọn.

Ví dụ:

```cpp
CSImGui::InitThemeLibrary(ThemeType::DarkMode);
```

Khuyến nghị gọi một lần sau khi tạo ImGui context.

Ví dụ:

```cpp
ImGui::CreateContext();

CSImGui::InitThemeLibrary(
    ThemeType::MidnightMode
);
```

---

# 5. Chuyển theme

## ApplyTheme()

```cpp
void CSImGui::ApplyTheme(ThemeType themetype);
```

Chọn theme mục tiêu.

Ví dụ:

```cpp
CSImGui::ApplyTheme(
    ThemeType::RetroMode
);
```

API này không chuyển màu ngay lập tức. Nó tạo `ThemeTransition`.

Sau đó mỗi frame cần:

```cpp
CSImGui::UpdateTheme(ImGui::GetIO().DeltaTime);
```

---

# 6. UpdateTheme()

```cpp
void CSImGui::UpdateTheme(float deltaTime);
```

Cập nhật animation chuyển theme.

Ví dụ:

```cpp
CSImGui::UpdateTheme(
    ImGui::GetIO().DeltaTime
);
```

Khuyến nghị:

```cpp
ImGui::NewFrame();

CSImGui::UpdateTheme(
    ImGui::GetIO().DeltaTime
);

// render GUI

ImGui::Render();
```

---

# 7. GetColors()

```cpp
ImVec4& CSImGui::GetColors(Col idx);
```

Lấy màu custom hiện tại.

Ví dụ:

```cpp
ImVec4 textColor =
    CSImGui::GetColors(Col_Text);
```

Có thể dùng trực tiếp:

```cpp
ImGui::PushStyleColor(
    ImGuiCol_Text,
    CSImGui::GetColors(Col_Text)
);

ImGui::Text("Hello");

ImGui::PopStyleColor();
```

Một số màu thường dùng:

```cpp
Col_Text
Col_TextDisabled
Col_WindowBg
Col_ChildBg
Col_PopupBg
Col_Border

Col_FrameBg
Col_FrameBgHovered
Col_FrameBgActive

Col_Button
Col_ButtonHovered
Col_ButtonActive

Col_CheckMark

Col_Header
Col_HeaderHovered
Col_HeaderActive

Col_Tab
Col_TabHovered
Col_TabActive

Col_TableHeaderBg
Col_TableRowBg
Col_TableRowBgAlt
```

---

# 8. GetStyle()

```cpp
Style& CSImGui::GetStyle();
```

Trả về style custom:

```cpp
struct Style {
    ImVec4 Colors[Col_COUNT];
};
```

Ví dụ:

```cpp
Style& style = CSImGui::GetStyle();

style.Colors[Col_Text] =
    ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
```

Lưu ý: đây là reference tới global style của GUI system.

Không nên thay đổi style từ nhiều thread.

---

# 9. Button API

## 9.1. ModernButton()

```cpp
bool ModernButton(
    const char* label,
    const ImVec2& size_arg = ImVec2(0, 0),
    bool primary = true
);
```

Ví dụ:

```cpp
if (CSImGui::ModernButton("Play")) {
    Play();
}
```

Kích thước:

```cpp
CSImGui::ModernButton(
    "Play",
    ImVec2(120, 40)
);
```

Button secondary:

```cpp
CSImGui::ModernButton(
    "Cancel",
    ImVec2(120, 40),
    false
);
```

---

# 10. SecondaryButton()

```cpp
bool SecondaryButton(
    const char* label,
    const ImVec2& size = ImVec2(0, 0)
);
```

Ví dụ:

```cpp
if (CSImGui::SecondaryButton("Cancel")) {
    CloseDialog();
}
```

Tương đương:

```cpp
CSImGui::ModernButton(
    "Cancel",
    ImVec2(0, 0),
    false
);
```

---

# 11. ModernButtonEx()

```cpp
bool ModernButtonEx(
    const char* label,
    const ImVec2& size_arg = ImVec2(0, 0)
);
```

Đây là biến thể button có hiệu ứng hover mở rộng.

Ví dụ:

```cpp
if (CSImGui::ModernButtonEx("Open")) {
    OpenFile();
}
```

---

# 12. ModernSmallButton()

```cpp
bool ModernSmallButton(
    const char* label
);
```

Ví dụ:

```cpp
if (CSImGui::ModernSmallButton("Reset")) {
    Reset();
}
```

Phù hợp với toolbar/action nhỏ.

---

# 13. ModernArrowButton()

```cpp
bool ModernArrowButton(
    const char* str_id,
    ImGuiDir dir,
    ImVec2 size = ImVec2(0, 0)
);
```

Ví dụ:

```cpp
CSImGui::ModernArrowButton(
    "##left",
    ImGuiDir_Left
);
```

Hoặc:

```cpp
CSImGui::ModernArrowButton(
    "##next",
    ImGuiDir_Right,
    ImVec2(32, 32)
);
```

Các direction:

```cpp
ImGuiDir_Left
ImGuiDir_Right
ImGuiDir_Up
ImGuiDir_Down
```

---

# 14. Checkbox

## ModernCheckbox()

```cpp
bool ModernCheckbox(
    const char* label,
    bool* v,
    CheckboxStyle style = CheckboxStyle::Tick,
    const ImVec2& size_arg = ImVec2(0, 0)
);
```

Các style:

```cpp
CheckboxStyle::Circle
CheckboxStyle::Tick
CheckboxStyle::Square
```

Ví dụ:

```cpp
bool enabled = false;

if (CSImGui::ModernCheckbox(
        "Enable subtitles",
        &enabled))
{
    ApplySubtitleSetting(enabled);
}
```

Circle:

```cpp
CSImGui::ModernCheckbox(
    "Audio",
    &audioEnabled,
    CheckboxStyle::Circle
);
```

Square:

```cpp
CSImGui::ModernCheckbox(
    "Video",
    &videoEnabled,
    CheckboxStyle::Square
);
```

---

# 15. Toggle

```cpp
bool ModernToggle(
    const char* str_id,
    bool* v,
    bool enabled = true,
    float scale = 1.0f
);
```

Ví dụ:

```cpp
bool fullscreen = false;

if (CSImGui::ModernToggle(
        "##fullscreen",
        &fullscreen))
{
    SetFullscreen(fullscreen);
}
```

Scale:

```cpp
CSImGui::ModernToggle(
    "##toggle",
    &enabled,
    true,
    1.5f
);
```

Disable:

```cpp
CSImGui::ModernToggle(
    "##toggle",
    &enabled,
    false
);
```

---

# 16. IconButton

## IconButtonStyle

```cpp
struct IconButtonStyle {
    bool drawButtonBg;
    ImU32 buttonBgColor;
    ImU32 buttonBgHovered;
    ImU32 buttonBgActive;

    float buttonRounding;

    bool drawButtonBorder;
    ImU32 buttonBorderColor;
    float buttonBorderThickness;

    bool drawIconBg;
    ImU32 iconBgColor;
    float iconBgRounding;

    bool drawIconBorder;
    ImU32 iconBorderColor;
    float iconBorderThickness;

    ImU32 iconNormal;
    ImU32 iconHovered;
    ImU32 iconActive;

    float iconHoverScale;
    float iconActiveScale;
};
```

---

# 17. GetDefaultIconButtonStyle()

```cpp
const IconButtonStyle&
GetDefaultIconButtonStyle();
```

Ví dụ:

```cpp
const auto& style =
    CSImGui::GetDefaultIconButtonStyle();
```

---

# 18. CustomIconButton()

API chính:

```cpp
bool CustomIconButton(
    const char* str_id,
    void (*drawFn)(
        ImDrawList*,
        ImVec2,
        ImVec2,
        ImU32,
        void*
    ),
    ImVec2 size,
    void* user_data,
    const IconButtonStyle& style
);
```

Callback icon:

```cpp
void DrawMyIcon(
    ImDrawList* dl,
    ImVec2 min,
    ImVec2 max,
    ImU32 color,
    void* userData
);
```

Sử dụng:

```cpp
CSImGui::CustomIconButton(
    "##play",
    DrawMyIcon,
    ImVec2(40, 40),
    nullptr,
    CSImGui::GetDefaultIconButtonStyle()
);
```

---

# 19. CustomIconButton() đơn giản

Có overload:

```cpp
bool CustomIconButton(
    const char* str_id,
    DrawCallback,
    ImVec2 size
);
```

Ví dụ:

```cpp
CSImGui::CustomIconButton(
    "##play",
    DrawPlayIcon,
    ImVec2(40, 40)
);
```

Với callback cũ:

```cpp
using OldIconFn =
    void (*)(ImDrawList*, ImVec2, ImVec2, ImU32);
```

Có thể gọi:

```cpp
CSImGui::CustomIconButton(
    "##next",
    DrawNextIcon,
    ImVec2(40, 40)
);
```

---

# 20. Media icons

## Play

```cpp
void DrawPlayIcon(
    ImDrawList* drawList,
    ImVec2 pMin,
    ImVec2 pMax,
    ImU32 color
);
```

Ví dụ:

```cpp
CSImGui::CustomIconButton(
    "##play",
    DrawPlayIcon,
    ImVec2(36, 36)
);
```

---

## Pause

```cpp
void DrawPauseIcon(...);
```

---

## Previous

```cpp
void DrawPrevIcon(...);
```

---

## Next

```cpp
void DrawNextIcon(...);
```

---

# 21. Animated Play/Pause icon

```cpp
struct PlayPauseData {
    float t = 0.0f;
    bool paused = false;
    bool hovered = false;
};
```

Ví dụ:

```cpp
PlayPauseData playPause;

playPause.paused = isPaused;

CSImGui::CustomIconButton(
    "##playpause",
    DrawPlayPauseIcon,
    ImVec2(40, 40),
    &playPause
);
```

Mỗi frame callback cập nhật:

```cpp
playPause.t
```

để chuyển đổi play ↔ pause.

---

# 22. Volume icon

```cpp
struct VolumeIconData {
    int volume = 100;
    bool isMuted = false;
    float waveT = 1.0f;
    float muteT = 0.0f;
};
```

Ví dụ:

```cpp
VolumeIconData volumeIcon;

volumeIcon.volume = volume;
volumeIcon.isMuted = muted;

CSImGui::CustomIconButton(
    "##volume",
    DrawVolumeIcon,
    ImVec2(36, 36),
    &volumeIcon
);
```

Icon tự thay đổi số lượng wave dựa trên volume:

```text
0        -> mute
1..33    -> 1 wave
34..66   -> 2 waves
67..100  -> 3 waves
```

---

# 23. System icons

## Fullscreen

```cpp
void DrawFullscreenIcon(...);
```

## Unfullscreen

```cpp
void DrawUnFullscreenIcon(...);
```

Ví dụ:

```cpp
CSImGui::CustomIconButton(
    "##fullscreen",
    DrawFullscreenIcon,
    ImVec2(36, 36)
);
```

---

# 24. Animated fullscreen icon

```cpp
struct FullscreenIconData {
    bool fullscreen = false;
    float t = 0.0f;
};
```

Ví dụ:

```cpp
FullscreenIconData data;
data.fullscreen = isFullscreen;

CSImGui::CustomIconButton(
    "##fullscreen",
    DrawFullscreenIconAnimated,
    ImVec2(36, 36),
    &data
);
```

---

# 25. Settings icon

```cpp
struct SettingsIconData {
    bool hovered;
    bool opened;
    float hover_t = 0.0f;
    float open_t = 0.0f;
    float angle = 0.0f;
};
```

Ví dụ:

```cpp
SettingsIconData data;

data.hovered = ImGui::IsItemHovered();
data.opened = settingsOpen;

CSImGui::CustomIconButton(
    "##settings",
    DrawSettingsIconAnimated,
    ImVec2(36, 36),
    &data
);
```

---

# 26. Option icon

```cpp
struct OptionIconData {
    bool hovered;
    bool opened;
    float hover_t = 0.0f;
    float open_t = 0.0f;
};
```

Sử dụng:

```cpp
OptionIconData data;

data.hovered = hovered;
data.opened = opened;

CSImGui::CustomIconButton(
    "##options",
    DrawOptionIconAnimated,
    ImVec2(36, 36),
    &data
);
```

---

# 27. Loading icon

```cpp
struct LoadingIconData {
    ImDrawList* drawList;
    float angle = 0.0f;
    float speed = 4.0f;
    ImVec2 pos;
    ImVec2 size;
};
```

Ví dụ:

```cpp
LoadingIconData loading{};
loading.speed = 5.0f;

CSImGui::CustomIconButton(
    "##loading",
    DrawLoadingIconAnimated,
    ImVec2(36, 36),
    &loading
);
```

---

# 28. Input Text

## C-string version

```cpp
bool ModernInputText(
    const char* label,
    char* buf,
    size_t buf_size,
    ImGuiInputTextFlags flags = 0
);
```

Ví dụ:

```cpp
char name[256] = {};

if (CSImGui::ModernInputText(
        "Name",
        name,
        sizeof(name)))
{
    OnNameChanged(name);
}
```

---

# 29. Input Text có width

```cpp
bool ModernInputText(
    const char* label,
    char* buf,
    size_t buf_size,
    float width,
    ImGuiInputTextFlags flags = 0
);
```

Ví dụ:

```cpp
char search[256] = {};

CSImGui::ModernInputText(
    "Search",
    search,
    sizeof(search),
    300.0f
);
```

---

# 30. std::string InputText

```cpp
bool ModernInputText(
    const char* label,
    std::string& buffer,
    float width,
    ImGuiInputTextFlags flags = 0
);
```

Ví dụ:

```cpp
std::string search;

if (CSImGui::ModernInputText(
        "Search",
        search,
        300.0f))
{
    Search(search);
}
```

API tự sử dụng:

```cpp
ImGuiInputTextFlags_CallbackResize
```

để resize `std::string`.

---

# 31. Multiline Input

C-string:

```cpp
bool ModernInputTextMultiline(
    const char* label,
    char* buf,
    size_t buf_size,
    const ImVec2& size = ImVec2(-1, 0),
    ImGuiInputTextFlags flags = 0
);
```

Ví dụ:

```cpp
char description[4096] = {};

CSImGui::ModernInputTextMultiline(
    "Description",
    description,
    sizeof(description),
    ImVec2(400, 150)
);
```

---

# 32. Multiline std::string

```cpp
bool ModernInputTextMultiline(
    const char* label,
    std::string& str,
    const ImVec2& size = ImVec2(-1, 0),
    ImGuiInputTextFlags flags = 0
);
```

Ví dụ:

```cpp
std::string script;

CSImGui::ModernInputTextMultiline(
    "Script",
    script,
    ImVec2(500, 300)
);
```

---

# 33. Search Combo

API:

```cpp
bool ModernSearchCombo(
    const char* label,
    std::string& current_value,
    const std::vector<std::string>& options,
    float custom_width = 200.0f,
    int max_items_visible = 6,
    std::function<bool(std::string&)> on_validate_confirm = nullptr,
    std::function<std::string(const std::string&)> on_get_dynamic_opt = nullptr
);
```

Ví dụ cơ bản:

```cpp
std::string selected = "English";

std::vector<std::string> languages = {
    "English",
    "Vietnamese",
    "Japanese",
    "Korean"
};

if (CSImGui::ModernSearchCombo(
        "Language",
        selected,
        languages))
{
    ApplyLanguage(selected);
}
```

---

# 34. Search Combo với validation

Callback:

```cpp
std::function<bool(std::string&)>
```

Ví dụ:

```cpp
auto validate =
    [](std::string& value) -> bool
{
    return !value.empty();
};
```

Sử dụng:

```cpp
CSImGui::ModernSearchCombo(
    "Provider",
    provider,
    providers,
    300.0f,
    6,
    validate
);
```

Nếu callback trả về:

```cpp
true
```

giá trị được xác nhận.

Nếu:

```cpp
false
```

giá trị mới không được xác nhận theo callback.

---

# 35. Dynamic option

Callback:

```cpp
std::function<std::string(
    const std::string&
)>
```

Dùng để sinh một option động dựa trên text search.

Ví dụ:

```cpp
auto dynamic =
    [](const std::string& query)
    -> std::string
{
    if (query.empty())
        return {};

    return "Add: " + query;
};
```

Sử dụng:

```cpp
CSImGui::ModernSearchCombo(
    "Provider",
    provider,
    providers,
    300.0f,
    6,
    validate,
    dynamic
);
```

---

# 36. NormalCombo

```cpp
bool NormalCombo(
    const char* label,
    std::string& current_item,
    const std::vector<std::string>& options,
    float custom_width = 200.0f,
    int max_items_visible = 5
);
```

Ví dụ:

```cpp
std::string mode = "Auto";

std::vector<std::string> modes = {
    "Auto",
    "Hardware",
    "Software"
};

if (CSImGui::NormalCombo(
        "Decoder",
        mode,
        modes))
{
    SetDecoderMode(mode);
}
```

---

# 37. Slider API

Slider chính:

```cpp
bool ModernSliderFloat(
    const char* label,
    float* v,
    float v_min,
    float v_max,
    float height = 4.0f,
    float grab_radius = 8.0f,
    const char* format = "%.3f",
    float custom_width = -1.0f,
    SliderFlags flags = SliderFlags_None
);
```

Ví dụ:

```cpp
float volume = 50.0f;

if (CSImGui::ModernSliderFloat(
        "Volume",
        &volume,
        0.0f,
        100.0f))
{
    SetVolume(volume);
}
```

---

# 38. Slider width

```cpp
CSImGui::ModernSliderFloat(
    "Volume",
    &volume,
    0.0f,
    100.0f,
    5.0f,
    8.0f,
    "%.0f",
    300.0f
);
```

Các tham số:

| Parameter | Ý nghĩa |
|---|---|
| `label` | ID/label |
| `v` | giá trị |
| `v_min` | min |
| `v_max` | max |
| `height` | chiều cao track |
| `grab_radius` | bán kính grab |
| `format` | format value |
| `custom_width` | width |
| `flags` | behavior |

---

# 39. ModernSliderFloatEx()

Đây là API slider đầy đủ:

```cpp
bool ModernSliderFloatEx(
    const char* label,
    float* v,
    float v_min,
    float v_max,
    float height = 4.0f,
    float grab_radius = 8.0f,
    const char* format = "%.3f",
    float custom_width = -1.0f,
    SliderFlags flags = SliderFlags_None,
    SliderRenderCallback render_cb = nullptr,
    SliderTooltipCallback tooltip_cb = nullptr,
    SliderSeekCallback seek_cb = nullptr,
    SliderNavCallback nav_cb = nullptr
);
```

Đây là API nên sử dụng khi cần:

- custom rendering;
- custom tooltip;
- custom seek;
- keyboard navigation;
- smooth preview;
- media-player timeline.

---

# 40. Slider flags

```cpp
SliderFlags_None
```

Tooltip:

```cpp
SliderFlags_TooltipHiden
SliderFlags_TooltipAlwaysShow
SliderFlags_TooltipAlwaysShowAction
SliderFlags_TooltipFollowMouse
SliderFlags_TooltipFade
SliderFlags_TooltipEase
SliderFlags_TooltipScale
SliderFlags_TooltipNoArrow
SliderFlags_TooltipNoBorder
```

Seek:

```cpp
SliderFlags_EnableClickSeek
SliderFlags_NoSeekOnClick
SliderFlags_DisableSeek
SliderFlags_EnableSmoothPreview
```

Navigation:

```cpp
SliderFlags_NoNav
```

Slider animation:

```cpp
SliderFlags_SliderEase
SliderFlags_SliderFade
```

Combined:

```cpp
SliderFlags_TooltipAnimation
SliderFlags_SliderpAnimation
SliderFlags_Animation
SliderFlags_TooltipDefault
SliderFlags_Default
```

---

# 41. Slider mặc định

`ModernSliderFloat()` tự bổ sung:

```cpp
SliderFlags_Default
SliderFlags_Animation
```

Do đó cách đơn giản:

```cpp
CSImGui::ModernSliderFloat(
    "Position",
    &position,
    0.0f,
    duration
);
```

đã có tooltip/click seek/animation theo cấu hình mặc định của implementation.

---

# 42. Slider seek callback

Các cấu trúc:

```cpp
struct SliderSeekRequest {
    float new_value;
    bool from_click;
    bool from_drag;
    bool is_hovered;
    bool is_final;
};
```

Kết quả:

```cpp
struct SliderSeekResult {
    bool accept = true;
    float value = 0.0f;
};
```

Callback:

```cpp
using SliderSeekCallback =
    std::function<
        SliderSeekResult(
            const SliderSeekRequest*
        )
    >;
```

Ví dụ:

```cpp
auto seek =
    [](const SliderSeekRequest* request)
    -> SliderSeekResult
{
    SliderSeekResult result;

    result.accept = true;
    result.value = request->new_value;

    return result;
};
```

Sau đó:

```cpp
CSImGui::ModernSliderFloatEx(
    "Position",
    &position,
    0.0f,
    duration,
    4.0f,
    8.0f,
    "%.1f",
    -1.0f,
    SliderFlags_Default,
    nullptr,
    nullptr,
    seek
);
```

---

# 43. Slider navigation callback

```cpp
using SliderNavCallback =
    std::function<
        void(
            float value,
            bool nav_left,
            bool nav_right
        )
    >;
```

Ví dụ:

```cpp
auto nav =
    [](float value,
       bool left,
       bool right)
{
    if (left)
        SeekRelative(-5.0);

    if (right)
        SeekRelative(5.0);
};
```

---

# 44. Slider render callback

```cpp
using SliderRenderCallback =
    std::function<
        void(
            Phase phase,
            Slot slot,
            SliderState* state,
            SliderRenderData* data,
            ImDrawList* draw_list
        )
    >;
```

Dùng khi muốn custom draw.

Ví dụ skeleton:

```cpp
auto render =
    [](Phase phase,
       Slot slot,
       SliderState* state,
       SliderRenderData* data,
       ImDrawList* dl)
{
    if (phase != Phase::Draw)
        return;

    // custom drawing
};
```

---

# 45. SliderTooltipCallback

```cpp
using SliderTooltipCallback =
    std::function<
        void(
            Phase phase,
            Slot slot,
            TooltipData* data,
            ImDrawList* draw_list
        )
    >;
```

Dùng để customize tooltip của slider.

---

# 46. SliderRenderData

Thông tin render:

```cpp
struct SliderRenderData {
    ImVec2 track_p1;
    ImVec2 track_p2;
    ImVec2 fill_p2;
    ImVec2 grab_center;

    float grab_radius;
    float track_height;

    float buffer_t;
    float chapter_range_start;
    float chapter_range_end;

    float value;
    float t;
    float visual_t;

    bool active;
    bool hovered;
    bool bar_hovered;
    bool grab_hovered;

    std::vector<float> markers;
};
```

Đặc biệt hữu ích cho media player:

```cpp
buffer_t
chapter_range_start
chapter_range_end
markers
```

Có thể dùng để biểu diễn:

```text
|---- buffered ----|
|--- chapters -----|
|-------- position -|
```

---

# 47. Slider markers

`SliderRenderData` có:

```cpp
std::vector<float> markers;
```

Giá trị marker nên biểu diễn theo normalized range:

```text
0.0 -> start
1.0 -> end
```

Ví dụ:

```cpp
data->markers = {
    0.15f,
    0.50f,
    0.82f
};
```

---

# 48. Tooltip API

Có hai API chính:

```cpp
CSImGui::ToolTip(...)
ToolTipEx(...)
```

---

# 49. ToolTip()

```cpp
bool ToolTip(
    const char* label,
    float delay = 3.0f,
    ToolTipFlags flags = ToolTipFlags_None
);
```

Ví dụ:

```cpp
CSImGui::ModernButton("Settings");

CSImGui::ToolTip(
    "Open settings",
    0.5f
);
```

---

# 50. ShowTooltipDelayed()

```cpp
void ShowTooltipDelayed(
    const char* text,
    bool hovering,
    double delaySeconds
);
```

Ví dụ:

```cpp
bool hovered =
    ImGui::IsItemHovered();

CSImGui::ShowTooltipDelayed(
    "Open settings",
    hovered,
    0.5
);
```

---

# 51. Tooltip flags

```cpp
ToolTipFlags_None
```

Mouse:

```cpp
ToolTipFlags_FollowMouse
ToolTipFlags_FollowMouse_Fixed_X
ToolTipFlags_FollowMouse_Fixed_Y
```

Position:

```cpp
ToolTipFlags_Fixed
ToolTipFlags_AutoPosition
ToolTipFlags_ClampItem
ToolTipFlags_ClampWindow
```

Visibility:

```cpp
ToolTipFlags_Hiden
ToolTipFlags_AlwaysShow
ToolTipFlags_AlwaysShowAction
```

Animation:

```cpp
ToolTipFlags_Ease
ToolTipFlags_Fade
ToolTipFlags_Scale
ToolTipFlags_Animation
```

Appearance:

```cpp
ToolTipFlags_NoBorder
ToolTipFlags_NoArrow
```

---

# 52. ToolTipEx()

```cpp
void ToolTipEx(
    TooltipData* td,
    ToolTipFlags flags = ToolTipFlags_None,
    TooltipCallback tooltip_cb = nullptr
);
```

Đây là API tooltip nâng cao.

---

# 53. TooltipData

```cpp
struct TooltipData {
    TooltipItemData item;
    TooltipBeginData config;
    TooltipAnimState* anim = nullptr;

    ImVec2 out_pos;
    ImVec2 out_size;
};
```

Có thể cấu hình:

- position;
- size;
- text;
- title;
- extra;
- image;
- colors;
- layout;
- arrow;
- border;
- animation.

---

# 54. TooltipItemData

Thông tin item:

```cpp
struct TooltipItemData {
    bool is_visible;
    bool active;
    bool hovered;

    float hovered_time;
    float hover_delay;

    float value;
    float seek_value;
    float v_min;
    float range;

    ImVec2 center;
    ImVec2 pos;
    ImVec2 size;
    ImVec2 mouse;
    ImVec2 cursor_size;
};
```

Đặc biệt `seek_value` được tính từ mouse position nếu:

```cpp
config.show_text == true
```

---

# 55. TooltipBeginData

Các field quan trọng:

```cpp
std::string text;
std::string title;
std::string extra;
```

Hiển thị:

```cpp
show_title
show_extra
show_text
```

Font:

```cpp
ImFont* font;
float fontsize;
```

Format:

```cpp
const char* format;
```

Image:

```cpp
ImTextureID image;
ImVec2 image_size;
bool show_image;
float aspect_ratio;
bool lock_aspect;
```

Layout:

```cpp
ImGuiTooltip::Layout_Vertical
ImGuiTooltip::Layout_Horizontal
ImGuiTooltip::Layout_Custom
```

Direction:

```cpp
Dir_Up
Dir_Down
Dir_Left
Dir_Right
Dir_UpLeft
Dir_UpRight
Dir_DownLeft
Dir_DownRight
```

---

# 56. Tooltip callback

```cpp
using TooltipCallback =
    std::function<
        void(
            Phase phase,
            Slot slot,
            TooltipData* data,
            ImDrawList* draw_list
        )
    >;
```

Có thể dùng để customize tooltip trước/during draw.

---

# 57. Text Effects

## ModernTextEffect()

```cpp
void ModernTextEffect(
    const char* text,
    const TextEffectStyle& style
);
```

Ví dụ:

```cpp
TextEffectStyle style;

style.effect = TextEffect::Gradient;
style.colorTopLeft =
    IM_COL32(255,255,255,255);
style.colorBottomRight =
    IM_COL32(0,180,255,255);

CSImGui::ModernTextEffect(
    "Hello",
    style
);
```

---

# 58. TextEffect

```cpp
enum class TextEffect {
    None,
    Gradient,
    Outline,
    Wave,
    Typewriter,
    Shimmer,
    FadeIn,
    Scrolling
};
```

Hiện implementation render trực tiếp:

```text
None
Gradient
Outline
Wave
Typewriter
Shimmer
Scrolling
```

`FadeIn` có enum nhưng implementation hiện tại không có nhánh render riêng cho nó.

---

# 59. TextGradient()

```cpp
void TextGradient(
    const char* text,
    ImU32 colStart,
    ImU32 colEnd
);
```

Ví dụ:

```cpp
CSImGui::TextGradient(
    "Im Player",
    IM_COL32(255,255,255,255),
    IM_COL32(0,180,255,255)
);
```

---

# 60. TextWave()

```cpp
void TextWave(
    const char* text,
    float speed
);
```

Ví dụ:

```cpp
CSImGui::TextWave(
    "Loading...",
    3.0f
);
```

---

# 61. TextScrolling()

```cpp
void TextScrolling(
    const char* text,
    float maxWidth,
    float speed = 40.0f,
    bool scrollOnlyOnHover = false
);
```

Ví dụ:

```cpp
CSImGui::TextScrolling(
    mediaTitle.c_str(),
    250.0f,
    40.0f,
    true
);
```

Rất phù hợp cho:

```text
Tên video dài
Tên bài hát
URL
Subtitle
Provider name
```

---

# 62. ModernTextEffectFmt()

```cpp
void ModernTextEffectFmt(
    const TextEffectStyle& style,
    const char* fmt,
    ...
);
```

Ví dụ:

```cpp
TextEffectStyle style;
style.effect = TextEffect::Wave;

CSImGui::ModernTextEffectFmt(
    style,
    "Volume: %.1f%%",
    volume
);
```

Buffer format nội bộ có kích thước:

```text
1024 bytes
```

Do đó không nên truyền chuỗi format có output cực lớn.

---

# 63. TextEffectStyle

```cpp
struct TextEffectStyle {
    TextEffect effect;

    ImU32 colorTopLeft;
    ImU32 colorBottomRight;
    ImU32 outlineColor;

    float outlineThickness;

    float waveAmplitude;
    float waveFrequency;
    float waveSpeed;

    float progress;

    float speed;
    float fontScale;

    float scrollWidth;
    float scrollSpeed;
    float scrollGap;

    bool scrollOnlyOnHover;
};
```

Typewriter:

```cpp
style.effect = TextEffect::Typewriter;
style.progress = 0.5f;
```

Wave:

```cpp
style.effect = TextEffect::Wave;
style.waveAmplitude = 4.0f;
style.waveFrequency = 5.0f;
style.waveSpeed = 3.0f;
```

Scrolling:

```cpp
style.effect = TextEffect::Scrolling;
style.scrollWidth = 300.0f;
style.scrollSpeed = 50.0f;
style.scrollGap = 30.0f;
```

---

# 64. Container API

## BeginCard()

```cpp
bool BeginCard();
void EndCard();
```

Mẫu sử dụng bắt buộc:

```cpp
if (CSImGui::BeginCard()) {

    ImGui::Text("Content");

    CSImGui::ModernButton("Action");

    CSImGui::EndCard();
}
```

`BeginCard()` là begin/end pair.

---

# 65. BeginModernChild()

```cpp
bool BeginModernChild(
    const char* str_id,
    const ImVec2& size = ImVec2(0, 0),
    bool border = false,
    ImGuiWindowFlags extra_flags = 0
);
```

Ví dụ:

```cpp
if (CSImGui::BeginModernChild(
        "##content",
        ImVec2(400, 300),
        true))
{
    ImGui::Text("Content");

    CSImGui::EndModernChild();
}
```

Phải gọi:

```cpp
EndModernChild()
```

khi `BeginModernChild()` trả về `true`.

---

# 66. BeginModernTabBar()

```cpp
bool BeginModernTabBar(
    const char* id,
    ImGuiTabBarFlags extra_flags = 0
);

void EndModernTabBar();
```

Ví dụ:

```cpp
if (CSImGui::BeginModernTabBar("MainTabs")) {

    if (CSImGui::ModernTabItem("Video")) {
        ImGui::Text("Video settings");

        CSImGui::EndModernTabItem();
    }

    if (CSImGui::ModernTabItem("Audio")) {
        ImGui::Text("Audio settings");

        CSImGui::EndModernTabItem();
    }

    CSImGui::EndModernTabBar();
}
```

---

# 67. ModernTabItem()

```cpp
bool ModernTabItem(
    const char* label,
    bool* p_open = NULL,
    ImGuiTabItemFlags flags = 0,
    ModernTabFlags m_flags = 0
);
```

Custom flags:

```cpp
ModernTabFlags_NoIndicator
ModernTabFlags_NoAnimation
ModernTabFlags_FullWidthBar
```

Ví dụ:

```cpp
CSImGui::ModernTabItem(
    "Audio",
    nullptr,
    0,
    ModernTabFlags_NoAnimation
);
```

---

# 68. PushModernWindowStyle()

```cpp
void PushModernWindowStyle();
```

Dùng:

```cpp
CSImGui::PushModernWindowStyle();

ImGui::Begin("Settings");

...

ImGui::End();

CSImGui::PopModernWindowStyle();
```

Đây là push/pop pair.

Không được quên:

```cpp
PopModernWindowStyle();
```

---

# 69. BeginInfoTable()

```cpp
bool BeginInfoTable(
    const char* id,
    int column_count = 2,
    float first_col_width = 120.0f,
    ImGuiTableFlags extra_flags = 0
);
```

Ví dụ:

```cpp
if (CSImGui::BeginInfoTable(
        "MediaInfo",
        2,
        120.0f))
{
    CSImGui::InfoRow(
        "Codec",
        "%s",
        codec.c_str()
    );

    CSImGui::InfoRow(
        "Resolution",
        "%dx%d",
        width,
        height
    );

    CSImGui::EndInfoTable();
}
```

---

# 70. InfoRow()

```cpp
void InfoRow(
    const char* label,
    const char* fmt,
    ...
);
```

Ví dụ:

```cpp
CSImGui::InfoRow(
    "Duration",
    "%.2f sec",
    duration
);
```

Nếu format tạo chuỗi rỗng, implementation hiển thị:

```text
None
```

---

# 71. BeginListTable()

```cpp
bool BeginListTable(
    const char* id,
    const std::vector<TableCol>& cols,
    ImGuiTableFlags extra_flags = 0
);
```

`TableCol`:

```cpp
struct TableCol {
    const char* name;
    float width;
};
```

Ví dụ:

```cpp
std::vector<TableCol> columns = {
    {"Name", 200.0f},
    {"Type", 120.0f},
    {"Status", 100.0f}
};

if (CSImGui::BeginListTable(
        "Files",
        columns))
{
    ...
    CSImGui::EndListTable();
}
```

Nếu:

```cpp
width > 0
```

column là fixed.

Nếu:

```cpp
width <= 0
```

column là stretch.

---

# 72. BeginListRow()

```cpp
bool BeginListRow(
    float height = 28.0f
);

void EndListRow();
```

Ví dụ:

```cpp
if (CSImGui::BeginListRow()) {

    ImGui::Text("video.mp4");

    CSImGui::EndListRow();
}
```

---

# 73. IsRowClicked()

```cpp
bool IsRowClicked();
```

Kiểm tra item hiện tại có được click/release.

Ví dụ:

```cpp
if (CSImGui::IsRowClicked()) {
    OpenFile(file);
}
```

---

# 74. ModernCollapsingHeader()

```cpp
bool ModernCollapsingHeader(
    const char* id,
    ImGuiTreeNodeFlags flags = 0
);
```

Ví dụ:

```cpp
if (CSImGui::ModernCollapsingHeader(
        "Advanced"))
{
    ImGui::Text("Advanced settings");
}
```

---

# 75. BeginModernPopup()

```cpp
bool BeginModernPopup(
    const char* name,
    bool* open = NULL,
    ImGuiWindowFlags flags = 0
);

void EndModernPopup();
```

Ví dụ:

```cpp
if (CSImGui::BeginModernPopup(
        "Confirm",
        &open))
{
    ImGui::Text("Are you sure?");

    if (CSImGui::ModernButton("Yes")) {
        Confirm();
        open = false;
    }

    CSImGui::EndModernPopup();
}
```

---

# 76. ModernTreeNode()

```cpp
bool ModernTreeNode(
    const char* id
);

void EndModernTreeNode();
```

Ví dụ:

```cpp
if (CSImGui::ModernTreeNode("Audio")) {

    ImGui::Text("Volume");
    ImGui::Text("Device");

    CSImGui::EndModernTreeNode();
}
```

Chỉ gọi `EndModernTreeNode()` khi `ModernTreeNode()` trả về `true`.

---

# 77. ModernSelectable()

```cpp
bool ModernSelectable(
    const char* label,
    bool selected,
    ImGuiSelectableFlags flags = 0,
    const ImVec2& size_arg = ImVec2(0, 0),
    bool enabled = true
);
```

Ví dụ:

```cpp
if (CSImGui::ModernSelectable(
        "English",
        selected == "English"))
{
    selected = "English";
}
```

Disable:

```cpp
CSImGui::ModernSelectable(
    "Unavailable",
    false,
    0,
    ImVec2(0, 0),
    false
);
```

---

# 78. ModernHeader()

```cpp
void ModernHeader(
    const char* title,
    float scale = 1.0f
);
```

Ví dụ:

```cpp
CSImGui::ModernHeader(
    "Audio Settings"
);
```

Scale:

```cpp
CSImGui::ModernHeader(
    "Video Settings",
    1.25f
);
```

---

# 79. DrawCardWithHole()

API low-level:

```cpp
void DrawCardWithHole(
    ImDrawList* dl,
    const ImVec2& cardMin,
    const ImVec2& cardMax,
    const ImVec2& holeMin,
    const ImVec2& holeMax,
    ImU32 fillCol,
    ImU32 borderCol,
    const CardHoleStyle& style
);
```

Dùng khi cần vẽ một card có vùng rỗng/hole.

Ví dụ:

```cpp
CardHoleStyle style;

CSImGui::DrawCardWithHole(
    ImGui::GetWindowDrawList(),
    cardMin,
    cardMax,
    holeMin,
    holeMax,
    fillColor,
    borderColor,
    style
);
```

---

# 80. CardHoleStyle

```cpp
struct CardHoleStyle {
    float rounding = 6.0f;
    float borderThickness = 1.5f;
};
```

Ví dụ:

```cpp
CardHoleStyle style;

style.rounding = 8.0f;
style.borderThickness = 2.0f;
```

---

# 81. Phase và Slot

Custom callback sử dụng:

```cpp
enum class Phase {
    None,
    AfterInit,
    Init,
    BeforeInie,
    Draw
};
```

Và:

```cpp
enum class Slot {
    None,
    Draw_layer0,
    Draw_layer1,
    Draw_layer2,
    Layout,
    End
};
```

Các callback có thể kiểm tra:

```cpp
if (phase == Phase::Draw) {
    ...
}
```

Ví dụ:

```cpp
if (slot == Slot::Draw_layer1) {
    ...
}
```

---

# 82. Kiến trúc callback

Slider callback:

```text
ModernSliderFloatEx
        |
        +-- SliderRenderCallback
        |
        +-- SliderTooltipCallback
        |
        +-- SliderSeekCallback
        |
        +-- SliderNavCallback
```

Tooltip:

```text
ToolTipEx
    |
    +-- TooltipCallback
```

Điều này cho phép application tách:

```text
Widget logic
     |
     +-- interaction
     |
     +-- data
     |
     +-- custom rendering
```

---

# 83. Ví dụ Media Player UI hoàn chỉnh

Một control bar có thể xây dựng như sau:

```cpp
void DrawPlaybackControls(
    bool& paused,
    float& position,
    float duration,
    float& volume,
    bool& muted)
{
    // Previous
    if (CSImGui::CustomIconButton(
            "##prev",
            DrawPrevIcon,
            ImVec2(36, 36)))
    {
        Previous();
    }

    ImGui::SameLine();

    // Play/Pause
    PlayPauseData playData;
    playData.paused = paused;

    if (CSImGui::CustomIconButton(
            "##playpause",
            DrawPlayPauseIcon,
            ImVec2(42, 42),
            &playData))
    {
        paused = !paused;

        if (paused)
            Pause();
        else
            Play();
    }

    ImGui::SameLine();

    // Next
    if (CSImGui::CustomIconButton(
            "##next",
            DrawNextIcon,
            ImVec2(36, 36)))
    {
        Next();
    }

    // Timeline
    CSImGui::ModernSliderFloat(
        "##timeline",
        &position,
        0.0f,
        duration,
        4.0f,
        8.0f,
        "%.1f",
        -1.0f,
        SliderFlags_Default
    );

    // Volume
    VolumeIconData volumeData;
    volumeData.volume = static_cast<int>(volume);
    volumeData.isMuted = muted;

    if (CSImGui::CustomIconButton(
            "##volume",
            DrawVolumeIcon,
            ImVec2(36, 36),
            &volumeData))
    {
        muted = !muted;
    }

    CSImGui::ModernSliderFloat(
        "##volume_slider",
        &volume,
        0.0f,
        100.0f,
        4.0f,
        7.0f,
        "%.0f",
        120.0f
    );
}
```

---

# 84. Ví dụ Settings UI

```cpp
void DrawSettings()
{
    CSImGui::ModernHeader("General");

    bool autoplay = true;

    CSImGui::ModernToggle(
        "##autoplay",
        &autoplay
    );

    ImGui::SameLine();

    ImGui::Text("Autoplay");

    std::string renderer = "OpenGL";

    std::vector<std::string> renderers = {
        "OpenGL",
        "Vulkan",
        "Software"
    };

    CSImGui::NormalCombo(
        "Renderer",
        renderer,
        renderers,
        250.0f
    );

    std::string language = "English";

    std::vector<std::string> languages = {
        "English",
        "Vietnamese",
        "Japanese",
        "Korean",
        "Chinese"
    };

    CSImGui::ModernSearchCombo(
        "Language",
        language,
        languages,
        250.0f
    );
}
```

---

# 85. Ví dụ thông tin media

```cpp
void DrawMediaInfo()
{
    CSImGui::ModernHeader("Media Information");

    if (CSImGui::BeginInfoTable(
            "MediaInfo",
            2,
            140.0f))
    {
        CSImGui::InfoRow(
            "File",
            "%s",
            fileName.c_str()
        );

        CSImGui::InfoRow(
            "Codec",
            "%s",
            codec.c_str()
        );

        CSImGui::InfoRow(
            "Resolution",
            "%dx%d",
            width,
            height
        );

        CSImGui::InfoRow(
            "FPS",
            "%.02f",
            fps
        );

        CSImGui::InfoRow(
            "Duration",
            "%.02f sec",
            duration
        );

        CSImGui::EndInfoTable();
    }
}
```

---

# 86. Ví dụ tab UI

```cpp
void DrawSettingsTabs()
{
    if (!CSImGui::BeginModernTabBar(
            "SettingsTabs"))
        return;

    if (CSImGui::ModernTabItem("General")) {

        DrawGeneralSettings();

        CSImGui::EndModernTabItem();
    }

    if (CSImGui::ModernTabItem("Video")) {

        DrawVideoSettings();

        CSImGui::EndModernTabItem();
    }

    if (CSImGui::ModernTabItem("Audio")) {

        DrawAudioSettings();

        CSImGui::EndModernTabItem();
    }

    CSImGui::EndModernTabBar();
}
```

---

# 87. Ví dụ text effect cho title

```cpp
TextEffectStyle style;

style.effect = TextEffect::Gradient;

style.colorTopLeft =
    IM_COL32(255, 255, 255, 255);

style.colorBottomRight =
    IM_COL32(0, 180, 255, 255);

style.fontScale = 1.2f;

CSImGui::ModernTextEffect(
    "Im Player",
    style
);
```

---

# 88. Ví dụ scrolling media title

```cpp
CSImGui::TextScrolling(
    mediaTitle.c_str(),
    300.0f,
    35.0f,
    true
);
```

Trong trường hợp:

```text
Tên video ngắn
```

text đứng yên.

Nếu:

```text
Tên video rất rất rất dài...
```

text tự scrolling.

Nếu:

```cpp
scrollOnlyOnHover = true;
```

thì chỉ scroll khi hover.

---

# 89. Quy tắc Begin/End

Các API sau phải được sử dụng theo cặp.

| Begin | End |
|---|---|
| `BeginCard()` | `EndCard()` |
| `BeginModernChild()` | `EndModernChild()` |
| `BeginModernTabBar()` | `EndModernTabBar()` |
| `ModernTabItem()` | `EndModernTabItem()` |
| `BeginInfoTable()` | `EndInfoTable()` |
| `BeginListTable()` | `EndListTable()` |
| `BeginListRow()` | `EndListRow()` |
| `BeginModernPopup()` | `EndModernPopup()` |
| `ModernTreeNode()` | `EndModernTreeNode()` |
| `PushModernWindowStyle()` | `PopModernWindowStyle()` |

Đây là nguyên tắc rất quan trọng để tránh phá ImGui style/window stack.

---

# 90. Pattern an toàn cho Begin API

Không nên:

```cpp
CSImGui::BeginModernChild(...);

DrawSomething();

CSImGui::EndModernChild();
```

Nên:

```cpp
if (CSImGui::BeginModernChild(...)) {

    DrawSomething();

    CSImGui::EndModernChild();
}
```

Tương tự với:

```cpp
BeginInfoTable
BeginListTable
BeginModernTabBar
BeginModernPopup
ModernTreeNode
```

---

# 91. Thread safety

Module hiện tại được thiết kế theo mô hình:

```text
ONE ImGuiContext
        |
        +-- ONE active GUI execution thread
                |
                +-- CSImGui widgets
                +-- ImGui state
                +-- ImDrawList
                +-- ImGuiStorage
                +-- Theme state
```

Không nên gọi:

```cpp
CSImGui::ModernButton(...)
```

từ worker thread.

Không nên gọi:

```cpp
CSImGui::GetColors(...)
CSImGui::UpdateTheme(...)
ToolTipEx(...)
ModernSliderFloat(...)
```

trên một thread khác trong khi render thread đang chạy.

---

# 92. Đặc biệt quan trọng với multi-window

Module sử dụng nhiều state thuộc ImGui:

```cpp
ImGui::GetStateStorage()
ImGui::GetCurrentWindow()
ImGui::GetWindowDrawList()
GImGui
```

Vì vậy nếu project có:

```text
Window A -> ImGuiContext A
Window B -> ImGuiContext B
```

thì mỗi thread phải đảm bảo context tương ứng đã được set/current trước khi gọi API.

Không được để:

```text
Thread A
    -> context A

Thread B
    -> context B

nhưng cùng lúc cùng truy cập shared GUI state
```

đặc biệt với:

```cpp
GetColors()
GetStyle()
ThemeLibrary
dhs
gtr
```

---

# 93. Theme hiện tại là global

Implementation sử dụng:

```cpp
static std::map<ThemeType, Style> ThemeLibrary;
static ThemeTransition gtr;
static Style dhs;
```

Do đó theme system hiện tại **không phải per-ImGuiContext**.

Nếu ứng dụng có nhiều ImGui context chạy đồng thời, cần đặc biệt chú ý.

Ví dụ:

```text
Window A
    Context A
        Render

Window B
    Context B
        Render
```

cùng gọi:

```cpp
CSImGui::UpdateTheme(...)
```

sẽ truy cập shared:

```cpp
gtr
dhs
```

Đây không phải thiết kế thread-safe.

---

# 94. Khuyến nghị kiến trúc cho Im_player

Nếu Im_player sử dụng nhiều window/render thread, nên tổ chức:

```text
Application
│
├── GuiThemeManager
│     ├── Window A theme state
│     └── Window B theme state
│
├── ImGuiContext A
│     └── GUI thread A
│
└── ImGuiContext B
      └── GUI thread B
```

Thay vì:

```text
Global dhs
Global gtr
Global ThemeLibrary
```

dùng per-context/per-window state.

---

# 95. State animation

Nhiều widget lưu animation state vào:

```cpp
ImGui::GetStateStorage()
```

Ví dụ:

```cpp
ModernButton
ModernCheckbox
ModernToggle
ModernSelectable
ModernTabItem
ModernInputText
ModernSlider
```

Do đó `str_id`/`label` phải ổn định.

Tốt:

```cpp
CSImGui::ModernButton(
    "Play"
);
```

Hoặc:

```cpp
CSImGui::ModernButton(
    "##play_button"
);
```

Không nên tạo ID ngẫu nhiên mỗi frame.

---

# 96. ID với icon button

Icon không có text hiển thị nên nên dùng:

```cpp
"##play"
"##pause"
"##next"
"##prev"
"##settings"
"##volume"
```

Ví dụ:

```cpp
CSImGui::CustomIconButton(
    "##settings",
    DrawSettingsIconAnimated,
    ImVec2(36,36),
    &settingsData
);
```

Không nên:

```cpp
CSImGui::CustomIconButton(
    "",
    ...
);
```

vì có thể gây ID collision.

---

# 97. State object của animated icon

Các object:

```cpp
PlayPauseData
VolumeIconData
SettingsIconData
FullscreenIconData
OptionIconData
LoadingIconData
```

nên có lifetime ổn định.

Không nên:

```cpp
void Draw()
{
    PlayPauseData data;

    ...
}
```

nếu animation cần giữ state liên tục giữa các frame.

Nên:

```cpp
struct PlayerUIState {
    PlayPauseData playPause;
    VolumeIconData volume;
    FullscreenIconData fullscreen;
};
```

và giữ object này lâu dài.

---

# 98. Slider cho media timeline

Kiến trúc phù hợp:

```text
MPV
 |
 +-- duration
 +-- position
 +-- cache/buffer
 +-- chapters
 |
 v
ModernSliderFloatEx
 |
 +-- seek callback
 +-- tooltip callback
 +-- render callback
```

Ví dụ:

```cpp
SliderFlags flags =
    SliderFlags_EnableClickSeek |
    SliderFlags_EnableSmoothPreview |
    SliderFlags_TooltipFollowMouse |
    SliderFlags_TooltipScale;
```

Sau đó:

```cpp
CSImGui::ModernSliderFloatEx(
    "##timeline",
    &position,
    0.0f,
    duration,
    4.0f,
    8.0f,
    "%.1f",
    -1.0f,
    flags,
    renderCallback,
    tooltipCallback,
    seekCallback,
    navCallback
);
```

Đây là API phù hợp nhất trong module cho playback timeline của Im_player.

---

# 99. Recommended API layering

Không nên để application gọi low-level API quá nhiều.

Nên chia:

```text
Application UI
       |
       v
ImPlayer widgets
       |
       v
CSImGui
       |
       v
Dear ImGui
       |
       v
Graphics backend
```

Ví dụ:

```cpp
DrawPlaybackBar()
```

nên sử dụng:

```cpp
ModernSliderFloatEx
CustomIconButton
TextScrolling
ToolTip
```

thay vì trực tiếp thao tác quá nhiều:

```cpp
ImGui::ButtonBehavior
ImGui::ItemAdd
ImDrawList::AddRectFilled
```

---

# 100. API nên dùng thường xuyên

## Button

```cpp
ModernButton()
SecondaryButton()
ModernSmallButton()
ModernArrowButton()
```

## Icon

```cpp
CustomIconButton()
DrawPlayIcon()
DrawPauseIcon()
DrawPrevIcon()
DrawNextIcon()
DrawVolumeIcon()
DrawFullscreenIcon()
```

## Input

```cpp
ModernInputText()
ModernInputTextMultiline()
ModernSearchCombo()
NormalCombo()
```

## Playback

```cpp
ModernSliderFloat()
ModernSliderFloatEx()
```

## Tooltip

```cpp
ToolTip()
ShowTooltipDelayed()
ToolTipEx()
```

## Text

```cpp
TextGradient()
TextWave()
TextScrolling()
ModernTextEffect()
```

## Container

```cpp
BeginCard()
BeginModernChild()
BeginModernTabBar()
ModernTabItem()
BeginInfoTable()
BeginListTable()
BeginModernPopup()
ModernTreeNode()
ModernSelectable()
ModernHeader()
```

---

# 101. API low-level nên hạn chế gọi trực tiếp

Các API sau có tính low-level hơn:

```cpp
DrawCardWithHole()
ToolTipEx()
ModernSliderFloatEx()
GetStyle()
GetColors()
```

Không phải không được dùng, nhưng nên sử dụng khi cần custom behavior.

Đặc biệt:

```cpp
ModernSliderFloatEx()
```

nên được dùng cho playback timeline/custom seek.

Còn UI slider thông thường nên dùng:

```cpp
ModernSliderFloat()
```

---

# 102. Mẫu frame chuẩn

Một frame GUI cơ bản:

```cpp
void RenderGUI()
{
    ImGui::NewFrame();

    CSImGui::UpdateTheme(
        ImGui::GetIO().DeltaTime
    );

    CSImGui::PushModernWindowStyle();

    if (ImGui::Begin("Main")) {

        CSImGui::ModernHeader(
            "Media Player"
        );

        CSImGui::ModernButton(
            "Open File"
        );

        ImGui::SameLine();

        CSImGui::ModernButton(
            "Settings"
        );

        CSImGui::TextScrolling(
            currentTitle.c_str(),
            300.0f,
            40.0f,
            true
        );
    }

    ImGui::End();

    CSImGui::PopModernWindowStyle();

    ImGui::Render();
}
```

---

# 103. Mẫu UI media player khuyến nghị

```text
┌──────────────────────────────────────────────┐
│                 Media Player                 │
├──────────────────────────────────────────────┤
│                                              │
│                Video Area                    │
│                                              │
├──────────────────────────────────────────────┤
│ Title: Very long media title ...             │
│                                              │
│  [Prev] [Play] [Next]                        │
│                                              │
│  0:32 ━━━━━━━━━━━●━━━━━━━━━━ 12:45           │
│                                              │
│  [Volume] ━━━━━━━●━━━━ 80%   [Fullscreen]  │
└──────────────────────────────────────────────┘
```

Có thể map trực tiếp:

```text
Title
  -> TextScrolling

Prev/Play/Next
  -> CustomIconButton

Timeline
  -> ModernSliderFloatEx

Volume
  -> DrawVolumeIcon
  -> ModernSliderFloat

Fullscreen
  -> DrawFullscreenIconAnimated
```

---

# 104. Lifecycle khuyến nghị

## Startup

```cpp
ImGui::CreateContext();

CSImGui::InitThemeLibrary(
    ThemeType::DarkMode
);
```

## Mỗi frame

```cpp
ImGui::NewFrame();

CSImGui::UpdateTheme(
    ImGui::GetIO().DeltaTime
);

DrawApplicationUI();

ImGui::Render();
```

## Shutdown

Không có `CSImGui::Shutdown()` riêng trong module hiện tại.

Shutdown ImGui theo lifecycle của application:

```cpp
ImGui::DestroyContext();
```

---

# 105. Checklist tích hợp

- [ ] Tạo ImGui context trước khi gọi `CSImGui`.
- [ ] Gọi `InitThemeLibrary()` sau khi context được tạo.
- [ ] Gọi `UpdateTheme()` mỗi frame nếu sử dụng theme transition.
- [ ] Widget phải được gọi trong đúng ImGui frame.
- [ ] Không gọi GUI API từ worker thread.
- [ ] ID của widget phải ổn định.
- [ ] Begin/End phải cân bằng.
- [ ] Push/Pop style phải cân bằng.
- [ ] Animated icon data phải có lifetime ổn định.
- [ ] `ModernSliderFloatEx()` chỉ dùng khi cần callback/custom behavior.
- [ ] Với media timeline nên dùng `SliderSeekCallback`.
- [ ] Với title dài nên dùng `TextScrolling`.
- [ ] Với multi-window cần kiểm soát ImGuiContext rất chặt.
- [ ] Không chia sẻ global theme state giữa nhiều GUI thread nếu chưa có synchronization/per-context design.

---

# 106. Quick Reference

```cpp
// Theme
CSImGui::InitThemeLibrary(ThemeType::DarkMode);
CSImGui::ApplyTheme(ThemeType::MidnightMode);
CSImGui::UpdateTheme(dt);

// Buttons
CSImGui::ModernButton("OK");
CSImGui::SecondaryButton("Cancel");
CSImGui::ModernSmallButton("Reset");
CSImGui::ModernArrowButton("##next", ImGuiDir_Right);

// Checkbox / Toggle
CSImGui::ModernCheckbox("Enabled", &enabled);
CSImGui::ModernToggle("##toggle", &enabled);

// Icons
CSImGui::CustomIconButton(
    "##play",
    DrawPlayIcon,
    ImVec2(36,36)
);

// Input
CSImGui::ModernInputText(
    "Name",
    name,
    sizeof(name),
    300.0f
);

// Combo
CSImGui::NormalCombo(
    "Mode",
    mode,
    modes
);

// Search
CSImGui::ModernSearchCombo(
    "Provider",
    provider,
    providers
);

// Slider
CSImGui::ModernSliderFloat(
    "Volume",
    &volume,
    0.0f,
    100.0f
);

// Tooltip
CSImGui::ToolTip(
    "Tooltip text",
    0.5f
);

// Text
CSImGui::TextGradient(
    "Title",
    color1,
    color2
);

CSImGui::TextScrolling(
    title.c_str(),
    300.0f
);

// Containers
if (CSImGui::BeginCard()) {
    ...
    CSImGui::EndCard();
}

if (CSImGui::BeginModernChild("child")) {
    ...
    CSImGui::EndModernChild();
}

// Header
CSImGui::ModernHeader("Settings");
```

---

# 107. Kết luận

Module `gui/` hiện tại có thể xem như một **Custom ImGui Widget Layer** cho Im_player.

Ba nhóm API quan trọng nhất đối với Im_player là:

```text
1. ModernSliderFloatEx
   -> playback / seek / buffer / chapter / tooltip

2. CustomIconButton
   -> playback controls / volume / fullscreen / settings

3. Container + Text APIs
   -> layout / settings / media information / title
```

Kiến trúc sử dụng khuyến nghị:

```text
                 Im_player UI
                      │
          ┌───────────┼───────────┐
          │           │           │
       Playback     Settings     Media Info
          │           │           │
          ▼           ▼           ▼
       Slider       Inputs      Containers
       Icons        Combo       Tables
       Tooltip      Toggle      Text
          │           │           │
          └───────────┼───────────┘
                      ▼
                   CSImGui
                      ▼
                 Dear ImGui
                      ▼
                Graphics Backend
```

Điểm cần đặc biệt lưu ý đối với kiến trúc Im_player nhiều window/thread là **GUI API hiện tại phụ thuộc mạnh vào ImGuiContext và có một số state global (`dhs`, `gtr`, `ThemeLibrary`, `G_LastHoveredID`, `search_buffers`)**. Vì vậy tài liệu API này mô tả cách sử dụng đúng ở mức chức năng, nhưng nếu muốn chạy nhiều `ImGuiContext` trên nhiều render thread thì layer này cần được phân tích/refactor thêm theo hướng **per-context GUI state** trước khi xem là thread-safe hoàn toàn.