#pragma once

#include <mpv/client.h>
#include <string>
#include <optional>

class Player; // Forward declaration

class PlayBackProperty {
public:
    PlayBackProperty(Player& player);

    std::optional<std::string> GetString(const std::string& name) const;
    std::optional<int64_t> GetInt(const std::string& name) const;
    std::optional<double> GetDouble(const std::string& name) const;
    std::optional<bool> GetFlag(const std::string& name) const;
    // For simplicity, GetNode is not implemented yet as it requires careful memory management.

private:
    Player& m_player;
};