#pragma once

#include "thread_id.h"
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <string>

/**
 * @brief Chứa thông tin theo dõi của một luồng được đăng ký thủ công.
 */
struct RegisteredThreadInfo {
    std::thread* threadPtr = nullptr;      ///< Con trỏ tới std::thread
    std::thread::id nativeId;             ///< ID native của OS để kiểm tra chính xác
};

class ThreadManager {
public:
    ThreadManager() = default;
    ~ThreadManager() = default;

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    // --- Phương thức cho Luồng Tự Động (Detached Task) ---
    /**
     * @brief Tạo và chạy một tác vụ trên luồng riêng biệt (Detached Thread).
     * 
     * **Cách hoạt động:**
     * - Kiểm tra xem ID đã đang chạy chưa (nếu `allowDuplicate == false`).
     * - Thêm ID vào danh sách `activeIDs_`.
     * - Khởi tạo `std::thread`, tự động xóa ID khỏi `activeIDs_` khi tác vụ hoàn thành hoặc ném ngoại lệ (Exception).
     * - Tách luồng (`detach()`) để luồng tự dọn dẹp bộ nhớ OS sau khi xong.
     * 
     * **Yêu cầu:** 
     * - `task` không được là `nullptr`.
     * - `task` phải tự quản lý bộ nhớ của các biến capture bên trong nó nếu tham chiếu ra bên ngoài.
     * 
     * @param id ID của luồng (Có thể dùng enum động hoặc chuỗi tùy ý "MyTask").
     * @param task Hàm/Lambda chứa công việc cần xử lý.
     * @param allowDuplicate Cho phép chạy nhiều luồng trùng ID cùng lúc hay không (Mặc định: false).
     */
    void Run(ThreadID id, std::function<void()> task, bool allowDuplicate = false);

    // --- Phương thức cho Luồng Đăng Ký Thủ Công (Managed Thread) ---
    
    /**
     * @brief Đăng ký một std::thread đang chạy vào hệ thống quản lý.
     * @param id ID đại diện cho luồng.
     * @param thread Con trỏ tới std::thread (phải còn sống trong suốt vòng đời đăng ký).
     */
    void Register(ThreadID id, std::thread* thread);

    /**
     * @brief Hủy đăng ký luồng khỏi hệ thống.
     * @param id ID đại diện.
     */
    void Unregister(ThreadID id);

    /**
     * @brief Chờ (Join) một luồng được đăng ký kết thúc từ bên ngoài.
     * @param id ID của luồng cần join.
     * @return true Nếu join thành công.
     */
    bool JoinRegistered(ThreadID id);

    /**
     * @brief Kiểm tra xem một luồng (cho dù là Run() hay Register()) có đang chạy hay không.
     * @param id ID luồng cần kiểm tra.
     */
    bool IsRunning(ThreadID id);

    /**
     * @brief Kiểm tra riêng luồng đăng ký thủ công có đang thực sự hoạt động và joinable hay không.
     */
    bool IsRegisteredAlive(ThreadID id);

    /**
     * @brief Dọn dẹp/Chờ toàn bộ các luồng đã đăng ký kết thúc (Thường gọi khi App Shutdown).
     */
    void JoinAllRegistered();

private:
    std::mutex mutex_;
    
    // Lưu danh sách ID của các luồng chạy bởi Run()
    std::unordered_set<ThreadID> activeIDs_; 

    // Lưu thông tin các luồng đăng ký thủ công
    std::unordered_map<ThreadID, RegisteredThreadInfo> registeredThreads_; 
};

ThreadManager& GetThreadManager();