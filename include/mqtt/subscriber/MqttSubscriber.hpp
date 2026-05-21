#pragma once

#include "ISubscriber.hpp"
#include "mqtt/core/ConnectionState.hpp"
#include "mqtt/monitoring/Metrics.hpp"

#include <memory>
#include <string>
#include <vector>

namespace mqtt {

// Native-socket MQTT 3.1.1 subscriber.  Zero external MQTT library dependencies;
// uses POSIX TCP sockets and OpenSSL for TLS.
class MqttSubscriber final : public ISubscriber {
public:
    explicit MqttSubscriber(ConnectionConfig config, TlsConfig tlsConfig = {});
    ~MqttSubscriber() override;

    MqttSubscriber(const MqttSubscriber&)            = delete;
    MqttSubscriber& operator=(const MqttSubscriber&) = delete;
    MqttSubscriber(MqttSubscriber&&)                 = delete;
    MqttSubscriber& operator=(MqttSubscriber&&)      = delete;

    Result<void> connect() override;
    Result<void> disconnect(std::chrono::milliseconds quiesce = std::chrono::milliseconds{500}) override;
    Result<void> reconnect() override;

    Result<void> subscribe(const std::string& topic, int qos, MessageCallback callback) override;
    Result<void> subscribe(const std::vector<std::string>& topics, int qos, MessageCallback callback) override;
    Result<void> unsubscribe(const std::string& topic) override;
    Result<void> unsubscribe(const std::vector<std::string>& topics) override;

    void registerFallbackCallback(MessageCallback callback) override;

    // startListening / stopListening are no-ops: the recv loop runs automatically after connect().
    Result<void> startListening() override;
    Result<void> stopListening() override;

    [[nodiscard]] ConnectionState state() const noexcept override;
    [[nodiscard]] bool isConnected() const noexcept override;

    [[nodiscard]] std::shared_ptr<Metrics> metrics() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace mqtt
