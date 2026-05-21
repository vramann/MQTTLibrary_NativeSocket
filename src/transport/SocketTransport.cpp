#include "mqtt/transport/SocketTransport.hpp"
#include "mqtt/logging/Logger.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <atomic>

namespace mqtt {

// ── PIMPL ─────────────────────────────────────────────────────────────────────

struct SocketTransport::Impl {
    int      fd{-1};
    SSL_CTX* ctx{nullptr};
    SSL*     ssl{nullptr};
    bool     connected{false};
    std::atomic<bool> stopFlag{false};
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string sslErrorString() {
    std::string out;
    unsigned long e;
    char buf[256];
    while ((e = ERR_get_error()) != 0) {
        ERR_error_string_n(e, buf, sizeof(buf));
        if (!out.empty()) out += "; ";
        out += buf;
    }
    return out.empty() ? "unknown SSL error" : out;
}

static void closeFd(int& fd) {
    if (fd >= 0) { ::close(fd); fd = -1; }
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

SocketTransport::SocketTransport() : impl_(std::make_unique<Impl>()) {
    // OpenSSL init is idempotent since 1.1.0; safe to call multiple times.
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
}

SocketTransport::~SocketTransport() {
    disconnect();
}

// ── connect ───────────────────────────────────────────────────────────────────

Result<void> SocketTransport::connect(const std::string& host, uint16_t port,
                                       const TlsConfig& tls,
                                       std::chrono::milliseconds timeout) {
    impl_->stopFlag.store(false);

    // ── DNS resolution ──────────────────────────────────────────────────────
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    int rv = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (rv != 0) {
        return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
            "getaddrinfo(" + host + "): " + gai_strerror(rv)});
    }

    // ── TCP socket + non-blocking connect ───────────────────────────────────
    impl_->fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (impl_->fd < 0) {
        ::freeaddrinfo(res);
        return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
            std::string("socket(): ") + strerror(errno)});
    }

    // Switch to non-blocking so connect can have a timeout.
    int flags = ::fcntl(impl_->fd, F_GETFL, 0);
    ::fcntl(impl_->fd, F_SETFL, flags | O_NONBLOCK);

    rv = ::connect(impl_->fd, res->ai_addr, res->ai_addrlen);
    ::freeaddrinfo(res);

    if (rv < 0 && errno != EINPROGRESS) {
        closeFd(impl_->fd);
        return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
            std::string("connect(): ") + strerror(errno)});
    }

    if (rv != 0) {
        struct pollfd pfd{impl_->fd, POLLOUT, 0};
        int pr = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
        if (pr <= 0) {
            closeFd(impl_->fd);
            return Result<void>::err(ConnectionException{
                pr == 0 ? ErrorCode::ConnectionTimeout : ErrorCode::ConnectionFailed,
                pr == 0 ? "TCP connect timed out" : "poll() error"});
        }
        int err = 0;
        socklen_t len = sizeof(err);
        ::getsockopt(impl_->fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            closeFd(impl_->fd);
            return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
                std::string("connect SO_ERROR: ") + strerror(err)});
        }
    }

    // Restore blocking mode.
    flags = ::fcntl(impl_->fd, F_GETFL, 0);
    ::fcntl(impl_->fd, F_SETFL, flags & ~O_NONBLOCK);

    // 500 ms receive timeout — keeps the recv loop responsive to stop requests.
    struct timeval tv{0, 500'000};
    ::setsockopt(impl_->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int one = 1;
    ::setsockopt(impl_->fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    ::setsockopt(impl_->fd, IPPROTO_TCP, TCP_NODELAY,  &one, sizeof(one));

    // ── Optional TLS handshake ───────────────────────────────────────────────
    if (tls.enabled) {
        impl_->ctx = SSL_CTX_new(TLS_client_method());
        if (!impl_->ctx) {
            closeFd(impl_->fd);
            return Result<void>::err(TlsException{ErrorCode::TlsHandshakeFailed,
                "SSL_CTX_new: " + sslErrorString()});
        }

        SSL_CTX_set_options(impl_->ctx,
            SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
            (tls.minVersion == TlsVersion::TLS_1_3
                ? (SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2)
                : 0L));

        if (tls.verifyPeer) {
            SSL_CTX_set_verify(impl_->ctx, SSL_VERIFY_PEER, nullptr);
            if (!tls.caCertFile.empty()) {
                if (!SSL_CTX_load_verify_locations(impl_->ctx, tls.caCertFile.c_str(), nullptr)) {
                    SSL_CTX_free(impl_->ctx); impl_->ctx = nullptr;
                    closeFd(impl_->fd);
                    return Result<void>::err(TlsException{ErrorCode::CertificateInvalid,
                        "load CA cert: " + sslErrorString()});
                }
            } else {
                SSL_CTX_set_default_verify_paths(impl_->ctx);
            }
        } else {
            SSL_CTX_set_verify(impl_->ctx, SSL_VERIFY_NONE, nullptr);
        }

        if (tls.clientCertFile && tls.clientKeyFile) {
            if (!SSL_CTX_use_certificate_file(impl_->ctx,
                                               tls.clientCertFile->c_str(), SSL_FILETYPE_PEM) ||
                !SSL_CTX_use_PrivateKey_file(impl_->ctx,
                                              tls.clientKeyFile->c_str(), SSL_FILETYPE_PEM)) {
                SSL_CTX_free(impl_->ctx); impl_->ctx = nullptr;
                closeFd(impl_->fd);
                return Result<void>::err(TlsException{ErrorCode::TlsHandshakeFailed,
                    "load client cert/key: " + sslErrorString()});
            }
        }

        impl_->ssl = SSL_new(impl_->ctx);
        SSL_set_fd(impl_->ssl, impl_->fd);
        SSL_set_tlsext_host_name(impl_->ssl, host.c_str());
        if (tls.verifyHostname) {
            SSL_set1_host(impl_->ssl, host.c_str());
        }

        // Non-blocking TLS handshake with deadline.
        ::fcntl(impl_->fd, F_SETFL, flags | O_NONBLOCK);
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (true) {
            int r = SSL_connect(impl_->ssl);
            if (r == 1) break;

            int e = SSL_get_error(impl_->ssl, r);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                if (ms <= 0) {
                    SSL_free(impl_->ssl); impl_->ssl = nullptr;
                    SSL_CTX_free(impl_->ctx); impl_->ctx = nullptr;
                    closeFd(impl_->fd);
                    return Result<void>::err(TlsException{ErrorCode::ConnectionTimeout,
                        "TLS handshake timed out"});
                }
                struct pollfd ppfd{impl_->fd,
                    (e == SSL_ERROR_WANT_READ) ? short(POLLIN) : short(POLLOUT), 0};
                ::poll(&ppfd, 1, static_cast<int>(std::min(ms, 200LL)));
            } else {
                SSL_free(impl_->ssl); impl_->ssl = nullptr;
                SSL_CTX_free(impl_->ctx); impl_->ctx = nullptr;
                closeFd(impl_->fd);
                return Result<void>::err(TlsException{ErrorCode::TlsHandshakeFailed,
                    "SSL_connect: " + sslErrorString()});
            }
        }

        // Restore blocking + re-apply recv timeout.
        ::fcntl(impl_->fd, F_SETFL, flags & ~O_NONBLOCK);
        ::setsockopt(impl_->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        MQTT_LOG_INFO("SocketTransport TLS handshake OK cipher={}",
                      SSL_get_cipher_name(impl_->ssl));
    }

    impl_->connected = true;
    MQTT_LOG_INFO("SocketTransport connected to {}:{}", host, port);
    return Result<void>::ok();
}

// ── disconnect ────────────────────────────────────────────────────────────────

void SocketTransport::disconnect() {
    impl_->stopFlag.store(true);
    impl_->connected = false;

    if (impl_->ssl) {
        SSL_shutdown(impl_->ssl);
        SSL_free(impl_->ssl);
        impl_->ssl = nullptr;
    }
    if (impl_->ctx) {
        SSL_CTX_free(impl_->ctx);
        impl_->ctx = nullptr;
    }
    if (impl_->fd >= 0) {
        ::shutdown(impl_->fd, SHUT_RDWR);
        closeFd(impl_->fd);
    }
}

void SocketTransport::requestStop() noexcept {
    impl_->stopFlag.store(true);
}

// ── sendAll ───────────────────────────────────────────────────────────────────

Result<void> SocketTransport::sendAll(const std::vector<uint8_t>& data) {
    return sendAll(data.data(), data.size());
}

Result<void> SocketTransport::sendAll(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r;
        if (impl_->ssl) {
            r = SSL_write(impl_->ssl, data + sent, static_cast<int>(len - sent));
            if (r <= 0) {
                return Result<void>::err(ConnectionException{ErrorCode::ConnectionLost,
                    "SSL_write: " + sslErrorString()});
            }
        } else {
            r = ::send(impl_->fd, data + sent, len - sent, MSG_NOSIGNAL);
            if (r < 0) {
                if (errno == EINTR) continue;
                return Result<void>::err(ConnectionException{ErrorCode::ConnectionLost,
                    std::string("send: ") + strerror(errno)});
            }
            if (r == 0) {
                return Result<void>::err(ConnectionException{ErrorCode::ConnectionLost,
                    "send: connection closed"});
            }
        }
        sent += static_cast<size_t>(r);
    }
    return Result<void>::ok();
}

// ── recvByte ──────────────────────────────────────────────────────────────────

Result<uint8_t> SocketTransport::recvByte() {
    for (;;) {
        if (impl_->stopFlag.load(std::memory_order_relaxed)) {
            return Result<uint8_t>::err(ConnectionException{ErrorCode::NotConnected, "Stopped"});
        }

        uint8_t b;
        ssize_t r;

        if (impl_->ssl) {
            r = SSL_read(impl_->ssl, &b, 1);
            if (r == 1) return Result<uint8_t>::ok(b);

            int e = SSL_get_error(impl_->ssl, static_cast<int>(r));
            if (e == SSL_ERROR_WANT_READ) {
                struct pollfd pfd{impl_->fd, POLLIN, 0};
                ::poll(&pfd, 1, 200);
                continue;
            }
            if (impl_->stopFlag.load())
                return Result<uint8_t>::err(ConnectionException{ErrorCode::NotConnected, "Stopped"});
            return Result<uint8_t>::err(ConnectionException{ErrorCode::ConnectionLost,
                "SSL_read: " + sslErrorString()});
        }

        r = ::recv(impl_->fd, &b, 1, 0);
        if (r == 1) return Result<uint8_t>::ok(b);

        if (r == 0) {
            return Result<uint8_t>::err(ConnectionException{ErrorCode::ConnectionLost,
                "Connection closed by peer"});
        }
        // r < 0
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            // SO_RCVTIMEO fired — check stop flag then retry.
            continue;
        }
        if (impl_->stopFlag.load())
            return Result<uint8_t>::err(ConnectionException{ErrorCode::NotConnected, "Stopped"});
        return Result<uint8_t>::err(ConnectionException{ErrorCode::ConnectionLost,
            std::string("recv: ") + strerror(errno)});
    }
}

// ── recvExact ─────────────────────────────────────────────────────────────────

Result<std::vector<uint8_t>> SocketTransport::recvExact(size_t n) {
    std::vector<uint8_t> buf(n);
    size_t got = 0;

    while (got < n) {
        if (impl_->stopFlag.load(std::memory_order_relaxed)) {
            return Result<std::vector<uint8_t>>::err(
                ConnectionException{ErrorCode::NotConnected, "Stopped"});
        }

        ssize_t r;
        if (impl_->ssl) {
            r = SSL_read(impl_->ssl, buf.data() + got, static_cast<int>(n - got));
            if (r <= 0) {
                int e = SSL_get_error(impl_->ssl, static_cast<int>(r));
                if (e == SSL_ERROR_WANT_READ) {
                    struct pollfd pfd{impl_->fd, POLLIN, 0};
                    ::poll(&pfd, 1, 200);
                    continue;
                }
                if (impl_->stopFlag.load())
                    return Result<std::vector<uint8_t>>::err(
                        ConnectionException{ErrorCode::NotConnected, "Stopped"});
                return Result<std::vector<uint8_t>>::err(
                    ConnectionException{ErrorCode::ConnectionLost, "SSL_read error in recvExact"});
            }
        } else {
            r = ::recv(impl_->fd, buf.data() + got, n - got, 0);
            if (r == 0) {
                return Result<std::vector<uint8_t>>::err(
                    ConnectionException{ErrorCode::ConnectionLost, "Connection closed by peer"});
            }
            if (r < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) continue;
                if (impl_->stopFlag.load())
                    return Result<std::vector<uint8_t>>::err(
                        ConnectionException{ErrorCode::NotConnected, "Stopped"});
                return Result<std::vector<uint8_t>>::err(
                    ConnectionException{ErrorCode::ConnectionLost,
                        std::string("recv: ") + strerror(errno)});
            }
        }
        got += static_cast<size_t>(r);
    }

    return Result<std::vector<uint8_t>>::ok(std::move(buf));
}

bool SocketTransport::isConnected() const noexcept { return impl_->connected; }

// ── parseBrokerUrl ────────────────────────────────────────────────────────────

BrokerAddress parseBrokerUrl(const std::string& url) {
    BrokerAddress addr;

    std::string rest = url;
    const auto schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        const auto scheme = url.substr(0, schemeEnd);
        if (scheme == "ssl" || scheme == "tls") {
            addr.useTls = true;
            addr.port   = 8883;
        }
        rest = url.substr(schemeEnd + 3);
    }

    // Strip trailing path segments (e.g. "host:8883/mqtt")
    const auto slashPos = rest.find('/');
    if (slashPos != std::string::npos) rest = rest.substr(0, slashPos);

    const auto colonPos = rest.rfind(':');
    if (colonPos != std::string::npos) {
        addr.host = rest.substr(0, colonPos);
        try {
            addr.port = static_cast<uint16_t>(std::stoi(rest.substr(colonPos + 1)));
        } catch (...) { /* keep default */ }
    } else {
        addr.host = rest;
    }

    return addr;
}

} // namespace mqtt
