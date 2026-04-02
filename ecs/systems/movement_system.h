#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_MOVEMENT_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_MOVEMENT_SYSTEM_H

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class MovementSystem : public ISystem {
public:
    MovementSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handleMoveEvent(const MoveEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_MOVEMENT_SYSTEM_H