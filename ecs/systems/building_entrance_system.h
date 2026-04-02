#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_ENTRANCE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_ENTRANCE_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

/**
 * @brief Manages the transition protocol between Macro-city coordinates and Micro-interior layouts.
 *        This system specifically handles the 'Entrance' and 'Exit' events to maintain spatial causality.
 */
class BuildingEntranceSystem : public ISystem {
public:
    BuildingEntranceSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        m_dispatcher.sink<BuildingEntranceEvent>().connect<&BuildingEntranceSystem::handleEntrance>(this);
    }

    void update(double delta_time) override {
        // Logic for NPC proximity-based transitions could go here
    }

    /**
     * @brief Transitions an entity into a building.
     */
    void handleEntrance(const BuildingEntranceEvent& event) {
        if (!m_registry.valid(event.building)) return;

        // 1. Ensure Interior is generated (via BuildingGenerationSystem)
        // Note: The BuildingGenerationSystem also listens for this event and handles generation.
        // We handle the positioning here or after generation.
        
        auto* interior = m_registry.try_get<InteriorGeneratedComponent>(event.building);
        if (!interior || !interior->is_generated) {
            // Wait for generation system or trigger it (handled by shared event)
            return;
        }

        if (interior->floor_entities.empty()) return;

        // 2. Reposition Visitor to Floor 0 of the building's micro-layer
        auto& visitor_pos = m_registry.get<PositionComponent>(event.visitor);
        
        // Save Macro-position for return trip if not already tracked
        // (The building entity itself acts as the anchor)

        entt::entity target_floor = interior->floor_entities[0];
        auto& floor_comp = m_registry.get<FloorComponent>(target_floor);
        
        // Convention: Micro layer ID is BuildingID * 100 + FloorIndex
        int new_layer_id = static_cast<int>(entt::to_integral(event.building)) * 100 + floor_comp.level;

        visitor_pos.layer_id = new_layer_id;
        visitor_pos.x = 5; // Default entrance point in interior
        visitor_pos.y = 5;

        if (m_registry.all_of<PlayerCurrentLayerComponent>(event.visitor)) {
            m_registry.get<PlayerCurrentLayerComponent>(event.visitor).current_z = new_layer_id;
        }

        m_dispatcher.enqueue<HUDNotificationEvent>({"Stepped into " + m_registry.get<NameComponent>(event.building).name, 2.0f, "#00FFFF"});
        
        // 3. Mark the building as 'active' for simulation scaling
        // (Active buildings get more granular micro-simulation ticks)
    }

    /**
     * @brief Transitions an entity out of a building back to the Macro grid.
     */
    void handleExit(entt::entity visitor, entt::entity building) {
        if (!m_registry.valid(building)) return;

        auto& b_pos = m_registry.get<PositionComponent>(building);
        auto& visitor_pos = m_registry.get<PositionComponent>(visitor);

        // Return to the building's macro location
        visitor_pos.layer_id = b_pos.layer_id;
        visitor_pos.x = b_pos.x;
        visitor_pos.y = b_pos.y;

        if (m_registry.all_of<PlayerCurrentLayerComponent>(visitor)) {
            m_registry.get<PlayerCurrentLayerComponent>(visitor).current_z = b_pos.layer_id;
        }

        m_dispatcher.enqueue<HUDNotificationEvent>({"Exited to city level.", 2.0f, "#FFFF00"});
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_ENTRANCE_SYSTEM_H
