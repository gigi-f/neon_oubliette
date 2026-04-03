#include "interaction_system.h"
#include "../components/components.h"
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
