#include "interaction_system.h"
#include <algorithm>
#include "../components/components.h"
#include "../components/transit_components.h"
#include "../event_declarations.h"
#include "building_generation_system.h"

namespace NeonOubliette::Systems {

InteractionSystem::InteractionSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher) {
}

void InteractionSystem::initialize() {
    event_dispatcher_.sink<InteractEvent>().connect<&InteractionSystem::handleInteractEvent>(this);
    event_dispatcher_.sink<PickupItemEvent>().connect<&InteractionSystem::handlePickupItemEvent>(this);
}

void InteractionSystem::update(double delta_time) {
    (void)delta_time;
}

void InteractionSystem::handleInteractEvent(const InteractEvent& event) {
    if (!registry_.valid(event.entity)) return;

    // Standard Mode interaction uses target coordinates from the event
    int tx = event.x;
    int ty = event.y;
    int tl = event.layer_id;

    // [NEW] 0. Check for RidingComponent (Unboarding) - Entity's current position/state
    if (registry_.all_of<RidingComponent>(event.entity)) {
        auto vehicle = registry_.get<RidingComponent>(event.entity).vehicle;
        if (registry_.valid(vehicle)) {
            if (registry_.all_of<TransitOccupantsComponent>(vehicle)) {
                auto& occupants = registry_.get<TransitOccupantsComponent>(vehicle).occupants;
                occupants.erase(std::remove(occupants.begin(), occupants.end(), event.entity), occupants.end());
            }
            
            // If the driver exits, clear the driver field in PersonalVehicleComponent
            if (registry_.all_of<PersonalVehicleComponent>(vehicle)) {
                auto& pv = registry_.get<PersonalVehicleComponent>(vehicle);
                if (pv.driver == event.entity) {
                    pv.driver = entt::null;
                }
            }
        }
        registry_.remove<RidingComponent>(event.entity);
        event_dispatcher_.trigger(HUDNotificationEvent{"Exited vehicle.", 1.5f, "#FFFF00"});
        return;
    }

    // [NEW] 0.5. Check for Personal Vehicles at target position
    auto pv_view = registry_.view<PersonalVehicleComponent, PositionComponent, TransitOccupantsComponent>();
    for (auto pv_entity : pv_view) {
        auto& pv_pos = pv_view.get<PositionComponent>(pv_entity);
        if (tx == pv_pos.x && ty == pv_pos.y && tl == pv_pos.layer_id) {
            auto& pv = pv_view.get<PersonalVehicleComponent>(pv_entity);
            auto& occupants = pv_view.get<TransitOccupantsComponent>(pv_entity).occupants;
            
            if (occupants.size() < (size_t)pv.capacity) {
                occupants.push_back(event.entity);
                registry_.emplace<RidingComponent>(event.entity, pv_entity);
                
                // If no driver, become the driver
                if (pv.driver == entt::null) {
                    pv.driver = event.entity;
                    event_dispatcher_.trigger(HUDNotificationEvent{"Took the wheel.", 1.5f, "#00FFFF"});
                } else {
                    event_dispatcher_.trigger(HUDNotificationEvent{"Entered as passenger.", 1.5f, "#00FFFF"});
                }
                return;
            }
        }
    }

    // [NEW] 0.6. Check for Transit Stations at target position
    auto station_view = registry_.view<TransitStationComponent, PositionComponent>();
    for (auto station : station_view) {
        auto& s_pos = station_view.get<PositionComponent>(station);
        if (tx == s_pos.x && ty == s_pos.y && tl == s_pos.layer_id) {
            // Find a vehicle at this station
            auto vehicle_view = registry_.view<TransitVehicleComponent, PositionComponent, TransitOccupantsComponent>();
            for (auto vehicle : vehicle_view) {
                auto& v_pos = vehicle_view.get<PositionComponent>(vehicle);
                if (v_pos.x == s_pos.x && v_pos.y == s_pos.y && v_pos.layer_id == s_pos.layer_id) {
                    auto& transit = vehicle_view.get<TransitVehicleComponent>(vehicle);
                    auto& occupants = vehicle_view.get<TransitOccupantsComponent>(vehicle).occupants;
                    if (occupants.size() < (size_t)transit.capacity) {
                        occupants.push_back(event.entity);
                        registry_.emplace<RidingComponent>(event.entity, vehicle);
                        event_dispatcher_.trigger(HUDNotificationEvent{"Boarded vehicle.", 1.5f, "#00FFFF"});
                        return;
                    }
                }
            }
        }
    }

    // 1. Check for Building Entrance Components (Doors) at target position
    auto entrance_view = registry_.view<BuildingEntranceComponent, PositionComponent>();
    for (auto door : entrance_view) {
        auto& d_pos = entrance_view.get<PositionComponent>(door);
        if (tx == d_pos.x && ty == d_pos.y && tl == d_pos.layer_id) {
            auto& entrance = entrance_view.get<BuildingEntranceComponent>(door);
            event_dispatcher_.enqueue<BuildingEntranceEvent>({event.entity, entrance.macro_building_id, door, d_pos.x, d_pos.y, d_pos.layer_id});
            return;
        }
    }

    // 2. Check for Portals (Exit Doors) at target position
    auto portal_view = registry_.view<PortalComponent, PositionComponent>();
    for (auto portal_ent : portal_view) {
        auto& p_pos = portal_view.get<PositionComponent>(portal_ent);
        if (tx == p_pos.x && ty == p_pos.y && tl == p_pos.layer_id) {
            auto& portal = portal_view.get<PortalComponent>(portal_ent);
            
            // Perform Teleport
            if (registry_.all_of<PositionComponent>(event.entity)) {
                auto& e_pos = registry_.get<PositionComponent>(event.entity);
                e_pos.x = portal.target_x;
                e_pos.y = portal.target_y;
                e_pos.layer_id = portal.target_layer;
                
                if (registry_.all_of<PlayerCurrentLayerComponent>(event.entity)) {
                    registry_.get<PlayerCurrentLayerComponent>(event.entity).current_z = e_pos.layer_id;
                }
            }
            
            event_dispatcher_.trigger(HUDNotificationEvent{"Exited to city.", 1.5f, "#FFFFFF"});
            return;
        }
    }

    // 3. Check for items on ground at target position
    auto item_view = registry_.view<ItemComponent, PositionComponent>();
    for (auto item : item_view) {
        auto& i_pos = item_view.get<PositionComponent>(item);
        if (tx == i_pos.x && ty == i_pos.y && tl == i_pos.layer_id) {
            event_dispatcher_.enqueue<PickupItemEvent>({event.entity, item, tx, ty, tl});
            return;
        }
    }

    // 4. [NEW] Check for adjacent windows - If target is a window, or adjacent?
    // Use target coordinates from event
    auto terrain_view = registry_.view<TerrainComponent, PositionComponent>();
    for (auto terrain_ent : terrain_view) {
        if (terrain_view.get<TerrainComponent>(terrain_ent).type == TerrainType::WINDOW) {
            auto& w_pos = terrain_view.get<PositionComponent>(terrain_ent);
            if (w_pos.layer_id == tl && w_pos.x == tx && w_pos.y == ty) {
                event_dispatcher_.trigger(HUDNotificationEvent{"You peer through the glass.", 2.0f, "#00FFFF"});
                // Open a surface scan of the window/interior tile
                event_dispatcher_.enqueue<InspectEvent>({event.entity, w_pos.layer_id, w_pos.x, w_pos.y, InspectionMode::SURFACE_SCAN});
                return;
            }
        }
    }

    // 5. Check for agents at target position (for SPEAK mode)
    auto player_interact_view = registry_.view<PlayerInteractionComponent>();
    InteractionMode current_mode = InteractionMode::OBSERVE;
    if (player_interact_view.contains(event.entity)) {
        current_mode = player_interact_view.get<PlayerInteractionComponent>(event.entity).current_mode;
    }

    if (current_mode == InteractionMode::SPEAK) {
        auto agent_view = registry_.view<AgentComponent, PositionComponent>();
        for (auto agent : agent_view) {
            if (agent == event.entity) continue; // Don't speak to yourself
            auto& a_pos = agent_view.get<PositionComponent>(agent);
            if (tx == a_pos.x && ty == a_pos.y && tl == a_pos.layer_id) {
                event_dispatcher_.trigger(DialogueEvent{event.entity, agent});
                return;
            }
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"No one to speak to here.", 1.0f, "#AAAAAA"});
        return;
    }

    // 6. OBSERVE mode: describe whatever is at the target tile
    if (current_mode == InteractionMode::OBSERVE) {
        // 6a. Named entity (building, kiosk, furniture, etc.)
        auto named_view = registry_.view<NameComponent, PositionComponent>();
        for (auto named_ent : named_view) {
            const auto& n_pos = named_view.get<PositionComponent>(named_ent);
            if (n_pos.x == tx && n_pos.y == ty && n_pos.layer_id == tl) {
                const auto& nm = named_view.get<NameComponent>(named_ent);
                if (!nm.name.empty()) {
                    event_dispatcher_.trigger(HUDNotificationEvent{"You see: " + nm.name, 2.5f, "#AADDFF"});
                    return;
                }
            }
        }

        // 6b. NPC / citizen
        auto npc_view = registry_.view<NPCComponent, PositionComponent>();
        for (auto npc : npc_view) {
            const auto& n_pos = npc_view.get<PositionComponent>(npc);
            if (n_pos.x == tx && n_pos.y == ty && n_pos.layer_id == tl) {
                std::string npc_name = "a citizen";
                if (registry_.all_of<NameComponent>(npc))
                    npc_name = registry_.get<NameComponent>(npc).name;
                event_dispatcher_.trigger(HUDNotificationEvent{"You see " + npc_name + ".", 2.0f, "#FFFF88"});
                return;
            }
        }

        // 6c. Terrain tile description
        auto terr_view = registry_.view<TerrainComponent, PositionComponent>();
        for (auto terr_ent : terr_view) {
            const auto& t_pos = terr_view.get<PositionComponent>(terr_ent);
            if (t_pos.x == tx && t_pos.y == ty && t_pos.layer_id == tl) {
                const auto& terr = terr_view.get<TerrainComponent>(terr_ent);
                std::string desc;
                switch (terr.type) {
                    case TerrainType::STREET:         desc = "a road."; break;
                    case TerrainType::SIDEWALK:       desc = "a sidewalk."; break;
                    case TerrainType::GRASS:          desc = "a patch of grass."; break;
                    case TerrainType::DIRT:           desc = "bare dirt."; break;
                    case TerrainType::CONCRETE_FLOOR: desc = "concrete."; break;
                    case TerrainType::WOOD_FLOOR:     desc = "a wooden floor."; break;
                    case TerrainType::WALL:           desc = "a wall."; break;
                    case TerrainType::WINDOW:         desc = "a window."; break;
                    case TerrainType::OFFICE_CARPET:  desc = "office carpet."; break;
                    case TerrainType::FLOWER_BED:     desc = "a flower bed."; break;
                    case TerrainType::WATER_FEATURE:  desc = "a water feature."; break;
                    case TerrainType::ARENA_FLOOR:    desc = "the arena floor."; break;
                    case TerrainType::ARENA_SEATING:  desc = "arena seating."; break;
                    case TerrainType::VOID:           desc = "deep water."; break;
                    default:                          desc = "the ground."; break;
                }
                // Also show glyph context if there's a renderable
                std::string glyph_hint;
                if (registry_.all_of<RenderableComponent>(terr_ent)) {
                    char g = registry_.get<RenderableComponent>(terr_ent).glyph;
                    if (g == '~') glyph_hint = " Looks like a river.";
                    else if (g == '=') glyph_hint = " Elevated rail track.";
                    else if (g == '"') glyph_hint = " Overgrown.";
                }
                event_dispatcher_.trigger(HUDNotificationEvent{"It's " + desc + glyph_hint, 2.0f, "#AADDAA"});
                return;
            }
        }

        // 6d. Nothing identifiable at all
        event_dispatcher_.trigger(HUDNotificationEvent{"Nothing here.", 1.5f, "#888888"});
        return;
    }

    // 7. Fallback (non-OBSERVE modes)
    event_dispatcher_.trigger(HUDNotificationEvent{"Nothing to interact with here.", 1.0f, "#AAAAAA"});
}

void InteractionSystem::handlePickupItemEvent(const PickupItemEvent& event) {
    if (!registry_.valid(event.picker_entity) || !registry_.valid(event.item_entity)) return;

    if (registry_.all_of<InventoryComponent>(event.picker_entity)) {
        auto& inv = registry_.get<InventoryComponent>(event.picker_entity);
        inv.contained_items.push_back(event.item_entity);
        
        // Remove from world (remove PositionComponent)
        registry_.remove<PositionComponent>(event.item_entity);
        
        std::string name = "item";
        if (registry_.all_of<NameComponent>(event.item_entity)) {
            name = registry_.get<NameComponent>(event.item_entity).name;
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"Picked up " + name, 1.5f, "#00FF00"});
    }
}

} // namespace NeonOubliette::Systems
