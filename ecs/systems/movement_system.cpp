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

        // Block movement while speaking
        if (m_registry.all_of<SpeechComponent>(event.entity)) return;

        // Block movement if this entity is the active dialogue target of the player
        auto dlg_view = m_registry.view<DialogueStateComponent>();
        if (!dlg_view.empty()) {
            const auto& dlg = dlg_view.get<DialogueStateComponent>(dlg_view.front());
            if (dlg.is_open && dlg.target_agent == event.entity) return;
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

        PositionComponent target_pos(target_x, target_y, target_layer);

        // --- Volumetric Collision Detection ---
        bool blocked = false;
        auto& spatial = get_spatial_query_cache(m_registry);

        // [A.8] Interior Nav Grid Check (Static Obstacles)
        entt::entity blocker = entt::null;
        if (target_layer != 0) {
            auto floor_view = m_registry.view<FloorComponent>();
            for (auto floor_ent : floor_view) {
                const auto& floor = floor_view.get<FloorComponent>(floor_ent);
                if (floor.layer_id == target_layer) {
                    if (!floor.nav_grid.is_passable(target_x, target_y)) {
                        blocked = true;
                        // For NavGrid, we don't assign a specific entity as the blocker 
                        // so it falls through to the "You walked into a wall" logic.
                    }
                    break;
                }
            }
        }
        
        if (!blocked) {
            auto obs_it = spatial.obstacle_at.find(target_pos);
            if (obs_it != spatial.obstacle_at.end()) {
                const entt::entity obstacle = obs_it->second;
                if (obstacle != event.entity && m_registry.valid(obstacle)) {
                    // Broken windows are passable crawl points.
                    if (!spatial.passable_window_tiles.count(target_pos)) {
                        // [B.1] Building entrances are traversable and trigger a transition event.
                        if (m_registry.all_of<BuildingComponent>(obstacle)) {
                            auto door_it = spatial.entrance_entity_at.find(target_pos);
                            if (door_it != spatial.entrance_entity_at.end()) {
                                m_dispatcher.trigger(BuildingEntranceEvent{event.entity, obstacle, door_it->second, target_x, target_y, target_layer});
                                // Door allows entry; layer transition is handled by BuildingEntranceSystem.
                                return;
                            }
                        }

                        blocked = true;
                        blocker = obstacle;
                    }
                }
            }
        }

        if (blocked) {
            if (m_registry.all_of<PlayerComponent>(event.entity)) {
                std::string message = "Blocked by an obstacle.";
                if (m_registry.valid(blocker)) {
                    if (auto* name_comp = m_registry.try_get<NameComponent>(blocker)) {
                        if (name_comp->name == "Wall") message = "You walked into a wall.";
                        else if (name_comp->name == "Window") message = "Blocked by a window.";
                        else if (name_comp->name == "Tree") message = "Blocked by a tree.";
                        else message = "Blocked by " + name_comp->name + ".";
                    } else if (auto* terrain = m_registry.try_get<TerrainComponent>(blocker)) {
                        if (terrain->type == TerrainType::WALL) message = "You walked into a wall.";
                        else if (terrain->type == TerrainType::WINDOW) message = "Blocked by a window.";
                    } else if (m_registry.all_of<BuildingComponent>(blocker)) {
                        message = "Blocked by a building.";
                    } else if (m_registry.all_of<PersonalVehicleComponent>(blocker)) {
                        message = "Blocked by a vehicle.";
                    }
                } else {
                    // Nav grid blockage is effectively a wall in an interior
                    message = "You walked into a wall.";
                }
                m_dispatcher.trigger(HUDNotificationEvent{message, 1.0f, "#FF5555"});
            }
            return;
        }

        pos.x = target_x;
        pos.y = target_y;
        pos.layer_id = target_layer;

        // [B.5] Update Room Index
        if (m_registry.all_of<InteriorStateComponent>(event.entity)) {
            auto& interior_state = m_registry.get<InteriorStateComponent>(event.entity);
            if (m_registry.valid(interior_state.building_entity) && m_registry.all_of<BuildingInteriorComponent>(interior_state.building_entity)) {
                const auto& interior = m_registry.get<BuildingInteriorComponent>(interior_state.building_entity);
                interior_state.current_room_index = -1;
                for (size_t i = 0; i < interior.rooms.size(); ++i) {
                    const auto& room = interior.rooms[i];
                    if (pos.x >= room.x && pos.x < room.x + room.width &&
                        pos.y >= room.y && pos.y < room.y + room.height) {
                        interior_state.current_room_index = (int)i;
                        break;
                    }
                }
            }
        }

        // [B.2] Portal Handling (Consistent Exits)
        auto portal_view = m_registry.view<PositionComponent, PortalComponent>();
        for (auto portal_ent : portal_view) {
            const auto& p_pos = portal_view.get<PositionComponent>(portal_ent);
            if (p_pos.x == pos.x && p_pos.y == pos.y && p_pos.layer_id == pos.layer_id) {
                const auto& portal = portal_view.get<PortalComponent>(portal_ent);
                
                // [M.2] Collision Check on Portal Destination
                bool portal_blocked = false;
                PositionComponent portal_target(portal.target_x, portal.target_y, portal.target_layer);
                auto portal_obs_it = spatial.obstacle_at.find(portal_target);
                if (portal_obs_it != spatial.obstacle_at.end()) {
                    const entt::entity portal_blocker = portal_obs_it->second;
                    if (portal_blocker != event.entity && m_registry.valid(portal_blocker) &&
                        !spatial.passable_window_tiles.count(portal_target)) {
                        portal_blocked = true;
                    }
                }
                
                if (portal_blocked) {
                    if (m_registry.all_of<PlayerComponent>(event.entity)) {
                        m_dispatcher.trigger(HUDNotificationEvent{"Portal blocked from the other side.", 2.0f, "#FF5555"});
                    }
                    break;
                }

                pos.x = portal.target_x;
                pos.y = portal.target_y;
                pos.layer_id = portal.target_layer;
                
                if (m_registry.all_of<PlayerCurrentLayerComponent>(event.entity)) {
                    m_registry.get<PlayerCurrentLayerComponent>(event.entity).current_z = pos.layer_id;
                }
                
                if (m_registry.all_of<PlayerComponent>(event.entity)) {
                    std::string message = "Traversed to layer " + std::to_string(pos.layer_id) + ".";
                    if (pos.layer_id == 0) message = "Exited to city level.";
                    else if (pos.layer_id == -1) message = "Descended into the sewers.";
                    else if (pos.layer_id == 5) message = "Accessed the elevated rail.";
                    
                    m_dispatcher.trigger(HUDNotificationEvent{message, 2.0f, "#FFFF00"});
                    
                    // Remove interior state if we are back in overworld (layer 0) or sewer
                    if (pos.layer_id <= 0 && m_registry.all_of<InteriorStateComponent>(event.entity)) {
                        m_registry.remove<InteriorStateComponent>(event.entity);
                    }
                }
                break;
            }
        }

        // Keep shared spatial cache coherent when a spatial blocker has moved.
        if (m_registry.all_of<ObstacleComponent>(event.entity) ||
            m_registry.all_of<TerrainComponent>(event.entity) ||
            m_registry.all_of<BuildingEntranceComponent>(event.entity)) {
            invalidate_spatial_query_cache(m_registry);
        }

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
