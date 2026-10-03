// WindowFactory.h
#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "WindowTemplate.h"
#include "WindowManager.h" 
#include "WindowInitializer.h" // Thêm Initializer


/**
 * @brief 
 * 
 */
class WindowTemplateRegistry {
private:
    std::unordered_map<std::string, WindowTemplate> templates;

public:
    /**
     * @brief 
     * 
     * @param name 
     * @param tpl 
     */
    void RegisterTemplate(const std::string& name, const WindowTemplate& tpl) {
        // Sử dụng insert_or_assign để tránh lỗi C2280 do toán tử gán của WindowTemplate bị xóa.
        // Điều này xảy ra vì WindowTemplate chứa các std::function có thể bắt giữ các đối tượng không thể sao chép.
        templates.insert_or_assign(name, tpl);
    }

    const WindowTemplate* GetTemplate(const std::string& name) const {
        auto it = templates.find(name);
        return (it != templates.end()) ? &it->second : nullptr;
    }
};

class WindowFactory {
private:
    WindowTemplateRegistry* registry;
    uint32_t nextId = 0;

public:
    /**
     * @brief Construct a new Window Factory object
     * 
     * @param reg 
     */
    WindowFactory(WindowTemplateRegistry* reg) : registry(reg) {}

    /**
     * @brief 
     * 
     * @param templateName 
     * @return WindowRuntime* 
     */
    WindowRuntime* Create(const std::string& templateName, WindowRuntime* parent = nullptr) {
        const auto* tpl = registry->GetTemplate(templateName);
        if (!tpl) return nullptr;

        // Window identity must be final before WindowRuntime construction:
        // lifecycle CREATE/START markers are emitted by the constructor and must
        // use the same stable ID later observed at STOP/DESTROY.
        const WindowId id = nextId++;
        auto* runtime = new WindowRuntime(id);
        runtime->info.templateName = templateName; // Lưu lại template name
        runtime->state = tpl->state;
        runtime->style = tpl->style;
        // Sử dụng Merge thay vì toán tử gán để tránh lỗi C2280,
        // vì PropertyBag chứa std::recursive_mutex không thể sao chép.
        runtime->properties.Merge(tpl->defaultProperties);
        if (tpl->rendererFactory) {
            runtime->renderer = tpl->rendererFactory();
        }
        if (tpl->windowloopFactory) {
            runtime->windowloop = tpl->windowloopFactory();
        }

        WindowInitializer initializer;
        if (!initializer.Initialize(runtime, tpl, parent)) {
            // Initializer đã log lỗi, chỉ cần dọn dẹp runtime
            delete runtime;
            return nullptr;
        }

        return runtime;
    }
};