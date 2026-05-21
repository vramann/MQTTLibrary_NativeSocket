#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mqtt {

enum class MqttVersion : uint8_t {
    V311 = 4,
    V50  = 5,
};

struct WillConfig {
    std::string topic;
    std::string payload;
    int         qos{1};
    bool        retained{false};
};

struct ReconnectPolicy {
    bool                         enabled{true};
    uint32_t                     maxRetries{0};       // 0 = unlimited
    std::chrono::milliseconds    initialDelay{1000};
    std::chrono::milliseconds    maxDelay{60000};
    double                       backoffMultiplier{2.0};
    bool                         jitter{true};
};

struct ConnectionConfig {
    // Broker
    std::vector<std::string> brokerUrls;              // e.g. "ssl://host:8883"
    std::string              clientId;

    // Protocol
    MqttVersion mqttVersion{MqttVersion::V311};
    bool        cleanSession{true};
    uint16_t    keepaliveSeconds{60};

    // Auth
    std::optional<std::string> username;
    std::optional<std::string> password;
    std::optional<std::string> authToken;

    // Timeouts
    std::chrono::milliseconds connectTimeout{10000};
    std::chrono::milliseconds ackTimeout{5000};

    // Offline buffering
    bool     offlineBufferingEnabled{true};
    uint32_t offlineBufferSize{1000};

    // LWT
    std::optional<WillConfig> will;

    // Reconnect
    ReconnectPolicy reconnect;

    // Thread pool
    uint32_t workerThreads{2};
};

} // namespace mqtt
