# Project Architecture Overview

## 1. High-Level Design

The project is a sophisticated, multi-window C++ media player built with a highly decoupled and well-defined architecture. It leverages several key design patterns to manage complexity, ensure thread safety, and maintain flexibility.

The core philosophy is **Composition over Inheritance**. Large, monolithic classes are avoided in favor of small, single-responsibility components that are aggregated into composite "runtime" objects.

## 2. Core Architectural Patterns

- **Singleton Managers**: Global systems are managed by thread-safe singletons, providing centralized access and control.
  - `WindowManager`: Manages the lifecycle of all `WindowRuntime` objects. It uses a thread-safe queue to handle window creation requests from any thread.
  - `MPVManager`: Mirrors the `WindowManager`'s design, managing the lifecycle of all `MPVSession` (media playback) instances.
  - `ConfigManager`: Handles loading, saving, and accessing application settings.

- **Composition**: The main runtime objects are containers for smaller, specialized components.
  - `WindowRuntime`: Represents a single window. It doesn't contain logic itself but owns and aggregates the window's state, style, properties, and its dedicated `WindowRenderer` and `WindowController`.
  - `MPVSession`: A composite object that bundles all functionality related to a single media playback instance, owning sub-components for handling properties, commands, and events.

- **Factory & Builder Pattern**: Window creation is a decoupled, two-step process.
  - `WindowTemplateBuilder`: A fluent interface is used to define the configuration for a *type* of window (e.g., "main_window", "playlist_window").
  - `WindowFactory`: Takes a template and constructs a fully-formed `WindowRuntime` object, abstracting the complex instantiation logic from the `WindowManager`.

- **Strategy Pattern (Graphics Abstraction)**: The rendering pipeline is completely decoupled from any specific graphics API (like OpenGL or D3D11).
  - `IGraphicsBackend`: This abstract interface defines a contract for all rendering operations (context creation, frame buffer management, MPV rendering integration).
  - Concrete implementations (`OpenGLBackend`, `D3D11Backend`) are created and injected into each window's renderer, making the rest of the application agnostic to the underlying graphics technology. This is the crucial pattern that allows SDL, ImGui, and `libmpv` to render harmoniously.

- **Facade Pattern (MPV Integration)**: The complexity of the low-level `libmpv` C API is hidden behind a clean, object-oriented C++ facade.
  - `MPVSession`: This class provides a high-level interface for controlling playback, observing properties, and dispatching commands, simplifying integration with the rest of the application.

- **Multi-Threaded Rendering**: The UI remains responsive by offloading rendering to dedicated threads.
  - The main application thread is responsible for handling SDL events and application logic.
  - When a window needs to be redrawn, it calls `RequestRender()` on its `UIRenderThread`.
  - The `UIRenderThread` wakes up, makes the window's graphics context current, executes the rendering commands, and swaps the buffers. This prevents rendering from blocking the main event loop.

## 3. Data & Event Flow

1.  **Initialization**: The `main()` function in `main1.cpp` starts SDL and initializes the core singleton managers (`WindowManager`, `MPVManager`, `ConfigManager`).
2.  **Window Creation**: `WindowTemplateBuilder` is used to define window blueprints. The `WindowManager` is then requested to create a window, which it delegates to the `WindowFactory`. The factory produces a `WindowRuntime` and spawns a dedicated `UIRenderThread` for it.
3.  **Event Handling**: The main thread runs a standard `SDL_Event` loop. Events are passed to the `WindowManager`, which identifies the target window and forwards the event to its `WindowController`.
4.  **UI & Video Rendering**:
    - The `WindowController` and other logic trigger a render request on the `UIRenderThread`.
    - On the render thread, the `WindowRenderer` uses the custom `CSImGui` library to build the UI.
    - Video rendering is integrated: the `IGraphicsBackend` provides `libmpv` with the necessary handles and framebuffer objects to render video frames directly into an ImGui texture.
    - The final composite image (UI + video) is drawn to the screen.

This architecture creates a robust, scalable, and maintainable foundation for a complex media application.
