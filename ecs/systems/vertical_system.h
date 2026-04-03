#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_VERTICAL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_VERTICAL_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

/**
 * @brief Handles vertical transitions (stairs, elevators) and layer changes.
 *        Bridges the gap between Macro-City (Layer 0) and Micro-Interiors.
 */
class VerticalSystem : public ISystem {
public:
    VerticalSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        m_dispatcher.sink<PlayerLayerChangeEvent>().connect<&VerticalSystem::handleLayerChange>(this);
    }

    void update(double delta_time) override {}

    void handleLayerChange(const PlayerLayerChangeEvent& event) {
        auto player_view = m_registry.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent>();
        for (auto player : player_view) {
            auto& pos = player_view.get<PositionComponent>(player);
            auto& layer = player_view.get<PlayerCurrentLayerComponent>(player);

            bool transition_found = false;

            // Check for localized transition components (Stairs/Elevators/Portals)
            auto transition_view = m_registry.view<PositionComponent>();
            for (auto t_entity : transition_view) {
                auto& t_pos = transition_view.get<PositionComponent>(t_entity);
                
                // Must be at the same physical location and layer
                if (t_pos.x == pos.x && t_pos.y == pos.y && t_pos.layer_id == pos.layer_id) {
                    
                    // Case 1: Stairs
                    if (m_registry.all_of<StairsComponent>(t_entity)) {
                        auto& stairs = m_registry.get<StairsComponent>(t_entity);
                        
                        // Use event.dz to determine direction if possible, but Stairs often have fixed targets
                        pos.layer_id = stairs.connects_to_layer;
                        layer.current_z = pos.layer_id;
                        transition_found = true;
                        m_dispatcher.enqueue<HUDNotificationEvent>("Used stairs.", 1.5f, "#AAAAFF");
                    }
                    
                    // Case 2: Portals (Exit Doors)
                    else if (m_registry.all_of<PortalComponent>(t_entity)) {
                        auto& portal = m_registry.get<PortalComponent>(t_entity);
                        
                        pos.x = portal.target_x;
                        pos.y = portal.target_y;
                        pos.layer_id = portal.target_layer;
                        layer.current_z = pos.layer_id;
                        transition_found = true;
                        m_dispatcher.enqueue<HUDNotificationEvent>("Passed through portal.", 1.5f, "#FFFFFF");
                    }

                    // Case 3: Elevator
                    else if (m_registry.all_of<ElevatorComponent>(t_entity)) {
                        int target_layer = pos.layer_id + event.dz;
                        pos.layer_id = target_layer; 
                        layer.current_z = pos.layer_id;
                        transition_found = true;
                        m_dispatcher.enqueue<HUDNotificationEvent>("Elevator moved.", 1.5f, "#00FFFF");
                    }
                }
                if (transition_found) break;
            }

            if (!transition_found && event.dz != 0) {
                m_dispatcher.enqueue<HUDNotificationEvent>("No vertical path here.", 1.0f, "#FF5555");
            }
        }
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_VERTICAL_SYSTEM_H
