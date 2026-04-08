#include "player_movement_system.h"

#include <iostream>

#include "../../ecs/component_declarations.h"
#include "../../ecs/components/lod_components.h"
#include "../../ecs/events.h"

namespace NeonOubliette::Systems {

PlayerMovementSystem::PlayerMovementSystem(entt::registry& registry, entt::dispatcher& event_dispatcher)
    : registry_(registry), event_dispatcher_(event_dispatcher) {
    std::cout << "PlayerMovementSystem constructed." << std::endl;
}

void PlayerMovementSystem::initialize() {
    event_dispatcher_.sink<ECS::PlayerMoveEvent>().connect<&PlayerMovementSystem::handlePlayerMoveEvent>(this);
    event_dispatcher_.sink<ECS::PlayerLayerChangeEvent>().connect<&PlayerMovementSystem::handlePlayerLayerChangeEvent>(
        this);
    std::cout << "PlayerMovementSystem initialized and subscribed to events." << std::endl;
}

void PlayerMovementSystem::update(double delta_time) {
    (void)delta_time;
}

void PlayerMovementSystem::handlePlayerMoveEvent(const ECS::PlayerMoveEvent& event) {
    // Block player movement while in dialogue
    auto dlg_view = registry_.view<NeonOubliette::DialogueStateComponent>();
    if (!dlg_view.empty()) {
        const auto& dlg = dlg_view.get<NeonOubliette::DialogueStateComponent>(dlg_view.front());
        if (dlg.is_open) return;
    }

    auto view = registry_.view<ECS::PositionComponent, ECS::PlayerCurrentLayerComponent>();
    for (auto entity : view) {
        auto& pos = view.get<ECS::PositionComponent>(entity);
        pos.x += event.dx;
        pos.y += event.dy;

        if (registry_.all_of<ECS::HUDComponent>(entity)) {
            auto& hud = registry_.get<ECS::HUDComponent>(entity);
            hud.notifications.push_back("Moved to (" + std::to_string(static_cast<int>(pos.x)) + ", " +
                                        std::to_string(static_cast<int>(pos.y)) + ")");
            if (hud.notifications.size() > 5) {
                hud.notifications.erase(hud.notifications.begin());
            }
        }
        break;
    }
}

void PlayerMovementSystem::handlePlayerLayerChangeEvent(const ECS::PlayerLayerChangeEvent& event) {
    auto view = registry_.view<ECS::PositionComponent, ECS::PlayerCurrentLayerComponent>();
    for (auto entity : view) {
        auto& pos = view.get<ECS::PositionComponent>(entity);
        auto& layer = view.get<ECS::PlayerCurrentLayerComponent>(entity);

        // [L.5] Power Grid Check for Elevators (Layer Change)
        if (pos.layer_id != 0 || (pos.layer_id == 0 && event.dz > 0)) {
            // Determine chunk
            int cx = pos.x / 40;
            int cy = pos.y / 40;
            
            auto grid_view = registry_.view<NeonOubliette::PowerGridComponent, NeonOubliette::ChunkComponent>();
            bool has_power = true;
            for (auto g_ent : grid_view) {
                const auto& chunk = grid_view.get<NeonOubliette::ChunkComponent>(g_ent);
                if (chunk.chunk_x == cx && chunk.chunk_y == cy) {
                    if (grid_view.get<NeonOubliette::PowerGridComponent>(g_ent).power_level < 0.2f) {
                        has_power = false;
                    }
                    break;
                }
            }
            
            if (!has_power) {
                event_dispatcher_.trigger(NeonOubliette::HUDNotificationEvent{"Elevator systems offline: No power.", 2.0f, "#FF0000"});
                return;
            }
        }

        pos.layer_id += event.dz;
        layer.current_z = pos.layer_id;

        if (registry_.all_of<ECS::HUDComponent>(entity)) {
            auto& hud = registry_.get<ECS::HUDComponent>(entity);
            hud.current_layer_display = pos.layer_id;
            hud.notifications.push_back("Changed layer to " + std::to_string(pos.layer_id));
            if (hud.notifications.size() > 5) {
                hud.notifications.erase(hud.notifications.begin());
            }
        }
        break;
    }
}

} // namespace NeonOubliette::Systems
