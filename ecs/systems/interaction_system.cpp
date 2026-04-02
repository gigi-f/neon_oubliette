#include "interaction_system.h"
#include "../components/components.h"
#include "../event_declarations.h"
#include "building_generation_system.h"

namespace NeonOubliette::Systems {

InteractionSystem::InteractionSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher) {}

void InteractionSystem::initialize() {}

void InteractionSystem::update(double delta_time) {
    auto player_view = registry_.view<PlayerComponent, PositionComponent>();
    for (auto player : player_view) {
        auto& pos = player_view.get<PositionComponent>(player);

        // Check for interactive objects at player's position
        auto building_view = registry_.view<BuildingComponent, PositionComponent>();
        for (auto building : building_view) {
            auto& b_pos = building_view.get<PositionComponent>(building);
            if (pos.x == b_pos.x && pos.y == b_pos.y && pos.layer_id == b_pos.layer_id) {
                // If on top of a building, offer entrance
                event_dispatcher_.enqueue<BuildingEntranceEvent>(player, building);
            }
        }
    }
}

} // namespace NeonOubliette::Systems
