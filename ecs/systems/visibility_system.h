#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] Manages vertical visibility and sensory propagation.
 *        Allows entities to "see" across layers via ShaftComponent and CeilingComponent.
 */
class VisibilitySystem : public ISystem {
public:
    VisibilitySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        auto player_view = m_registry.view<PlayerComponent, PositionComponent, VerticalViewComponent>();
        for (auto player : player_view) {
            auto& pos = player_view.get<PositionComponent>(player);
            auto& view = player_view.get<VerticalViewComponent>(player);

            // Determine active layers visible to the player
            std::vector<int> visible_layers;
            visible_layers.push_back(pos.layer_id);

            // Check if player is standing on a Shaft (vertical visibility)
            auto shaft_view = m_registry.view<PositionComponent, ShaftComponent>();
            bool in_shaft = false;
            for (auto shaft : shaft_view) {
                auto& s_pos = shaft_view.get<PositionComponent>(shaft);
                if (s_pos.x == pos.x && s_pos.y == pos.y && s_pos.layer_id == pos.layer_id) {
                    in_shaft = true;
                    break;
                }
            }

            if (in_shaft) {
                // Propagate visibility up and down
                for (int i = 1; i <= view.view_distance; ++i) {
                    visible_layers.push_back(pos.layer_id + i);
                    visible_layers.push_back(pos.layer_id - i);
                }
            }
            
            // Store or emit visibility state for RenderingSystem
            // (e.g., via a global VisibilityState component)
        }
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H
