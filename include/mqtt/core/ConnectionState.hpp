#pragma once

#include <atomic>
#include <string_view>

namespace mqtt {

enum class ConnectionState : uint8_t {
    Disconnected = 0,
    Connecting,
    Connected,
    Reconnecting,
    Disconnecting,
    Failed,
};

constexpr std::string_view toString(ConnectionState s) noexcept {
    switch (s) {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Connecting:    return "Connecting";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::Reconnecting:  return "Reconnecting";
        case ConnectionState::Disconnecting: return "Disconnecting";
        case ConnectionState::Failed:        return "Failed";
    }
    return "Unknown";
}

} // namespace mqtt
