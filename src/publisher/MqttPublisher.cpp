#include "mqtt/publisher/MqttPublisher.hpp"
#include "mqtt/logging/Logger.hpp"
#include "mqtt/monitoring/Metrics.hpp"
#include "mqtt/protocol/MqttPacket.hpp"
#include "mqtt/transport/SocketTransport.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace mqtt {

// ── PIMPL ─────────────────────────────────────────────────────────────────────

struct MqttPublisher::Impl {
    ConnectionConfig config;
    TlsConfig        tlsConfig;

    SocketTransport              transport;
    std::shared_ptr<Metrics>     metrics;
    std::atomic<ConnectionState> state{ConnectionState::Disconnected};
    int                          defaultQos{0};
    bool                         defaultRetained{false};

    // Packet ID counter (1–65535, wraps, skips 0)
    std::atomic<uint16_t>        nextPacketId{1};
    std::mutex                   sendMtx;
    std::chrono::steady_clock::time_point lastSend;

    // PUBACK tracking: packet-id → promise<ErrorCode>
    std::mutex                                             pubackMtx;
    std::unordered_map<uint16_t, std::promise<ErrorCode>> pendingPubacks;

    // Background threads
    std::jthread recvThread;
    std::jthread keepaliveThread;
    std::jthread reconnectThread;
    std::mutex                   reconnectMtx;
    std::condition_variable      reconnectCv;
    bool                         reconnectRequested{false};

    // ── Helpers ───────────────────────────────────────────────────────────────

    void transitionState(ConnectionState s) {
        const auto prev = state.exchange(s);
        if (prev != s)
            MQTT_LOG_DEBUG("Publisher: {} → {}", toString(prev), toString(s));
    }

    uint16_t allocPacketId() {
        uint16_t id = nextPacketId.fetch_add(1);
        if (id == 0) id = nextPacketId.fetch_add(1);
        return id;
    }

    void failAllPending() {
        std::lock_guard lock(pubackMtx);
        for (auto& [id, p] : pendingPubacks) {
            try { p.set_value(ErrorCode::ConnectionLost); } catch (...) {}
        }
        pendingPubacks.clear();
    }

    // ── Thread launchers ──────────────────────────────────────────────────────

    void startRecvLoop(MqttPublisher& owner);
    void startKeepalive();
    void scheduleReconnect(MqttPublisher& owner);
};

// ── Receive loop (handles PUBACK and PINGRESP) ─────────────────────────────────

void MqttPublisher::Impl::startRecvLoop(MqttPublisher& owner) {
    recvThread = std::jthread([this, &owner](std::stop_token stop) {
        while (!stop.stop_requested()) {
            auto pktR = protocol::readPacket(transport);
            if (!pktR) {
                if (stop.stop_requested()) break;
                MQTT_LOG_WARN("Publisher recv: {}", pktR.error().message());
                metrics->setConnected(false);
                metrics->recordReconnect();
                transitionState(ConnectionState::Reconnecting);
                failAllPending();
                if (config.reconnect.enabled) scheduleReconnect(owner);
                break;
            }

            using T = protocol::PacketType;
            switch (pktR.value().type) {

            case T::PUBACK: {
                uint16_t id = protocol::parsePacketId(pktR.value().body);
                std::lock_guard lock(pubackMtx);
                if (auto it = pendingPubacks.find(id); it != pendingPubacks.end()) {
                    try { it->second.set_value(ErrorCode::Ok); } catch (...) {}
                    pendingPubacks.erase(it);
                }
                break;
            }

            case T::PINGRESP:
                MQTT_LOG_TRACE("Publisher: PINGRESP");
                break;

            default:
                MQTT_LOG_TRACE("Publisher: ignoring packet type={}",
                               static_cast<int>(pktR.value().type));
                break;
            }
        }
    });
}

// ── Keepalive: sends PINGREQ at keepalive/2 intervals ─────────────────────────

void MqttPublisher::Impl::startKeepalive() {
    keepaliveThread = std::jthread([this](std::stop_token stop) {
        const auto interval = std::chrono::seconds{
            config.keepaliveSeconds > 0 ? config.keepaliveSeconds / 2 : 30};

        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds{1});
            if (stop.stop_requested()) break;
            if (state.load() != ConnectionState::Connected) continue;

            if (std::chrono::steady_clock::now() - lastSend >= interval) {
                std::lock_guard lock(sendMtx);
                transport.sendAll(protocol::buildPingreq());
                lastSend = std::chrono::steady_clock::now();
                MQTT_LOG_TRACE("Publisher: PINGREQ sent");
            }
        }
    });
}

// ── Reconnect with exponential back-off + jitter ──────────────────────────────

void MqttPublisher::Impl::scheduleReconnect(MqttPublisher& owner) {
    {
        std::lock_guard lock(reconnectMtx);
        reconnectRequested = true;
        reconnectCv.notify_one();
    }
    if (reconnectThread.joinable()) return;

    reconnectThread = std::jthread([this, &owner](std::stop_token stop) {
        auto delay = config.reconnect.initialDelay;
        uint32_t attempt = 0;
        std::mt19937_64 rng{std::random_device{}()};

        while (!stop.stop_requested()) {
            {
                std::unique_lock lock(reconnectMtx);
                reconnectCv.wait(lock, [&] {
                    return reconnectRequested || stop.stop_requested();
                });
                if (stop.stop_requested()) break;
                reconnectRequested = false;
            }

            if (state.load() == ConnectionState::Connected) break;

            ++attempt;
            if (config.reconnect.maxRetries > 0 && attempt > config.reconnect.maxRetries) {
                MQTT_LOG_ERROR("Publisher: exhausted {} reconnect retries", attempt - 1);
                transitionState(ConnectionState::Failed);
                break;
            }

            MQTT_LOG_INFO("Publisher: reconnect #{} in {}ms", attempt, delay.count());
            std::this_thread::sleep_for(delay);

            if (auto r = owner.connect(); r) {
                MQTT_LOG_INFO("Publisher: reconnected on attempt #{}", attempt);
                break;
            } else {
                MQTT_LOG_WARN("Publisher: reconnect #{} failed: {}", attempt, r.error().message());
            }

            delay = std::chrono::milliseconds{
                static_cast<long long>(delay.count() * config.reconnect.backoffMultiplier)};
            if (delay > config.reconnect.maxDelay) delay = config.reconnect.maxDelay;
            if (config.reconnect.jitter) {
                std::uniform_int_distribution<long long> jit(0, delay.count() / 4);
                delay += std::chrono::milliseconds{jit(rng)};
            }

            std::lock_guard lock(reconnectMtx);
            reconnectRequested = true;
            reconnectCv.notify_one();
        }
    });
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

MqttPublisher::MqttPublisher(ConnectionConfig config, TlsConfig tlsConfig)
    : impl_(std::make_unique<Impl>())
{
    impl_->config    = std::move(config);
    impl_->tlsConfig = std::move(tlsConfig);
    impl_->metrics   = std::make_shared<Metrics>();

    if (impl_->config.brokerUrls.empty())
        throw std::invalid_argument("MqttPublisher: brokerUrls must not be empty");

    if (impl_->config.clientId.empty()) {
        impl_->config.clientId = "mqtt-pub-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    MQTT_LOG_INFO("MqttPublisher created broker={} clientId={}",
                  impl_->config.brokerUrls[0], impl_->config.clientId);
}

MqttPublisher::~MqttPublisher() {
    if (isConnected()) {
        try { disconnect(std::chrono::milliseconds{1000}); } catch (...) {}
    }
    impl_->transport.requestStop();
    // jthreads join on destruction; request stop so they exit quickly.
    impl_->recvThread.request_stop();
    impl_->keepaliveThread.request_stop();
    if (impl_->reconnectThread.joinable()) {
        impl_->reconnectThread.request_stop();
        impl_->reconnectCv.notify_all();
    }
}

// ── connect ───────────────────────────────────────────────────────────────────

Result<void> MqttPublisher::connect() {
    if (impl_->state == ConnectionState::Connected)
        return Result<void>::err(ConnectionException{ErrorCode::AlreadyConnected, "Already connected"});

    impl_->transitionState(ConnectionState::Connecting);

    const auto addr = parseBrokerUrl(impl_->config.brokerUrls[0]);
    auto tlsCfg = impl_->tlsConfig;
    if (addr.useTls) tlsCfg.enabled = true;

    if (auto r = impl_->transport.connect(addr.host, addr.port, tlsCfg,
                                           impl_->config.connectTimeout); !r) {
        impl_->transitionState(ConnectionState::Failed);
        return r;
    }

    // ── Send CONNECT ──────────────────────────────────────────────────────────
    protocol::ConnectParams cp;
    cp.clientId         = impl_->config.clientId;
    cp.cleanSession     = impl_->config.cleanSession;
    cp.keepaliveSeconds = impl_->config.keepaliveSeconds;
    if (impl_->config.username) cp.username = impl_->config.username;
    if (impl_->config.password) {
        cp.password = std::vector<uint8_t>{
            impl_->config.password->begin(), impl_->config.password->end()};
    }
    if (impl_->config.will) {
        protocol::WillParams wp;
        wp.topic    = impl_->config.will->topic;
        wp.payload.assign(impl_->config.will->payload.begin(),
                           impl_->config.will->payload.end());
        wp.qos      = impl_->config.will->qos;
        wp.retained = impl_->config.will->retained;
        cp.will = wp;
    }

    {
        std::lock_guard lock(impl_->sendMtx);
        if (auto r = impl_->transport.sendAll(protocol::buildConnect(cp)); !r) {
            impl_->transitionState(ConnectionState::Failed);
            return r;
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    // ── Wait for CONNACK (before recv loop starts) ────────────────────────────
    auto pktR = protocol::readPacket(impl_->transport);
    if (!pktR) {
        impl_->transitionState(ConnectionState::Failed);
        return Result<void>::err(std::move(pktR).error());
    }
    if (pktR.value().type != protocol::PacketType::CONNACK) {
        impl_->transitionState(ConnectionState::Failed);
        return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
            "Expected CONNACK, got packet type " +
            std::to_string(static_cast<int>(pktR.value().type))});
    }
    const auto connack = protocol::parseConnack(pktR.value().body);
    if (connack.returnCode != 0) {
        impl_->transitionState(ConnectionState::Failed);
        return Result<void>::err(ConnectionException{ErrorCode::ConnectionFailed,
            "CONNACK refused, return code " + std::to_string(connack.returnCode)});
    }

    impl_->transitionState(ConnectionState::Connected);
    impl_->metrics->setConnected(true);
    impl_->startRecvLoop(*this);
    impl_->startKeepalive();

    MQTT_LOG_INFO("MqttPublisher connected to {}", impl_->config.brokerUrls[0]);
    return Result<void>::ok();
}

// ── disconnect ────────────────────────────────────────────────────────────────

Result<void> MqttPublisher::disconnect(std::chrono::milliseconds /*quiesce*/) {
    if (!isConnected()) return Result<void>::ok();

    impl_->transitionState(ConnectionState::Disconnecting);

    {
        std::lock_guard lock(impl_->sendMtx);
        impl_->transport.sendAll(protocol::buildDisconnect());
    }

    impl_->transport.requestStop();
    impl_->recvThread.request_stop();
    impl_->keepaliveThread.request_stop();
    impl_->transport.disconnect();
    impl_->transitionState(ConnectionState::Disconnected);
    impl_->metrics->setConnected(false);
    impl_->failAllPending();

    return Result<void>::ok();
}

Result<void> MqttPublisher::reconnect() {
    if (isConnected()) disconnect();
    return connect();
}

// ── publish (synchronous) ─────────────────────────────────────────────────────

Result<void> MqttPublisher::publish(const Message& msg) {
    if (!isConnected())
        return Result<void>::err(PublishException{ErrorCode::NotConnected, "Not connected"});

    const auto t0  = std::chrono::steady_clock::now();
    const int  qos = msg.qos > 0 ? msg.qos : impl_->defaultQos;

    uint16_t               id = 0;
    std::future<ErrorCode> fut;

    if (qos > 0) {
        id = impl_->allocPacketId();
        std::promise<ErrorCode> prom;
        fut = prom.get_future();
        std::lock_guard lock(impl_->pubackMtx);
        impl_->pendingPubacks[id] = std::move(prom);
    }

    {
        std::lock_guard lock(impl_->sendMtx);
        auto pkt = protocol::buildPublish(msg.topic, msg.payload, qos,
                                           msg.retained, msg.duplicate, id);
        if (auto r = impl_->transport.sendAll(pkt); !r) {
            if (qos > 0) {
                std::lock_guard pl(impl_->pubackMtx);
                impl_->pendingPubacks.erase(id);
            }
            impl_->metrics->recordPublishFail();
            return Result<void>::err(PublishException{ErrorCode::PublishFailed, r.error().message()});
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    if (qos > 0) {
        if (fut.wait_for(impl_->config.ackTimeout) == std::future_status::timeout) {
            std::lock_guard lock(impl_->pubackMtx);
            impl_->pendingPubacks.erase(id);
            impl_->metrics->recordPublishFail();
            return Result<void>::err(PublishException{ErrorCode::PublishTimeout,
                "PUBACK timed out for topic: " + msg.topic});
        }
        if (const auto ec = fut.get(); ec != ErrorCode::Ok) {
            impl_->metrics->recordPublishFail();
            return Result<void>::err(PublishException{ec, "Publish failed"});
        }
    }

    impl_->metrics->recordPublished();
    impl_->metrics->recordPublishLatency(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0));

    return Result<void>::ok();
}

// ── publishAsync ──────────────────────────────────────────────────────────────

Result<int> MqttPublisher::publishAsync(Message msg, PublishAckCallback cb) {
    if (!isConnected() && !impl_->config.offlineBufferingEnabled)
        return Result<int>::err(PublishException{ErrorCode::NotConnected, "Not connected"});

    const int      qos = msg.qos > 0 ? msg.qos : impl_->defaultQos;
    const uint16_t id  = impl_->allocPacketId();

    if (qos > 0 && cb) {
        std::promise<ErrorCode> prom;
        auto sharedFut = prom.get_future().share();
        {
            std::lock_guard lock(impl_->pubackMtx);
            impl_->pendingPubacks[id] = std::move(prom);
        }
        // Deliver callback on a detached thread so the caller isn't blocked.
        std::thread([sharedFut, cb = std::move(cb), id]() mutable {
            try { cb(id, sharedFut.get()); }
            catch (...) {}
        }).detach();
    }

    {
        std::lock_guard lock(impl_->sendMtx);
        auto pkt = protocol::buildPublish(msg.topic, msg.payload, qos,
                                           msg.retained, msg.duplicate, id);
        if (auto r = impl_->transport.sendAll(pkt); !r) {
            impl_->metrics->recordPublishFail();
            if (cb) cb(id, ErrorCode::PublishFailed);
            return Result<int>::err(PublishException{ErrorCode::PublishFailed, r.error().message()});
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    impl_->metrics->recordPublished();
    return Result<int>::ok(static_cast<int>(id));
}

// ── flush / accessors ─────────────────────────────────────────────────────────

Result<void> MqttPublisher::flush(std::chrono::milliseconds /*timeout*/) {
    return Result<void>::ok();
}

void MqttPublisher::setDefaultQoS(int qos)           { impl_->defaultQos      = qos;      }
void MqttPublisher::setDefaultRetained(bool retained) { impl_->defaultRetained = retained; }

ConnectionState MqttPublisher::state() const noexcept  { return impl_->state.load(); }
bool MqttPublisher::isConnected() const noexcept {
    return impl_->state.load() == ConnectionState::Connected;
}
std::shared_ptr<Metrics> MqttPublisher::metrics() const noexcept { return impl_->metrics; }

} // namespace mqtt
