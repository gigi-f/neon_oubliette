#ifndef LOGGING_SYSTEM_H
#define LOGGING_SYSTEM_H

#include <entt/entt.hpp>
#include <deque>
#include <string>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class LoggingSystem : public ISystem {
public:
    LoggingSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override;

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;

    std::deque<std::string> pending_lines_;
    double flush_accumulator_seconds_ = 0.0;
    static constexpr std::size_t kFlushBatchSize = 256;
    static constexpr std::size_t kHardQueueLimit = 4096;
    static constexpr double kFlushIntervalSeconds = 0.1;

    void handleLogEvent(const LogEvent& event);
    void flushPendingLogs(std::size_t max_lines);
};

} // namespace NeonOubliette

#endif // LOGGING_SYSTEM_H
