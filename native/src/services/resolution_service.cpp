#include "resolution_service.h"
#include <winhttp.h>
#include <vector>

bool IsResolutionServiceRunning() {
    HINTERNET hSession = WinHttpOpen(L"ResolutionCheck/1.0", 
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 5005, 0);
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

void LaunchResolutionService() {
    STARTUPINFOW si = { sizeof(si) };
    ZeroMemory(&services.g_ResolutionServiceProcess, sizeof(services.g_ResolutionServiceProcess));

    std::wstring cmd = L"py src\\service\\resolution_service.py";
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');  // null terminate

    if (!CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL, NULL, FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &services.g_ResolutionServiceProcess))
    {
        // log lỗi GetLastError() nếu muốn
        return;
    }

    // **Không đóng handle ở đây** để giữ tiến trình sống và quản lý được
}




void StopResolutionService() {
    std::lock_guard<std::mutex> lock(services.g_serviceMutex);
    if (services.g_ResolutionServiceProcess.hProcess) {
        TerminateProcess(services.g_ResolutionServiceProcess.hProcess, 0);
        CloseHandle(services.g_ResolutionServiceProcess.hProcess);
        CloseHandle(services.g_ResolutionServiceProcess.hThread);
        services.g_ResolutionServiceProcess = {0};

        services.g_ResolutionServiceRunning.store(false);  // Reset trạng thái service
        services.g_ServiceStarted.store(false);            // Reset trạng thái sẵn sàng

        services.g_serviceCv.notify_all();
    }
}


void EnsureResolutionServiceRunning() {
    bool expected = false;
    if (services.g_ResolutionServiceRunning.compare_exchange_strong(expected, true)) {
        // Nếu biến từ false chuyển thành true tức là ta đang chạy service lần đầu
        if (!IsResolutionServiceRunning()) {
            LaunchResolutionService();

            constexpr int maxWaitMs = 5000;
            constexpr int waitStepMs = 100;
            int waited = 0;

            while (waited < maxWaitMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(waitStepMs));
                waited += waitStepMs;
                if (IsResolutionServiceRunning()) {
                    break;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(services.g_serviceMutex);
            services.g_ServiceStarted.store(true);
        }
        services.g_serviceCv.notify_all();
    } else {
        // Nếu service đang chạy hoặc đã chạy rồi, chờ cho nó sẵn sàng
        std::unique_lock<std::mutex> lock(services.g_serviceMutex);
        services.g_serviceCv.wait(lock, []() { return services.g_ServiceStarted.load(); });
    }
}
