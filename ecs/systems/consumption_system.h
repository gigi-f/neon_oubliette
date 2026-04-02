#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CONSUMPTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CONSUMPTION_SYSTEM_H

#include <entt/entt.hpp>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class ConsumptionSystem : public ISystem {
public:
    ConsumptionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handleConsumeItemEvent(const ConsumeItemEvent& event);

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CONSUMPTION_SYSTEM_H
