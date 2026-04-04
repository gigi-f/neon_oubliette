#include "barter_system.h"
#include <algorithm>
#include <cmath>
#include "../components/simulation_layers.h"

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

            // Target evaluates: How much do I want what they're giving me?
            float offered_utility_to_target = calculateUtilityValue(event.target_entity, event.offered_items);
            
            // Target evaluates: How much do I want what they're taking from me?
            float requested_utility_to_target = calculateUtilityValue(event.target_entity, event.requested_items);

            // Dynamic acceptance factor based on relationship
            float greed_multiplier = 1.1f; // Standard 10% markup
            
            if (m_registry.all_of<Layer2CognitiveComponent>(event.target_entity)) {
                auto& target_cog = m_registry.get<Layer2CognitiveComponent>(event.target_entity);
                
                // Faction influence on greed
                if (m_registry.all_of<Layer4PoliticalComponent>(event.initiator_entity)) {
                    auto& init_pol = m_registry.get<Layer4PoliticalComponent>(event.initiator_entity);
                    if (target_cog.reputation_scores.count(init_pol.primary_faction)) {
                        float rep = target_cog.reputation_scores.at(init_pol.primary_faction);
                        // Lower reputation = higher greed (max 2.0x, min 0.8x for allies)
                        greed_multiplier = std::clamp(1.1f - (rep / 100.0f), 0.8f, 2.0f);
                    }
                }
            }

            if (offered_utility_to_target >= requested_utility_to_target * greed_multiplier) {
                m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, BarterState::ACCEPT});
                m_dispatcher.enqueue(HUDNotificationEvent{target_name + " accepts the trade from " + initiator_name, 2.0f, "#00FF00"});
            } else {
                m_dispatcher.enqueue(BarterEvent{event.initiator_entity, event.target_entity, event.offered_items, event.requested_items, BarterState::REJECT});
                m_dispatcher.enqueue(HUDNotificationEvent{target_name + " rejects: 'Too low utility for me.'", 2.0f, "#FF5555"});
            }
            break;
        }
        case BarterState::ACCEPT: {
            auto& initiator_inv = m_registry.get<InventoryComponent>(event.initiator_entity);
            auto& target_inv = m_registry.get<InventoryComponent>(event.target_entity);

            for (auto item : event.offered_items) {
                initiator_inv.contained_items.erase(std::remove(initiator_inv.contained_items.begin(), initiator_inv.contained_items.end(), item), initiator_inv.contained_items.end());
                target_inv.contained_items.push_back(item);
                if (m_registry.all_of<ContainedByComponent>(item)) {
                    m_registry.get<ContainedByComponent>(item).container = event.target_entity;
                }
            }

            for (auto item : event.requested_items) {
                target_inv.contained_items.erase(std::remove(target_inv.contained_items.begin(), target_inv.contained_items.end(), item), target_inv.contained_items.end());
                initiator_inv.contained_items.push_back(item);
                if (m_registry.all_of<ContainedByComponent>(item)) {
                    m_registry.get<ContainedByComponent>(item).container = event.initiator_entity;
                }
            }

            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade successful!", 2.0f, "#00FF00" });
            
            // Faction standing boost for a successful trade
            if (m_registry.all_of<Layer4PoliticalComponent>(event.initiator_entity) && m_registry.all_of<Layer4PoliticalComponent>(event.target_entity)) {
                auto target_faction = m_registry.get<Layer4PoliticalComponent>(event.target_entity).primary_faction;
                m_dispatcher.enqueue(AgentFactionReputationEvent{event.initiator_entity, target_faction, 2.0f});
            }
            break;
        }
        case BarterState::REJECT: {
            m_dispatcher.enqueue(HUDNotificationEvent{ "Trade rejected.", 2.0f, "#FFA500" });
            if (m_registry.all_of<NeedsComponent>(event.initiator_entity)) {
                m_registry.get<NeedsComponent>(event.initiator_entity).frustration += 5.0f;
            }
            break;
        }
    }
}

float BarterSystem::calculateUtilityValue(entt::entity agent_entity, const std::vector<entt::entity>& items) {
    float total_utility = 0.0f;
    
    bool has_needs = m_registry.all_of<NeedsComponent>(agent_entity);
    float hunger = has_needs ? m_registry.get<NeedsComponent>(agent_entity).hunger : 100.0f;
    float thirst = has_needs ? m_registry.get<NeedsComponent>(agent_entity).thirst : 100.0f;

    for (auto item : items) {
        float base_val = getBaseItemValue(item);
        float item_utility = base_val;

        // Apply need-based multipliers
        if (m_registry.all_of<ConsumableComponent>(item)) {
            auto& cons = m_registry.get<ConsumableComponent>(item);
            
            if (cons.restores_hunger > 0 && hunger < 80.0f) {
                float mult = 1.0f + (80.0f - hunger) / 10.0f;
                item_utility *= mult;
            }
            
            if (cons.restores_thirst > 0 && thirst < 80.0f) {
                float mult = 1.0f + (80.0f - thirst) / 10.0f;
                item_utility *= mult;
            }
        }

        total_utility += item_utility;
    }
    
    return total_utility;
}

float BarterSystem::getBaseItemValue(entt::entity item_entity) {
    if (m_registry.all_of<ItemValueComponent>(item_entity)) {
        return static_cast<float>(m_registry.get<ItemValueComponent>(item_entity).value);
    }
    return 1.0f;
}

} // namespace NeonOubliette
