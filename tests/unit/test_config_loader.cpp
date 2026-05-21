#include "mqtt/config/ConfigLoader.hpp"

#include <gtest/gtest.h>

namespace mqtt::test {

static const char* kYaml = R"yaml(
broker:
  host: broker.example.com
  port: 1883

client_id: test-client
mqtt_version: 4
clean_session: true
keepalive: 30

publisher:
  default_topic: "vehicle/data"
  default_qos: 1
  queue_depth: 500

subscribers:
  - topics:
      - "vehicle/#"
      - "fleet/+/status"
    qos: 1

logging:
  level: debug
  file: /tmp/mqtt.log
)yaml";

TEST(ConfigLoaderTest, ParseYamlBrokerUrl) {
    auto r = ConfigLoader::loadFromYamlString(kYaml);
    ASSERT_TRUE(r.hasValue()) << r.error().message();
    const auto& conn = r.value().publisher.broker.connection;
    ASSERT_FALSE(conn.brokerUrls.empty());
    EXPECT_EQ(conn.brokerUrls[0], "tcp://broker.example.com:1883");
    EXPECT_EQ(conn.clientId, "test-client");
    EXPECT_EQ(conn.keepaliveSeconds, 30u);
}

TEST(ConfigLoaderTest, ParseYamlPublisher) {
    auto r = ConfigLoader::loadFromYamlString(kYaml);
    ASSERT_TRUE(r.hasValue());
    const auto& pub = r.value().publisher;
    EXPECT_EQ(pub.defaultTopic, "vehicle/data");
    EXPECT_EQ(pub.defaultQos, 1);
    EXPECT_EQ(pub.asyncQueueDepth, 500u);
}

TEST(ConfigLoaderTest, ParseYamlSubscribers) {
    auto r = ConfigLoader::loadFromYamlString(kYaml);
    ASSERT_TRUE(r.hasValue());
    ASSERT_EQ(r.value().subscribers.size(), 1u);
    EXPECT_EQ(r.value().subscribers[0].topics.size(), 2u);
}

TEST(ConfigLoaderTest, ParseYamlLogging) {
    auto r = ConfigLoader::loadFromYamlString(kYaml);
    ASSERT_TRUE(r.hasValue());
    EXPECT_EQ(r.value().logLevel, "debug");
    EXPECT_EQ(r.value().logFile,  "/tmp/mqtt.log");
}

TEST(ConfigLoaderTest, MissingFileReturnsError) {
    auto r = ConfigLoader::loadFromFile("/nonexistent/path/config.yaml");
    EXPECT_FALSE(r.hasValue());
    EXPECT_EQ(r.error().code(), ErrorCode::ConfigInvalid);
}

static const char* kJson = R"json({
  "broker": { "host": "localhost", "port": 1883 },
  "client_id": "json-client",
  "logging": { "level": "warn" }
})json";

TEST(ConfigLoaderTest, ParseJson) {
    auto r = ConfigLoader::loadFromJsonString(kJson);
    ASSERT_TRUE(r.hasValue()) << r.error().message();
    EXPECT_EQ(r.value().publisher.broker.connection.clientId, "json-client");
    EXPECT_EQ(r.value().logLevel, "warn");
}

} // namespace mqtt::test
