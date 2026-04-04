#include "consumption_system.h"

#include <iostream>
#include <algorithm>

namespace NeonOubliette {

ConsumptionSystem::ConsumptionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<ConsumeItemEvent>().connect<&ConsumptionSystem::handleConsumeItemEvent>(*this);
}

void ConsumptionSystem::handleConsumeItemEvent(const ConsumeItemEvent& event) {
    if (!registry.valid(event.consumer_entity) || !registry.valid(event.item_to_consume_entity))
        return;

    if (registry.all_of<NeedsComponent>(event.consumer_entity) &&
        registry.all_of<ConsumableComponent>(event.item_to_consume_entity)) {
        auto& needs = registry.get<NeedsComponent>(event.consumer_entity);
        const auto& consumable = registry.get<ConsumableComponent>(event.item_to_consume_entity);

        needs.hunger = std::min(100.0f, needs.hunger + static_cast<float>(consumable.restores_hunger));
        needs.thirst = std::min(100.0f, needs.thirst + static_cast<float>(consumable.restores_thirst));

        // Feedback
        std::string consumer_name = "Someone";
        if (registry.all_of<NameComponent>(event.consumer_entity)) {
            consumer_name = registry.get<NameComponent>(event.consumer_entity).name;
        }

        std::string item_name = "an item";
        if (registry.all_of<NameComponent>(event.item_to_consume_entity)) {
            item_name = registry.get<NameComponent>(event.item_to_consume_entity).name;
        }

        std::string log_msg = consumer_name + " consumed " + item_name + ". Hunger: " + std::to_string(needs.hunger) +
                              ", Thirst: " + std::to_string(needs.thirst);

        dispatcher.trigger(LogEvent{log_msg, LogSeverity::INFO, "ConsumptionSystem"});
        dispatcher.trigger(HUDNotificationEvent{log_msg, 2.0f, "#00FF00"}); // Green for positive feedback

        // Remove from inventory first
        if (registry.all_of<InventoryComponent>(event.consumer_entity)) {
            auto& inv = registry.get<InventoryComponent>(event.consumer_entity);
            inv.contained_items.erase(std::remove(inv.contained_items.begin(), inv.contained_items.end(), event.item_to_consume_entity), inv.contained_items.end());
        }

        // Remove the item after consumption
        registry.destroy(event.item_to_consume_entity);
    }
}

} // namespace NeonOubliette
