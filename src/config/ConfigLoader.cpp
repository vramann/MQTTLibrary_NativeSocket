#include "mqtt/config/ConfigLoader.hpp"
#include "mqtt/logging/Logger.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <sstream>

namespace mqtt {

Result<AppConfig> ConfigLoader::loadFromFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<AppConfig>::err(ConfigException{ErrorCode::ConfigInvalid,
            "Config file not found: " + path.string()});
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<AppConfig>::err(ConfigException{ErrorCode::ConfigParseError,
            "Cannot open config file: " + path.string()});
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string content = ss.str();

    const auto ext = path.extension().string();
    if (ext == ".yaml" || ext == ".yml") return parseYaml(content);
    if (ext == ".json")                  return parseJson(content);

    return Result<AppConfig>::err(ConfigException{ErrorCode::ConfigParseError,
        "Unsupported config format: " + ext});
}

Result<AppConfig> ConfigLoader::loadFromYamlString(const std::string& yaml) {
    return parseYaml(yaml);
}

Result<AppConfig> ConfigLoader::loadFromJsonString(const std::string& json) {
    return parseJson(json);
}

// ── YAML parser ────────────────────────────────────────────────────────────────

static ConnectionConfig yamlToConnection(const YAML::Node& n) {
    ConnectionConfig cfg;

    if (n["broker"]) {
        const auto& b = n["broker"];
        if (b["urls"]) {
            for (const auto& u : b["urls"])
                cfg.brokerUrls.push_back(u.as<std::string>());
        } else if (b["host"] && b["port"]) {
            const bool useTls = n["tls"] && n["tls"]["enabled"] && n["tls"]["enabled"].as<bool>();
            const std::string scheme = useTls ? "ssl" : "tcp";
            cfg.brokerUrls.push_back(scheme + "://" + b["host"].as<std::string>()
                                     + ":" + std::to_string(b["port"].as<uint16_t>()));
        }
    }

    if (n["client_id"])  cfg.clientId = n["client_id"].as<std::string>();
    if (n["username"])   cfg.username = n["username"].as<std::string>();
    if (n["password"])   cfg.password = n["password"].as<std::string>();

    if (n["mqtt_version"])
        cfg.mqttVersion = n["mqtt_version"].as<int>() == 5
                          ? MqttVersion::V50 : MqttVersion::V311;
    if (n["clean_session"])      cfg.cleanSession     = n["clean_session"].as<bool>();
    if (n["keepalive"])          cfg.keepaliveSeconds  = n["keepalive"].as<uint16_t>();
    if (n["connect_timeout_ms"]) cfg.connectTimeout    =
        std::chrono::milliseconds{n["connect_timeout_ms"].as<int>()};

    if (n["reconnect"]) {
        const auto& r = n["reconnect"];
        if (r["enabled"])            cfg.reconnect.enabled           = r["enabled"].as<bool>();
        if (r["max_retries"])        cfg.reconnect.maxRetries         = r["max_retries"].as<uint32_t>();
        if (r["initial_delay_ms"])   cfg.reconnect.initialDelay       =
            std::chrono::milliseconds{r["initial_delay_ms"].as<int>()};
        if (r["max_delay_ms"])       cfg.reconnect.maxDelay           =
            std::chrono::milliseconds{r["max_delay_ms"].as<int>()};
        if (r["backoff_multiplier"]) cfg.reconnect.backoffMultiplier  =
            r["backoff_multiplier"].as<double>();
    }

    if (n["will"]) {
        const auto& w = n["will"];
        WillConfig will;
        will.topic    = w["topic"].as<std::string>("");
        will.payload  = w["payload"].as<std::string>("");
        will.qos      = w["qos"].as<int>(1);
        will.retained = w["retained"].as<bool>(false);
        cfg.will = will;
    }

    return cfg;
}

static TlsConfig yamlToTls(const YAML::Node& n) {
    TlsConfig tls;
    if (!n || !n["tls"]) return tls;
    const auto& t = n["tls"];

    tls.enabled = t["enabled"].as<bool>(false);
    if (!tls.enabled) return tls;

    if (t["ca_cert"])        tls.caCertFile        = t["ca_cert"].as<std::string>();
    if (t["client_cert"])    tls.clientCertFile     = t["client_cert"].as<std::string>();
    if (t["client_key"])     tls.clientKeyFile      = t["client_key"].as<std::string>();
    if (t["key_password"])   tls.clientKeyPassword  = t["key_password"].as<std::string>();
    if (t["verify_peer"])    tls.verifyPeer         = t["verify_peer"].as<bool>(true);
    if (t["verify_hostname"])tls.verifyHostname      = t["verify_hostname"].as<bool>(true);

    return tls;
}

Result<AppConfig> ConfigLoader::parseYaml(const std::string& content) {
    try {
        const YAML::Node root = YAML::Load(content);
        AppConfig cfg;

        cfg.publisher.broker.connection = yamlToConnection(root);
        cfg.publisher.broker.tls        = yamlToTls(root);

        if (root["publisher"]) {
            const auto& p = root["publisher"];
            if (p["default_topic"]) cfg.publisher.defaultTopic   = p["default_topic"].as<std::string>();
            if (p["default_qos"])   cfg.publisher.defaultQos      = p["default_qos"].as<int>();
            if (p["queue_depth"])   cfg.publisher.asyncQueueDepth = p["queue_depth"].as<uint32_t>();
        }

        if (root["subscribers"]) {
            for (const auto& s : root["subscribers"]) {
                SubscriberConfig sc;
                sc.broker.connection = yamlToConnection(root);
                sc.broker.tls        = yamlToTls(root);
                if (s["topics"])
                    for (const auto& t : s["topics"])
                        sc.topics.push_back(t.as<std::string>());
                if (s["qos"]) sc.defaultQos = s["qos"].as<int>();
                cfg.subscribers.push_back(std::move(sc));
            }
        }

        if (root["logging"]) {
            const auto& l = root["logging"];
            if (l["file"])  cfg.logFile  = l["file"].as<std::string>();
            if (l["level"]) cfg.logLevel = l["level"].as<std::string>();
        }

        return Result<AppConfig>::ok(std::move(cfg));
    } catch (const YAML::Exception& e) {
        return Result<AppConfig>::err(ConfigException{ErrorCode::ConfigParseError,
            std::string("YAML parse error: ") + e.what()});
    }
}

// ── JSON parser ────────────────────────────────────────────────────────────────

Result<AppConfig> ConfigLoader::parseJson(const std::string& content) {
    try {
        using json = nlohmann::json;
        const json root = json::parse(content);
        AppConfig cfg;

        auto getStr = [](const json& j, const char* key, std::string def = "") -> std::string {
            return j.contains(key) ? j.at(key).get<std::string>() : def;
        };

        auto& conn = cfg.publisher.broker.connection;
        if (root.contains("broker")) {
            const auto& b = root["broker"];
            if (b.contains("urls")) {
                for (const auto& u : b["urls"]) conn.brokerUrls.push_back(u.get<std::string>());
            } else {
                const bool useTls = root.contains("tls") && root["tls"].value("enabled", false);
                conn.brokerUrls.push_back((useTls ? "ssl" : "tcp") + std::string("://")
                    + getStr(b, "host", "localhost")
                    + ":" + std::to_string(b.value("port", 1883)));
            }
        }
        conn.clientId = getStr(root, "client_id");
        if (root.contains("username")) conn.username = root["username"].get<std::string>();
        if (root.contains("password")) conn.password = root["password"].get<std::string>();

        if (root.contains("tls")) {
            const auto& t = root["tls"];
            auto& tls = cfg.publisher.broker.tls;
            tls.enabled = t.value("enabled", false);
            if (tls.enabled) {
                if (t.contains("ca_cert"))     tls.caCertFile    = t["ca_cert"].get<std::string>();
                if (t.contains("client_cert")) tls.clientCertFile = t["client_cert"].get<std::string>();
                if (t.contains("client_key"))  tls.clientKeyFile  = t["client_key"].get<std::string>();
            }
        }

        if (root.contains("logging")) {
            cfg.logFile  = root["logging"].value("file",  "");
            cfg.logLevel = root["logging"].value("level", "info");
        }

        return Result<AppConfig>::ok(std::move(cfg));
    } catch (const nlohmann::json::exception& e) {
        return Result<AppConfig>::err(ConfigException{ErrorCode::ConfigParseError,
            std::string("JSON parse error: ") + e.what()});
    }
}

} // namespace mqtt
