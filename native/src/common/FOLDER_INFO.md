# Metadata
- **Last Scan:** 2024-07-24
- **Source Files:** 1
- **Hash:** TBD
- **Depends On:** `<stdexcept>`, `<string>`
- **Scanned Files:** `Exception.h`

# 📂 Thư Mục: `common`

## 1. Architecture Decisions & Design Patterns
- **Patterns:** N/A (Foundational Utility)
- **Decisions:** A custom exception class, `AppException`, is used throughout the application to provide enhanced debugging information. This is a deliberate choice to improve error diagnostics over using standard exceptions alone.

## 2. Dependency & Ownership Graph
- **Dependency:** Any module that uses `THROW_APP_EXCEPTION` depends on this header.
- **Ownership:** `AppException` objects are typically created on the stack and thrown by value. Their lifetime is managed by the C++ exception handling mechanism.

## 3. Thread Model & Event/Data Flow
- **Thread Model:** This module is thread-agnostic and thread-safe. Exception objects are typically confined to the thread in which they are thrown.
- **Data Flow:** `THROW_APP_EXCEPTION(message)` → `AppException` object created → stack unwinding → `catch (const AppException& e)` → `e.what()` provides detailed error string.

## 4. Public APIs & Configuration
- **APIs:**
    - `class AppException`: The custom exception class.
    - `THROW_APP_EXCEPTION(msg)`: A macro to instantiate and throw an `AppException` with automatic file, line, and function context.

## 5. Risk Matrix & Error-Prone Areas (Classified)
- **Risk:** Low. This is a standard and robust utility. The only risk is developers forgetting to catch `AppException` at appropriate boundaries, leading to uncaught exception crashes, but `main.cpp` already has a top-level catcher.

## 6. Technical Debt (TODO / FIXME / HACK)
- None. This module is clean and serves its purpose well.
