#include "MPVProperty.h"
#include "mpv/player/MPVPlayer.h"

MPVProperty::MPVProperty(MPVPlayer& player) : m_player(player) {}

std::optional<std::string> MPVProperty::GetString(const std::string& name) const {
    if (!m_player.GetHandle()) return std::nullopt;
    char* value = mpv_get_property_string(m_player.GetHandle(), name.c_str());
    if (!value) return std::nullopt;
    std::string result(value);
    mpv_free(value);
    return result;
}

std::optional<int64_t> MPVProperty::GetInt(const std::string& name) const {
    if (!m_player.GetHandle()) return std::nullopt;
    int64_t value;
    if (mpv_get_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_INT64, &value) < 0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> MPVProperty::GetDouble(const std::string& name) const {
    if (!m_player.GetHandle()) return std::nullopt;
    double value;
    if (mpv_get_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_DOUBLE, &value) < 0) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> MPVProperty::GetFlag(const std::string& name) const {
    if (!m_player.GetHandle()) return std::nullopt;
    int value;
    if (mpv_get_property(m_player.GetHandle(), name.c_str(), MPV_FORMAT_FLAG, &value) < 0) {
        return std::nullopt;
    }
    return value != 0;
}