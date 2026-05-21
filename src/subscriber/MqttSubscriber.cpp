#include "mqtt/subscriber/MqttSubscriber.hpp"
#include "mqtt/logging/Logger.hpp"
#include "mqtt/monitoring/Metrics.hpp"
#include "mqtt/protocol/MqttPacket.hpp"
#include "mqtt/transport/SocketTransport.hpp"

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace mqtt {

// ── PIMPL ─────────────────────────────────────────────────────────────────────

struct MqttSubscriber::Impl {
    ConnectionConfig config;
    TlsConfig        tlsConfig;

    SocketTransport              transport;
    std::shared_ptr<Metrics>     metrics;
    std::atomic<ConnectionState> state{ConnectionState::Disconnected};

    // Per-topic subscriptions
    struct TopicSub { int qos; MessageCallback callback; };
    mutable std::shared_mutex                             subsMtx;
    std::unordered_map<std::string, TopicSub>             subscriptions;
    MessageCallback                                       fallbackCallback;

    // Packet IDs
    std::atomic<uint16_t>        nextPacketId{1};
    std::mutex                   sendMtx;
    std::chrono::steady_clock::time_point lastSend;

    // SUBACK / UNSUBACK promises
    std::mutex                                              ackMtx;
    std::unordered_map<uint16_t, std::promise<uint8_t>>    pendingSubacks;   // rc
    std::unordered_map<uint16_t, std::promise<void>>       pendingUnsubacks;

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
            MQTT_LOG_DEBUG("Subscriber: {} → {}", toString(prev), toString(s));
    }

    uint16_t allocPacketId() {
        uint16_t id = nextPacketId.fetch_add(1);
        if (id == 0) id = nextPacketId.fetch_add(1);
        return id;
    }

    // MQTT wildcard filter matching (RFC 4.7)
    static bool topicMatchesFilter(const std::string& topic, const std::string& filter) {
        size_t ti = 0, fi = 0;
        while (fi < filter.size() && ti < topic.size()) {
            if (filter[fi] == '#') return true;
            if (filter[fi] == '+') {
                while (ti < topic.size() && topic[ti] != '/') ++ti;
                ++fi;
            } else if (filter[fi] == topic[ti]) {
                ++fi; ++ti;
            } else {
                return false;
            }
        }
        if (fi < filter.size() && filter[fi] == '#') return true;
        return fi == filter.size() && ti == topic.size();
    }

    void failAllAcks() {
        std::lock_guard lock(ackMtx);
        for (auto& [id, p] : pendingSubacks)
            try { p.set_value(0x80); } catch (...) {}
        pendingSubacks.clear();

        for (auto& [id, p] : pendingUnsubacks)
            try { p.set_exception(std::make_exception_ptr(
                ConnectionException{ErrorCode::ConnectionLost, "Disconnected"})); } catch (...) {}
        pendingUnsubacks.clear();
    }

    void dispatchPublish(const protocol::PublishInfo& info) {
        metrics->recordReceived();

        // QoS 1: ACK immediately before dispatching to avoid redelivery.
        if (info.qos == 1) {
            std::lock_guard lock(sendMtx);
            transport.sendAll(protocol::buildPuback(info.packetId));
        }

        Message msg;
        msg.topic     = info.topic;
        msg.payload   = info.payload;
        msg.qos       = info.qos;
        msg.retained  = info.retained;
        msg.duplicate = info.dup;

        MessageCallback cb;
        {
            std::shared_lock lock(subsMtx);
            if (auto it = subscriptions.find(msg.topic); it != subscriptions.end()) {
                cb = it->second.callback;
            } else {
                for (const auto& [pattern, sub] : subscriptions) {
                    if (topicMatchesFilter(msg.topic, pattern)) {
                        cb = sub.callback;
                        break;
                    }
                }
                if (!cb && fallbackCallback) cb = fallbackCallback;
            }
        }

        if (cb) {
            try { cb(std::move(msg)); }
            catch (const std::exception& e) {
                MQTT_LOG_ERROR("Subscriber: callback exception on '{}': {}", info.topic, e.what());
            }
        } else {
            MQTT_LOG_WARN("Subscriber: no handler for topic '{}'", info.topic);
            metrics->recordDropped();
        }
    }

    void resubscribeAll() {
        std::shared_lock lock(subsMtx);
        for (const auto& [topic, sub] : subscriptions) {
            const uint16_t id = allocPacketId();
            auto pkt = protocol::buildSubscribe(id, {{topic, sub.qos}});
            std::lock_guard sl(sendMtx);
            transport.sendAll(pkt);
            MQTT_LOG_INFO("Subscriber: re-subscribed '{}' QoS={}", topic, sub.qos);
        }
    }

    void startRecvLoop(MqttSubscriber& owner);
    void startKeepalive();
    void scheduleReconnect(MqttSubscriber& owner);
};

// ── Receive loop (handles PUBLISH, SUBACK, UNSUBACK, PINGRESP) ─────────────────

void MqttSubscriber::Impl::startRecvLoop(MqttSubscriber& owner) {
    recvThread = std::jthread([this, &owner](std::stop_token stop) {
        while (!stop.stop_requested()) {
            auto pktR = protocol::readPacket(transport);
            if (!pktR) {
                if (stop.stop_requested()) break;
                MQTT_LOG_WARN("Subscriber recv: {}", pktR.error().message());
                metrics->setConnected(false);
                metrics->recordReconnect();
                transitionState(ConnectionState::Reconnecting);
                failAllAcks();
                if (config.reconnect.enabled) scheduleReconnect(owner);
                break;
            }

            using T = protocol::PacketType;
            const auto& pkt = pktR.value();

            switch (pkt.type) {

            case T::PUBLISH:
                try {
                    dispatchPublish(protocol::parsePublish(pkt.flags, pkt.body));
                } catch (const std::exception& e) {
                    MQTT_LOG_ERROR("Subscriber: PUBLISH parse error: {}", e.what());
                }
                break;

            case T::SUBACK:
                try {
                    auto info = protocol::parseSuback(pkt.body);
                    std::lock_guard lock(ackMtx);
                    if (auto it = pendingSubacks.find(info.packetId);
                        it != pendingSubacks.end()) {
                        uint8_t rc = info.returnCodes.empty() ? 0x80 : info.returnCodes[0];
                        try { it->second.set_value(rc); } catch (...) {}
                        pendingSubacks.erase(it);
                    }
                } catch (const std::exception& e) {
                    MQTT_LOG_ERROR("Subscriber: SUBACK parse error: {}", e.what());
                }
                break;

            case T::UNSUBACK: {
                uint16_t id = protocol::parsePacketId(pkt.body);
                std::lock_guard lock(ackMtx);
                if (auto it = pendingUnsubacks.find(id); it != pendingUnsubacks.end()) {
                    try { it->second.set_value(); } catch (...) {}
                    pendingUnsubacks.erase(it);
                }
                break;
            }

            case T::PINGRESP:
                MQTT_LOG_TRACE("Subscriber: PINGRESP");
                break;

            default:
                MQTT_LOG_TRACE("Subscriber: ignoring packet type={}",
                               static_cast<int>(pkt.type));
                break;
            }
        }
    });
}

// ── Keepalive ─────────────────────────────────────────────────────────────────

void MqttSubscriber::Impl::startKeepalive() {
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
                MQTT_LOG_TRACE("Subscriber: PINGREQ sent");
            }
        }
    });
}

// ── Reconnect ─────────────────────────────────────────────────────────────────

void MqttSubscriber::Impl::scheduleReconnect(MqttSubscriber& owner) {
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
                MQTT_LOG_ERROR("Subscriber: exhausted {} reconnect retries", attempt - 1);
                transitionState(ConnectionState::Failed);
                break;
            }

            MQTT_LOG_INFO("Subscriber: reconnect #{} in {}ms", attempt, delay.count());
            std::this_thread::sleep_for(delay);

            if (auto r = owner.connect(); r) {
                MQTT_LOG_INFO("Subscriber: reconnected on attempt #{}", attempt);
                break;
            } else {
                MQTT_LOG_WARN("Subscriber: reconnect #{} failed: {}", attempt, r.error().message());
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

MqttSubscriber::MqttSubscriber(ConnectionConfig config, TlsConfig tlsConfig)
    : impl_(std::make_unique<Impl>())
{
    impl_->config    = std::move(config);
    impl_->tlsConfig = std::move(tlsConfig);
    impl_->metrics   = std::make_shared<Metrics>();

    if (impl_->config.brokerUrls.empty())
        throw std::invalid_argument("MqttSubscriber: brokerUrls must not be empty");

    if (impl_->config.clientId.empty()) {
        impl_->config.clientId = "mqtt-sub-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    MQTT_LOG_INFO("MqttSubscriber created broker={} clientId={}",
                  impl_->config.brokerUrls[0], impl_->config.clientId);
}

MqttSubscriber::~MqttSubscriber() {
    try { stopListening(); } catch (...) {}
    if (isConnected()) {
        try { disconnect(std::chrono::milliseconds{1000}); } catch (...) {}
    }
    impl_->transport.requestStop();
    impl_->recvThread.request_stop();
    impl_->keepaliveThread.request_stop();
    if (impl_->reconnectThread.joinable()) {
        impl_->reconnectThread.request_stop();
        impl_->reconnectCv.notify_all();
    }
}

// ── connect ───────────────────────────────────────────────────────────────────

Result<void> MqttSubscriber::connect() {
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

    {
        std::lock_guard lock(impl_->sendMtx);
        if (auto r = impl_->transport.sendAll(protocol::buildConnect(cp)); !r) {
            impl_->transitionState(ConnectionState::Failed);
            return r;
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    // ── Read CONNACK ──────────────────────────────────────────────────────────
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
    impl_->resubscribeAll();   // re-subscribe on reconnect (no-op on fresh connect)
    impl_->startRecvLoop(*this);
    impl_->startKeepalive();

    MQTT_LOG_INFO("MqttSubscriber connected to {}", impl_->config.brokerUrls[0]);
    return Result<void>::ok();
}

// ── disconnect ────────────────────────────────────────────────────────────────

Result<void> MqttSubscriber::disconnect(std::chrono::milliseconds /*quiesce*/) {
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

    return Result<void>::ok();
}

Result<void> MqttSubscriber::reconnect() {
    if (isConnected()) disconnect();
    return connect();
}

// ── subscribe ─────────────────────────────────────────────────────────────────

Result<void> MqttSubscriber::subscribe(const std::string& topic, int qos,
                                        MessageCallback callback) {
    {
        std::unique_lock lock(impl_->subsMtx);
        impl_->subscriptions[topic] = {qos, callback};
    }
    if (!isConnected()) return Result<void>::ok();

    const uint16_t id = impl_->allocPacketId();
    std::promise<uint8_t> prom;
    auto fut = prom.get_future();
    {
        std::lock_guard lock(impl_->ackMtx);
        impl_->pendingSubacks[id] = std::move(prom);
    }

    {
        std::lock_guard lock(impl_->sendMtx);
        auto pkt = protocol::buildSubscribe(id, {{topic, qos}});
        if (auto r = impl_->transport.sendAll(pkt); !r) {
            std::lock_guard al(impl_->ackMtx);
            impl_->pendingSubacks.erase(id);
            std::unique_lock sl(impl_->subsMtx);
            impl_->subscriptions.erase(topic);
            return Result<void>::err(SubscribeException{ErrorCode::SubscribeFailed, r.error().message()});
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    if (fut.wait_for(impl_->config.ackTimeout) == std::future_status::timeout) {
        std::lock_guard lock(impl_->ackMtx);
        impl_->pendingSubacks.erase(id);
        return Result<void>::err(SubscribeException{ErrorCode::SubscribeFailed,
            "SUBACK timed out for: " + topic});
    }

    const uint8_t rc = fut.get();
    if (rc == 0x80) {
        std::unique_lock sl(impl_->subsMtx);
        impl_->subscriptions.erase(topic);
        return Result<void>::err(SubscribeException{ErrorCode::SubscribeFailed,
            "Broker rejected subscription to: " + topic});
    }

    MQTT_LOG_INFO("Subscriber: subscribed '{}' QoS={}", topic, qos);
    return Result<void>::ok();
}

Result<void> MqttSubscriber::subscribe(const std::vector<std::string>& topics,
                                        int qos, MessageCallback callback) {
    for (const auto& t : topics) {
        if (auto r = subscribe(t, qos, callback); !r) return r;
    }
    return Result<void>::ok();
}

// ── unsubscribe ───────────────────────────────────────────────────────────────

Result<void> MqttSubscriber::unsubscribe(const std::string& topic) {
    { std::unique_lock lock(impl_->subsMtx); impl_->subscriptions.erase(topic); }
    if (!isConnected()) return Result<void>::ok();

    const uint16_t id = impl_->allocPacketId();
    std::promise<void> prom;
    auto fut = prom.get_future();
    {
        std::lock_guard lock(impl_->ackMtx);
        impl_->pendingUnsubacks[id] = std::move(prom);
    }

    {
        std::lock_guard lock(impl_->sendMtx);
        auto pkt = protocol::buildUnsubscribe(id, {topic});
        if (auto r = impl_->transport.sendAll(pkt); !r) {
            std::lock_guard al(impl_->ackMtx);
            impl_->pendingUnsubacks.erase(id);
            return Result<void>::err(SubscribeException{ErrorCode::UnsubscribeFailed,
                r.error().message()});
        }
        impl_->lastSend = std::chrono::steady_clock::now();
    }

    if (fut.wait_for(impl_->config.ackTimeout) == std::future_status::timeout) {
        std::lock_guard lock(impl_->ackMtx);
        impl_->pendingUnsubacks.erase(id);
        return Result<void>::err(SubscribeException{ErrorCode::UnsubscribeFailed,
            "UNSUBACK timed out for: " + topic});
    }

    fut.get(); // propagates exception on disconnect
    return Result<void>::ok();
}

Result<void> MqttSubscriber::unsubscribe(const std::vector<std::string>& topics) {
    for (const auto& t : topics) {
        if (auto r = unsubscribe(t); !r) return r;
    }
    return Result<void>::ok();
}

void MqttSubscriber::registerFallbackCallback(MessageCallback callback) {
    std::unique_lock lock(impl_->subsMtx);
    impl_->fallbackCallback = std::move(callback);
}

// ── startListening / stopListening (no-ops: recv loop runs from connect()) ────

Result<void> MqttSubscriber::startListening() { return Result<void>::ok(); }
Result<void> MqttSubscriber::stopListening()  { return Result<void>::ok(); }

// ── Accessors ─────────────────────────────────────────────────────────────────

ConnectionState MqttSubscriber::state() const noexcept { return impl_->state.load(); }
bool MqttSubscriber::isConnected() const noexcept {
    return impl_->state.load() == ConnectionState::Connected;
}
std::shared_ptr<Metrics> MqttSubscriber::metrics() const noexcept { return impl_->metrics; }

} // namespace mqtt
