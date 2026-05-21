#include "mqtt/protocol/MqttPacket.hpp"
#include "mqtt/transport/SocketTransport.hpp"
#include "mqtt/logging/Logger.hpp"

namespace mqtt::protocol {

// ── Wire-encoding helpers ─────────────────────────────────────────────────────

static void appendVarLen(std::vector<uint8_t>& buf, uint32_t value) {
    do {
        uint8_t b = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value > 0) b |= 0x80;
        buf.push_back(b);
    } while (value > 0);
}

static void appendU16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

static void appendMqttString(std::vector<uint8_t>& buf, const std::string& s) {
    appendU16(buf, static_cast<uint16_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

static void prependFixedHeader(std::vector<uint8_t>& body, uint8_t typeByte) {
    std::vector<uint8_t> hdr;
    hdr.push_back(typeByte);
    appendVarLen(hdr, static_cast<uint32_t>(body.size()));
    hdr.insert(hdr.end(), body.begin(), body.end());
    body = std::move(hdr);
}

// ── CONNECT ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> buildConnect(const ConnectParams& p) {
    std::vector<uint8_t> body;

    // Variable header
    appendMqttString(body, "MQTT");         // Protocol Name (MQTT 3.1.1)
    body.push_back(0x04);                   // Protocol Level

    uint8_t flags = 0;
    if (p.cleanSession)  flags |= 0x02;
    if (p.will) {
        flags |= 0x04;
        flags |= static_cast<uint8_t>((p.will->qos & 0x03) << 3);
        if (p.will->retained) flags |= 0x20;
    }
    if (p.username) flags |= 0x80;
    if (p.password) flags |= 0x40;
    body.push_back(flags);

    appendU16(body, p.keepaliveSeconds);

    // Payload
    appendMqttString(body, p.clientId);
    if (p.will) {
        appendMqttString(body, p.will->topic);
        appendU16(body, static_cast<uint16_t>(p.will->payload.size()));
        body.insert(body.end(), p.will->payload.begin(), p.will->payload.end());
    }
    if (p.username) appendMqttString(body, *p.username);
    if (p.password) {
        appendU16(body, static_cast<uint16_t>(p.password->size()));
        body.insert(body.end(), p.password->begin(), p.password->end());
    }

    prependFixedHeader(body, 0x10);
    return body;
}

// ── PUBLISH ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> buildPublish(const std::string& topic,
                                   const Payload& payload,
                                   int qos, bool retained, bool dup,
                                   uint16_t packetId) {
    std::vector<uint8_t> body;
    appendMqttString(body, topic);
    if (qos > 0) appendU16(body, packetId);
    body.insert(body.end(), payload.begin(), payload.end());

    uint8_t b0 = 0x30;
    if (dup)      b0 |= 0x08;
    b0 |= static_cast<uint8_t>((qos & 0x03) << 1);
    if (retained) b0 |= 0x01;

    prependFixedHeader(body, b0);
    return body;
}

// ── PUBACK ────────────────────────────────────────────────────────────────────

std::vector<uint8_t> buildPuback(uint16_t packetId) {
    return {
        0x40, 0x02,
        static_cast<uint8_t>(packetId >> 8),
        static_cast<uint8_t>(packetId & 0xFF)
    };
}

// ── SUBSCRIBE ─────────────────────────────────────────────────────────────────

std::vector<uint8_t> buildSubscribe(uint16_t packetId,
                                     const std::vector<std::pair<std::string, int>>& subs) {
    std::vector<uint8_t> body;
    appendU16(body, packetId);
    for (const auto& [topic, qos] : subs) {
        appendMqttString(body, topic);
        body.push_back(static_cast<uint8_t>(qos & 0x03));
    }
    prependFixedHeader(body, 0x82);
    return body;
}

// ── UNSUBSCRIBE ───────────────────────────────────────────────────────────────

std::vector<uint8_t> buildUnsubscribe(uint16_t packetId,
                                       const std::vector<std::string>& topics) {
    std::vector<uint8_t> body;
    appendU16(body, packetId);
    for (const auto& t : topics) appendMqttString(body, t);
    prependFixedHeader(body, 0xA2);
    return body;
}

// ── Zero-payload control packets ──────────────────────────────────────────────

std::vector<uint8_t> buildPingreq()    { return {0xC0, 0x00}; }
std::vector<uint8_t> buildDisconnect() { return {0xE0, 0x00}; }

// ── Read a complete packet from the transport ─────────────────────────────────

Result<IncomingPacket> readPacket(SocketTransport& t) {
    // Fixed header byte 1: packet type + flags
    auto b0r = t.recvByte();
    if (!b0r) return Result<IncomingPacket>::err(b0r.error());

    const uint8_t b0  = b0r.value();
    const auto    type  = static_cast<PacketType>(b0 >> 4);
    const uint8_t flags = b0 & 0x0F;

    // Remaining length: variable-length encoding, 1–4 bytes
    uint32_t remaining  = 0;
    uint32_t multiplier = 1;
    for (int i = 0; i < 4; ++i) {
        auto br = t.recvByte();
        if (!br) return Result<IncomingPacket>::err(br.error());
        const uint8_t b = br.value();
        remaining += (b & 0x7F) * multiplier;
        multiplier *= 128;
        if ((b & 0x80) == 0) break;
        if (i == 3) {
            return Result<IncomingPacket>::err(SerializationException{
                ErrorCode::DeserializationFailed, "Malformed remaining-length field"});
        }
    }

    // Body
    std::vector<uint8_t> body;
    if (remaining > 0) {
        auto bodyR = t.recvExact(remaining);
        if (!bodyR) return Result<IncomingPacket>::err(bodyR.error());
        body = std::move(bodyR.value());
    }

    MQTT_LOG_TRACE("readPacket type={} flags={:#x} remaining={}",
                   static_cast<int>(type), flags, remaining);
    return Result<IncomingPacket>::ok(IncomingPacket{type, flags, std::move(body)});
}

// ── Deserialisers ─────────────────────────────────────────────────────────────

ConnackInfo parseConnack(const std::vector<uint8_t>& body) {
    if (body.size() < 2)
        throw SerializationException{ErrorCode::DeserializationFailed, "CONNACK body too short"};
    return {static_cast<bool>(body[0] & 0x01), body[1]};
}

PublishInfo parsePublish(uint8_t flags, const std::vector<uint8_t>& body) {
    PublishInfo p;
    p.dup      = (flags & 0x08) != 0;
    p.qos      = (flags >> 1) & 0x03;
    p.retained = (flags & 0x01) != 0;

    size_t pos = 0;
    if (body.size() < 2)
        throw SerializationException{ErrorCode::DeserializationFailed, "PUBLISH body too short"};

    const uint16_t topicLen =
        (static_cast<uint16_t>(body[pos]) << 8) | body[pos + 1];
    pos += 2;

    if (pos + topicLen > body.size())
        throw SerializationException{ErrorCode::DeserializationFailed, "PUBLISH topic overflow"};

    p.topic.assign(reinterpret_cast<const char*>(body.data() + pos), topicLen);
    pos += topicLen;

    if (p.qos > 0) {
        if (pos + 2 > body.size())
            throw SerializationException{ErrorCode::DeserializationFailed, "PUBLISH missing packet ID"};
        p.packetId = (static_cast<uint16_t>(body[pos]) << 8) | body[pos + 1];
        pos += 2;
    }

    p.payload.assign(body.begin() + static_cast<ptrdiff_t>(pos), body.end());
    return p;
}

SubackInfo parseSuback(const std::vector<uint8_t>& body) {
    if (body.size() < 3)
        throw SerializationException{ErrorCode::DeserializationFailed, "SUBACK body too short"};
    SubackInfo s;
    s.packetId = (static_cast<uint16_t>(body[0]) << 8) | body[1];
    s.returnCodes.assign(body.begin() + 2, body.end());
    return s;
}

uint16_t parsePacketId(const std::vector<uint8_t>& body) {
    if (body.size() < 2) return 0;
    return (static_cast<uint16_t>(body[0]) << 8) | body[1];
}

} // namespace mqtt::protocol
