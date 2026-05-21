#pragma once

#include "IPublisher.hpp"
#include "mqtt/core/ConnectionState.hpp"
#include "mqtt/monitoring/Metrics.hpp"

#include <memory>

namespace mqtt {

// Native-socket MQTT 3.1.1 publisher.  Zero external MQTT library dependencies;
// uses POSIX TCP sockets and OpenSSL for TLS.
class MqttPublisher final : public IPublisher {
public:
    explicit MqttPublisher(ConnectionConfig config, TlsConfig tlsConfig = {});
    ~MqttPublisher() override;

    MqttPublisher(const MqttPublisher&)            = delete;
    MqttPublisher& operator=(const MqttPublisher&) = delete;
    MqttPublisher(MqttPublisher&&)                 = delete;
    MqttPublisher& operator=(MqttPublisher&&)      = delete;

    Result<void> connect() override;
    Result<void> disconnect(std::chrono::milliseconds quiesce = std::chrono::milliseconds{500}) override;
    Result<void> reconnect() override;

    Result<void> publish(const Message& msg) override;
    Result<int>  publishAsync(Message msg, PublishAckCallback cb = {}) override;
    Result<void> flush(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) override;

    void setDefaultQoS(int qos) override;
    void setDefaultRetained(bool retained) override;

    [[nodiscard]] ConnectionState state() const noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;

    [[nodiscard]] std::shared_ptr<Metrics> metrics() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace mqtt
