#include "barter_system.h"
#include <algorithm>

namespace NeonOubliette {

BarterSystem::BarterSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<BarterEvent>().connect<&BarterSystem::handleBarterEvent>(*this);
}

void BarterSystem::handleBarterEvent(const BarterEvent& event) {
    switch (event.state) {
        case BarterState::REQUEST: {
            auto& target_name = m_registry.get<NameComponent>(event.target_entity).name;
            auto& initiator_name = m_registry.get<NameComponent>(event.initiator_entity).name;

            int offered_value = calculateTotalValue(event.offered_items);
            int requested_value = calculateTotalValue(event.requested_items);

            float acceptance_multiplier = 1.0f;
            if (m_registry.all_of<FactionComponent>(event.initiator_entity) && m_registry.all_of<FactionComponent>(event.target_entity)) {
                auto& initiator_faction = m_registry.get<FactionComponent>(event.initiator_entity);
                auto& target_faction = m_registry.get<FactionComponent>(event.target_entity);
                if (initiator_faction.faction_id == target_faction.faction_id) {
                    acceptance_multiplier = 0.8f;
                }
            }

            if (offered_value >= requested_value * acceptance_multiplier) {
                m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, BarterState::ACCEPT});
                m_dispatcher.enqueue(HUDNotificationEvent{target_name + " accepts the trade from " + initiator_name, 2.0f, "#00FF00"});
            } else {
                m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, BarterState::REJECT});
                m_dispatcher.enqueue(HUDNotificationEvent{target_name + " rejects the trade from " + initiator_name, 2.0f, "#FF0000"});
            }
            break;
        }
        case BarterState::ACCEPT: {
            auto& initiator_inventory = m_registry.get<InventoryComponent>(event.initiator_entity);
            auto& target_inventory = m_registry.get<InventoryComponent>(event.target_entity);

            for (auto item : event.offered_items) {
                initiator_inventory.contained_items.erase(std::remove(initiator_inventory.contained_items.begin(), initiator_inventory.contained_items.end(), item), initiator_inventory.contained_items.end());
                target_inventory.contained_items.push_back(item);
            }

            for (auto item : event.requested_items) {
                target_inventory.contained_items.erase(std::remove(target_inventory.contained_items.begin(), target_inventory.contained_items.end(), item), target_inventory.contained_items.end());
                initiator_inventory.contained_items.push_back(item);
            }

            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade successful!", 2.0f, "#00FF00" });
            break;
        }
        case BarterState::REJECT: {
            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade rejected.", 2.0f, "#FFA500" });
            if(m_registry.all_of<NeedsComponent>(event.initiator_entity)){
                m_registry.get<NeedsComponent>(event.initiator_entity).frustration += 15.0f;
            }
            break;
        }
    }
}

int BarterSystem::calculateTotalValue(const std::vector<entt::entity>& items) {
    int total_value = 0;
    for (auto item : items) {
        if (m_registry.all_of<ItemValueComponent>(item)) {
            total_value += m_registry.get<ItemValueComponent>(item).value;
        }
    }
    return total_value;
}

} // namespace NeonOubliette
