#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace mqtt {

struct MetricsSnapshot {
    uint64_t messagesPublished{0};
    uint64_t publishFailures{0};
    uint64_t messagesReceived{0};
    uint64_t droppedMessages{0};
    uint64_t reconnectCount{0};
    uint64_t pendingQueueSize{0};
    double   avgPublishLatencyMs{0.0};
    bool     connected{false};
};

class Metrics {
public:
    void recordPublished() noexcept    { ++messagesPublished_; }
    void recordPublishFail() noexcept  { ++publishFailures_;   }
    void recordReceived() noexcept     { ++messagesReceived_;  }
    void recordDropped() noexcept      { ++droppedMessages_;   }
    void recordReconnect() noexcept    { ++reconnectCount_;    }
    void setPendingQueue(uint64_t n) noexcept { pendingQueue_.store(n); }
    void setConnected(bool v) noexcept { connected_.store(v); }

    void recordPublishLatency(std::chrono::microseconds us) noexcept {
        const auto sample = static_cast<double>(us.count()) / 1000.0;
        const double prev = avgLatencyMs_.load(std::memory_order_relaxed);
        avgLatencyMs_.store(prev * 0.9 + sample * 0.1, std::memory_order_relaxed);
    }

    [[nodiscard]] MetricsSnapshot snapshot() const noexcept {
        return {
            messagesPublished_.load(),
            publishFailures_.load(),
            messagesReceived_.load(),
            droppedMessages_.load(),
            reconnectCount_.load(),
            pendingQueue_.load(),
            avgLatencyMs_.load(),
            connected_.load(),
        };
    }

    void reset() noexcept {
        messagesPublished_.store(0);
        publishFailures_.store(0);
        messagesReceived_.store(0);
        droppedMessages_.store(0);
        reconnectCount_.store(0);
        pendingQueue_.store(0);
        avgLatencyMs_.store(0.0);
    }

private:
    std::atomic<uint64_t> messagesPublished_{0};
    std::atomic<uint64_t> publishFailures_{0};
    std::atomic<uint64_t> messagesReceived_{0};
    std::atomic<uint64_t> droppedMessages_{0};
    std::atomic<uint64_t> reconnectCount_{0};
    std::atomic<uint64_t> pendingQueue_{0};
    std::atomic<double>   avgLatencyMs_{0.0};
    std::atomic<bool>     connected_{false};
};

} // namespace mqtt
