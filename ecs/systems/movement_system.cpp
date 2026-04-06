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

        // [A.8] Interior Nav Grid Check (Static Obstacles)
        if (target_layer != 0) {
            auto floor_view = m_registry.view<FloorComponent>();
            for (auto floor_ent : floor_view) {
                const auto& floor = floor_view.get<FloorComponent>(floor_ent);
                if (floor.layer_id == target_layer) {
                    if (!floor.nav_grid.is_passable(target_x, target_y)) {
                        blocked = true;
                    }
                    break;
                }
            }
        }
        
        if (!blocked) {
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
                    
                    // [B.1] Check if this is a building and if we are hitting a door
                    if (m_registry.all_of<BuildingComponent>(obstacle)) {
                        bool found_door = false;
                        auto door_view = m_registry.view<PositionComponent, BuildingEntranceComponent>();
                        for (auto door_ent : door_view) {
                            const auto& d_pos = door_view.get<PositionComponent>(door_ent);
                            if (d_pos.x == target_x && d_pos.y == target_y && d_pos.layer_id == target_layer) {
                                found_door = true;
                                m_dispatcher.trigger(BuildingEntranceEvent{event.entity, obstacle, door_ent, target_x, target_y, target_layer});
                                break;
                            }
                        }
                        if (found_door) {
                            // Door allows entry; we stop processing move here as layer transition is handled by BuildingEntranceSystem
                            return;
                        }
                    }

                    blocked = true;
                    break;
                }
            }
        }

        // Check against single-tile obstacles without size component
        if (!blocked) {
            auto single_obs = m_registry.view<PositionComponent, ObstacleComponent>(entt::exclude<SizeComponent>);
            for (auto obstacle : single_obs) {
                if (obstacle == event.entity) continue;
                const auto& o_pos = single_obs.get<PositionComponent>(obstacle);
                if (o_pos.x == target_x && o_pos.y == target_y && o_pos.layer_id == target_layer) {
                    // [NEW] Broken windows are passable (crawl point)
                    if (m_registry.all_of<TerrainComponent>(obstacle)) {
                        if (m_registry.get<TerrainComponent>(obstacle).type == TerrainType::WINDOW) {
                            if (auto* phys = m_registry.try_get<Layer0PhysicsComponent>(obstacle)) {
                                if (phys->structural_integrity < 0.5f) {
                                    // Passable!
                                    continue;
                                }
                            }
                        }
                    }
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
                pos.x = portal.target_x;
                pos.y = portal.target_y;
                pos.layer_id = portal.target_layer;
                
                if (m_registry.all_of<PlayerCurrentLayerComponent>(event.entity)) {
                    m_registry.get<PlayerCurrentLayerComponent>(event.entity).current_z = pos.layer_id;
                }
                
                if (m_registry.all_of<PlayerComponent>(event.entity)) {
                    m_dispatcher.trigger(HUDNotificationEvent{"Exited to city level.", 2.0f, "#FFFF00"});
                    // Remove interior state if we are back in overworld (layer 0)
                    if (pos.layer_id == 0 && m_registry.all_of<InteriorStateComponent>(event.entity)) {
                        m_registry.remove<InteriorStateComponent>(event.entity);
                    }
                }
                break;
            }
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
