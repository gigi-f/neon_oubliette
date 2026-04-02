#include "item_usage_system.h"

#include <iostream>

namespace NeonOubliette {

ItemUsageSystem::ItemUsageSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<UseItemEvent>().connect<&ItemUsageSystem::handleUseItemEvent>(*this);
}

void ItemUsageSystem::handleUseItemEvent(const UseItemEvent& event) {
    if (!registry.valid(event.item_in_inventory_entity))
        return;

    if (registry.all_of<ConsumableComponent>(event.item_in_inventory_entity)) {
        dispatcher.trigger<ConsumeItemEvent>({event.user_entity, event.item_in_inventory_entity});
    }
}

} // namespace NeonOubliette