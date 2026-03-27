
bool InitMainWindow();
void Cleanup();
void ShutdownMainWindow();

void Render(PlaybackState state);

void HandleMainWindowEvent(const SDL_Event& e);
void HandleSidebarWindowEvent(const SDL_Event& e);
void ManagerRender ();
