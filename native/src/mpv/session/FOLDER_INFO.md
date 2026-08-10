# Metadata
- **Last Scan:** 2026-07-24
- **Source Files:** 6 (approx.)
- **Hash:** [N/A for now]
- **Depends On:** `mpv/player`, `mpv/observer`
- **Scanned Files:** `MPVManager.h`, `MPVSession.h`, `MPVPlayer.h`, `MPVObserver.h`, `MPVCommandDispatcher.h`, ...

# 📂 Folder: `mpv/session`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** Singleton, Facade, Composition.
- **Decisions:** This module provides a high-level C++ abstraction over the `libmpv` C API. It mirrors the `windows` module's architecture: a central `MPVManager` singleton owns all `MPVSession` objects. The `MPVSession` class acts as a Facade, simplifying interaction with the complex `libmpv` library by composing smaller, specialized components.

## 2. Dependency & Ownership Graph
### Dependency
`MPVManager` → `MPVSession`
`MPVSession` → `MPVPlayer`
`MPVSession` → `MPVObserver`
`MPVSession` → `libmpv` (External)

### Ownership & Lifetime
- `MPVManager` (Singleton) **owns** all `MPVSession` objects via `std::vector<std::unique_ptr<MPVSession>>`.
- `MPVSession` (Facade/Composite) **owns** its `MPVPlayer`, `MPVObserver`, and `MPVCommandDispatcher`.
- The lifetime of a session is managed by `MPVManager`.

## 3. Thread Model & Event/Data Flow
- **Main Thread:** Creates and destroys `MPVSession` objects via the `MPVManager`.
- **MPV Thread (`libmpv`):** `libmpv` runs its own internal event loop on a separate thread.
- **Callback/Event Thread:** `MPVObserver` receives property change notifications and events from the `libmpv` thread. These are often handled in callbacks.
- **Synchronization:** `std::mutex` is critical when accessing data shared between the main thread and the MPV callback thread (e.g., updating UI state based on player status).
- **Data Flow:** `MPVObserver` → `PropertyBag` → `WindowRenderer`. Player state changes are observed and pushed into a property bag, which the UI then reads to update itself.

## 4. Public APIs & Configuration
- **APIs:** `MPVManager::GetInstance()`, `CreateSession()`. `MPVSession` provides a Facade with methods like `LoadFile()`, `Play()`, `Pause()`, `Seek()`.
- **Configuration:** `MPVSession` is configured on creation, often with handles or pointers from the `IGraphicsBackend` for video rendering.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Thread:** High risk. Callbacks from `libmpv` execute on an MPV-controlled thread. Any UI updates or access to non-thread-safe data from these callbacks without proper synchronization will cause instability and crashes.
- **Memory:** `libmpv` is a C library. Care must be taken to manage the lifetime of `mpv_handle` and other C-style resources, ensuring they are destroyed correctly. The C++ wrapper helps but does not eliminate this risk.
- **Exception:** C-to-C++ boundary. Errors from `libmpv` are typically returned as error codes, not exceptions. These must be checked diligently.

## 6. Technical Debt (TODO / FIXME / HACK)
- **TODO:** The `PropertyBag` mechanism for data flow could be replaced with a more explicit observer/subscriber pattern for better type safety and clarity.
- **FIXME:** Error handling from the `libmpv` API could be more robustly translated into C++ exceptions or error types.
