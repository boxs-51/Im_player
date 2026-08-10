# Metadata
- **Last Scan:** 2024-07-24
- **Source Files:** 5
- **Hash:** TBD
- **Depends On:** `log`, `nlohmann`, `cpr`
- **Scanned Files:** `api_manager.cpp`, `api_manager.h`, `api_types.h`, `api_log.h`, `provider/gemini.h`

# 📂 Thư Mục: `api`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** 
    - **Singleton:** `APIManager` is a singleton providing a global access point to the API management system.
    - **Strategy:** The `IAPIProvider` interface and its concrete implementations (`GeminiProvider`) follow the Strategy pattern. This allows the system to be configured with different API providers.
    - **Producer-Consumer (Thread-safe Queue):** `APIManager` uses a worker thread and a thread-safe queue to process API requests asynchronously. The main thread is the producer, enqueuing tasks, while the worker thread is the consumer, executing requests.
- **Decisions:**
    - API requests are handled in a separate worker thread to avoid blocking the main UI thread.
    - API keys are loaded from an external configuration file for better security and flexibility.
    - A state machine (`ProviderState`) is used to manage the lifecycle of each API provider (e.g., Active, Paused).

## 2. Dependency & Ownership Graph
### Dependency
`APIManager` → `IAPIProvider` → (Concrete Provider, e.g., `GeminiProvider`) → (External libraries like `cpr` for HTTP requests - *currently mocked*).

### Ownership & Lifetime
- `APIManager` **owns** a map of `std::shared_ptr<IAPIProvider>`, managing the lifetime of the provider instances.
- The `APIManager`'s lifetime is tied to the application's lifetime (as a singleton).
- The worker thread `m_workerThread` is owned by the `APIManager` and is safely joined upon shutdown.

## 3. Thread Model & Event/Data Flow
- **Main Thread:** Enqueues API tasks via `APIManager::EnqueueTask()`.
- **Worker Thread (`m_workerThread`):**
    - Sleeps until a task is available in the queue (using `std::condition_variable`).
    - Dequeues and processes API tasks by calling `ExecuteRequest` on the appropriate provider if its state is `Active`.
- **Synchronization:** `std::mutex` (`m_queueMutex`) protects the task queue, and `std::condition_variable` (`m_cv`) is used for efficient thread synchronization.
- **Data Flow:** `string payload` → `APITask` → `m_taskQueue` → `WorkerLoop` → `IAPIProvider::ExecuteRequest` → `string response`.

## 4. Public APIs & Configuration
- **APIs:**
    - `APIManager::Instance()`: Access the singleton instance.
    - `APIManager::InitConfigs(const std::string& configFilePath)`: Load API keys and initialize providers.
    - `APIManager::RegisterProvider(...)`: Add a new API provider.
    - `APIManager::SetProviderState(...)`: Change the state of a provider.
    - `APIManager::EnqueueTask(...)`: Add a new API request to the queue.
    - `APIManager::Shutdown()`: Gracefully stops the worker thread.
- **Configuration:**
    - API keys are configured in an external file. The manager reads this file to initialize providers.
    - The state of each provider (`Active`/`Paused`) can be configured at runtime via the UI.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Thread:** 
    - **Data Race:** The current implementation using `std::lock_guard` for `m_providers` and `m_taskQueue` is safe. The risk is low.
    - **Deadlock:** Unlikely with the current simple lock mechanism.
- **Exception:** 
    - `ExecuteRequest` is called outside of a try-catch block in the `WorkerLoop`. An uncaught exception within a provider (e.g., a network error) would terminate the worker thread, halting all future API processing.
- **Security:**
    - API keys are loaded into memory. The code has a comment suggesting to clear them from RAM after use, which is a good practice that should be implemented.

## 6. Technical Debt (TODO / FIXME / HACK)
- **HACK:** The `GeminiProvider::ExecuteRequest` is currently a mock. It uses `std::this_thread::sleep_for` and returns a hardcoded string. A real implementation using a library like `cpr` is needed.
- **TODO:** The code mentions `GPTProvider` and `YouTubeProvider`, but they are not implemented.
- **TODO:** The response from the API is just printed to `std::cout`. A proper mechanism to send the response back to the caller (e.g., via a callback, future, or another queue) is needed for the data to be useful.
- **CONSIDER:** The `WorkerLoop` could be enhanced to handle task prioritization or cancellation.
- **CONSIDER:** A mechanism to report errors from the worker thread back to the main thread would make the system more robust.
