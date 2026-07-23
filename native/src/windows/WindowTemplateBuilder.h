// WindowTemplateBuilder.h
#pragma once

#include "WindowTemplate.h"
#include "WindowFactory.h" // For WindowTemplateRegistry
#include "utils.h"         // Để có FrameTimer
#include <functional>
#include <string>
#include <memory>
#include <utility>
#include <tuple>           // Cần thiết cho std::tuple và std::apply (C++17)

// Forward declarations for renderer and backend base classes
class WindowRenderer;
class IGraphicsBackend;

class WindowTemplateBuilder {
private:
    WindowTemplate m_tpl;

public:
    WindowTemplateBuilder() = default;

    WindowTemplateBuilder& WithName(const std::string& name) {
        m_tpl.name = name;
        return *this;
    }

    WindowTemplateBuilder& WithStyle(const std::function<void(WindowStyle&)>& styleSetter) {
        styleSetter(m_tpl.style);
        return *this;
    }

    WindowTemplateBuilder& WithState(const std::function<void(WindowState&)>& stateSetter) {
        stateSetter(m_tpl.state);
        return *this;
    }

    WindowTemplateBuilder& WithProperties(const std::function<void(PropertyBag&)>& propsSetter) {
        propsSetter(m_tpl.defaultProperties);
        return *this;
    }

    template<typename T, typename... Args>
    WindowTemplateBuilder& WithBackend(Args&&... args) {
        static_assert(std::is_base_of<IGraphicsBackend, T>::value, "T must derive from IGraphicsBackend");
        
        // C++17 Solution: Đóng gói args vào tuple
        m_tpl.graphicsBackendFactory = [args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply([](auto&&... unpacked_args) {
                return std::make_unique<T>(std::forward<decltype(unpacked_args)>(unpacked_args)...);
            }, std::move(args_tuple));
        };
        return *this;
    }

    template<typename T, typename... Args>
    WindowTemplateBuilder& WithRenderer(Args&&... args) {
        static_assert(std::is_base_of<WindowRenderer, T>::value, "T must derive from WindowRenderer");
        
        // C++17 Solution: Đóng gói args vào tuple
        m_tpl.rendererFactory = [args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply([](auto&&... unpacked_args) {
                return std::make_unique<T>(std::forward<decltype(unpacked_args)>(unpacked_args)...);
            }, std::move(args_tuple));
        };
        return *this;
    }

    WindowTemplateBuilder& WithLoop(int fps) {
        m_tpl.windowloopFactory = [=]() {
            return std::make_unique<FrameTimer>(fps);
        };
        return *this;
    }

    // Final step: build the template
    WindowTemplate Build() {
        return m_tpl;
    }

    // Convenience method to build and register in one go
    void Register(WindowTemplateRegistry& registry, const std::string& templateName) {
        if (m_tpl.name.empty()) {
            m_tpl.name = templateName;
        }
        registry.RegisterTemplate(templateName, Build());
    }
};