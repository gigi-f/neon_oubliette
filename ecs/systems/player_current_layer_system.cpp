#include "player_current_layer_system.h"

#include "../component_declarations.h" // For PositionComponent, PlayerCurrentLayerComponent, VerticalViewComponent

namespace NeonOubliette::Systems {

PlayerCurrentLayerSystem::PlayerCurrentLayerSystem(entt::registry& registry) : registry_(registry) {
}

void PlayerCurrentLayerSystem::update(double delta_time) {
    entt::registry& registry = registry_;
    // Placeholder logic for PlayerCurrentLayerSystem

    // Example: Find the player entity and print its current layer (placeholder)
    auto view = registry.view<ECS::PositionComponent, ECS::PlayerCurrentLayerComponent>();
    for (auto entity : view) {
        auto& pos = view.get<ECS::PositionComponent>(entity);
    }

    // Example: Find entities with VerticalViewComponent and update (placeholder)
    auto vertical_view_view = registry.view<ECS::VerticalViewComponent>();
    for (auto entity : vertical_view_view) {
        auto& vertical_view = vertical_view_view.get<ECS::VerticalViewComponent>(entity);
        // Update vertical_view.visible_layers based on scanner logic
    }
}

} // namespace NeonOubliette::Systems
