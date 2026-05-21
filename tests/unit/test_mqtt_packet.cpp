#include "mqtt/protocol/MqttPacket.hpp"

#include <gtest/gtest.h>
#include <string>

namespace mqtt::test {

using namespace mqtt::protocol;

// ── Helpers ────────────────────────────────────────────────────────────────────

// Skip over the variable-length remaining-length field; returns index of first body byte.
static size_t skipFixedHeader(const std::vector<uint8_t>& pkt) {
    size_t pos = 1; // skip first byte (type+flags)
    while (pkt[pos] & 0x80) ++pos;
    return pos + 1;
}

static uint16_t readU16(const std::vector<uint8_t>& pkt, size_t pos) {
    return static_cast<uint16_t>((static_cast<uint16_t>(pkt[pos]) << 8) | pkt[pos + 1]);
}

// ── buildConnect ──────────────────────────────────────────────────────────────

TEST(MqttPacketTest, BuildConnectFixedHeader) {
    ConnectParams p;
    p.clientId = "test";
    auto pkt = buildConnect(p);
    EXPECT_EQ(pkt[0], 0x10); // CONNECT
}

TEST(MqttPacketTest, BuildConnectProtocolNameAndLevel) {
    ConnectParams p;
    p.clientId = "c";
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt);
    // Protocol Name: 0x00 0x04 'M' 'Q' 'T' 'T'
    EXPECT_EQ(pkt[pos + 0], 0x00);
    EXPECT_EQ(pkt[pos + 1], 0x04);
    EXPECT_EQ(pkt[pos + 2], 'M');
    EXPECT_EQ(pkt[pos + 3], 'Q');
    EXPECT_EQ(pkt[pos + 4], 'T');
    EXPECT_EQ(pkt[pos + 5], 'T');
    EXPECT_EQ(pkt[pos + 6], 0x04); // Protocol Level = 4 (MQTT 3.1.1)
}

TEST(MqttPacketTest, BuildConnectCleanSessionFlag) {
    ConnectParams p;
    p.clientId     = "c";
    p.cleanSession = true;
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt) + 7; // after protocol name+level
    EXPECT_TRUE(pkt[pos] & 0x02);          // clean session bit
}

TEST(MqttPacketTest, BuildConnectNoCleanSession) {
    ConnectParams p;
    p.clientId     = "c";
    p.cleanSession = false;
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt) + 7;
    EXPECT_FALSE(pkt[pos] & 0x02);
}

TEST(MqttPacketTest, BuildConnectUsernamePasswordFlags) {
    ConnectParams p;
    p.clientId = "c";
    p.username = "user";
    p.password = std::vector<uint8_t>{'s','e','c','r','e','t'};
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt) + 7;
    EXPECT_TRUE(pkt[pos] & 0x80); // username flag
    EXPECT_TRUE(pkt[pos] & 0x40); // password flag
}

TEST(MqttPacketTest, BuildConnectWillFlag) {
    ConnectParams p;
    p.clientId = "c";
    WillParams will;
    will.topic   = "lw/t";
    will.payload = {'o','f','f'};
    will.qos     = 1;
    p.will = will;
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt) + 7;
    EXPECT_TRUE(pkt[pos]  & 0x04);   // will flag
    EXPECT_EQ((pkt[pos] >> 3) & 0x03, 1u); // will QoS
}

TEST(MqttPacketTest, BuildConnectKeepAlive) {
    ConnectParams p;
    p.clientId          = "c";
    p.keepaliveSeconds  = 120;
    auto pkt = buildConnect(p);
    size_t pos = skipFixedHeader(pkt) + 8; // after flags byte
    EXPECT_EQ(readU16(pkt, pos), 120u);
}

// ── buildPublish ───────────────────────────────────────────────────────────────

TEST(MqttPacketTest, BuildPublishQos0Header) {
    auto pkt = buildPublish("t", {}, 0, false, false, 0);
    EXPECT_EQ(pkt[0], 0x30);
}

TEST(MqttPacketTest, BuildPublishQos1Header) {
    auto pkt = buildPublish("t", {}, 1, false, false, 1);
    EXPECT_EQ(pkt[0], 0x32); // QoS1 flag
}

TEST(MqttPacketTest, BuildPublishRetainedBit) {
    auto pkt = buildPublish("t", {}, 0, true, false, 0);
    EXPECT_TRUE(pkt[0] & 0x01);
}

TEST(MqttPacketTest, BuildPublishDupBit) {
    auto pkt = buildPublish("t", {}, 1, false, true, 1);
    EXPECT_TRUE(pkt[0] & 0x08);
}

TEST(MqttPacketTest, BuildPublishQos1PacketId) {
    const std::string topic = "a/b";
    Payload pl{0xAB};
    auto pkt = buildPublish(topic, pl, 1, false, false, 99);
    size_t pos = skipFixedHeader(pkt);
    uint16_t topicLen = readU16(pkt, pos);
    EXPECT_EQ(topicLen, static_cast<uint16_t>(topic.size()));
    pos += 2 + topicLen;
    EXPECT_EQ(readU16(pkt, pos), 99u); // packet ID
}

TEST(MqttPacketTest, BuildPublishPayloadContent) {
    Payload pl{'h','e','l','l','o'};
    auto pkt = buildPublish("x", pl, 0, false, false, 0);
    size_t pos = skipFixedHeader(pkt);
    pos += 2 + 1; // topic len (2) + "x" (1)
    ASSERT_GE(pkt.size(), pos + 5);
    EXPECT_EQ(std::string(pkt.begin() + static_cast<ptrdiff_t>(pos), pkt.end()), "hello");
}

// ── buildPuback ────────────────────────────────────────────────────────────────

TEST(MqttPacketTest, BuildPuback) {
    auto pkt = buildPuback(0x0102);
    ASSERT_EQ(pkt.size(), 4u);
    EXPECT_EQ(pkt[0], 0x40);
    EXPECT_EQ(pkt[1], 0x02);
    EXPECT_EQ(pkt[2], 0x01);
    EXPECT_EQ(pkt[3], 0x02);
}

// ── buildSubscribe ─────────────────────────────────────────────────────────────

TEST(MqttPacketTest, BuildSubscribeHeader) {
    auto pkt = buildSubscribe(1, {{"t/#", 1}});
    EXPECT_EQ(pkt[0], 0x82);
}

TEST(MqttPacketTest, BuildSubscribePacketIdAndTopic) {
    const std::string topic = "sensor/#";
    auto pkt = buildSubscribe(7, {{topic, 2}});
    size_t pos = skipFixedHeader(pkt);
    EXPECT_EQ(readU16(pkt, pos), 7u); // packet ID
    pos += 2;
    uint16_t topicLen = readU16(pkt, pos);
    EXPECT_EQ(topicLen, static_cast<uint16_t>(topic.size()));
    pos += 2;
    std::string t(reinterpret_cast<const char*>(&pkt[pos]), topicLen);
    EXPECT_EQ(t, topic);
    pos += topicLen;
    EXPECT_EQ(pkt[pos], 0x02); // QoS 2
}

// ── buildPingreq / buildDisconnect ─────────────────────────────────────────────

TEST(MqttPacketTest, BuildPingreq) {
    auto pkt = buildPingreq();
    ASSERT_EQ(pkt.size(), 2u);
    EXPECT_EQ(pkt[0], 0xC0);
    EXPECT_EQ(pkt[1], 0x00);
}

TEST(MqttPacketTest, BuildDisconnect) {
    auto pkt = buildDisconnect();
    ASSERT_EQ(pkt.size(), 2u);
    EXPECT_EQ(pkt[0], 0xE0);
    EXPECT_EQ(pkt[1], 0x00);
}

// ── parseConnack ───────────────────────────────────────────────────────────────

TEST(MqttPacketTest, ParseConnackSuccess) {
    std::vector<uint8_t> body{0x00, 0x00};
    auto info = parseConnack(body);
    EXPECT_FALSE(info.sessionPresent);
    EXPECT_EQ(info.returnCode, 0u);
}

TEST(MqttPacketTest, ParseConnackSessionPresent) {
    std::vector<uint8_t> body{0x01, 0x00};
    auto info = parseConnack(body);
    EXPECT_TRUE(info.sessionPresent);
}

TEST(MqttPacketTest, ParseConnackBadCredentials) {
    std::vector<uint8_t> body{0x00, 0x04};
    auto info = parseConnack(body);
    EXPECT_EQ(info.returnCode, 4u);
}

TEST(MqttPacketTest, ParseConnackTooShortThrows) {
    EXPECT_THROW(parseConnack({0x00}), SerializationException);
}

// ── parsePublish ───────────────────────────────────────────────────────────────

TEST(MqttPacketTest, ParsePublishQos0) {
    const std::string topic = "sensors/temp";
    std::vector<uint8_t> body;
    body.push_back(0x00);
    body.push_back(static_cast<uint8_t>(topic.size()));
    body.insert(body.end(), topic.begin(), topic.end());
    body.insert(body.end(), {'2','5','.','0'});

    auto info = parsePublish(0x00, body);
    EXPECT_EQ(info.topic, topic);
    EXPECT_EQ(info.qos, 0);
    EXPECT_EQ(info.payload.size(), 4u);
    EXPECT_FALSE(info.retained);
    EXPECT_FALSE(info.dup);
}

TEST(MqttPacketTest, ParsePublishQos1WithPacketId) {
    const std::string topic = "a/b";
    std::vector<uint8_t> body;
    body.push_back(0x00);
    body.push_back(static_cast<uint8_t>(topic.size()));
    body.insert(body.end(), topic.begin(), topic.end());
    body.push_back(0x00); body.push_back(0x05); // packet ID = 5
    body.push_back(0xFF);

    auto info = parsePublish(0x02, body); // flags: QoS1
    EXPECT_EQ(info.topic, topic);
    EXPECT_EQ(info.qos, 1);
    EXPECT_EQ(info.packetId, 5u);
    ASSERT_EQ(info.payload.size(), 1u);
    EXPECT_EQ(info.payload[0], 0xFF);
}

TEST(MqttPacketTest, ParsePublishRetainedAndDup) {
    const std::string topic = "t";
    std::vector<uint8_t> body{0x00, 0x01, 't'};
    auto info = parsePublish(0x09, body); // dup=1, QoS0, retained=1
    EXPECT_TRUE(info.dup);
    EXPECT_TRUE(info.retained);
}

// ── parseSuback ────────────────────────────────────────────────────────────────

TEST(MqttPacketTest, ParseSubackGrantedQos1) {
    std::vector<uint8_t> body{0x00, 0x03, 0x01};
    auto info = parseSuback(body);
    EXPECT_EQ(info.packetId, 3u);
    ASSERT_EQ(info.returnCodes.size(), 1u);
    EXPECT_EQ(info.returnCodes[0], 0x01u);
}

TEST(MqttPacketTest, ParseSubackFailure) {
    std::vector<uint8_t> body{0x00, 0x01, 0x80};
    auto info = parseSuback(body);
    EXPECT_EQ(info.returnCodes[0], 0x80u);
}

TEST(MqttPacketTest, ParseSubackTooShortThrows) {
    EXPECT_THROW(parseSuback({0x00, 0x01}), SerializationException);
}

// ── parsePacketId ─────────────────────────────────────────────────────────────

TEST(MqttPacketTest, ParsePacketId) {
    std::vector<uint8_t> body{0x00, 0x2A, 0xFF}; // id = 42
    EXPECT_EQ(parsePacketId(body), 42u);
}

TEST(MqttPacketTest, ParsePacketIdEmpty) {
    EXPECT_EQ(parsePacketId({}), 0u);
}

} // namespace mqtt::test
