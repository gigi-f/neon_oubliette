#ifndef NEON_OUBLIETTE_PLAYER_MOVEMENT_SYSTEM_H
#define NEON_OUBLIETTE_PLAYER_MOVEMENT_SYSTEM_H

#include <entt/entt.hpp>

#include "../../ecs/events.h"
#include "system_scheduler.h"

namespace NeonOubliette::Systems {

class PlayerMovementSystem : public ISystem {
public:
    PlayerMovementSystem(entt::registry& registry, entt::dispatcher& event_dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    // Event handlers
    void handlePlayerMoveEvent(const ECS::PlayerMoveEvent& event);
    void handlePlayerLayerChangeEvent(const ECS::PlayerLayerChangeEvent& event);

private:
    entt::registry& registry_;
    entt::dispatcher& event_dispatcher_;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_PLAYER_MOVEMENT_SYSTEM_H
