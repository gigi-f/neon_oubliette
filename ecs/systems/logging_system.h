#ifndef LOGGING_SYSTEM_H
#define LOGGING_SYSTEM_H

#include <entt/entt.hpp>

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

    void handleLogEvent(const LogEvent& event);
};

} // namespace NeonOubliette

#endif // LOGGING_SYSTEM_H
