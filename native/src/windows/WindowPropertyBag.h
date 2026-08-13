// WindowPropertyBag.h
#pragma once
#include <string>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <vector>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <initializer_list>
#include <mutex> // Thêm thư viện mutex

/**
 * @brief Lớp PropertyBag lưu trữ linh hoạt theo cơ chế Key-Value linh hoạt (Type-safe).
 */
class PropertyBag {
private:
    mutable std::recursive_mutex m_mutex; // Sử dụng recursive_mutex để tránh deadlock khi gọi lồng
    std::unordered_map<std::string, std::any> properties;

public:
    PropertyBag() = default;

    /**
     * @brief Copy constructor tự định nghĩa để xử lý mutex một cách an toàn.
     * Chỉ sao chép 'properties', không sao chép mutex.
     */
    PropertyBag(const PropertyBag& other)
    {
        std::lock_guard<std::recursive_mutex> lock(other.m_mutex);
        properties = other.properties;
    }

    /**
     * @brief Copy assignment operator tự định nghĩa để xử lý mutex một cách an toàn.
     */
    PropertyBag& operator=(const PropertyBag& other)
    {
        if (this != &other)
        {
            std::scoped_lock lock(m_mutex, other.m_mutex);
            properties = other.properties;
        }
        return *this;
    }

    // ==========================================
    // 1. Quản lý chung (Clear, Size, Empty)
    // ==========================================

    /**
     * @brief Xóa toàn bộ các phần tử trong PropertyBag.
     * @example
     *   bag.Clear();
     */
    void Clear() noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        properties.clear();
    }

    /**
     * @brief Kiểm tra PropertyBag có trống hay không.
     * @return true nếu rỗng, false nếu có chứa phần tử.
     * @example
     *   if (bag.Empty()) { ... }
     */
    bool Empty() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return properties.empty();
    }

    /**
     * @brief Lấy số lượng thuộc tính hiện đang lưu trữ.
     * @return std::size_t Số lượng thuộc tính.
     * @example
     *   std::size_t count = bag.Size();
     */
    std::size_t Size() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return properties.size();
    }

    // ==========================================
    // 2. Các hàm lấy / gán cơ bản
    // ==========================================

    /**
     * @brief Gán hoặc cập nhật giá trị cho một key.
     * @tparam T Kiểu dữ liệu của value.
     * @param key Tên nhận diện.
     * @param value Giá trị gán.
     * @example
     *   bag.Set("volume", 80);
     *   bag.Set("title", std::string("Media Player"));
     */
    template<typename T>
    void Set(std::string_view key, T&& value) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        properties[std::string(key)] = std::forward<T>(value);
    }
    
    template<typename T>
    void Set(std::string_view key, const T& value) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        properties[std::string(key)] = value;
    }

    /**
     * @brief Lấy giá trị bản sao, nếu không tồn tại hoặc sai kiểu sẽ trả về defaultValue.
     * @tparam T Kiểu dữ liệu mong muốn.
     * @param key Tên nhận diện.
     * @param defaultValue Giá trị mặc định trả về nếu lấy thất bại.
     * @return T Giá trị lấy được hoặc defaultValue.
     * @example
     *   int vol = bag.GetValue<int>("volume", 50);
     */
    template<typename T>
    T GetValue(std::string_view key, const T& defaultValue = T()) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        if (it != properties.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    /**
     * @brief Lấy con trỏ tới phần tử bên trong (Non-const).
     * @tparam T Kiểu dữ liệu mong muốn.
     * @param key Tên nhận diện.
     * @return T* Con trỏ tới dữ liệu hoặc nullptr nếu không tìm thấy/sai kiểu.
     * @example
     *   if (auto* vol = bag.GetPtr<int>("volume")) {
     *       *vol = 90;
     *   }
     */
    template<typename T>
    T* GetPtr(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        // LƯU Ý: Con trỏ trả về có thể trở thành dangling. Cần có cơ chế quản lý vòng đời ở tầng cao hơn.
        auto it = properties.find(std::string(key));
        if (it == properties.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    /**
     * @brief Lấy con trỏ tới phần tử bên trong (Const).
     * @tparam T Kiểu dữ liệu mong muốn.
     * @param key Tên nhận diện.
     * @return const T* Con trỏ const tới dữ liệu hoặc nullptr.
     * @example
     *   if (const auto* vol = bag.GetPtr<int>("volume")) {
     *       std::cout << *vol;
     *   }
     */
    template<typename T>
    const T* GetPtr(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        // LƯU Ý: Con trỏ trả về có thể trở thành dangling. Cần có cơ chế quản lý vòng đời ở tầng cao hơn.
        auto it = properties.find(std::string(key));
        if (it == properties.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    /**
     * @brief Thử lấy giá trị ra biến out mà không quăng Exception.
     * @tparam T Kiểu dữ liệu mong muốn.
     * @param key Tên nhận diện.
     * @param out Biến nhận kết quả.
     * @return true Lấy thành công, false Nếu thất bại.
     * @example
     *   int vol = 0;
     *   if (bag.TryGet("volume", vol)) {
     *       // Dùng vol...
     *   }
     */
    template<typename T>
    bool TryGet(std::string_view key, T& out) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        const T* ptr = GetPtr<T>(key);
        if (ptr) {
            out = *ptr;
            return true;
        }
        return false;
    }

    /**
     * @brief Lấy tham chiếu tới giá trị lưu bên trong (Non-const).
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @return T& Tham chiếu trực tiếp.
     * @throws std::runtime_error Nếu key không tồn tại hoặc éo sai kiểu.
     * @example
     *   try {
     *       int& vol = bag.GetRef<int>("volume");
     *       vol = 100;
     *   } catch (const std::exception& e) { ... }
     */
    template<typename T>
    T& GetRef(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        // LƯU Ý: Tham chiếu trả về có thể trở thành dangling. Cần có cơ chế quản lý vòng đời ở tầng cao hơn.
        T* ptr = GetPtr<T>(key);
        if (!ptr) throw std::runtime_error("Property not found or type mismatch: " + std::string(key));
        return *ptr;
    }

    /**
     * @brief Lấy tham chiếu tới giá trị lưu bên trong (Const).
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @return const T& Tham chiếu const trực tiếp.
     * @throws std::runtime_error Nếu key không tồn tại hoặc éo sai kiểu.
     * @example
     *   const auto& vol = bag.GetRef<int>("volume");
     */
    template<typename T>
    const T& GetRef(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        // LƯU Ý: Tham chiếu trả về có thể trở thành dangling. Cần có cơ chế quản lý vòng đời ở tầng cao hơn.
        const T* ptr = GetPtr<T>(key);
        if (!ptr) throw std::runtime_error("Property not found or type mismatch: " + std::string(key));
        return *ptr;
    }

    /**
     * @brief Kiểm tra key có tồn tại không.
     * @example
     *   if (bag.Has("volume")) { ... }
     */
    bool Has(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return properties.find(std::string(key)) != properties.end();
    }

    /**
     * @brief Biệt danh của Has().
     * @example
     *   if (bag.Contains("volume")) { ... }
     */
    bool Contains(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return Has(key);
    }

    /**
     * @brief Xóa một thuộc tính khỏi bag.
     * @example
     *   bag.Remove("volume");
     */
    void Remove(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        properties.erase(std::string(key));
    }

    // ==========================================
    // 3. Truy vấn kiểu dữ liệu (Type Query)
    // ==========================================

    /**
     * @brief Kiểm tra key có khớp chính xác với kiểu T hay không.
     * @tparam T Kiểu dữ liệu cần đối soát.
     * @param key Tên thuộc tính.
     * @return true đúng kiểu T, false nếu sai kiểu hoặc không có key.
     * @example
     *   if (bag.Is<int>("volume")) {
     *       // Xử lý khi volume chính xác là kiểu int
     *   }
     */
    template<typename T>
    bool Is(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        return (it != properties.end()) && (it->second.type() == typeid(T));
    }

    /**
     * @brief Lấy thông tin type_info của giá trị thuộc key.
     * @param key Tên thuộc tính.
     * @return const std::type_info* Con trỏ type_info hoặc nullptr nếu không có key.
     * @example
     *   if (auto type = bag.GetType("video")) {
     *       std::cout << type->name();
     *   }
     */
    const std::type_info* GetType(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        if (it != properties.end()) {
            return &it->second.type();
        }
        return nullptr;
    }

    // ==========================================
    // 4. Khởi tạo / Thay thế / Đảm bảo đối tượng
    // ==========================================

    /**
     * @brief Khởi tạo đối tượng T trực tiếp tại chỗ (in-place placement) mà không thông qua copy tạm.
     * @tparam T Kiểu đối tượng.
     * @tparam Args Danh sách các tham số của Constructor.
     * @param key Tên nhận diện.
     * @param args Các đối số truyền vào Constructor của T.
     * @return T& Tham chiếu đến đối tượng vừa được khởi tạo.
     * @example
     *   struct VideoInfo { int w; int h; std::string path; };
     *   auto& info = bag.Emplace<VideoInfo>("video", 1920, 1080, "test.mp4");
     */
    template<typename T, typename... Args>
    T& Emplace(std::string_view key, Args&&... args) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto [it, inserted] = properties.insert_or_assign(
            std::string(key), 
            std::make_any<T>(std::forward<Args>(args)...)
        );
        return std::any_cast<T&>(it->second);
    }

    /**
     * @brief Lấy đối tượng nếu đã có, hoặc tự động tạo mới nếu chưa tồn tại (Dùng default constructor).
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @return T& Tham chiếu tới giá trị.
     * @example
     *   auto& cache = bag.GetOrCreate<MyCache>("cache");
     */
    template<typename T>
    T& GetOrCreate(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        if (it == properties.end()) {
            it = properties.emplace(std::string(key), T{}).first;
        }
        return std::any_cast<T&>(it->second);
    }

    /**
     * @brief Đảm bảo key tồn tại. Tương tự GetOrCreate nhưng thể hiện rõ ý định "khởi tạo bắt buộc".
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @return T& Tham chiếu tới giá trị.
     * @example
     *   auto& info = bag.Ensure<VideoInfo>("VideoInfo");
     */
    template<typename T>
    T& Ensure(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return GetOrCreate<T>(key);
    }

    /**
     * @brief Chỉ đặt giá trị mới nếu key CHƯA tồn tại.
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @param value Giá trị cần thêm.
     * @return true Đã thêm thành công, false Key đã tồn tại từ trước (không làm gì).
     * @example
     *   bool created = bag.SetIfAbsent("WindowId", 1001);
     */
    template<typename T>
    bool SetIfAbsent(std::string_view key, T&& value) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto [it, inserted] = properties.try_emplace(std::string(key), std::forward<T>(value));
        return inserted;
    }

    /**
     * @brief Chỉ thay đổi giá trị nếu key ĐÃ tồn tại.
     * @tparam T Kiểu dữ liệu.
     * @param key Tên nhận diện.
     * @param value Giá trị mới.
     * @return true Thay đổi thành công, false Nếu key chưa từng tồn tại.
     * @example
     *   bool updated = bag.Replace("WindowId", 2002);
     */
    template<typename T>
    bool Replace(std::string_view key, T&& value) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        if (it != properties.end()) {
            it->second = std::forward<T>(value);
            return true;
        }
        return false;
    }

    /**
     * @brief Đổi tên key cũ sang key mới. Dữ liệu gốc giữ nguyên.
     * @param oldKey Key cần đổi.
     * @param newKey Key mới.
     * @return true Đổi tên thành công, false Nếu không thấy oldKey.
     * @example
     *   bag.Rename("old_vol", "volume");
     */
    bool Rename(std::string_view oldKey, std::string_view newKey) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto node = properties.extract(std::string(oldKey));
        if (node.empty()) return false;
        node.key() = std::string(newKey);
        properties.insert(std::move(node));
        return true;
    }

    // ==========================================
    // 5. Duyệt & Liệt kê (Iterate / List)
    // ==========================================

    /**
     * @brief Lấy danh sách toàn bộ tên key đang lưu trong Bag.
     * @return std::vector<std::string> Danh sách các key.
     * @example
     *   for (const auto& key : bag.Keys()) {
     *       std::cout << key << "\n";
     *   }
     */
    std::vector<std::string> Keys() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::string> keys;
        keys.reserve(properties.size());
        for (const auto& [k, v] : properties) {
            keys.push_back(k);
        }
        return keys;
    }

    /**
     * @brief Duyệt qua từng thuộc tính bằng hàm Callback / Lambda.
     * @tparam Func Kiểu callable nhận (const std::string& key, std::any& value).
     * @param func Lambda hoặc Function Pointer.
     * @example
     *   bag.ForEach([](const std::string& key, std::any& val) {
     *       std::cout << "Key: " << key << "\n";
     *   });
     */
    template<typename Func>
    void ForEach(Func&& func) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& [k, v] : properties) {
            func(k, v);
        }
    }

    /**
     * @brief Duyệt qua từng thuộc tính bằng hàm Callback (phiên bản Read-only Const).
     * @example
     *   constBag.ForEach([](const std::string& key, const std::any& val) { ... });
     */
    template<typename Func>
    void ForEach(Func&& func) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto& [k, v] : properties) {
            func(k, v);
        }
    }

    // ==========================================
    // 6. Truy cập gốc (Raw std::any)
    // ==========================================

    /**
     * @brief Lấy con trỏ `std::any` trực tiếp để tự ép kiểu hoặc serialize.
     * @param key Tên thuộc tính.
     * @return std::any* Con trỏ gốc hoặc nullptr.
     * @example
     *   if (std::any* rawAny = bag.GetAny("video")) { ... }
     */
    std::any* GetAny(std::string_view key) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        return (it != properties.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Lấy con trỏ `const std::any` trực tiếp (Const).
     * @example
     *   if (const std::any* rawAny = bag.GetAny("video")) { ... }
     */
    const std::any* GetAny(std::string_view key) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = properties.find(std::string(key));
        return (it != properties.end()) ? &it->second : nullptr;
    }

    // ==========================================
    // 7. Thao tác nâng cao (Merge, CopyIf, RemoveIf, Visit)
    // ==========================================

    /**
     * @brief Trộn (Merge) tất cả thuộc tính từ `other` vào Bag hiện tại. Key bị trùng sẽ bị ghi đè.
     * @param other Bag nguồn cần chép sang.
     * @example
     *   PropertyBag sessionProps, windowProps;
     *   windowProps.Merge(sessionProps);
     */
    void Merge(const PropertyBag& other) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto& [k, v] : other.properties) {
            properties[k] = v;
        }
    }

    /**
     * @brief Sao chép chỉ những key chỉ định từ `other` sang Bag hiện tại.
     * @param other Bag nguồn.
     * @param keysToCopy Danh sách các key cần chép.
     * @example
     *   bag.CopyIf(otherBag, { "VideoInfo", "PlaybackStatus", "Subtitle" });
     */
    void CopyIf(const PropertyBag& other, std::initializer_list<std::string_view> keysToCopy) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto k : keysToCopy) {
            if (auto ptr = other.GetAny(k)) {
                properties[std::string(k)] = *ptr;
            }
        }
    }

    /**
     * @brief Xóa các phần tử thỏa mãn điều kiện predicate (Lambda).
     * @tparam Pred Hàm kiểm tra nhận (const std::string& key, std::any& val) -> bool.
     * @param pred Trả về true để xóa key đó.
     * @example
     *   // Xóa tất cả các key tạm bắt đầu bằng "Temp."
     *   bag.RemoveIf([](const std::string& key, const std::any&) {
     *       return key.rfind("Temp.", 0) == 0;
     *   });
     */
    template<typename Pred>
    void RemoveIf(Pred&& pred) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto it = properties.begin(); it != properties.end(); ) {
            if (pred(it->first, it->second)) {
                it = properties.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Thực thi hàm callback một cách an toàn nếu key tồn tại và đúng kiểu T.
     * @tparam T Kiểu dữ liệu dự kiến.
     * @tparam Func Callable dạng `void(T&)` hoặc `void(const T&)`.
     * @param key Tên nhận diện.
     * @param func Hàm xử lý dữ liệu.
     * @return true Thực thi thành công, false Nếu không có key hoặc không đúng kiểu T.
     * @example
     *   bag.Visit<VideoInfo>("VideoInfo", [](VideoInfo& info) {
     *       info.width = 1920;
     *   });
     */
    template<typename T, typename Func>
    bool Visit(std::string_view key, Func&& func) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (auto* ptr = GetPtr<T>(key)) {
            func(*ptr);
            return true;
        }
        return false;
    }

    /**
     * @brief Thực thi hàm callback an toàn (phiên bản Const).
     * @example
     *   bag.Visit<VideoInfo>("VideoInfo", [](const VideoInfo& info) {
     *       std::cout << info.width;
     *   });
     */
    template<typename T, typename Func>
    bool Visit(std::string_view key, Func&& func) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (const auto* ptr = GetPtr<T>(key)) {
            func(*ptr);
            return true;
        }
        return false;
    }
};