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

    if (registry_.all_of<PositionComponent>(event.entity)) {
        auto& pos = registry_.get<PositionComponent>(event.entity);

        // [NEW] 0. Check for RidingComponent (Unboarding)
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

        // [NEW] 0.5. Check for Personal Vehicles at current position or adjacent?
        // Let's check current position first.
        auto pv_view = registry_.view<PersonalVehicleComponent, PositionComponent, TransitOccupantsComponent>();
        for (auto pv_entity : pv_view) {
            auto& pv_pos = pv_view.get<PositionComponent>(pv_entity);
            if (pos.x == pv_pos.x && pos.y == pv_pos.y && pos.layer_id == pv_pos.layer_id) {
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

        // [NEW] 0.6. Check for Transit Stations (Boarding)
        auto station_view = registry_.view<TransitStationComponent, PositionComponent>();
        for (auto station : station_view) {
            auto& s_pos = station_view.get<PositionComponent>(station);
            if (pos.x == s_pos.x && pos.y == s_pos.y && pos.layer_id == s_pos.layer_id) {
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

        // 1. Check for Building Entrance Components (Doors)
        auto entrance_view = registry_.view<BuildingEntranceComponent, PositionComponent>();
        for (auto door : entrance_view) {
            auto& d_pos = entrance_view.get<PositionComponent>(door);
            if (pos.x == d_pos.x && pos.y == d_pos.y && pos.layer_id == d_pos.layer_id) {
                auto& entrance = entrance_view.get<BuildingEntranceComponent>(door);
                event_dispatcher_.enqueue<BuildingEntranceEvent>({event.entity, entrance.macro_building_id, d_pos.x, d_pos.y, d_pos.layer_id});
                return;
            }
        }

        // 2. Check for Portals (Exit Doors)
        auto portal_view = registry_.view<PortalComponent, PositionComponent>();
        for (auto portal_ent : portal_view) {
            auto& p_pos = portal_view.get<PositionComponent>(portal_ent);
            if (pos.x == p_pos.x && pos.y == p_pos.y && pos.layer_id == p_pos.layer_id) {
                auto& portal = portal_view.get<PortalComponent>(portal_ent);
                
                // Perform Teleport
                pos.x = portal.target_x;
                pos.y = portal.target_y;
                pos.layer_id = portal.target_layer;
                
                if (registry_.all_of<PlayerCurrentLayerComponent>(event.entity)) {
                    registry_.get<PlayerCurrentLayerComponent>(event.entity).current_z = pos.layer_id;
                }
                
                event_dispatcher_.trigger(HUDNotificationEvent{"Exited to city.", 1.5f, "#FFFFFF"});
                return;
            }
        }

        // 3. Check for items on ground
        auto item_view = registry_.view<ItemComponent, PositionComponent>();
        for (auto item : item_view) {
            auto& i_pos = item_view.get<PositionComponent>(item);
            if (pos.x == i_pos.x && pos.y == i_pos.y && pos.layer_id == i_pos.layer_id) {
                event_dispatcher_.enqueue<PickupItemEvent>({event.entity, item, pos.x, pos.y, pos.layer_id});
                return;
            }
        }

        // 4. Fallback
        event_dispatcher_.trigger(HUDNotificationEvent{"Nothing to interact with here.", 1.0f, "#AAAAAA"});
    }
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
