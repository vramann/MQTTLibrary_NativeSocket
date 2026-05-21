#pragma once

#include "mqtt/core/Error.hpp"
#include "mqtt/core/Message.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mqtt {
class SocketTransport;
} // forward declare

namespace mqtt::protocol {

// MQTT 3.1.1 packet type nibble values (upper 4 bits of fixed header byte 1).
enum class PacketType : uint8_t {
    CONNECT     = 1,
    CONNACK     = 2,
    PUBLISH     = 3,
    PUBACK      = 4,
    PUBREC      = 5,
    PUBREL      = 6,
    PUBCOMP     = 7,
    SUBSCRIBE   = 8,
    SUBACK      = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK    = 11,
    PINGREQ     = 12,
    PINGRESP    = 13,
    DISCONNECT  = 14,
};

// ── CONNECT builder ────────────────────────────────────────────────────────────

struct WillParams {
    std::string          topic;
    std::vector<uint8_t> payload;
    int                  qos{1};
    bool                 retained{false};
};

struct ConnectParams {
    std::string                         clientId;
    bool                                cleanSession{true};
    uint16_t                            keepaliveSeconds{60};
    std::optional<std::string>          username;
    std::optional<std::vector<uint8_t>> password;
    std::optional<WillParams>           will;
};

// ── Serialisers ────────────────────────────────────────────────────────────────

std::vector<uint8_t> buildConnect(const ConnectParams& p);

std::vector<uint8_t> buildPublish(const std::string& topic,
                                   const Payload& payload,
                                   int qos, bool retained, bool dup,
                                   uint16_t packetId);

std::vector<uint8_t> buildPuback(uint16_t packetId);

std::vector<uint8_t> buildSubscribe(uint16_t packetId,
                                     const std::vector<std::pair<std::string, int>>& subs);

std::vector<uint8_t> buildUnsubscribe(uint16_t packetId,
                                       const std::vector<std::string>& topics);

std::vector<uint8_t> buildPingreq();
std::vector<uint8_t> buildDisconnect();

// ── Incoming packet ────────────────────────────────────────────────────────────

struct IncomingPacket {
    PacketType           type{PacketType::CONNECT};
    uint8_t              flags{0};
    std::vector<uint8_t> body;
};

// Reads exactly one MQTT packet from the transport (blocks until complete).
Result<IncomingPacket> readPacket(SocketTransport& t);

// ── Deserialisers ──────────────────────────────────────────────────────────────

struct ConnackInfo {
    bool    sessionPresent{false};
    uint8_t returnCode{0};
};
ConnackInfo parseConnack(const std::vector<uint8_t>& body);

struct PublishInfo {
    std::string topic;
    uint16_t    packetId{0};
    Payload     payload;
    int         qos{0};
    bool        retained{false};
    bool        dup{false};
};
PublishInfo parsePublish(uint8_t flags, const std::vector<uint8_t>& body);

struct SubackInfo {
    uint16_t             packetId{0};
    std::vector<uint8_t> returnCodes;
};
SubackInfo parseSuback(const std::vector<uint8_t>& body);

// Returns the 2-byte packet identifier from the start of a variable header.
uint16_t parsePacketId(const std::vector<uint8_t>& body);

} // namespace mqtt::protocol
