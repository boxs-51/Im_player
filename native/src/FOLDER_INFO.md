# Metadata
- **Last Scan:** 2024-07-24
- **Source Files:** 11
- **Hash:** (Not calculated)
- **Depends On:** `native/include`, `native/imgui`, `mpv`, `SDL2`, `nlohmann/json`, `freetype`, `wintoast`
- **Scanned Files:** `FontManager.cpp`, `gl3w.c`, `globals.cpp`, `hotkey_handler.cpp`, `main1.cpp`, `notification.cpp`, `settings_manager.cpp`, `settings_manager.h`, `stb_impl.cpp`, `utils.cpp`, `wintoastlib.cpp`

# 📂 Thư Mục: `native/src`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** 
  - **Singleton:** `ConfigManager`, `FontManager`, `MPVManager`, `WindowManager` are all implemented as Singletons.
  - **Factory:** A `WindowFactory` combined with a `WindowTemplateBuilder` is used to create different types of windows (`VideoPlayerMain`, `MockSubWindow`).
  - **Facade:** `hotkey_handler.cpp` acts as a facade for various subsystems triggered by user input (Playback control, UI popups).
  - **Strategy:** The graphics backend is abstracted (`OpenGLBackend`, `D3D11Backend`), allowing different rendering strategies to be plugged into a `WindowRuntime`.
- **Decisions:**
  - **Multi-Window Architecture:** The application is built around a `WindowManager` that can manage multiple `WindowRuntime` instances, each potentially in its own thread.
  - **Single-Instance Application:** Enforced using a named Mutex (`Global\MyUniqueApp_MutexID`) and a named pipe (`\.\pipe\MyUniqueAppPipe`) to forward command-line arguments from subsequent instances to the first one.
  - **Thread-Safe Configuration:** `ConfigManager` uses a `std::shared_mutex` to protect settings data, allowing concurrent reads and exclusive writes.

## 2. Dependency & Ownership Graph
### Dependency
`main1.cpp` → `WindowManager` → `WindowRuntime` → `IGraphicsBackend`, `MPVSession`, `ImGuiContext`
`main1.cpp` → `FontManager`, `ConfigManager`, `MPVManager`, `ThreadManager`
`hotkey_handler.cpp` → `MPVSession::Commander`, `WindowManager`

### Ownership & Lifetime
- `main()` creates and owns `WindowManager`.
- `WindowManager` owns all `WindowRuntime` instances.
- `MPVManager` owns all `MPVSession` instances. WindowRuntime holds a raw pointer to its `MPVSession`.
- `FontManager` loads fonts into a shared `ImFontAtlas` owned by a `WindowSharedGroup`, which is then shared by related `WindowRuntime`s. Font data ownership is transferred to ImGui.

## 3. Thread Model & Event/Data Flow
- **Main Thread:** Runs the main application loop (`while (running)`), processes SDL events, and dispatches them to the appropriate `WindowRuntime`. It also processes window creation/destruction queues.
- **Render Thread:** Each `WindowRuntime` has its own `UIRenderThread` for handling ImGui rendering, decoupled from the main loop.
- **Pipe Server Thread:** A dedicated background thread (`PipeServerThread`) listens for connections on a named pipe to handle the single-instance logic.
- **URL Fetch Thread:** `CallThread_URLFetch` (from `utils.h`, likely defined in `threads` subdir) is used to fetch video info in a background thread.
- **Synchronization:** 
  - `std::mutex` (`g_mutex`) is used in `globals.cpp`, which is a high-risk global lock.
  - `std::shared_mutex` is used correctly within `ConfigManager` for thread-safe access to settings.
  - `WindowRuntime` uses a `std::mutex` (`stateMutex`) to protect its state when accessed from different threads.
- **Event Flow:** `SDL_PollEvent` (Main Thread) → `HandleWindowRuntimeEvent` → `runtime->resource.graphicsBackend->ProcessEvent(e)` & `HandleHotkeys(e)`. Custom events like `SDL_MPV_RENDER_UPDATE` trigger rendering flags.
- **Data Flow:**
  - **Settings:** `ConfigManager` loads from JSON → `v_Settings` / `c_Settings` structs → Accessed via `GetVideoSettings()`/`GetCommonSettings()`.
  - **Playback:** `hotkey_handler` → `MPVSession::Commander` → `mpv_command`.
  - **New URL from Pipe:** `PipeServerThread` → `OnArgumentsReceived` → `CallThread_URLFetch`.

## 4. Public APIs & Configuration
- **Entry Point:** `main(int argc, char** argv)` in `main1.cpp`.
- **Configuration:** Managed entirely by `ConfigManager`. Settings are stored in `data/settings_video.json` and `data/settings_common.json`.
- **Hotkeys:** `HandleHotkeys` is the main entry point for processing keyboard inputs.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Thread:** **[CRITICAL]** The extensive use of global variables in `globals.cpp` (e.g., `g_searchQuery`, `pendingSeekTime`) protected by a single, coarse-grained `g_mutex` is extremely dangerous and a likely source of data races and deadlocks. The logic is hard to follow and not encapsulated.
- **Memory:** `main1.cpp` involves complex object lifetimes (Windows, MPV Sessions, Renderers). While `unique_ptr` and `shared_ptr` seem to be used, a leak is possible if cleanup order is wrong, especially during shutdown.
- **Complexity:** `main1.cpp` is very long and handles too many responsibilities (initialization, main loop, event handling, state management). This makes it difficult to maintain and debug. `FontManager.cpp` also has very high complexity due to handling multiple font loading strategies and backends.
- **Exception:** `main()` has a top-level `try/catch` block for `AppException` and `std::exception`, which is good for preventing crashes, but it might hide more specific errors occurring in the application logic.

## 6. Technical Debt (TODO / FIXME / HACK)
- **Refactor `globals.cpp`:** The entire file should be eliminated. Its state should be moved into appropriate classes (e.g., `PlaybackManager`, `SearchState`) with clear ownership and thread-safe accessors. This is the highest priority technical debt.
- **Refactor `main1.cpp`:** The `main` function should be broken down. Initialization logic could be moved to a dedicated `Application` class. The main loop could be simplified by delegating more work to the `WindowManager` and other managers.
- **Refactor `utils.cpp`:** This file contains a mix of unrelated functionalities (MPV config, string utils, UI anims, pathing). These should be grouped into more cohesive modules (e.g., `PathUtils`, `UIHelpers`, `MPVConfigurator`).
- **Inconsistent Header Location:** `settings_manager.h` is located in `native/src` instead of `native/include` with other headers.
