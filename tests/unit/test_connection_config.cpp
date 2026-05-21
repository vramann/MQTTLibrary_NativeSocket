#include "mqtt/core/ConnectionConfig.hpp"

#include <gtest/gtest.h>
#include <chrono>

namespace mqtt::test {

TEST(ConnectionConfigTest, Defaults) {
    ConnectionConfig cfg;
    EXPECT_TRUE(cfg.brokerUrls.empty());
    EXPECT_EQ(cfg.mqttVersion, MqttVersion::V311);
    EXPECT_TRUE(cfg.cleanSession);
    EXPECT_EQ(cfg.keepaliveSeconds, 60u);
    EXPECT_EQ(cfg.connectTimeout, std::chrono::milliseconds{10000});
    EXPECT_TRUE(cfg.offlineBufferingEnabled);
    EXPECT_TRUE(cfg.reconnect.enabled);
    EXPECT_EQ(cfg.reconnect.backoffMultiplier, 2.0);
}

TEST(ConnectionConfigTest, WillConfig) {
    ConnectionConfig cfg;
    WillConfig will;
    will.topic    = "device/status";
    will.payload  = "offline";
    will.qos      = 1;
    will.retained = true;
    cfg.will = will;

    ASSERT_TRUE(cfg.will.has_value());
    EXPECT_EQ(cfg.will->topic,   "device/status");
    EXPECT_EQ(cfg.will->payload, "offline");
}

TEST(ConnectionConfigTest, MqttV5) {
    ConnectionConfig cfg;
    cfg.mqttVersion = MqttVersion::V50;
    EXPECT_EQ(cfg.mqttVersion, MqttVersion::V50);
    EXPECT_EQ(static_cast<uint8_t>(MqttVersion::V50), 5u);
}

} // namespace mqtt::test
