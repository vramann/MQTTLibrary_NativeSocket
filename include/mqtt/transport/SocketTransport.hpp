#pragma once

#include "mqtt/core/Error.hpp"
#include "mqtt/transport/TlsConfig.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mqtt {

// Low-level TCP transport with optional OpenSSL TLS.
// Not thread-safe: callers must serialise send and recv independently.
class SocketTransport {
public:
    SocketTransport();
    ~SocketTransport();

    SocketTransport(const SocketTransport&)            = delete;
    SocketTransport& operator=(const SocketTransport&) = delete;
    SocketTransport(SocketTransport&&)                 = delete;
    SocketTransport& operator=(SocketTransport&&)      = delete;

    // Connect to host:port.  If tls.enabled the handshake is performed here.
    Result<void> connect(const std::string& host, uint16_t port,
                         const TlsConfig& tls,
                         std::chrono::milliseconds timeout);

    // Close socket and free TLS resources.
    void disconnect();

    // Send all bytes; retries on partial write.
    Result<void> sendAll(const std::vector<uint8_t>& data);
    Result<void> sendAll(const uint8_t* data, size_t len);

    // Receive exactly n bytes; blocks until all arrive or an error occurs.
    // Uses a 500 ms SO_RCVTIMEO to allow the stop-flag to be checked.
    Result<std::vector<uint8_t>> recvExact(size_t n);
    Result<uint8_t>              recvByte();

    [[nodiscard]] bool isConnected() const noexcept;

    // Signal recvByte/recvExact to return NotConnected immediately.
    void requestStop() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Parsed representation of a broker URL such as "tcp://host:1883" or "ssl://host:8883".
struct BrokerAddress {
    std::string host;
    uint16_t    port{1883};
    bool        useTls{false};
};

BrokerAddress parseBrokerUrl(const std::string& url);

} // namespace mqtt
