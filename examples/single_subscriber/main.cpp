#include "mqtt/config/ConfigLoader.hpp"
#include "mqtt/logging/Logger.hpp"
#include "mqtt/subscriber/MqttSubscriber.hpp"

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

    mqtt::Logger::instance().init("mqtt-sub", "", mqtt::LogLevel::Info);

    const std::string configPath = (argc > 1) ? argv[1] : "configs/default.yaml";
    auto cfgResult = mqtt::ConfigLoader::loadFromFile(configPath);
    if (!cfgResult) {
        std::cerr << "Config error: " << cfgResult.error().message() << "\n";
        return EXIT_FAILURE;
    }
    const auto& cfg = cfgResult.value();

    mqtt::MqttSubscriber subscriber(cfg.publisher.broker.connection,
                                     cfg.publisher.broker.tls);

    auto connectResult = subscriber.connect();
    if (!connectResult) {
        std::cerr << "Connect failed: " << connectResult.error().message() << "\n";
        return EXIT_FAILURE;
    }

    for (const auto& subCfg : cfg.subscribers) {
        for (const auto& topic : subCfg.topics) {
            auto r = subscriber.subscribe(topic, subCfg.defaultQos,
                [](mqtt::Message msg) {
                    std::cout << "[" << msg.topic << "] "
                              << msg.payloadAsString() << "\n";
                });
            if (!r) {
                std::cerr << "Subscribe failed for '" << topic
                          << "': " << r.error().message() << "\n";
                return EXIT_FAILURE;
            }
            std::cout << "Subscribed to: " << topic << "\n";
        }
    }

    subscriber.startListening();
    std::cout << "Listening (native socket). Press Ctrl+C to stop.\n";

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    subscriber.stopListening();
    subscriber.disconnect();

    const auto snap = subscriber.metrics()->snapshot();
    std::cout << "Stopped. Received=" << snap.messagesReceived
              << " Dropped=" << snap.droppedMessages << "\n";
    return EXIT_SUCCESS;
}
