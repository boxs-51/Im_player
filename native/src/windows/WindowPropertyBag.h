// WindowPropertyBag.h
#pragma once
#include <string>
#include <unordered_map>
#include <any>

class PropertyBag {
private:
    std::unordered_map<std::string, std::any> properties;

public:
    /**
     * @brief 
     * 
     * @tparam T 
     * @param key 
     * @param value 
     */
    template<typename T>
    void Set(const std::string& key, const T& value) {
        properties[key] = value;
    }
    /**
     * @brief 
     * 
     * @tparam T 
     * @param key 
     * @param defaultValue 
     * @return T 
     */
    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T()) const {
        auto it = properties.find(key);
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
     * @brief 
     * 
     * @param key 
     * @return true 
     * @return false 
     */
    bool Has(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
    /**
     * @brief 
     * 
     * @param key 
     * @return true 
     * @return false 
     */
    bool Contains(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
    /**
     * @brief 
     * 
     * @param key 
     */
    void Remove(const std::string& key) {
        properties.erase(key);
    }
};