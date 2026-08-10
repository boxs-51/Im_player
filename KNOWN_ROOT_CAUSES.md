# Known Root Cause Patterns
- **Pattern:** `IM_ASSERT` failure at `ImGui::NewFrame()`
  - **Root Cause:** Wrong/Unbound GL Context before ImGui frame start, or multi-threaded UI call without mutex.
  - **Fix Strategy:** Verify context binding on render thread (`SDL_GL_MakeCurrent`).