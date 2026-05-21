#pragma once

#include "mqtt/core/ConnectionConfig.hpp"
#include "mqtt/core/Error.hpp"
#include "mqtt/transport/TlsConfig.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace mqtt {

struct BrokerConfig {
    ConnectionConfig connection;
    TlsConfig        tls;
};

struct PublisherConfig {
    BrokerConfig broker;
    std::string  defaultTopic;
    int          defaultQos{0};
    bool         defaultRetained{false};
    uint32_t     asyncQueueDepth{1000};
};

struct SubscriberConfig {
    BrokerConfig             broker;
    std::vector<std::string> topics;
    int                      defaultQos{0};
};

struct AppConfig {
    PublisherConfig                publisher;
    std::vector<SubscriberConfig>  subscribers;
    std::string                    logFile;
    std::string                    logLevel{"info"};
};

class ConfigLoader {
public:
    [[nodiscard]] static Result<AppConfig> loadFromFile(const std::filesystem::path& path);
    [[nodiscard]] static Result<AppConfig> loadFromYamlString(const std::string& yaml);
    [[nodiscard]] static Result<AppConfig> loadFromJsonString(const std::string& json);

private:
    static Result<AppConfig> parseYaml(const std::string& content);
    static Result<AppConfig> parseJson(const std::string& content);

    static ConnectionConfig parseConnectionConfig(const auto& node);
    static TlsConfig        parseTlsConfig(const auto& node);
};

} // namespace mqtt
