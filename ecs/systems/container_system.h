#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CONTAINER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CONTAINER_SYSTEM_H

#include <entt/entt.hpp>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class ContainerSystem : public ISystem {
public:
    ContainerSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handleContainerInteractionEvent(const ContainerInteractionEvent& event);

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CONTAINER_SYSTEM_H
