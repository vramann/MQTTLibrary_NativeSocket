#include "mqtt/config/ConfigLoader.hpp"
#include "mqtt/logging/Logger.hpp"
#include "mqtt/publisher/MqttPublisher.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

static volatile sig_atomic_t g_running = 1;
static void onSignal(int) { g_running = 0; }

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    mqtt::Logger::instance().init("mqtt-pub", "", mqtt::LogLevel::Info);

    const std::string configPath = (argc > 1) ? argv[1] : "configs/default.yaml";
    auto cfgResult = mqtt::ConfigLoader::loadFromFile(configPath);
    if (!cfgResult) {
        std::cerr << "Config error: " << cfgResult.error().message() << "\n";
        return EXIT_FAILURE;
    }
    const auto& cfg = cfgResult.value();

    mqtt::MqttPublisher publisher(cfg.publisher.broker.connection,
                                   cfg.publisher.broker.tls);

    auto connectResult = publisher.connect();
    if (!connectResult) {
        std::cerr << "Connect failed: " << connectResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Publisher connected (native socket). Press Ctrl+C to stop.\n";

    uint64_t seq = 0;
    while (g_running) {
        const std::string topic   = cfg.publisher.defaultTopic.empty()
                                    ? "mqtt/test" : cfg.publisher.defaultTopic;
        const std::string payload = R"({"seq":)" + std::to_string(seq++)
                                  + R"(,"ts":")" + std::to_string(
                                      std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch()).count())
                                  + R"("})";

        auto msg = mqtt::Message::fromString(topic, payload, cfg.publisher.defaultQos);
        if (auto r = publisher.publish(msg); !r) {
            std::cerr << "Publish failed: " << r.error().message() << "\n";
        } else {
            std::cout << "Published [" << seq << "] → " << topic << "\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    publisher.disconnect();
    const auto snap = publisher.metrics()->snapshot();
    std::cout << "Stopped. Published=" << snap.messagesPublished
              << " Failures=" << snap.publishFailures << "\n";
    return EXIT_SUCCESS;
}
