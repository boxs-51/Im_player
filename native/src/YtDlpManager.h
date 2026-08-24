#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <array>
#include <memory>
#include <chrono>
#include <future>
#include <mutex>
#include <atomic>

namespace fs = std::filesystem;

// Trạng thái vận hành dùng cho UI/UX
enum class YtDlpStatus {
    Unchecked,      // Chưa kiểm tra
    Searching,      // Đang tìm file thực thi
    NotFound,       // Không tìm thấy yt-dlp.exe
    Ready,          // Đã sẵn sàng hoạt động
    Updating,       // Đang trong quá trình cập nhật
    Executing,      // Đang chạy câu lệnh yt-dlp
    Error           // Gặp lỗi thao tác
};

// Cấu trúc dữ liệu phản hồi phục vụ hiển thị UI
struct YtDlpResult {
    bool success = false;
    YtDlpStatus status = YtDlpStatus::Unchecked;
    std::string version = "Unknown";
    std::string path = "";
    std::string output = "";
    std::string error_msg = "";
};

class YtDlpManager {
public:
    static YtDlpManager& GetInstance() {
        static YtDlpManager instance;
        return instance;
    }

    // Prevents copy/move
    YtDlpManager(const YtDlpManager&) = delete;
    YtDlpManager& operator=(const YtDlpManager&) = delete;

    // --- API DÀNH CHO UI / SYSTEM ---

    // 1. Kiểm tra khởi tạo & Cập nhật tự động (Async - không gây freeze UI)
    void InitOrUpdateAsync(bool force_update = false) {
        if (m_is_busy) return;
        m_is_busy = true;
        m_status = YtDlpStatus::Searching;

        std::thread([this, force_update]() {
            YtDlpResult res;
            res.path = FindExecutable();

            if (res.path.empty()) {
                SetState(YtDlpStatus::NotFound, "Không tìm thấy yt-dlp.exe trong ứng dụng hoặc PATH hệ thống.");
                m_is_busy = false;
                return;
            }

            res.version = FetchVersion(res.path);
            m_current_path = res.path;
            m_current_version = res.version;

            // Kiểm tra cập nhật nếu ép buộc hoặc kiểm tra định kỳ lần đầu mở app
            if (force_update || ShouldAutoUpdate()) {
                SetState(YtDlpStatus::Updating, "Đang tiến hành kiểm tra & cập nhật yt-dlp...");
                std::string update_out = ExecCmd("\"" + res.path + "\" -U");

                if (update_out.find("Updated") != std::string::npos || 
                    update_out.find("up to date") != std::string::npos) {
                    m_current_version = FetchVersion(res.path);
                    m_last_update_check = std::chrono::steady_clock::now();
                    SetState(YtDlpStatus::Ready, "Cập nhật thành công! Bản mới nhất: " + m_current_version, update_out);
                } else {
                    SetState(YtDlpStatus::Error, "Lỗi cập nhật yt-dlp.", update_out);
                }
            } else {
                SetState(YtDlpStatus::Ready, "Sẵn sàng hoạt động (Version: " + m_current_version + ")");
            }

            m_is_busy = false;
        }).detach();
    }

    // 2. Thực thi lệnh yt-dlp tùy chỉnh công khai (Async - Trả về std::future cho UI hứng)
    std::future<YtDlpResult> ExecuteAsync(const std::string& args) {
        return std::async(std::launch::async, [this, args]() {
            YtDlpResult res;
            if (m_current_path.empty()) {
                res.status = YtDlpStatus::NotFound;
                res.error_msg = "Chưa phát hiện yt-dlp.exe!";
                return res;
            }

            m_is_busy = true;
            m_status = YtDlpStatus::Executing;

            std::string full_cmd = "\"" + m_current_path + "\" " + args;
            res.output = ExecCmd(full_cmd.c_str());
            res.path = m_current_path;
            res.version = m_current_version;
            
            // Xử lý kết quả lệnh
            if (!res.output.empty() && res.output.find("ERROR:") == std::string::npos) {
                res.success = true;
                res.status = YtDlpStatus::Ready;
            } else {
                res.success = false;
                res.status = YtDlpStatus::Error;
                res.error_msg = "Lỗi khi thực thi câu lệnh yt-dlp.";
            }

            m_status = YtDlpStatus::Ready;
            m_is_busy = false;
            return res;
        });
    }

    // --- API TRUY VẤN TRẠNG THÁI HIỂN THỊ UI ---

    YtDlpStatus GetStatus() const { return m_status; }
    std::string GetStatusMessage() const { std::lock_guard<std::mutex> lock(m_mutex); return m_last_message; }
    std::string GetLastOutput() const { std::lock_guard<std::mutex> lock(m_mutex); return m_last_output; }
    std::string GetVersion() const { return m_current_version; }
    std::string GetPath() const { return m_current_path; }
    bool IsBusy() const { return m_is_busy; }

private:
    YtDlpManager() = default;

    std::atomic<YtDlpStatus> m_status{ YtDlpStatus::Unchecked };
    std::atomic<bool> m_is_busy{ false };
    
    std::string m_current_path = "";
    std::string m_current_version = "Unknown";
    std::string m_last_message = "";
    std::string m_last_output = "";

    mutable std::mutex m_mutex;
    std::chrono::steady_clock::time_point m_last_update_check{};
    const int UPDATE_INTERVAL_HOURS = 24; // Tự động kiểm tra sau mỗi 24 tiếng

    bool ShouldAutoUpdate() const {
        if (m_last_update_check.time_since_epoch().count() == 0) return true;
        auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
            std::chrono::steady_clock::now() - m_last_update_check).count();
        return elapsed >= UPDATE_INTERVAL_HOURS;
    }

    void SetState(YtDlpStatus status, const std::string& msg, const std::string& output = "") {
        m_status = status;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_message = msg;
        if (!output.empty()) m_last_output = output;
    }

    std::string FindExecutable() {
        std::vector<fs::path> search_paths = {
            fs::current_path() / "yt-dlp.exe",
            fs::current_path() / "tools" / "yt-dlp.exe",
            fs::current_path() / "bin" / "yt-dlp.exe"
        };

        for (const auto& path : search_paths) {
            if (fs::exists(path)) return path.string();
        }

        if (ExecCmd("yt-dlp --version").find("not recognized") == std::string::npos) {
            return "yt-dlp";
        }

        return "";
    }

    std::string FetchVersion(const std::string& path) {
        std::string ver = ExecCmd("\"" + path + "\" --version");
        ver.erase(ver.find_last_not_of(" \n\r\t") + 1);
        return ver.empty() ? "Unknown" : ver;
    }

    static std::string ExecCmd(const std::string& cmd) {
        std::array<char, 512> buffer;
        std::string result;
        #if defined(_WIN32)
            std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
        #else
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        #endif
        if (!pipe) return "";
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }
};