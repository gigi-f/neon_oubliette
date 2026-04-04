#include "movement_system.h"
#include "../event_declarations.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/transit_components.h"

namespace NeonOubliette {

MovementSystem::MovementSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<MoveEvent>().connect<&MovementSystem::handleMoveEvent>(*this);
}

void MovementSystem::handleMoveEvent(const MoveEvent& event) {
    if (!m_registry.valid(event.entity)) return;

    if (m_registry.all_of<PositionComponent>(event.entity)) {
        // [NEW] Social Deference Check (Phase 4.4)
        if (auto* hierarchy = m_registry.try_get<SocialHierarchyComponent>(event.entity)) {
            if (hierarchy->yield_ticks_remaining > 0) {
                // 50% chance to skip move if yielding
                if (rand() % 100 < 50) return;
            }
        }

        // [NEW] Prevent movement if riding (unless specifically handled)
        if (m_registry.all_of<RidingComponent>(event.entity)) {
            if (m_registry.all_of<PlayerComponent>(event.entity)) {
                m_dispatcher.trigger(HUDNotificationEvent{"Cannot move manually while in a vehicle.", 1.0f, "#FFFF00"});
            }
            return;
        }

        auto& pos = m_registry.get<PositionComponent>(event.entity);
        
        int target_x = pos.x + event.dx;
        int target_y = pos.y + event.dy;
        int target_layer = event.layer_id;

        // --- World Bounds Clamping (Phase 1.2) ---
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (!config_view.empty()) {
            auto config_entity = *config_view.begin();
            const auto& config = config_view.get<WorldConfigComponent>(config_entity);
            
            if (target_x < 0) target_x = 0;
            if (target_x >= config.width) target_x = config.width - 1;
            if (target_y < 0) target_y = 0;
            if (target_y >= config.height) target_y = config.height - 1;
        }

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

        // [NEW] Carry all occupants with the vehicle
        if (m_registry.all_of<TransitOccupantsComponent>(event.entity)) {
            auto& occupants = m_registry.get<TransitOccupantsComponent>(event.entity).occupants;
            for (auto occupant : occupants) {
                if (m_registry.valid(occupant) && m_registry.all_of<PositionComponent>(occupant)) {
                    auto& occ_pos = m_registry.get<PositionComponent>(occupant);
                    occ_pos.x = target_x;
                    occ_pos.y = target_y;
                    occ_pos.layer_id = target_layer;
                }
            }
        }

        if (m_registry.all_of<PlayerComponent>(event.entity)) {
            m_dispatcher.trigger(InspectEvent{event.entity, pos.layer_id, pos.x, pos.y, InspectionMode::GLANCE});
        }
    }
}

} // namespace NeonOubliette
