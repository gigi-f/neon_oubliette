#include "economic_system.h"
#include <algorithm>
#include <cmath>
#include <map>
#include "../components/lod_components.h"

namespace NeonOubliette {

EconomicSystem::EconomicSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<PurchaseEvent>().connect<&EconomicSystem::handlePurchase>(*this);
}

void EconomicSystem::initialize() {
    // Initial setup if needed
}

void EconomicSystem::update(double delta_time) {
    // L3 update (every 10 turns)
    
    // 1. Calculate Market Conditions (Supply/Demand/Scarcity)
    calculateLocalMarketConditions();

    // 2. Process Agent Wages and Consumption Costs
    processAgentWages();
}

void EconomicSystem::calculateLocalMarketConditions() {
    auto chunk_view = m_registry.view<ChunkComponent, MarketDemandComponent>();
    auto agent_view = m_registry.view<PositionComponent, NeedsComponent, Layer3EconomicComponent>();
    auto item_view = m_registry.view<PositionComponent, ItemComponent>();
    auto container_view = m_registry.view<PositionComponent, ContainerComponent>();

    // Reset demand and supply for each chunk
    for (auto chunk_ent : chunk_view) {
        auto& market = chunk_view.get<MarketDemandComponent>(chunk_ent);
        market.item_type_demand.clear();
        market.item_type_scarcity.clear();
        // Prepare supply counts for common items
        std::map<uint32_t, int> supply_counts;

        auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
        int min_x = chunk.chunk_x * 40;
        int max_x = (chunk.chunk_x + 1) * 40;
        int min_y = chunk.chunk_y * 40;
        int max_y = (chunk.chunk_y + 1) * 40;

        // Aggregate Supply (Items on floor)
        for (auto item_ent : item_view) {
            auto& pos = item_view.get<PositionComponent>(item_ent);
            if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                auto& item = item_view.get<ItemComponent>(item_ent);
                supply_counts[item.item_type_id]++;
            }
        }

        // Aggregate Supply (Items in containers)
        for (auto cont_ent : container_view) {
            auto& pos = container_view.get<PositionComponent>(cont_ent);
            if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                auto& container = container_view.get<ContainerComponent>(cont_ent);
                for (auto inner_item : container.contained_items) {
                    if (m_registry.all_of<ItemComponent>(inner_item)) {
                        supply_counts[m_registry.get<ItemComponent>(inner_item).item_type_id]++;
                    }
                }
            }
        }

        // Aggregate Demand from Agents
        for (auto agent_ent : agent_view) {
            auto& pos = agent_view.get<PositionComponent>(agent_ent);
            if (pos.x >= min_x && pos.x < max_x && pos.y >= min_y && pos.y < max_y) {
                auto& needs = agent_view.get<NeedsComponent>(agent_ent);
                
                // Hunger demand (using item_type_id 1 as placeholder for generic food)
                if (needs.hunger < 80.0f) {
                    market.item_type_demand[1] += (80.0f - needs.hunger) / 10.0f;
                }
                // Thirst demand (using item_type_id 2 as placeholder for generic water)
                if (needs.thirst < 80.0f) {
                    market.item_type_demand[2] += (80.0f - needs.thirst) / 10.0f;
                }
            }
        }

        // Calculate Scarcity Multipliers
        for (auto& [type, demand] : market.item_type_demand) {
            int supply = supply_counts[type] > 0 ? supply_counts[type] : 1; // Avoid divide by zero
            // Scarcity = Demand / Supply
            float scarcity = (demand + 1.0f) / (float)supply;
            market.item_type_scarcity[type] = std::clamp(scarcity, 0.5f, 10.0f);
        }
    }
}

void EconomicSystem::processAgentWages() {
    auto agent_view = m_registry.view<Layer3EconomicComponent, Layer2CognitiveComponent>();
    
    for (auto agent_ent : agent_view) {
        auto& econ = agent_view.get<Layer3EconomicComponent>(agent_ent);
        auto& cog = agent_view.get<Layer2CognitiveComponent>(agent_ent);
        
        // Passive income based on status (dominance/rep)
        int wage = 5;
        if (cog.dominance > 0.5f) wage += 10;
        
        econ.cash_on_hand += wage;
        
        // Passive decay (lifestyle costs)
        if (econ.cash_on_hand > 0) {
            econ.cash_on_hand -= 2; // Fixed cost of living
        }
    }
}

void EconomicSystem::handlePurchase(const PurchaseEvent& event) {
    if (m_registry.all_of<Layer3EconomicComponent>(event.buyer_npc_id)) {
        auto& econ = m_registry.get<Layer3EconomicComponent>(event.buyer_npc_id);
        if (econ.cash_on_hand >= (int)event.price_paid) {
            econ.cash_on_hand -= (int)event.price_paid;
            
            m_dispatcher.enqueue<LogEvent>({
                "NPC " + std::to_string((uint32_t)event.buyer_npc_id) + " purchased " + event.item_name,
                LogSeverity::INFO,
                "EconomicSystem"
            });
        }
    }
}

} // namespace NeonOubliette
