#pragma once

#include <mpv/client.h>
#include <string>
#include <optional>

class MPVPlayer; // Forward declaration

class MPVProperty {
public:
    MPVProperty(MPVPlayer& player);

    std::optional<std::string> GetString(const std::string& name) const;
    std::optional<int64_t> GetInt(const std::string& name) const;
    std::optional<double> GetDouble(const std::string& name) const;
    std::optional<bool> GetFlag(const std::string& name) const;
    // For simplicity, GetNode is not implemented yet as it requires careful memory management.

private:
    MPVPlayer& m_player;
};