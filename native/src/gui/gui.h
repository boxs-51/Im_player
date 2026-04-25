
#pragma once

#include <vector>
#include <string>
#include <map>
#include <imgui.h>
#include <stdint.h>
#include <functional>
#define SMOOTH_LERP(speed, dt) (1.0f - expf(-(speed) * (dt)))
#define IM_COL32_LERP(A, B, T) \
    IM_COL32( \
        (ImU32)(((A) >> IM_COL32_R_SHIFT & 0xFF) + (((B) >> IM_COL32_R_SHIFT & 0xFF) - ((A) >> IM_COL32_R_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_G_SHIFT & 0xFF) + (((B) >> IM_COL32_G_SHIFT & 0xFF) - ((A) >> IM_COL32_G_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_B_SHIFT & 0xFF) + (((B) >> IM_COL32_B_SHIFT & 0xFF) - ((A) >> IM_COL32_B_SHIFT & 0xFF)) * (T)), \
        (ImU32)(((A) >> IM_COL32_A_SHIFT & 0xFF) + (((B) >> IM_COL32_A_SHIFT & 0xFF) - ((A) >> IM_COL32_A_SHIFT & 0xFF)) * (T))  \
    )
typedef uint8_t Col;
typedef uint8_t Var;
typedef uint16_t ModernTabFlags;
typedef uint16_t ModernWinDownFlags;

enum Var_ {
    Var_COUNT,
};
enum Col_
{
    Col_Text,
    Col_TextDisabled,
    Col_WindowBg,              // Background of normal windows
    Col_ChildBg,               // Background of child windows
    Col_PopupBg,               // Background of popups, menus, tooltips windows
    Col_Border,
    Col_BorderShadow,
    Col_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
    Col_FrameBgHovered,
    Col_FrameBgActive,
    Col_TitleBg,               // Title bar
    Col_TitleBgActive,         // Title bar when focused
    Col_TitleBgCollapsed,      // Title bar when collapsed
    Col_MenuBarBg,
    Col_ScrollbarBg,
    Col_ScrollbarGrab,
    Col_ScrollbarGrabHovered,
    Col_ScrollbarGrabActive,
    Col_CheckMark,             // Checkbox tick and RadioButton circle
    Col_SliderGrab,
    Col_SliderGrabHovered,
    Col_SliderGrabActive,
    Col_Button,
    Col_ButtonHovered,
    Col_ButtonActive,
    Col_Header,                // Header* color are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    Col_HeaderHovered,
    Col_HeaderActive,
    Col_Separator,
    Col_SeparatorHovered,
    Col_SeparatorActive,
    Col_ResizeGrip,            // Resize grip in lower-right and lower-left corners of windows.
    Col_ResizeGripHovered,
    Col_ResizeGripActive,
    Col_InputTextCursor,       // InputText cursor/caret
    Col_TabHovered,            // Tab background, when hovered
    Col_Tab,                   // Tab background, when tab-bar is focused & tab is unselected
    Col_TabSelected,           // Tab background, when tab-bar is focused & tab is selected
    Col_TabSelectedOverline,   // Tab horizontal overline, when tab-bar is focused & tab is selected
    Col_TabActive,
    Col_TabUnfocused,
    Col_TabUnfocusedActive,
    Col_TabDimmed,             // Tab background, when tab-bar is unfocused & tab is unselected
    Col_TabDimmedSelected,     // Tab background, when tab-bar is unfocused & tab is selected
    Col_TabDimmedSelectedOverline,//..horizontal overline, when tab-bar is unfocused & tab is selected
    Col_DockingPreview,        // Preview overlay color when about to docking something
    Col_DockingEmptyBg,        // Background color for empty node (e.g. CentralNode with no window docked into it)
    Col_PlotLines,
    Col_PlotLinesHovered,
    Col_PlotHistogramHovered,
    Col_PlotHistogramActive,
    Col_PlotHistogram,
    Col_TableHeaderBg,         // Table header background
    Col_TableBorderStrong,     // Table outer and header borders (prefer using Alpha=1.0 here)
    Col_TableBorderLight,      // Table inner borders (prefer using Alpha=1.0 here)
    Col_TableRowBg,            // Table row background (even rows)
    Col_TableRowBgAlt,         // Table row background (odd rows)
    Col_TextLink,              // Hyperlink color
    Col_TextSelected,
    Col_TextSelectedBg,        // Selected text inside an InputText
    Col_TreeLines,             // Tree node hierarchy outlines when using ImGuiTreeNodeFlags_DrawLines
    Col_DragDropTarget,        // Rectangle highlighting a drop target
    Col_NavCursor,             // Color of keyboard/gamepad navigation cursor/rectangle, when visible
    Col_NavWindowingHighlight, // Highlight window when using CTRL+TAB
    Col_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when active
    Col_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is active
    Col_COUNT
};
enum ModernTabFlags_ {
    ModernTabFlags_None             = 0,
    ModernTabFlags_NoIndicator      = 1 << 0, // Tắt thanh kẻ dưới chân
    ModernTabFlags_NoAnimation      = 1 << 1, // Tắt hiệu ứng mượt
    ModernTabFlags_FullWidthBar     = 1 << 2, // Thanh kẻ dài 100% thay vì 80%
};
enum ModernWinDownFlags_{
   ModernWinDownFlags_None          = 0,
   ModernWinDownFlags_NoTitleBar    = 1 << 0,
};


enum class CheckboxStyle : uint8_t {
    Circle,     // Kiểu chấm tròn (như Radio Button nhưng đa chọn)
    Tick,       // Kiểu dấu tích cổ điển (Vẽ bằng đường thẳng)
    Square      // Kiểu hình vuông đặc (Fill)
};
enum class ThemeType : uint8_t {
    DarkMode,
    LightMode,
    MidnightMode,
    RetroMode
};
struct Stytle{
    ImVec4 Colors[Col_COUNT];
};

 
struct ThemeTransition {
    Stytle startTheme;   // Màu lúc bắt đầu bấm nút
    Stytle targetTheme;  // Màu đích muốn tới
    float progress = 1.0f;    // 1.0 nghĩa là đã xong, < 1.0 là đang chạy
    float speed = 2.5f;       // Tốc độ chuyển đổi
    bool active = false;
};

struct SliderTooltipData
{
    // ===== INPUT =====
    float value = 0.0f;
    float seek_value = 0.0f;
    float hovered_time =0.0f;

    bool active = false;
    bool hovered = false;
    bool show_tooltip =false;
    
    float hover_delay = 0.2f;
    float range = 0.0f;
    float dt = 8.0f;
    float fontsize = 12.0f;
    float v_min = 0.0f;
    ImFont *font;
    ImDrawList *draw_list;

    ImVec2 center = ImVec2(0, 0);
    ImVec2 pos_item = ImVec2(0, 0);
    ImVec2 size_item = ImVec2(0, 0);
    ImVec2 mouse = ImVec2(0, 0);
    ImVec2 cursor_size = ImVec2(0, 0);

    // ===== TEXT =====
    //char text[128];
    //char title[286];
    //char extra[564];

    std::string text;
    std::string title;
    std::string extra;

    bool show_title = false;
    bool show_extra = false;
    bool show_text = true;

    int title_max_lines = 2;
    const char* format = "%.03f";

    enum Alignment { Left, Center, Right };
    Alignment align = Center;
    // ===== IMAGE =====
    ImTextureID image = 0;
    ImVec2 image_size = ImVec2(0,0);
    bool show_image = false;

    // ===== LAYOUT =====
    enum LayoutMode
    {
        Layout_Vertical,   // image -> title -> text
        Layout_Horizontal, // image | text
        Layout_Custom
    };
    LayoutMode layout = Layout_Vertical;
    bool lock_dir = false;

    enum TooltipDirection {
        TooltipDir_Up,
        TooltipDir_Down,
        TooltipDir_Left,
        TooltipDir_Right,
        TooltipDir_UpLeft,
        TooltipDir_UpRight,
        TooltipDir_DownLeft,
        TooltipDir_DownRight
    };

    TooltipDirection dir = TooltipDir_Down;

    std::vector<std::string> cached_title;
    std::vector<std::string> cached_text;
    std::vector<std::string> cached_extra;

    std::string last_raw_title; // Để kiểm tra thay đổi
    std::string last_raw_text;
    std::string last_raw_extra;

    float padding_content = 6.0f;
    float spacing_content = 4.0f;
    float padding = 12.0f;
    float spacing_mouse = 12.0f;

    float rounding = 4.0f;
    float border_thickness = 1.0f;

    float arrow_width = 5.0f;
    float arrow_height = 5.0f;

    float arrow_scale_min = 0.5f;
    float arrow_scale_max = 1.2f;

    float arrow_corner_bias = 0.25f;

    float edge_safe_padding = 1.0f;
    // ===== SIZE LIMIT =====
    float max_width  = 260.0f;
    float max_height = 200.0f;

    float aspect_ratio = 16.0f / 9.0f; // cho image
    bool lock_aspect = true;

    //=======Colors=======
    ImU32 col_bg = IM_COL32(0, 0, 0, 200);
    ImU32 col_text = IM_COL32(255, 255, 255, 255);
    ImU32 col_border = IM_COL32(255, 255, 255, 255);
    ImU32 col_title = IM_COL32(255, 255, 120, 255);
    ImU32 col_extra = IM_COL32(180, 180, 180, 255);

    // ===== DRAW FLAGS =====
    bool skip_draw_bg = false;
    bool skip_draw_border = false;
    bool skip_draw_text = false;
    bool skip_draw_arrow = false;
    
    bool skip_draw = false;

    // ===== OUTPUT =====
    ImVec2 pos = ImVec2(0,0);
    ImVec2 size = ImVec2(0,0);

    struct TooltipAnimState
    {
        float alpha = 0.0f;
        float speedfade = 12.0f;
        float speedease = 15.0f;
        ImVec2 pos = ImVec2(0,0);
        ImVec2 size = ImVec2(0,0);

        ImVec2 last_arrow_dir = ImVec2(0, -1);

    }* anim = nullptr; // pointer đến struct animation để có thể điều khiển hiệu ứng mượt mà từ callback render

};

struct SliderRenderData
{
    // geometry
    ImVec2 track_p1 = ImVec2(0, 0);
    ImVec2 track_p2 = ImVec2(0, 0);
    ImVec2 fill_p2 = ImVec2(0, 0);
    ImVec2 grab_center = ImVec2(0, 0);

    // size
    float grab_radius = 5.0f;
    float track_height = 5.0f;
    float buffer_t = -1;;
    float chapter_range_start = -1.0f;
    float chapter_range_end   = -1.0f;
    float height = 0.0f;
    float height_on_hover = 0.0f;
    float marker_thickness = 2.0f;
    float grab_shadow_size = 3.0f;
     
    // value
    float value = 0.0f;
    float t = 0.0f;
    float visual_t = 0.0f;

    // state
    bool active = false;
    bool hovered = false;
    bool bar_hovered = false;
    bool grab_hovered = false;

    enum SlideDir { None, Left, Right, Up, Down } slide_dir = None;

    // default colors
    /*
    ImU32 col_track = IM_COL32(60,60,60,180);
    ImU32 col_fill = IM_COL32(100,100,100,255);
    ImU32 col_border = IM_COL32(100,100,100,255);
    ImU32 col_grab = IM_COL32(150,150,150,255);
    ImU32 col_grab_border = IM_COL32(255,255,255,255);
    ImU32 col_buffer = IM_COL32(100,100,100,128);
    ImU32 col_marker = IM_COL32(255,255,255,128);
    ImU32 col_chapter_range = IM_COL32(100,100,255,60);
    ImU32 col_grab_shadow    = IM_COL32(0, 0, 0, 30); // (0  , 0  , 0  , 30)
    */
    ImVec4 col_track          = ImVec4(0.235f, 0.235f, 0.235f, 0.706f); // (60, 60, 60, 180)
    ImVec4 col_fill           = ImVec4(0.392f, 0.392f, 0.392f, 1.000f); // (100, 100, 100, 255)
    ImVec4 col_border         = ImVec4(0.392f, 0.392f, 0.392f, 1.000f); // (100, 100, 100, 255)
    ImVec4 col_grab           = ImVec4(0.588f, 0.588f, 0.588f, 1.000f); // (150, 150, 150, 255)
    ImVec4 col_grab_border    = ImVec4(1.000f, 1.000f, 1.000f, 1.000f); // (255, 255, 255, 255)
    ImVec4 col_buffer         = ImVec4(0.392f, 0.392f, 0.392f, 0.502f); // (100, 100, 100, 128)
    ImVec4 col_marker         = ImVec4(1.000f, 1.000f, 1.000f, 0.502f); // (255, 255, 255, 128)
    ImVec4 col_chapter_range  = ImVec4(0.392f, 0.392f, 1.000f, 0.235f); // (100, 100, 255, 60)
    ImVec4 col_grab_shadow    = ImVec4(0.000f, 0.000f, 0.000f, 0.117f); // (0  , 0  , 0  , 30)

    // control flags
    bool skip_draw_track = false;
    bool skip_draw_fill  = false;
    bool skip_draw_grab  = false;
    bool skip_draw_border = false;
    bool skip_draw_buffer = false;
    bool skip_draw_markers = false;
    bool skip_draw_chapter_range = false;

    bool skip_draw = false;

    std::vector<float> markers = {}; // giá trị từ 0.0f đến 1.0f, dùng để đánh dấu các điểm đặc biệt trên track (ví dụ: chapter markers)
    
    struct SliderAnimState
    {
        float hover = 0.0f;
        float active = 0.0f;
        float bar_hover = 0.0f;
        float grab_hover = 0.0f;
        float grab_scale = 1.0f;
        float speedease = 15.0f;
        float bar_height_scale = 1.0f;
        
        ImVec4 col_track          = ImVec4(0.235f, 0.235f, 0.235f, 0.706f); // (60, 60, 60, 180)
        ImVec4 col_fill           = ImVec4(0.392f, 0.392f, 0.392f, 1.000f); // (100, 100, 100, 255)
        ImVec4 col_border         = ImVec4(0.392f, 0.392f, 0.392f, 1.000f); // (100, 100, 100, 255)
        ImVec4 col_grab           = ImVec4(0.588f, 0.588f, 0.588f, 1.000f); // (150, 150, 150, 255)
        ImVec4 col_grab_border    = ImVec4(1.000f, 1.000f, 1.000f, 1.000f); // (255, 255, 255, 255)
        ImVec4 col_buffer         = ImVec4(0.392f, 0.392f, 0.392f, 0.502f); // (100, 100, 100, 128)
        ImVec4 col_marker         = ImVec4(1.000f, 1.000f, 1.000f, 0.502f); // (255, 255, 255, 128)
        ImVec4 col_chapter_range  = ImVec4(0.392f, 0.392f, 1.000f, 0.235f); // (100, 100, 255, 60)
        ImVec4 col_grab_shadow    = ImVec4(0.000f, 0.000f, 0.000f, 0.117f); // (0  , 0  , 0  , 30)
        /*
        ImU32 col_track = IM_COL32(1, 1, 1, 1);
        ImU32 col_fill = IM_COL32(1, 1, 1, 1);
        ImU32 col_grab = IM_COL32(1, 1, 1, 1);*/

    }* anim = nullptr; // pointer đến struct animation để có thể điều khiển hiệu ứng mượt mà từ callback render;
};

typedef uint16_t SliderFlags;
enum SliderFlags_ 
{
    SliderFlags_None                    = 0,
    SliderFlags_TooltipHiden            = 1 << 0,
    SliderFlags_TooltipAlwaysShow       = 1 << 1,
    SliderFlags_TooltipAlwaysShowAction = 1 << 2,
    SliderFlags_TooltipFollowMouse      = 1 << 3,
    SliderFlags_TooltipFade             = 1 << 4,
    SliderFlags_TooltipEase             = 1 << 5,   
    SliderFlags_TooltipScale            = 1 << 6,
    SliderFlags_TooltipNoArrow          = 1 << 7,
    SliderFlags_EnableClickSeek         = 1 << 8,
    SliderFlags_NoNav                   = 1 << 9,
    SliderFlags_TooltipNoBorder         = 1 << 10,
    SliderFlags_NoSeekOnClick           = 1 << 11, // Chỉ dùng khi có EnableClickSeek, cấm seek khi click vào track, van cho phep khi drag, điều này giúp tránh việc người dùng bấm nhầm khi chỉ muốn hover vào track để xem tooltip mà không muốn seek
    SliderFlags_DisableSeek             = 1 << 12, // Cấm seek hoàn toàn, bao gồm cả click và drag
    SliderFlags_EnableSmoothPreview     = 1 << 13, // Bật mượt cho giá trị preview (khi drag), nếu tắt thì preview sẽ nhảy thẳng đến giá trị mới mà không có animation
    SliderFlags_SliderEase              = 1 << 14,   
    SliderFlags_SliderFade              = 1 << 15, 
    SliderFlags_TooltipAnimation        =  SliderFlags_TooltipEase | SliderFlags_TooltipFade | SliderFlags_TooltipScale,
    SliderFlags_SliderpAnimation        =  SliderFlags_SliderEase | SliderFlags_SliderFade | SliderFlags_TooltipScale,
    SliderFlags_Animation               =  SliderFlags_TooltipAnimation | SliderFlags_SliderpAnimation,
    // Các cờ liên quan đến tooltip sẽ tự động tắt nếu có SliderFlags_TooltipHiden
    SliderFlags_TooltipDefault = SliderFlags_TooltipFollowMouse | SliderFlags_TooltipScale,
    SliderFlags_Default = SliderFlags_TooltipDefault | SliderFlags_EnableClickSeek ,
};
typedef uint16_t ToolTipFlags;
enum ToolTipFlags_ {
    ToolTipFlags_None                   = 0,
    ToolTipFlags_FollowMouse            = 1 << 0,
    ToolTipFlags_Ease                   = 1 << 1,
    ToolTipFlags_Fade                   = 1 << 2,
    ToolTipFlags_FollowMouse_Fixed_Y    = 1 << 3,
    ToolTipFlags_FollowMouse_Fixed_X    = 1 << 4,
    ToolTipFlags_Fixed                  = 1 << 5,
    ToolTipFlags_Hiden                  = 1 << 6,
    ToolTipFlags_AlwaysShow             = 1 << 7,
    ToolTipFlags_AlwaysShowAction       = 1 << 8,
    ToolTipFlags_Scale                  = 1 << 9,
    ToolTipFlags_NoBorder               = 1 << 10,
    ToolTipFlags_NoArrow                = 1 << 11,
    ToolTipFlags_ClampItem              = 1 << 12,
    ToolTipFlags_ClampWindow            = 1 << 13,
    ToolTipFlags_AutoPosition           = 1 << 14,
    ToolTipFlags_Animation = ToolTipFlags_Ease | ToolTipFlags_Fade | ToolTipFlags_Scale

};
enum class Phase {
    None,
    AfterInit,
    Init,
    BeforeInie,
    Draw
};

enum class Slot {
    None,
    Draw_layer0, // Below all regular content
    Draw_layer1, // Between regular content and default ImGui elements like the grab handle
    Draw_layer2, // Above all regular content
    Layout,      // Can be used to add invisible elements that affect layout (e.g. for spacing), but no actual drawing
    End

};

struct SliderState
{
    ImGuiID id; 
    ImGuiID preview_id;
    ImGuiID tooltip_id;
    ImGuiID tooltip_anim_id;
    ImGuiID slider_id;
    ImGuiID slider_anim_id;

    ImGuiContext* g;
    ImDrawList* draw_list;
    ImFont* font;
    float fontsize;

    float anim_t;
    float display_v;
    float t;
    float dt;
    float range;
    float grab_radius;
    float width;
    float height;
    float height_max;
    float v_min;
    float v_max;

    ImVec2 pos;
    ImVec2 size;
    ImVec2 mouse;
    ImVec2 grab_center;

    bool hovered;
    bool grab_hovered;
    bool bar_hovered;
    bool active;

    struct SliderPreviewValue {
    private:
        float preview_value = 0.0f;
        bool  is_previewing = false;

    public:
        // ===== READ ONLY =====
        float GetDisplayValue(float actual_v) const
        {
            return is_previewing ? preview_value : actual_v;
        }

        bool IsPreviewing() const { return is_previewing; }

        float GetPreviewValue() const { return preview_value; }

        // ===== INTERNAL UPDATE =====
        void BeginPreview(float v)
        {
            preview_value = v;
            is_previewing = true;
        }

        void UpdatePreview(float v)
        {
            preview_value = v;
        }

        void EndPreview()
        {
            is_previewing = false;
        }
    }* preview = nullptr;
};
struct SliderSeekRequest
{
    float new_value;

    bool from_click;
    bool from_drag;

    bool is_hovered;
    bool is_final;   // mouse release

};

struct SliderSeekResult
{
    bool accept = true;     // có cho phép update không
    float value = 0.0f;     // giá trị cuối cùng (có thể bị clamp/snap)
};
using OldIconFn = void(*)(ImDrawList*, ImVec2, ImVec2, ImU32);
using SliderRenderCallback = std::function<void(Phase phase , Slot slot, SliderRenderData* data, ImDrawList* draw_list)>;
using SliderTooltipCallback = std::function<void(Phase phase, Slot slot, SliderTooltipData* data, ImDrawList* draw_list)>; 
using SliderSeekCallback = std::function<SliderSeekResult(const SliderSeekRequest*)>;
using SliderNavCallback = std::function<void(float value, bool nav_left, bool nav_right)>;


namespace CSImGui{
    struct TableCol {
        const char* name;
        float width; // 0.0f là Stretch, > 0.0f là Fixed
    };
    struct CardHoleStyle
    {
        float rounding = 6.0f;
        float borderThickness = 1.5f;
    };
    struct IconButtonStyle
    {
        // Button background: Dùng Alpha thấp cho cảm giác "Glassmorphism"
        bool drawButtonBg = true;
        ImU32 buttonBgColor      = IM_COL32(255, 255, 255, 20);  // Rất mờ, chỉ đủ thấy lớp nền
        ImU32 buttonBgHovered    = IM_COL32(255, 255, 255, 45);  // Sáng nhẹ lên khi hover
        ImU32 buttonBgActive     = IM_COL32(255, 255, 255, 65);  // Đậm hơn chút khi nhấn
        float buttonRounding     = 8.0f;                         // Bo góc tròn hơn nhìn hiện đại hơn

        // Button border: Viền cực mảnh và mờ để định hình khối
        bool drawButtonBorder = true; 
        ImU32 buttonBorderColor = IM_COL32(255, 255, 255, 30);   
        float buttonBorderThickness = 1.0f;

        // Icon background
        bool drawIconBg = false;
        ImU32 iconBgColor = IM_COL32(0, 0, 0, 40);
        float iconBgRounding = 4.0f;

        // Icon border
        bool drawIconBorder = false;
        ImU32 iconBorderColor = IM_COL32(255, 255, 255, 160);
        float iconBorderThickness = 1.2f;


        // Icon: Tránh dùng màu cam/vàng gắt, dùng màu trắng/xanh nhạt sang trọng hơn
        ImU32 iconNormal   = IM_COL32(230, 230, 230, 200); // Hơi mờ ở trạng thái nghỉ
        ImU32 iconHovered  = IM_COL32(255, 255, 255, 255); // Sáng rực hoàn toàn khi hover
        ImU32 iconActive   = IM_COL32(180, 210, 255, 255); // Màu xanh nhạt nhẹ khi nhấn (cảm giác công nghệ)

        // Icon animation strength (scale)
        float iconHoverScale = 1.05f;
        float iconActiveScale = 0.95f;
    };
    
    void ApplyTheme(ThemeType themetype);
    void InitThemeLibrary(ThemeType  themetype);
    void UpdateTheme(float deltaTime);
    ImVec4& GetColors(Col idx);
    Stytle& GetStyle();
    const IconButtonStyle& GetDefaultIconButtonStyle();

    bool CustomIconButton(const char* str_id,void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),ImVec2 size,void* user_data,const IconButtonStyle& style);
    bool CustomIconButton(const char* str_id,void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),ImVec2 size);
    bool CustomIconButton(const char* str_id,OldIconFn oldFn,ImVec2 size);
    bool CustomIconButton(const char* str_id,void(*drawFn)(ImDrawList*, ImVec2, ImVec2, ImU32, void*),ImVec2 size, void* user_data);

    void InfoRow(const char* label, const char* fmt, ...);
    bool BeginInfoTable(const char* id, int column_count = 2, float first_col_width = 120.0f, ImGuiTableFlags extra_flags = 0);
    void EndInfoTable();
    bool BeginCard();
    void EndCard();
    bool BeginModernChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false, ImGuiWindowFlags extra_flags = 0);
    void EndModernChild();
    bool BeginModernTabBar(const char* id , ImGuiTabBarFlags extra_flags = 0);
    void EndModernTabBar();
    bool ModernTabItem(const char* label, bool* p_open = NULL, ImGuiTabItemFlags flags = 0, ModernTabFlags m_flags = 0);
    void EndModernTabItem();
    void PushModernWindowStyle();
    void PopModernWindowStyle();
    bool ModernButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0), bool primary = true);

    bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));

    bool ModernButtonEx(const char* label, const ImVec2& size_arg = ImVec2(0, 0));
    bool ModernCheckbox(const char* label, bool* v, CheckboxStyle style = CheckboxStyle::Tick);

    bool ModernInputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0);
    bool ModernInputTextMultiline(const char* label, std::string& str, const ImVec2& size = ImVec2(-1, 0), ImGuiInputTextFlags flags = 0);
    bool ModernInputText(const char* label, char* buf, size_t buf_size, float width, ImGuiInputTextFlags flags = 0);
    bool ModernInputText(const char* label, std::string& buffer, float width, ImGuiInputTextFlags flags = 0);

    bool ModernSelectable(const char* label, bool selected, ImGuiSelectableFlags flags = 0, const ImVec2& size_arg = ImVec2(0, 0), bool enabled = true);
    bool BeginListTable(const char* id, const std::vector<TableCol>& cols, ImGuiTableFlags extra_flags = 0);
    void EndListTable();
    bool BeginListRow(float height = 28.0f);
    void EndListRow();

    bool IsRowClicked();
    bool ModernCollapsingHeader(const char* id, ImGuiTreeNodeFlags flags = 0);

    bool ModernSearchCombo(const char* label,
        std::string& current_value,
        const std::vector<std::string>& options,
        float custom_width = 200.0f,
        int max_items_visible = 6,
        std::function<bool(std::string&)> on_validate_confirm = nullptr,
        std::function<std::string(const std::string&)> on_get_dynamic_opt = nullptr);
    
    bool NormalCombo(const char* label, std::string& current_item, const std::vector<std::string>& options, float custom_width = 200.0f, int max_items_visible = 5);

    bool BeginModernPopup(const char* name, bool* open = NULL, ImGuiWindowFlags flags = 0);

    void EndModernPopup();

    void EndModernTreeNode();
    bool ModernTreeNode(const char* id);

    bool ModernSliderFloatEx(const char* label, float* v, float v_min, float v_max, 
                                    float height = 4.0f, float grab_radius = 8.0f, 
                                    const char* format = "%.3f", float custom_width = -1.0f,
                                    SliderFlags flags = SliderFlags_None,
                                    SliderRenderCallback render_cb = nullptr,
                                    SliderTooltipCallback tooltip_cb = nullptr,
                                    SliderSeekCallback seek_cb = nullptr,
                                    SliderNavCallback nav_cb = nullptr);
    
    bool ModernSliderFloat(const char* label, float* v, float v_min, float v_max, 
                            float height = 4.0f, float grab_radius = 8.0f, 
                            const char* format = "%.3f", float custom_width = -1.0f,
                            SliderFlags flags = SliderFlags_None);
    bool ModernInputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0);
    bool ModernSmallButton(const char* label);
    bool ModernArrowButton(const char* str_id, ImGuiDir dir, ImVec2 size = ImVec2(0, 0));
    void ShowTooltipDelayed(const char* text, bool hovering, double delaySeconds, const char* id);
    bool ModernToggle(const char* str_id, bool* v, bool enabled = true, float scale = 1.0f);
    void ModernHeader(const char* title, float scale = 1.0f);
    void DrawCardWithHole(ImDrawList* dl,const ImVec2& cardMin,
        const ImVec2& cardMax,const ImVec2& holeMin,const ImVec2& holeMax,
        ImU32 fillCol,ImU32 borderCol,const CardHoleStyle& style);
    bool ToolTip(const char* label , float delay = 3.0f, ToolTipFlags flags = ToolTipFlags_None);
}

