#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mqtt {

using Payload = std::vector<uint8_t>;

struct UserProperty {
    std::string key;
    std::string value;
};

struct Message {
    std::string  topic;
    Payload      payload;
    int          qos{0};
    bool         retained{false};
    bool         duplicate{false};

    // MQTT5 extensions (unused in MQTT 3.1.1 native impl, kept for API parity)
    std::optional<std::chrono::seconds>    messageExpiry;
    std::optional<std::string>             contentType;
    std::optional<std::string>             responseTopic;
    std::optional<Payload>                 correlationData;
    std::vector<UserProperty>              userProperties;

    [[nodiscard]] std::string payloadAsString() const {
        return {reinterpret_cast<const char*>(payload.data()), payload.size()};
    }

    static Message fromString(std::string topic, std::string data, int qos = 0, bool retained = false) {
        Message m;
        m.topic    = std::move(topic);
        m.qos      = qos;
        m.retained = retained;
        m.payload.assign(data.begin(), data.end());
        return m;
    }
};

using MessageCallback = std::function<void(Message)>;

} // namespace mqtt
