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
    event_dispatcher_.sink<DropItemEvent>().connect<&InteractionSystem::handleDropItemEvent>(this);
    event_dispatcher_.sink<AttackEvent>().connect<&InteractionSystem::handleAttackEvent>(this);
    event_dispatcher_.sink<StealAttemptEvent>().connect<&InteractionSystem::handleStealAttemptEvent>(this);
}

void InteractionSystem::update(double delta_time) {
    (void)delta_time;
}

void InteractionSystem::handleDropItemEvent(const DropItemEvent& event) {
    if (!registry_.valid(event.dropper_entity) || !registry_.valid(event.item_entity)) return;

    if (registry_.all_of<InventoryComponent>(event.dropper_entity)) {
        auto& inv = registry_.get<InventoryComponent>(event.dropper_entity);
        inv.contained_items.erase(std::remove(inv.contained_items.begin(), inv.contained_items.end(), event.item_entity), inv.contained_items.end());
        
        // Add back to world
        registry_.emplace_or_replace<PositionComponent>(event.item_entity, event.x, event.y, event.layer_id);
        
        // --- [P.2] Charity Marking & Witnessing ---
        if (registry_.all_of<PlayerComponent>(event.dropper_entity)) {
            uint64_t current_tick = 0;
            auto city_view = registry_.view<CityComponent>();
            if (!city_view.empty()) current_tick = registry_.get<CityComponent>(city_view.front()).time_tick;
            registry_.emplace_or_replace<DroppedByPlayerComponent>(event.item_entity, current_tick);

            // Witnessing: If anyone sees the player drop it, they get credit for charity immediately.
            auto witness_view = registry_.view<PositionComponent, VisibilityComponent, NPCComponent>();
            for (auto witness : witness_view) {
                if (witness == event.dropper_entity) continue;
                auto& w_pos = witness_view.get<PositionComponent>(witness);
                if (w_pos.layer_id == event.layer_id) {
                    auto& w_vis = witness_view.get<VisibilityComponent>(witness);
                    if (w_vis.visible_tiles.count(PositionComponent(event.x, event.y, event.layer_id))) {
                        // NPC saw the drop!
                        if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(witness)) {
                            event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                                event.dropper_entity,
                                pol->primary_faction,
                                1.0f // Initial witness credit
                            });
                        }
                    }
                }
            }
        }

        std::string name = "item";
        if (registry_.all_of<ItemComponent>(event.item_entity)) {
            name = registry_.get<ItemComponent>(event.item_entity).name;
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"Dropped " + name, 1.5f, "#FFAA00"});
    }
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

    // [O.4] Check for Factory interactions at target position
    auto factory_view = registry_.view<FactoryComponent, PositionComponent>();
    for (auto factory_ent : factory_view) {
        auto& f_pos = factory_view.get<PositionComponent>(factory_ent);
        if (tx == f_pos.x && ty == f_pos.y && tl == f_pos.layer_id) {
            auto& factory = factory_view.get<FactoryComponent>(factory_ent);
            
            // If the factory is a jobsite, start shift
            if (registry_.all_of<PlayerComponent>(event.entity)) {
                if (registry_.all_of<ActivityComponent>(event.entity)) {
                    event_dispatcher_.trigger(HUDNotificationEvent{"Already busy working.", 1.5f, "#AAAAAA"});
                } else {
                    // Check if player has a job there (already defined contract)
                    bool has_job = false;
                    auto job_view = registry_.view<EmploymentContractComponent>();
                    if (registry_.all_of<EmploymentContractComponent>(event.entity)) {
                        auto& job = registry_.get<EmploymentContractComponent>(event.entity);
                        if (job.boss_entity == factory_ent || job.job_title.find("Factory") != std::string::npos) {
                             has_job = true;
                        }
                    }

                    if (has_job) {
                        event_dispatcher_.trigger(StartActivityEvent{event.entity, ActivityType::WORKING, 50, factory_ent, entt::null, "Working shift at factory"});
                        event_dispatcher_.trigger(HUDNotificationEvent{"Shift started. Working...", 2.0f, "#00FFFF"});
                    } else {
                        // Opportunity to apply?
                        registry_.emplace_or_replace<EmploymentContractComponent>(event.entity, 25, 50, factory_ent, "Factory Assembler");
                        event_dispatcher_.trigger(HUDNotificationEvent{"Hired as Factory Assembler! Press interact again to start shift.", 3.0f, "#00FFCC"});
                    }
                }
            }
            return;
        }
    }

    // [NEW] 3.5. Check for Pirate Nodes at target position
    auto node_view = registry_.view<PirateNodeComponent, PositionComponent>();
    for (auto node_ent : node_view) {
        auto& n_pos = node_view.get<PositionComponent>(node_ent);
        if (tx == n_pos.x && ty == n_pos.y && tl == n_pos.layer_id) {
            // If the node is hidden, reveal it first
            auto& node = node_view.get<PirateNodeComponent>(node_ent);
            if (node.is_hidden) {
                node.is_hidden = false;
                event_dispatcher_.trigger(HUDNotificationEvent{"Discovered hidden broadcast node.", 2.0f, "#FF00FF"});
            } else {
                // [N.4] Open Console UI via Dialogue metaphor
                event_dispatcher_.trigger(DialogueEvent{event.entity, node_ent});
            }
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
                // [P.3] Reputation Check
                if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(agent)) {
                    if (auto* rep = registry_.try_get<ReputationComponent>(event.entity)) {
                        ReputationTier tier = rep->get_tier(pol->primary_faction);
                        if (tier == ReputationTier::EXCOMMUNICATED) {
                            event_dispatcher_.trigger(HUDNotificationEvent{"This faction has excommunicated you. They will not speak.", 2.5f, "#FF3333"});
                            return;
                        }
                    }
                }

                event_dispatcher_.trigger(DialogueEvent{event.entity, agent});
                return;
            }
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"No one to speak to here.", 1.0f, "#AAAAAA"});
        return;
    }

    // 5.5. TRADE mode: open barter UI with agent at target tile [Phase T.1]
    if (current_mode == InteractionMode::TRADE) {
        auto agent_view = registry_.view<AgentComponent, PositionComponent>();
        for (auto agent : agent_view) {
            if (agent == event.entity) continue; 
            auto& a_pos = agent_view.get<PositionComponent>(agent);
            if (tx == a_pos.x && ty == a_pos.y && tl == a_pos.layer_id) {
                // [P.3] Reputation Check
                if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(agent)) {
                    if (auto* rep = registry_.try_get<ReputationComponent>(event.entity)) {
                        ReputationTier tier = rep->get_tier(pol->primary_faction);
                        if (tier == ReputationTier::EXCOMMUNICATED || tier == ReputationTier::HOSTILE) {
                             event_dispatcher_.trigger(HUDNotificationEvent{"Faction standing too low to trade.", 2.5f, "#FF3333"});
                             return;
                        }
                    }
                }
                event_dispatcher_.trigger(OpenBarterEvent{event.entity, agent});
                return;
            }
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"No one to trade with here.", 1.0f, "#AAAAAA"});
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
                    case TerrainType::SEWER_FLOOR:    desc = "the damp sewer floor."; break;
                    case TerrainType::SEWER_WATER:    desc = "fetid sewer water."; break;
                    case TerrainType::VOID:           desc = "deep water."; break;
                    default:                          desc = "the ground."; break;
                }
                // Also show glyph context if there's a renderable
                std::string glyph_hint;
                if (registry_.all_of<RenderableComponent>(terr_ent)) {
                    char g = registry_.get<RenderableComponent>(terr_ent).glyph;
                    if (terr.type == TerrainType::RAIL) glyph_hint = " Elevated rail track.";
                    else if (g == '~') glyph_hint = " Looks like a river.";
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

    // 7. ACTION mode: pickup, steal, search container, attack
    if (current_mode == InteractionMode::ACTION) {
        // 7a. Pick up item on ground at target tile
        auto item_view = registry_.view<ItemComponent, PositionComponent>();
        for (auto item_ent : item_view) {
            const auto& i_pos = item_view.get<PositionComponent>(item_ent);
            if (i_pos.x == tx && i_pos.y == ty && i_pos.layer_id == tl) {
                event_dispatcher_.trigger(PickupItemEvent{event.entity, item_ent, tx, ty, tl});
                return;
            }
        }

        // 7b. Attack or steal from NPC at target tile
        auto agent_view = registry_.view<AgentComponent, PositionComponent>();
        for (auto agent : agent_view) {
            if (agent == event.entity) continue;
            const auto& a_pos = agent_view.get<PositionComponent>(agent);
            if (a_pos.x == tx && a_pos.y == ty && a_pos.layer_id == tl) {
                // Check if player is holding a weapon — attack
                bool has_weapon = false;
                if (registry_.all_of<HUDComponent>(event.entity)) {
                    auto& hud = registry_.get<HUDComponent>(event.entity);
                    if (registry_.valid(hud.held_item) && registry_.all_of<WeaponComponent>(hud.held_item)) {
                        has_weapon = true;
                    }
                }

                if (has_weapon) {
                    event_dispatcher_.trigger(AttackEvent{event.entity, agent});
                    return;
                }

                // Otherwise attempt pickpocket (steal from NPC inventory)
                event_dispatcher_.trigger(StealAttemptEvent{event.entity, agent});
                return;
            }
        }

        // 7c. Search / open a container at target tile
        auto container_view = registry_.view<ContainerComponent, PositionComponent>();
        for (auto container_ent : container_view) {
            const auto& c_pos = container_view.get<PositionComponent>(container_ent);
            if (c_pos.x == tx && c_pos.y == ty && c_pos.layer_id == tl) {
                auto& container = container_view.get<ContainerComponent>(container_ent);
                if (container.is_locked) {
                    // Check if player holds a matching key
                    bool unlocked = false;
                    if (registry_.all_of<InventoryComponent>(event.entity)) {
                        for (auto key_ent : registry_.get<InventoryComponent>(event.entity).contained_items) {
                            if (registry_.all_of<KeyComponent>(key_ent)) {
                                unlocked = true;
                                container.is_locked = false;
                                std::string key_name = "key";
                                if (registry_.all_of<NameComponent>(key_ent)) key_name = registry_.get<NameComponent>(key_ent).name;
                                event_dispatcher_.trigger(HUDNotificationEvent{"Unlocked with " + key_name + ".", 2.0f, "#FFFF00"});
                                break;
                            }
                        }
                    }
                    if (!unlocked) {
                        event_dispatcher_.trigger(HUDNotificationEvent{"It's locked.", 1.5f, "#FF5555"});
                        return;
                    }
                }
                container.is_open = !container.is_open;
                if (!container.is_open) {
                    event_dispatcher_.trigger(HUDNotificationEvent{"Closed.", 1.0f, "#AAAAAA"});
                    return;
                }
                // List contents
                if (container.contained_items.empty()) {
                    event_dispatcher_.trigger(HUDNotificationEvent{"Empty.", 1.5f, "#888888"});
                } else {
                    std::string contents = "Contains: ";
                    for (int ci = 0; ci < (int)container.contained_items.size() && ci < 4; ++ci) {
                        auto item_e = container.contained_items[ci];
                        std::string iname = "item";
                        if (registry_.all_of<NameComponent>(item_e)) iname = registry_.get<NameComponent>(item_e).name;
                        if (ci > 0) contents += ", ";
                        contents += iname;
                    }
                    if (container.contained_items.size() > 4) contents += "...";
                    event_dispatcher_.trigger(HUDNotificationEvent{contents, 3.0f, "#AADDFF"});
                    // Transfer all items to the player's inventory
                    if (registry_.all_of<InventoryComponent>(event.entity)) {
                        auto& inv = registry_.get<InventoryComponent>(event.entity);
                        for (auto take_ent : container.contained_items) {
                            inv.contained_items.push_back(take_ent);
                        }
                        container.contained_items.clear();
                        // Mark as theft if inside a building and witnessed
                        if (registry_.all_of<InteriorStateComponent>(event.entity)) {
                            event_dispatcher_.enqueue<CrimeReportEvent>({
                                event.entity, container_ent, tx, ty, tl, "THEFT", 0
                            });
                        }
                    }
                }
                return;
            }
        }

        // 7d. Use held item on nothing / self (e.g. consume food)
        if (registry_.all_of<HUDComponent>(event.entity)) {
            auto& hud = registry_.get<HUDComponent>(event.entity);
            if (registry_.valid(hud.held_item)) {
                event_dispatcher_.trigger(UseItemEvent{event.entity, hud.held_item});
                return;
            }
        }

        event_dispatcher_.trigger(HUDNotificationEvent{"Nothing to act on here.", 1.0f, "#AAAAAA"});
        return;
    }

    // 8. Fallback (non-OBSERVE modes)
    event_dispatcher_.trigger(HUDNotificationEvent{"Nothing to interact with here.", 1.0f, "#AAAAAA"});
}

void InteractionSystem::handlePickupItemEvent(const PickupItemEvent& event) {
    if (!registry_.valid(event.picker_entity) || !registry_.valid(event.item_entity)) return;

    bool is_player = registry_.all_of<PlayerComponent>(event.picker_entity);

    if (registry_.all_of<InventoryComponent>(event.picker_entity)) {
        auto& inv = registry_.get<InventoryComponent>(event.picker_entity);
        inv.contained_items.push_back(event.item_entity);
        
        // --- [P.2] Charity Detection ---
        if (!is_player && registry_.all_of<DroppedByPlayerComponent>(event.item_entity)) {
            auto const& dropped = registry_.get<DroppedByPlayerComponent>(event.item_entity);
            uint64_t current_tick = 0;
            auto city_view = registry_.view<CityComponent>();
            if (!city_view.empty()) current_tick = registry_.get<CityComponent>(city_view.front()).time_tick;
            
            // Only count as charity if picked up within 500 ticks of drop
            if (current_tick - dropped.tick_dropped < 500) {
                // If it's a "POOR" NPC (Hunger/Thirst low), increase player reputation
                bool is_needy = false;
                if (auto* needs = registry_.try_get<NeedsComponent>(event.picker_entity)) {
                    if (needs->hunger < 40.0f || needs->thirst < 40.0f) is_needy = true;
                }

                if (is_needy) {
                    auto player_view = registry_.view<PlayerComponent>();
                    if (!player_view.empty()) {
                        auto player = player_view.front();
                        
                        // Faction reputation boost
                        if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(event.picker_entity)) {
                            event_dispatcher_.enqueue<AgentFactionReputationEvent>({
                                player,
                                pol->primary_faction,
                                3.0f // Charity is well-regarded
                            });
                        }
                        
                        // Fame boost
                        if (auto* rep = registry_.try_get<ReputationComponent>(player)) {
                            rep->fame = std::min(1.0f, rep->fame + 0.005f);
                        }

                        std::string picker_name = "A citizen";
                        if (auto* name_comp = registry_.try_get<NameComponent>(event.picker_entity)) {
                            picker_name = name_comp->name;
                        }
                        event_dispatcher_.trigger(HUDNotificationEvent{picker_name + " appreciated your charity.", 3.0f, "#55FF55"});
                    }
                }
            }
        }
        registry_.remove<DroppedByPlayerComponent>(event.item_entity);

        // --- [I.5] Theft Detection ---
        if (is_player) {
            // If item is in a building (interior), and not owned by player (implied for now)
            // check if any guard sees the player.
            bool in_interior = registry_.all_of<InteriorStateComponent>(event.picker_entity);
            if (in_interior) {
                auto guard_view = registry_.view<PositionComponent, PatrolComponent, VisibilityComponent>();
                for (auto guard : guard_view) {
                    const auto& g_pos = guard_view.get<PositionComponent>(guard);
                    if (g_pos.layer_id == event.layer_id) {
                        const auto& g_vis = guard_view.get<VisibilityComponent>(guard);
                        if (g_vis.visible_tiles.count(PositionComponent(event.x, event.y, event.layer_id))) {
                            // Guard saw the player pick it up!
                            event_dispatcher_.enqueue<CrimeReportEvent>({
                                event.picker_entity,
                                entt::null, // Victim unknown or the building owner
                                event.x, event.y, event.layer_id,
                                "THEFT",
                                0 // tick filled by system
                            });
                            event_dispatcher_.trigger(HUDNotificationEvent{"Witnessed! Theft reported.", 2.0f, "#FF0000"});
                            break;
                        }
                    }
                }
            }
        }
        
        // Remove from world (remove PositionComponent)
        registry_.remove<PositionComponent>(event.item_entity);
        
        std::string name = "item";
        if (registry_.all_of<NameComponent>(event.item_entity)) {
            name = registry_.get<NameComponent>(event.item_entity).name;
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"Picked up " + name, 1.5f, "#00FF00"});
    }
}

void InteractionSystem::handleAttackEvent(const AttackEvent& event) {
    if (!registry_.valid(event.attacker_entity) || !registry_.valid(event.target_entity)) return;

    int damage = 5; // unarmed
    std::string weapon_name = "fist";

    if (registry_.all_of<HUDComponent>(event.attacker_entity)) {
        auto& hud = registry_.get<HUDComponent>(event.attacker_entity);
        if (registry_.valid(hud.held_item) && registry_.all_of<WeaponComponent>(hud.held_item)) {
            damage = registry_.get<WeaponComponent>(hud.held_item).damage;
            if (registry_.all_of<NameComponent>(hud.held_item))
                weapon_name = registry_.get<NameComponent>(hud.held_item).name;
            // Degrade weapon
            if (registry_.all_of<DurabilityComponent>(hud.held_item)) {
                auto& dur = registry_.get<DurabilityComponent>(hud.held_item);
                dur.current = std::max(0.0f, dur.current - 1.0f);
            }
        }
    }

    if (registry_.all_of<NPCComponent>(event.target_entity)) {
        auto& npc = registry_.get<NPCComponent>(event.target_entity);
        npc.health = std::max(0, npc.health - damage);

        std::string target_name = "target";
        if (registry_.all_of<NameComponent>(event.target_entity))
            target_name = registry_.get<NameComponent>(event.target_entity).name;

        event_dispatcher_.trigger(HUDNotificationEvent{
            "Hit " + target_name + " with " + weapon_name + " for " + std::to_string(damage) + " dmg.",
            2.0f, "#FF4444"
        });
    }

    // Crime report only if witnessed or victim survives to report it
    if (registry_.all_of<PlayerComponent>(event.attacker_entity)) {
        const auto& pos = registry_.get<PositionComponent>(event.target_entity);

        bool victim_survives = registry_.all_of<NPCComponent>(event.target_entity) &&
            registry_.get<NPCComponent>(event.target_entity).health > 0;

        bool witnessed = false;
        auto witness_view = registry_.view<PositionComponent, VisibilityComponent, NPCComponent>();
        for (auto witness : witness_view) {
            if (witness == event.attacker_entity || witness == event.target_entity) continue;
            const auto& w_pos = witness_view.get<PositionComponent>(witness);
            if (w_pos.layer_id == pos.layer_id) {
                const auto& w_vis = witness_view.get<VisibilityComponent>(witness);
                if (w_vis.visible_tiles.count(pos)) { witnessed = true; break; }
            }
        }

        if (victim_survives || witnessed) {
            event_dispatcher_.enqueue<CrimeReportEvent>({
                event.attacker_entity, event.target_entity,
                pos.x, pos.y, pos.layer_id,
                "ASSAULT", 0
            });
            std::string reporter = victim_survives ? "Victim" : "Witness";
            event_dispatcher_.trigger(HUDNotificationEvent{
                reporter + " reporting assault to authorities.", 2.5f, "#FF8800"
            });
        }
    }
}

void InteractionSystem::handleStealAttemptEvent(const StealAttemptEvent& event) {
    if (!registry_.valid(event.thief_entity) || !registry_.valid(event.target_entity)) return;
    if (!registry_.all_of<InventoryComponent>(event.target_entity)) {
        event_dispatcher_.trigger(HUDNotificationEvent{"Nothing to steal.", 1.0f, "#AAAAAA"});
        return;
    }

    auto& target_inv = registry_.get<InventoryComponent>(event.target_entity);
    if (target_inv.contained_items.empty()) {
        event_dispatcher_.trigger(HUDNotificationEvent{"Their pockets are empty.", 1.5f, "#AAAAAA"});
        return;
    }

    // Take first item from target's inventory
    entt::entity stolen = target_inv.contained_items.front();
    target_inv.contained_items.erase(target_inv.contained_items.begin());

    registry_.emplace_or_replace<StolenComponent>(stolen, event.target_entity);

    if (registry_.all_of<InventoryComponent>(event.thief_entity)) {
        registry_.get<InventoryComponent>(event.thief_entity).contained_items.push_back(stolen);
    }

    std::string item_name = "item";
    if (registry_.all_of<NameComponent>(stolen)) item_name = registry_.get<NameComponent>(stolen).name;

    event_dispatcher_.trigger(HUDNotificationEvent{"Stole " + item_name + ".", 2.0f, "#FF8800"});

    // Crime report only if victim notices or a bystander witnesses the theft
    if (registry_.all_of<PlayerComponent>(event.thief_entity)) {
        const auto& pos = registry_.get<PositionComponent>(event.target_entity);
        const auto& thief_pos = registry_.get<PositionComponent>(event.thief_entity);

        bool victim_notices = false;
        if (registry_.all_of<VisibilityComponent>(event.target_entity)) {
            const auto& v_vis = registry_.get<VisibilityComponent>(event.target_entity);
            if (v_vis.visible_tiles.count(thief_pos)) victim_notices = true;
        }

        bool witnessed = false;
        if (!victim_notices) {
            auto witness_view = registry_.view<PositionComponent, VisibilityComponent, NPCComponent>();
            for (auto witness : witness_view) {
                if (witness == event.thief_entity || witness == event.target_entity) continue;
                const auto& w_pos = witness_view.get<PositionComponent>(witness);
                if (w_pos.layer_id == pos.layer_id) {
                    const auto& w_vis = witness_view.get<VisibilityComponent>(witness);
                    if (w_vis.visible_tiles.count(thief_pos)) { witnessed = true; break; }
                }
            }
        }

        if (victim_notices || witnessed) {
            event_dispatcher_.enqueue<CrimeReportEvent>({
                event.thief_entity, event.target_entity,
                pos.x, pos.y, pos.layer_id,
                "THEFT", 0
            });
            std::string reporter = victim_notices ? "Victim" : "Witness";
            event_dispatcher_.trigger(HUDNotificationEvent{
                reporter + " reporting theft to authorities.", 2.5f, "#FF8800"
            });
        }
    }
}

} // namespace NeonOubliette::Systems
