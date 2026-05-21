#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <spdlog/spdlog.h>

namespace mqtt {

enum class LogLevel : uint8_t {
    Trace    = 0,
    Debug    = 1,
    Info     = 2,
    Warn     = 3,
    Error    = 4,
    Critical = 5,
    Off      = 6,
};

class Logger {
public:
    static Logger& instance();

    void init(std::string_view loggerName,
              std::string_view logFile     = "",
              LogLevel         level       = LogLevel::Info,
              size_t           maxFileSize = 10 * 1024 * 1024,
              size_t           maxFiles    = 5);

    void setLevel(LogLevel level);

    [[nodiscard]] std::shared_ptr<spdlog::logger> raw() const noexcept;

private:
    Logger();
    ~Logger();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#ifdef NDEBUG
#  define MQTT_LOG_TRACE(...)    do {} while(0)
#  define MQTT_LOG_DEBUG(...)    do {} while(0)
#else
#  define MQTT_LOG_TRACE(...) \
     do { if (auto _l = mqtt::Logger::instance().raw()) _l->trace(__VA_ARGS__); } while(0)
#  define MQTT_LOG_DEBUG(...) \
     do { if (auto _l = mqtt::Logger::instance().raw()) _l->debug(__VA_ARGS__); } while(0)
#endif

#define MQTT_LOG_INFO(...) \
    do { if (auto _l = mqtt::Logger::instance().raw()) _l->info(__VA_ARGS__); } while(0)
#define MQTT_LOG_WARN(...) \
    do { if (auto _l = mqtt::Logger::instance().raw()) _l->warn(__VA_ARGS__); } while(0)
#define MQTT_LOG_ERROR(...) \
    do { if (auto _l = mqtt::Logger::instance().raw()) _l->error(__VA_ARGS__); } while(0)
#define MQTT_LOG_CRITICAL(...) \
    do { if (auto _l = mqtt::Logger::instance().raw()) _l->critical(__VA_ARGS__); } while(0)

} // namespace mqtt
