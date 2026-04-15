#pragma once
#include "wintoastlib.h"
#include "notification.h"
#include "utils.h"

#include <mpv/mpv_ui_settings.h>
#include <mpv/mpv_data.h>

#include <string>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>   // IShellLink
#include <objbase.h>

#include <iostream>
#include <string>
#include <thread>

#define PIPE_NAME "\\\\.\\pipe\\MyUniqueAppPipe"
using namespace WinToastLib;

static MPVPlaybackStatus& g_playbackStatus = GetMPVPlaybackStatus();
static DragResizeState& g_DragResizeState = GetDragResizeState();

bool CreateShortcut(const std::wstring& shortcutName,
                    const std::wstring& targetPath,
                    const std::wstring& appId)
{
    CoInitialize(nullptr);

    wchar_t startMenuPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTMENU, nullptr, 0, startMenuPath))) {
        return false;
    }

    std::wstring shortcutPath = std::wstring(startMenuPath) +
        L"\\Programs\\" + shortcutName + L".lnk";

    IShellLinkW* pShellLink = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr,
                                 CLSCTX_INPROC_SERVER,
                                 IID_IShellLinkW,
                                 (void**)&pShellLink);

    if (FAILED(hr)) return false;

    // Set đường dẫn tới exe
    pShellLink->SetPath(targetPath.c_str());

    // Set working dir
    pShellLink->SetWorkingDirectory(targetPath.substr(0, targetPath.find_last_of(L"\\")).c_str());

    // ⚠️ Quan trọng: Set AppUserModelID
    IPropertyStore* pPropStore;
    hr = pShellLink->QueryInterface(IID_IPropertyStore, (void**)&pPropStore);

    if (SUCCEEDED(hr)) {
        PROPVARIANT pv;
        InitPropVariantFromString(appId.c_str(), &pv);

        pPropStore->SetValue(PKEY_AppUserModel_ID, pv);
        pPropStore->Commit();

        PropVariantClear(&pv);
        pPropStore->Release();
    }

    // Save shortcut
    IPersistFile* pPersistFile;
    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);

    if (SUCCEEDED(hr)) {
        hr = pPersistFile->Save(shortcutPath.c_str(), TRUE);
        pPersistFile->Release();
    }

    pShellLink->Release();
    CoUninitialize();

    return SUCCEEDED(hr);
}
bool InitNotification() {
    if (!WinToast::isCompatible()) return false;

    std::wstring exePath = L"D:\\ProJecy2\\imgui_player\\bin\\Debug\\imgui_player.exe";
    
    std::wstring appId = L"MyCompany.MyPlayer.Player.1.0";

    

    CreateShortcut(
        L"My MPV Player",
        exePath,
        appId
    );

    WinToast::instance()->setAppName(L"My MPV Player");

    WinToast::instance()->setAppUserModelId(
        WinToast::configureAUMI(L"MyCompany", L"MyPlayer", L"Player", L"1.0")
    );

    if (!WinToast::instance()->initialize()) {
        return false;
    }

    return true;
}

void ShowNotification(const std::wstring& title, const std::wstring& content) {
    WinToastTemplate templ(WinToastTemplate::Text02);

    templ.setTextField(title, WinToastTemplate::FirstLine);
    templ.setTextField(content, WinToastTemplate::SecondLine);

    WinToast::instance()->showToast(templ, nullptr);
}
void NotifyMPV() {
    std::string title = g_playbackStatus.mediaTitle.empty() ? "Unknown Title" : g_playbackStatus.mediaTitle.c_str();
    bool paused = g_playbackStatus.isPaused;

    std::wstring status = paused ? L"Paused" : L"Playing";

    ShowNotification(ToWString(title), status);
}


// Hàm xử lý khi nhận được tham số mới
void OnArgumentsReceived(const std::string& args) {
    std::cout << "Received: " << args << std::endl;
    
    HWND hwnd = g_DragResizeState.hwnd_windown_main;
    if (hwnd) {
        // Kiểm tra nếu đang bị thu nhỏ (minimized)
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        } else {
            ShowWindow(hwnd, SW_SHOW);
        }
        SetForegroundWindow(hwnd);
        CallThread_URLFetch(args,true);
    }
}

// Thread lắng nghe Pipe
void PipeServerThread() {
    while (true) {
        HANDLE hPipe = CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_INBOUND, 
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, 0, 8192, 0, NULL);

        if (hPipe != INVALID_HANDLE_VALUE) {
            if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
                char buffer[1024];
                DWORD bytesRead;
                if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                    buffer[bytesRead] = '\0';
                    OnArgumentsReceived(buffer);
                }
            }
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
        }
    }
}
void SendArgsToFirstInstance(int argc, char* argv[]) {
    // Chờ pipe sẵn sàng trong tối đa 1 giây
    if (WaitNamedPipeA(PIPE_NAME, 1000)) {
        HANDLE hPipe = CreateFileA(PIPE_NAME, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) {
            std::string args;
            for (int i = 1; i < argc; ++i) {
                args += (i > 1 ? " " : "") + std::string(argv[i]);
            }
            
            DWORD bytesWritten;
            WriteFile(hPipe, args.c_str(), (DWORD)args.length(), &bytesWritten, NULL);
            CloseHandle(hPipe);
        }
    }
}