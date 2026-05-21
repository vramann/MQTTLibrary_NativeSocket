#include "mqtt/logging/Logger.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <vector>

namespace mqtt {

struct Logger::Impl {
    std::shared_ptr<spdlog::logger> logger;
};

Logger::Logger() : impl_(std::make_unique<Impl>()) {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    impl_->logger = std::make_shared<spdlog::logger>("mqtt", sink);
    impl_->logger->set_level(spdlog::level::info);
    impl_->logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [tid:%t] %v");
    spdlog::register_logger(impl_->logger);
}

Logger::~Logger() {
    if (impl_ && impl_->logger) {
        impl_->logger->flush();
        spdlog::drop(impl_->logger->name());
    }
    spdlog::shutdown();
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(std::string_view loggerName,
                  std::string_view logFile,
                  LogLevel         level,
                  size_t           maxFileSize,
                  size_t           maxFiles) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (!logFile.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            std::string(logFile), maxFileSize, maxFiles));
    }

    if (impl_->logger) spdlog::drop(impl_->logger->name());

    impl_->logger = std::make_shared<spdlog::logger>(
        std::string(loggerName), sinks.begin(), sinks.end());
    impl_->logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [tid:%t] %v");
    setLevel(level);
    spdlog::register_logger(impl_->logger);
    spdlog::flush_on(spdlog::level::warn);
}

static spdlog::level::level_enum toSpdlog(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

void Logger::setLevel(LogLevel level) {
    if (impl_->logger) impl_->logger->set_level(toSpdlog(level));
}

std::shared_ptr<spdlog::logger> Logger::raw() const noexcept {
    return impl_->logger;
}

} // namespace mqtt
