# Metadata
- **Last Scan:** 2024-07-24
- **Source Files:** 4
- **Hash:** TBD
- **Depends On:** `imgui`
- **Scanned Files:** `gui.h`, `gui.cpp`, `gui_widgets.h`, `gui_widgets.cpp`

# 📂 Thư Mục: `gui`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** 
    - **Facade:** The `CSImGui` namespace acts as a Facade, providing a single, clean, high-level API for a complex underlying UI system.
    - **Static Class:** The `CSImGui` namespace functions as a static class, grouping all UI-related functionality.
    - **Callback/Delegate:** Complex widgets like `ModernSliderFloatEx` use `std::function` callbacks (`SliderRenderCallback`, `SliderSeekCallback`) to allow for extreme customization of their rendering and behavior.
- **Decisions:**
    - **Custom UI Toolkit:** Instead of using default ImGui widgets, a comprehensive, project-specific UI kit has been built on top of ImGui's low-level drawing APIs (`ImDrawList`). This creates a unique and consistent "design system".
    - **Procedural Animation & Icons:** All animated icons (Play/Pause, Volume, Settings, etc.) are drawn procedurally. This avoids dependency on external image assets, reduces binary size, and allows for dynamic, smooth animations.
    - **Sophisticated Theming Engine:** A theming system is implemented to allow runtime switching between multiple visual styles (Dark, Light, etc.) with smooth, interpolated transitions.

## 2. Dependency & Ownership Graph
### Dependency
(Every UI-displaying module, e.g., `WindowRenderer`) → `CSImGui` → `ImGui`

### Ownership & Lifetime
- This module is largely stateless. It consists of functions that draw UI based on the data passed to them.
- Widget state (e.g., animation progress) is stored within ImGui's own state management system (`ImGui::GetStateStorage()`), which is the correct idiomatic approach.
- The `ThemeLibrary` and current theme style (`dhs`) are static, with a global lifetime.

## 3. Thread Model & Event/Data Flow
- **Render Thread:** All functions in this module are designed to be called exclusively from a dedicated `UIRenderThread`, between `ImGui::NewFrame()` and `ImGui::Render()`. They are not thread-safe by design.
- **Data Flow:**
    - **Input:** Functions take application state (e.g., a `float*` for a slider, a `bool*` for a checkbox) as input.
    - **Output:** Functions return a `bool` to indicate user interaction (e.g., `true` if a button was clicked). They modify the input data directly via pointers.

## 4. Public APIs & Configuration
- **APIs:** The static methods within the `CSImGui` namespace constitute the public API.
    - **Theming:** `InitThemeLibrary()`, `ApplyTheme()`, `UpdateTheme()`.
    - **Widgets:** `ModernButton()`, `ModernCheckbox()`, `ModernSliderFloatEx()`, `ModernSearchCombo()`, `ModernTabItem()`, etc.
    - **Containers:** `BeginModernChild()`, `BeginCard()`, `BeginModernPopup()`.
- **Configuration:** The entire visual appearance is configured via the `ApplyTheme()` function and the `Stytle` struct.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Rendering:** Low risk. As it's built on ImGui, the primary risk is assertion failures from incorrect API usage (e.g., conflicting IDs, calling functions outside the NewFrame/Render loop).
- **Performance:** The complexity of custom-drawn widgets, especially those with many vertices or complex animations, could impact frame rates on the render thread if not used judiciously. The `ModernSliderFloatEx` is particularly complex.
- **Complexity:** The sheer number of custom widgets and styles makes the module large. `gui_widgets.cpp` is over 130KB, indicating very high module complexity that could be difficult for a new developer to grasp.

## 6. Technical Debt (TODO / FIXME / HACK)
- **Documentation:** The extensive custom API would significantly benefit from detailed comments or a dedicated "demo" window within the application to showcase all widgets and their features.
- **Maintainability:** The large size of `gui_widgets.cpp` suggests it could be broken down further into smaller files, perhaps grouping related widgets (e.g., `gui_sliders.cpp`, `gui_combos.cpp`).
- **Low-level Tricks:** The code uses `ImGui::GetStateStorage()` and direct `ImDrawList` calls extensively. While powerful, this is a low-level approach that can be brittle and may break with future ImGui updates if the internal API changes.
