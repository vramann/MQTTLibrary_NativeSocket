#pragma once

#include "mqtt/core/ConnectionConfig.hpp"
#include "mqtt/core/ConnectionState.hpp"
#include "mqtt/core/Error.hpp"
#include "mqtt/core/Message.hpp"
#include "mqtt/transport/TlsConfig.hpp"

#include <chrono>
#include <functional>
#include <string>

namespace mqtt {

using PublishAckCallback = std::function<void(int token, ErrorCode result)>;

class IPublisher {
public:
    virtual ~IPublisher() = default;

    virtual Result<void> connect() = 0;
    virtual Result<void> disconnect(std::chrono::milliseconds quiesce = std::chrono::milliseconds{500}) = 0;
    virtual Result<void> reconnect() = 0;

    virtual Result<void> publish(const Message& msg) = 0;
    virtual Result<int>  publishAsync(Message msg, PublishAckCallback cb = {}) = 0;
    virtual Result<void> flush(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) = 0;

    virtual void setDefaultQoS(int qos) = 0;
    virtual void setDefaultRetained(bool retained) = 0;

    [[nodiscard]] virtual ConnectionState state() const noexcept = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
};

} // namespace mqtt
