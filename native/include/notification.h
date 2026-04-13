#include <string>

bool InitNotification();
void ShowNotification(const std::wstring& title, const std::wstring& content);

bool CreateShortcut(const std::wstring& shortcutName ,
                    const std::wstring& targetPath,
                    const std::wstring& appId);

void NotifyMPV() ;

void SendArgsToFirstInstance(int argc, char* argv[]);
// Thread lắng nghe Pipe
void PipeServerThread();