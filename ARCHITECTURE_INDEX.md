# Architecture Index

## Core Managers (Singletons)
- **`WindowManager`**: `native/src/windows/WindowManager.h`
  - Manages the lifecycle of all application windows.
  - Thread-safe queue for window creation.
- **`MPVManager`**: `native/src/mpv/session/MPVManager.h`
  - Manages the lifecycle of all media playback sessions (`MPVSession`).
- **`ConfigManager`**: (Implied from `main1.cpp`)
  - Manages application configuration.

## Core Runtime Objects (Composition)
- **`WindowRuntime`**: `native/src/windows/WindowRuntime.h`
  - Composite object representing a single window.
  - Owns `WindowRenderer`, `WindowController`, `PropertyBag`.
- **`MPVSession`**: `native/src/mpv/session/MPVSession.h`
  - Facade for a `libmpv` instance.
  - Owns `MPVPlayer`, `MPVObserver`, `MPVCommandDispatcher`.

## Design Patterns
- **Factory/Builder**: `native/src/windows/WindowFactory.h`
  - Creates `WindowRuntime` instances from `WindowTemplate`s.
  - `WindowTemplateBuilder` provides a fluent API for defining templates.
- **Strategy (Graphics)**: `native/src/backends/IGraphicsBackend.h`
  - Interface for rendering backends (OpenGL, D3D11).
  - Decouples rendering code from specific graphics APIs.
- **Facade (MPV)**: `native/src/mpv/session/MPVSession.h`
  - Provides a clean C++ interface to the `libmpv` C API.

## UI & Rendering
- **`CSImGui`**: `native/src/gui/gui.h`
  - Custom UI component library built on ImGui.
- **`UIRenderThread`**: `native/src/windows/UIRenderThread.h`
  - Per-window render thread.
  - Keeps the main UI thread responsive.

## Application Entry
- **`main()`**: `native/src/main1.cpp`
  - Initializes all systems and runs the main SDL event loop.
