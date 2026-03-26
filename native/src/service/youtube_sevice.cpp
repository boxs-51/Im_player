#include "youtube_sevice.h"
#include "globals.h"
bool IsYouTubeServiceRunning() {
    HINTERNET hSession = WinHttpOpen(L"YouTubeService/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 8000, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/ping",
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

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
void LaunchYouTubeService() {
    STARTUPINFOW si = { sizeof(si) };
    ZeroMemory(&g_YouTubeServiceProcess, sizeof(g_YouTubeServiceProcess));

    std::wstring cmd = L"py native\\src\\service\\youtube_service\\app.py";
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0'); // null terminate

    if (!CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL, NULL, FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &g_YouTubeServiceProcess))
    {
        // log lỗi GetLastError() nếu muốn
        return;
    }
}
void EnsureYouTubeServiceRunning() {
    bool expected = false;
    if (g_YouTubeServiceRunning.compare_exchange_strong(expected, true)) {
        if (!IsYouTubeServiceRunning()) {
            LaunchYouTubeService();

            constexpr int maxWaitMs = 5000;
            constexpr int waitStepMs = 100;
            int waited = 0;

            while (waited < maxWaitMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitStepMs));
                waited += waitStepMs;
                if (IsYouTubeServiceRunning()) break;
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_youtubeServiceMutex);
            g_YouTubeServiceStarted.store(true);
        }
        g_youtubeServiceCv.notify_all();
    } else {
        std::unique_lock<std::mutex> lock(g_youtubeServiceMutex);
        g_youtubeServiceCv.wait(lock, []() { return g_YouTubeServiceStarted.load(); });
    }
}

void StopYouTubeService() {
    std::lock_guard<std::mutex> lock(g_youtubeServiceMutex);

    if (g_YouTubeServiceProcess.hProcess) {
        // Dừng tiến trình Python service
        TerminateProcess(g_YouTubeServiceProcess.hProcess, 0);

        CloseHandle(g_YouTubeServiceProcess.hProcess);
        CloseHandle(g_YouTubeServiceProcess.hThread);

        g_YouTubeServiceProcess = {0};

        // Reset trạng thái
        g_YouTubeServiceRunning.store(false);
        g_YouTubeServiceStarted.store(false);

        g_youtubeServiceCv.notify_all();
    }
}