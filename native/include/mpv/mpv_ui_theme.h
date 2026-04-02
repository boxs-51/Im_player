#include "imgui.h"
struct ThemeColors {
    ImVec4 WindowBg_ModernWindowStyle;
    ImVec4 TitleBg_ModernWindowStyle;
    ImVec4 TitleBgActive_ModernWindowStyle;
    ImVec4 Border_ModernWindowStyle;
    ImVec4 Separator_ModernWindowStyle;

    ImVec4 ChildBg_ModernChild;
    ImVec4 Border_ModernChild;
    ImVec4 Text_ModernChild;

    ImVec4 ChildBg_Card;
    ImVec4 Border_Card;
    ImVec4 Text_Card;

    ImVec4 TableRowBg_InfoTable;
    ImVec4 TableRowBgAlt_InfoTable;

    ImVec4 Text_ModernTabBar;
    ImVec4 Tab_ModernTabBar;
    ImVec4 TabHovered_ModernTabBar;
    ImVec4 TabActive_ModernTabBar;
    ImVec4 TabUnfocused_ModernTabBar;
    ImVec4 TabUnfocusedActive_ModernTabBar;

    ImVec4 Button_ModernButton;
    ImVec4 ButtonHovered_ModernButton;
    ImVec4 ButtonActive_ModernButton;
    ImVec4 Text_ModernButton;

    ImVec4 Button_SecondaryButton;
    ImVec4 ButtonHovered_SecondaryButton;
    ImVec4 ButtonActive_SecondaryButton;
    ImVec4 Border_SecondaryButton;

    ImVec4 FrameBg_ModernCheckbox;
    ImVec4 FrameBgHovered_ModernCheckbox;
    ImVec4 FrameBgActive_ModernCheckbox;
    ImVec4 CheckMark_ModernCheckbox;

    ImVec4 FrameBg_ModernInputTextMultiline;
    ImVec4 FrameBgHovered_ModernInputTextMultiline;
    ImVec4 FrameBgActive_ModernInputTextMultiline;
    ImVec4 Border_ModernInputTextMultiline;
    ImVec4 TextSelectedBg_ModernInputTextMultiline;

    ImVec4 Header_ModernSelectable;
    ImVec4 HeaderHovered_ModernSelectable;
    ImVec4 HeaderActive_ModernSelectable;
    ImVec4 ImGuiCol_Text_Selected_ModernSelectable;
    ImVec4 ImGuiCol_Text_UnSelected_ModernSelectable;

    ImVec4 TableHeaderBg_ListTable;
    ImVec4 Text_ListTable;
    ImVec4 TableRowBgAlt_ListTable;

    ImVec4 Header_ModernCollapsingHeader;
};


inline void SetDarkTheme() {
    ThemeColors GTheme;

    GTheme.WindowBg_ModernWindowStyle = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    GTheme.TitleBg_ModernWindowStyle = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    GTheme.TitleBgActive_ModernWindowStyle = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.Border_ModernWindowStyle = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.Separator_ModernWindowStyle = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);

    GTheme.ChildBg_ModernChild = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    GTheme.Border_ModernChild = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    GTheme.Text_ModernChild = ImVec4(1.0f,1.0f,1.0f,1.0f);

    GTheme.ChildBg_Card = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
    GTheme.Border_Card = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
    GTheme.Text_Card = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);

    GTheme.TableRowBg_InfoTable = ImVec4(0, 0, 0, 0);
    GTheme.TableRowBgAlt_InfoTable = ImVec4(1, 1, 1, 0.04f);

    GTheme.Text_ModernTabBar = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    GTheme.Tab_ModernTabBar = ImVec4(0, 0, 0, 0);
    GTheme.TabHovered_ModernTabBar = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
    GTheme.TabActive_ModernTabBar = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    GTheme.TabUnfocused_ModernTabBar = ImVec4(0, 0, 0, 0);
    GTheme.TabUnfocusedActive_ModernTabBar = ImVec4(0.1f, 0.45f, 0.9f, 0.7f);

    GTheme.Button_ModernButton = ImVec4(0.12f, 0.45f, 0.90f, 1.0f);
    GTheme.ButtonHovered_ModernButton = ImVec4(0.15f, 0.55f, 1.00f, 1.0f);
    GTheme.ButtonActive_ModernButton = ImVec4(0.10f, 0.35f, 0.80f, 1.0f);
    GTheme.Text_ModernButton = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);

    GTheme.Button_SecondaryButton = ImVec4(0.20f, 0.20f, 0.22f, 0.0f);
    GTheme.ButtonHovered_SecondaryButton = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
    GTheme.ButtonActive_SecondaryButton = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
    GTheme.Border_SecondaryButton = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);

    GTheme.FrameBg_ModernCheckbox = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
    GTheme.FrameBgHovered_ModernCheckbox = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    GTheme.FrameBgActive_ModernCheckbox = ImVec4(0.15f, 0.45f, 0.90f, 0.5f);
    GTheme.CheckMark_ModernCheckbox = ImVec4(0.12f, 0.45f, 0.90f, 1.0f);

    GTheme.FrameBg_ModernInputTextMultiline = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    GTheme.FrameBgHovered_ModernInputTextMultiline = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    GTheme.FrameBgActive_ModernInputTextMultiline = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    GTheme.Border_ModernInputTextMultiline = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    GTheme.TextSelectedBg_ModernInputTextMultiline = ImVec4(0.10f, 0.40f, 0.75f, 0.50f);

    GTheme.Header_ModernSelectable = ImVec4(0.12f, 0.45f, 0.90f, 0.70f);
    GTheme.HeaderHovered_ModernSelectable = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    GTheme.HeaderActive_ModernSelectable = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    GTheme.ImGuiCol_Text_Selected_ModernSelectable = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    GTheme. ImGuiCol_Text_UnSelected_ModernSelectable = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

    GTheme.TableHeaderBg_ListTable = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    GTheme.Text_ListTable = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    GTheme.TableRowBgAlt_ListTable = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    
    GTheme.Header_ModernCollapsingHeader = ImVec4(0.15f, 0.15f, 0.17f, 1.0f);
}

inline void SetLightTheme() {
    ThemeColors GTheme;

}