# Metadata
- **Last Scan:** 2026-07-24
- **Source Files:** 6 (approx.)
- **Hash:** [N/A for now]
- **Depends On:** `mpv/player`, `mpv/observer`
- **Scanned Files:** `PlayerManager.h`, `PlayerSession.h`, `Player.h`, `PlaybackObserver.h`, `PlaybackCommand.h`, ...

# 📂 Folder: `mpv/session`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** Singleton, Facade, Composition.
- **Decisions:** This module provides a high-level C++ abstraction over the `libmpv` C API. It mirrors the `windows` module's architecture: a central `PlayerManager` singleton owns all `PlayerSession` objects. The `PlayerSession` class acts as a Facade, simplifying interaction with the complex `libmpv` library by composing smaller, specialized components.

## 2. Dependency & Ownership Graph
### Dependency
`PlayerManager` → `PlayerSession`
`PlayerSession` → `Player`
`PlayerSession` → `PlaybackObserver`
`PlayerSession` → `libmpv` (External)

### Ownership & Lifetime
- `PlayerManager` (Singleton) **owns** all `PlayerSession` objects via `std::vector<std::unique_ptr<PlayerSession>>`.
- `PlayerSession` (Facade/Composite) **owns** its `Player`, `PlaybackObserver`, and `PlaybackCommand`.
- The lifetime of a session is managed by `PlayerManager`.

## 3. Thread Model & Event/Data Flow
- **Main Thread:** Creates and destroys `PlayerSession` objects via the `PlayerManager`.
- **MPV Thread (`libmpv`):** `libmpv` runs its own internal event loop on a separate thread.
- **Callback/Event Thread:** `PlaybackObserver` receives property change notifications and events from the `libmpv` thread. These are often handled in callbacks.
- **Synchronization:** `std::mutex` is critical when accessing data shared between the main thread and the MPV callback thread (e.g., updating UI state based on player status).
- **Data Flow:** `PlaybackObserver` → `PropertyBag` → `WindowRenderer`. Player state changes are observed and pushed into a property bag, which the UI then reads to update itself.

## 4. Public APIs & Configuration
- **APIs:** `PlayerManager::GetInstance()`, `CreateSession()`. `PlayerSession` provides a Facade with methods like `LoadFile()`, `Play()`, `Pause()`, `Seek()`.
- **Configuration:** `PlayerSession` is configured on creation, often with handles or pointers from the `IGraphicsBackend` for video rendering.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Thread:** High risk. Callbacks from `libmpv` execute on an MPV-controlled thread. Any UI updates or access to non-thread-safe data from these callbacks without proper synchronization will cause instability and crashes.
- **Memory:** `libmpv` is a C library. Care must be taken to manage the lifetime of `mpv_handle` and other C-style resources, ensuring they are destroyed correctly. The C++ wrapper helps but does not eliminate this risk.
- **Exception:** C-to-C++ boundary. Errors from `libmpv` are typically returned as error codes, not exceptions. These must be checked diligently.

## 6. Technical Debt (TODO / FIXME / HACK)
- **TODO:** The `PropertyBag` mechanism for data flow could be replaced with a more explicit observer/subscriber pattern for better type safety and clarity.
- **FIXME:** Error handling from the `libmpv` API could be more robustly translated into C++ exceptions or error types.
