#include "movement_system.h"
#include "../event_declarations.h"
#include "../components/components.h"

namespace NeonOubliette {

MovementSystem::MovementSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<MoveEvent>().connect<&MovementSystem::handleMoveEvent>(*this);
}

void MovementSystem::handleMoveEvent(const MoveEvent& event) {
    if (!m_registry.valid(event.entity)) return;

    if (m_registry.all_of<PositionComponent>(event.entity)) {
        auto& pos = m_registry.get<PositionComponent>(event.entity);
        
        int target_x = pos.x + event.dx;
        int target_y = pos.y + event.dy;
        int target_layer = event.layer_id;

        // --- Volumetric Collision Detection ---
        bool blocked = false;
        
        // We check the destination (target_x, target_y) against all physical volumes
        auto obs_view = m_registry.view<PositionComponent, ObstacleComponent, SizeComponent>();
        for (auto obstacle : obs_view) {
            if (obstacle == event.entity) continue;

            const auto& o_pos = obs_view.get<PositionComponent>(obstacle);
            const auto& o_size = obs_view.get<SizeComponent>(obstacle);

            // Check if target point is inside the obstacle's rectangle
            if (target_layer == o_pos.layer_id &&
                target_x >= o_pos.x && target_x < o_pos.x + o_size.width &&
                target_y >= o_pos.y && target_y < o_pos.y + o_size.height) {
                blocked = true;
                break;
            }
        }

        // Check against single-tile obstacles without size component
        if (!blocked) {
            auto single_obs = m_registry.view<PositionComponent, ObstacleComponent>(entt::exclude<SizeComponent>);
            for (auto obstacle : single_obs) {
                if (obstacle == event.entity) continue;
                const auto& o_pos = single_obs.get<PositionComponent>(obstacle);
                if (o_pos.x == target_x && o_pos.y == target_y && o_pos.layer_id == target_layer) {
                    blocked = true;
                    break;
                }
            }
        }

        if (blocked) {
            if (m_registry.all_of<PlayerComponent>(event.entity)) {
                m_dispatcher.trigger(HUDNotificationEvent{"Obstacle in real-space path.", 1.0f, "#FF5555"});
            }
            return;
        }

        pos.x = target_x;
        pos.y = target_y;
        pos.layer_id = target_layer;

        if (m_registry.all_of<PlayerComponent>(event.entity)) {
            m_dispatcher.trigger(InspectEvent{event.entity, pos.layer_id, pos.x, pos.y, InspectionMode::GLANCE});
        }
    }
}

} // namespace NeonOubliette
