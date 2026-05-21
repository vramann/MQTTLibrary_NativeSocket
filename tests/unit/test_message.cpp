#include "mqtt/core/Message.hpp"

#include <gtest/gtest.h>

namespace mqtt::test {

TEST(MessageTest, FromStringRoundtrip) {
    auto msg = Message::fromString("sensors/temp", "42.5", 1, false);
    EXPECT_EQ(msg.topic, "sensors/temp");
    EXPECT_EQ(msg.payloadAsString(), "42.5");
    EXPECT_EQ(msg.qos, 1);
    EXPECT_FALSE(msg.retained);
}

TEST(MessageTest, EmptyPayload) {
    auto msg = Message::fromString("a/b", "");
    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.payloadAsString(), "");
}

TEST(MessageTest, BinaryPayload) {
    Message msg;
    msg.topic   = "binary/data";
    msg.payload = {0x00, 0xFF, 0x42};
    EXPECT_EQ(msg.payload.size(), 3u);
    EXPECT_EQ(msg.payload[1], 0xFF);
}

} // namespace mqtt::test
