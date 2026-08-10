# Symbol Index
- **WindowRuntime**
  - Type: `Class` | Defined: `src/window/WindowRuntime.h`
  - References: `WindowFactory`, `WindowManager`, `Renderer`
  - Lifecycle: `WindowFactory::Create()` ──> `WindowRuntime::Init()` ──> `Render()` ──> `Destroy()`
- **IM_ASSERT**
  - Type: `Macro` | Defined: `vendor/imgui/imgui.h`
  - Domain: `UI Context / Assertion`