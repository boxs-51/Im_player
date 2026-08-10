# Metadata
- **Last Scan:** 2026-07-24
- **Source Files:** 10 (approx.)
- **Hash:** [N/A for now]
- **Depends On:** `backends`, `gui`, `common`
- **Scanned Files:** `WindowManager.h`, `WindowRuntime.h`, `WindowFactory.h`, `WindowTemplateBuilder.h`, `UIRenderThread.h`, ...

# 📂 Folder: `windows`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** Singleton, Factory, Builder, Composition.
- **Decisions:** This module is the heart of the windowing system. A central `WindowManager` singleton owns all `WindowRuntime` objects. The Factory and Builder patterns are used to decouple the complex process of window creation and configuration. Each window runs its rendering on a separate `UIRenderThread` to keep the main application responsive.

## 2. Dependency & Ownership Graph
### Dependency
`WindowManager` → `WindowFactory` → `WindowRuntime` → `UIRenderThread`
`WindowRuntime` → `IGraphicsBackend` (Strategy)
`WindowRuntime` → `IWindowRenderer` / `IWindowController`

### Ownership & Lifetime
- `WindowManager` (Singleton) **owns** all `WindowRuntime` objects via `std::vector<std::unique_ptr<WindowRuntime>>`.
- `WindowRuntime` (Composite) **owns** its `WindowRenderer`, `WindowController`, and `PropertyBag`.
- The lifetime of a window is strictly managed by `WindowManager::QueueCreateWindow` and `WindowManager::DestroyWindow`.

## 3. Thread Model & Event/Data Flow
- **Main Thread:** Interacts with `WindowManager` to create/destroy windows and push SDL events.
- **Render Thread (`UIRenderThread`):** Each window has a dedicated thread for rendering. It is woken up by `RequestRender()` from the main thread. This thread owns the graphics context.
- **Synchronization:** `std::mutex` and `std::condition_variable` are used to signal render requests to the `UIRenderThread`. A thread-safe queue in `WindowManager` handles creation requests.
- **Event Flow:** `SDL_Event` → `WindowManager` → `WindowController`.
- **Data Flow:** `PropertyBag` is used for window-specific state.

## 4. Public APIs & Configuration
- **APIs:** `WindowManager::GetInstance()`, `QueueCreateWindow()`, `DestroyWindow()`, `PushEvent()`. `WindowTemplateBuilder` provides a fluent API for configuration.
- **Configuration:** Window appearance and behavior are defined via `WindowTemplate` and `WindowStyle` objects.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Thread:** High risk. Incorrect synchronization between the main thread and render threads could lead to data races or deadlocks. Accessing graphics contexts from the wrong thread will cause crashes.
- **Memory:** `std::unique_ptr` mitigates many ownership risks, but dangling pointers are still possible if objects are accessed after the owning `WindowRuntime` is destroyed.
- **Complexity:** The interaction between the factory, builder, runtime, and render thread is complex and can be difficult to debug.

## 6. Technical Debt (TODO / FIXME / HACK)
- **TODO:** Event handling could be moved to a more robust queue system within each `WindowRuntime` instead of direct function calls.
- **FIXME:** Need to ensure all resources held by `WindowRuntime` (especially graphics resources) are properly released on the correct thread during destruction.
