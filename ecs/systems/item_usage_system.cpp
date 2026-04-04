#include "item_usage_system.h"
#include <iostream>
#include <algorithm>
#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

ItemUsageSystem::ItemUsageSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<UseItemEvent>().connect<&ItemUsageSystem::handleUseItemEvent>(*this);
}

void ItemUsageSystem::handleUseItemEvent(const UseItemEvent& event) {
    if (!registry.valid(event.item_in_inventory_entity) || !registry.valid(event.user_entity))
        return;

    // 1. Check for Consumables
    if (registry.all_of<ConsumableComponent>(event.item_in_inventory_entity)) {
        dispatcher.trigger<ConsumeItemEvent>({event.user_entity, event.item_in_inventory_entity});
        return;
    }

    // 2. Check for Usable Tools/Items
    if (registry.all_of<UsableComponent>(event.item_in_inventory_entity)) {
        const auto& usable = registry.get<UsableComponent>(event.item_in_inventory_entity);
        std::string item_name = "item";
        if (registry.all_of<NameComponent>(event.item_in_inventory_entity)) {
            item_name = registry.get<NameComponent>(event.item_in_inventory_entity).name;
        }

        bool used = false;
        std::string feedback = "Used " + item_name;

        // Logic based on effect_id
        if (usable.effect_id == "diagnostic_scanner") {
            if (registry.all_of<PositionComponent>(event.user_entity)) {
                auto& pos = registry.get<PositionComponent>(event.user_entity);
                dispatcher.trigger(InspectEvent{event.user_entity, pos.layer_id, pos.x, pos.y, InspectionMode::SURFACE_SCAN});
                feedback = "Scanner pulse sent.";
                used = true;
            }
        } else if (usable.effect_id == "bio_scanner") {
             if (registry.all_of<PositionComponent>(event.user_entity)) {
                auto& pos = registry.get<PositionComponent>(event.user_entity);
                dispatcher.trigger(InspectEvent{event.user_entity, pos.layer_id, pos.x, pos.y, InspectionMode::BIOLOGICAL_AUDIT});
                feedback = "Biological audit pulse sent.";
                used = true;
            }
        } else if (usable.effect_id == "repair_tool") {
            feedback = "Tool active. No repairable target detected.";
            used = true; 
        } else {
            feedback = "Used " + item_name + " with no effect.";
            used = true;
        }

        if (used) {
            dispatcher.trigger(HUDNotificationEvent{feedback, 2.0f, "#FFFFFF"});
        }
    }
}

} // namespace NeonOubliette