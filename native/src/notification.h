#include <string>

class PlayerStateSystem;

bool InitNotification();
void ShowNotification(const std::wstring& title, const std::wstring& content);

bool CreateShortcut(const std::wstring& shortcutName ,
                    const std::wstring& targetPath,
                    const std::wstring& appId);

// Consume a thread-safe playback snapshot; do not guess an unknown state.
void NotifyMPV(const PlayerStateSystem& state);

void SendArgsToFirstInstance(int argc, char* argv[]);
// Thread lắng nghe Pipe
void PipeServerThread();