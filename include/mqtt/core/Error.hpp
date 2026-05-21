#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace mqtt {

enum class ErrorCode : int {
    Ok = 0,
    // Connection
    ConnectionFailed,
    ConnectionLost,
    ConnectionTimeout,
    AlreadyConnected,
    NotConnected,
    // TLS
    TlsHandshakeFailed,
    CertificateInvalid,
    CertificateExpired,
    // Auth
    AuthenticationFailed,
    AuthorizationDenied,
    // Publish
    PublishFailed,
    PublishQueueFull,
    PublishTimeout,
    // Subscribe
    SubscribeFailed,
    UnsubscribeFailed,
    // Config
    ConfigParseError,
    ConfigInvalid,
    // Serialization
    SerializationFailed,
    DeserializationFailed,
    // Internal
    InternalError,
    NotSupported,
};

class MqttException : public std::runtime_error {
public:
    explicit MqttException(ErrorCode code, std::string msg)
        : std::runtime_error(msg), code_(code), msg_(std::move(msg)) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return msg_; }

private:
    ErrorCode   code_;
    std::string msg_;
};

class ConnectionException    : public MqttException { using MqttException::MqttException; };
class TlsException           : public MqttException { using MqttException::MqttException; };
class AuthException          : public MqttException { using MqttException::MqttException; };
class PublishException       : public MqttException { using MqttException::MqttException; };
class SubscribeException     : public MqttException { using MqttException::MqttException; };
class ConfigException        : public MqttException { using MqttException::MqttException; };
class SerializationException : public MqttException { using MqttException::MqttException; };

template<typename T, typename E = MqttException>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(E error) {
        Result r;
        r.error_ = std::move(error);
        return r;
    }

    [[nodiscard]] bool hasValue() const noexcept { return has_value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value_; }

    [[nodiscard]] T& value() { return value_; }
    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] const E& error() const { return error_.value(); }

private:
    Result() : has_value_(false) {}
    explicit Result(T v) : value_(std::move(v)), has_value_(true) {}

    T                  value_{};
    std::optional<E>   error_;
    bool               has_value_{true};
};

template<typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true); }
    static Result err(E error) {
        Result r(false);
        r.error_ = std::move(error);
        return r;
    }

    [[nodiscard]] bool hasValue() const noexcept { return has_value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value_; }
    [[nodiscard]] const E& error() const { return error_.value(); }

private:
    explicit Result(bool ok) : has_value_(ok) {}
    std::optional<E> error_;
    bool             has_value_;
};

} // namespace mqtt
