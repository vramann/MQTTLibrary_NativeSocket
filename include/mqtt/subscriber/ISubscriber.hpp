#pragma once

#include "mqtt/core/ConnectionConfig.hpp"
#include "mqtt/core/ConnectionState.hpp"
#include "mqtt/core/Error.hpp"
#include "mqtt/core/Message.hpp"
#include "mqtt/transport/TlsConfig.hpp"

#include <string>
#include <vector>

namespace mqtt {

class ISubscriber {
public:
    virtual ~ISubscriber() = default;

    virtual Result<void> connect() = 0;
    virtual Result<void> disconnect(std::chrono::milliseconds quiesce = std::chrono::milliseconds{500}) = 0;
    virtual Result<void> reconnect() = 0;

    virtual Result<void> subscribe(const std::string& topic, int qos, MessageCallback callback) = 0;
    virtual Result<void> subscribe(const std::vector<std::string>& topics, int qos, MessageCallback callback) = 0;

    virtual Result<void> unsubscribe(const std::string& topic) = 0;
    virtual Result<void> unsubscribe(const std::vector<std::string>& topics) = 0;

    virtual void registerFallbackCallback(MessageCallback callback) = 0;

    virtual Result<void> startListening() = 0;
    virtual Result<void> stopListening() = 0;

    [[nodiscard]] virtual ConnectionState state() const noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
};

} // namespace mqtt
