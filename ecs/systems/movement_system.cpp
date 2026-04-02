#include "movement_system.h"
#include "../event_declarations.h"
#include "../components/components.h"

namespace NeonOubliette {

MovementSystem::MovementSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<MoveEvent>().connect<&MovementSystem::handleMoveEvent>(*this);
}

void MovementSystem::handleMoveEvent(const MoveEvent& event) {
    if (!m_registry.valid(event.entity))
        return;

    if (m_registry.all_of<PositionComponent>(event.entity)) {
        auto& pos = m_registry.get<PositionComponent>(event.entity);
        pos.x += event.dx;
        pos.y += event.dy;
        pos.layer_id = event.layer_id;

        // If it's the player, trigger a GLANCE inspection on the new position
        if (m_registry.all_of<PlayerComponent>(event.entity)) {
            m_dispatcher.trigger<InspectEvent>({event.entity, pos.layer_id, pos.x, pos.y, InspectionMode::GLANCE});
            
            // Also check adjacent tiles for entities to glance at
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    // Trigger glance event for adjacent tiles
                    // (The InspectionSystem handles cases where no entities exist)
                    m_dispatcher.trigger<InspectEvent>({event.entity, pos.layer_id, pos.x + dx, pos.y + dy, InspectionMode::GLANCE});
                }
            }
        }
    }
}

} // namespace NeonOubliette
