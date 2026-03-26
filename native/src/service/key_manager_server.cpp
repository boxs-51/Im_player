#include "globals.h"
#include "key_manager.h"
bool IsKeyManagerRunning() {
    HINTERNET hSession = WinHttpOpen(L"KeyManagerCheck/1.0", 
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 6000, 0); // port Key Manager
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/status",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    bool result = false;

    if (hRequest) {
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            if (WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD statusCode = 0;
                DWORD size = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    NULL, &statusCode, &size, NULL);
                result = (statusCode == 200);
            }
        }
        WinHttpCloseHandle(hRequest);
    }

    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

void LaunchKeyManagerService() {
    STARTUPINFOW si = { sizeof(si) };
    ZeroMemory(&g_KeyServiceProcess, sizeof(g_KeyServiceProcess));

    std::wstring cmd = L"py src\\service\\key_manager_server.py"; // chỉnh đường dẫn
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');

    if (!CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL, NULL, FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &g_KeyServiceProcess
    )) {
        std::cerr << "Failed to launch Key Manager process\n";
    }
}

void StopKeyManagerService() {
    std::lock_guard<std::mutex> lock(g_keyServiceMutex);
    if (g_KeyServiceProcess.hProcess) {
        TerminateProcess(g_KeyServiceProcess.hProcess, 0);
        CloseHandle(g_KeyServiceProcess.hProcess);
        CloseHandle(g_KeyServiceProcess.hThread);
        g_KeyServiceProcess = {0};
        g_KeyServiceStarted.store(false);
        g_keyServiceCv.notify_all();
    }
}

void EnsureKeyManagerServiceRunning() {
    bool expected = false;
    if (g_KeyServiceStarted.compare_exchange_strong(expected, true)) {
        if (!IsKeyManagerRunning()) {
            LaunchKeyManagerService();

            constexpr int maxWaitMs = 5000;
            constexpr int waitStepMs = 100;
            int waited = 0;

            while (waited < maxWaitMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitStepMs));
                waited += waitStepMs;
                if (IsKeyManagerRunning()) break;
            }
        }

        std::lock_guard<std::mutex> lock(g_keyServiceMutex);
        g_KeyServiceStarted.store(true);
        g_keyServiceCv.notify_all();
    } else {
        std::unique_lock<std::mutex> lock(g_keyServiceMutex);
        g_keyServiceCv.wait(lock, []() { return g_KeyServiceStarted.load(); });
    }
}
