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
    m_dispatcher.sink<ResourceExtractedEvent>().connect<&EconomicSystem::handleResourceExtracted>(*this);
    m_dispatcher.sink<CrisisEffectEvent>().connect<&EconomicSystem::handleCrisisEffect>(*this);
    m_dispatcher.sink<CrisisResolvedEvent>().connect<&EconomicSystem::handleCrisisResolved>(*this);
}

void EconomicSystem::initialize() {
    // Initial setup if needed
}

void EconomicSystem::update(double delta_time) {
    (void)delta_time; // Turn-based simulation

    // 1. Calculate Market Conditions (Supply/Demand/Scarcity)
    calculateLocalMarketConditions();

    // 2. Process Agent Wages and Consumption Costs
    processAgentWages();

    // 3. Update Property Values based on Demographic Pressure [J.4]
    updatePropertyValues();

    // 4. Apply crisis effects to macro markets [L.2]
    if (m_active_economic_crisis_severity > 0.0f) {
        auto market_view = m_registry.view<MacroMarketComponent>();
        for (auto entity : market_view) {
            auto& market = market_view.get<MacroMarketComponent>(entity);
            // Increase unemployment
            market.unemployment_rate = std::min(0.5f, market.unemployment_rate + m_active_economic_crisis_severity * 0.01f);
            // Suppress wage index
            market.wage_index = std::max(0.2f, market.wage_index - m_active_economic_crisis_severity * 0.02f);
        }
    } else {
        // Natural recovery if no crisis
        auto market_view = m_registry.view<MacroMarketComponent>();
        for (auto entity : market_view) {
            auto& market = market_view.get<MacroMarketComponent>(entity);
            if (market.wage_index < 1.0f) market.wage_index += 0.005f;
            if (market.unemployment_rate > 0.05f) market.unemployment_rate -= 0.001f;
        }
    }
}

void EconomicSystem::updatePropertyValues() {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    auto prop_view = m_registry.view<PropertyComponent, PositionComponent>();
    auto zone_view = m_registry.view<MacroZoneComponent, MacroMarketComponent>();

    for (auto entity : prop_view) {
        auto& prop = prop_view.get<PropertyComponent>(entity);
        const auto& pos = prop_view.get<PositionComponent>(entity);

        // Find macro zone
        int mx = pos.x / macro_cell_size;
        int my = pos.y / macro_cell_size;

        entt::entity target_zone = entt::null;
        for (auto zone_ent : zone_view) {
            const auto& zone = zone_view.get<MacroZoneComponent>(zone_ent);
            if (zone.macro_x == mx && zone.macro_y == my) {
                target_zone = zone_ent;
                break;
            }
        }

        if (target_zone != entt::null) {
            const auto& market = m_registry.get<MacroMarketComponent>(target_zone);
            const auto& zone = m_registry.get<MacroZoneComponent>(target_zone);

            // Gentrification: Rising wealth increases property value
            float gentrification = (market.average_wealth - 100.0f) * 0.05f;
            
            // Crime/Pressure Decay
            float crime_decay = market.crime_rate * 5.0f;
            float pressure_decay = (zone.pressure > 5.0f) ? (zone.pressure - 5.0f) * 2.0f : 0.0f;

            prop.current_market_value += static_cast<int>(gentrification - crime_decay - pressure_decay);
            
            // Clamp to a reasonable floor/ceiling
            prop.current_market_value = std::max(100, std::min(50000, prop.current_market_value));
        }
    }
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
                
                // Hunger demand
                if (needs.hunger < 80.0f) {
                    market.item_type_demand[1] += (80.0f - needs.hunger) / 10.0f;
                }
                // Thirst demand
                if (needs.thirst < 80.0f) {
                    market.item_type_demand[2] += (80.0f - needs.thirst) / 10.0f;
                }

                // [T.2] Healthcare demand: Low health increases medical demand
                if (auto* npc = m_registry.try_get<NPCComponent>(agent_ent)) {
                    if (npc->health < 70) {
                        // Using item_type_id 103 (Cacogen Gland) as a stand-in for medical reagent demand
                        market.item_type_demand[103] += (70.0f - npc->health) / 5.0f;
                    }
                }

                // [T.2] Environmental demand: Storms increase tool demand (Scrap/Plates)
                auto weather_view = m_registry.view<WeatherComponent>();
                if (!weather_view.empty()) {
                    auto const& weather = m_registry.get<WeatherComponent>(weather_view.front());
                    if (weather.state == WeatherState::ELECTRICAL_STORM || weather.state == WeatherState::ACID_RAIN) {
                        market.item_type_demand[101] += 2.0f; // Scrap
                        market.item_type_demand[200] += 5.0f; // Plates
                    }
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
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    auto agent_view = m_registry.view<Layer3EconomicComponent, Layer2CognitiveComponent, PositionComponent>();
    auto market_view = m_registry.view<MacroMarketComponent, MacroZoneComponent>();

    for (auto agent_ent : agent_view) {
        auto& econ = agent_view.get<Layer3EconomicComponent>(agent_ent);
        auto& cog = agent_view.get<Layer2CognitiveComponent>(agent_ent);
        const auto& pos = agent_view.get<PositionComponent>(agent_ent);
        
        // Find local market for wage index
        float local_wage_index = 1.0f;
        int mx = pos.x / macro_cell_size;
        int my = pos.y / macro_cell_size;

        for (auto market_ent : market_view) {
            const auto& zone = market_view.get<MacroZoneComponent>(market_ent);
            if (zone.macro_x == mx && zone.macro_y == my) {
                local_wage_index = market_view.get<MacroMarketComponent>(market_ent).wage_index;
                break;
            }
        }

        int wage = 0;
        
        // 1. Contract-based wage (highest priority)
        if (auto* contract = m_registry.try_get<EmploymentContractComponent>(agent_ent)) {
            wage = static_cast<int>(contract->wage * local_wage_index);
            // [L.2] Economic Crisis impact on wages
            if (m_active_economic_crisis_severity > 0.0f) {
                wage = static_cast<int>(static_cast<float>(wage) * (1.0f - (m_active_economic_crisis_severity * 0.5f)));
            }
        } else {
            // 2. Passive income based on status (dominance/rep) for self-employed/unemployed
            wage = 5;
            if (cog.dominance > 0.5f) wage += 10;
        }
        
        econ.cash_on_hand += wage;
        
        // Passive decay (lifestyle costs)
        if (econ.cash_on_hand > 0) {
            int base_cost = 2;
            // [L.2] Economic Crisis impact on cost of living (inflation)
            if (m_active_economic_crisis_severity > 0.0f) {
                base_cost += static_cast<int>(m_active_economic_crisis_severity * 5.0f);
            }
            econ.cash_on_hand -= base_cost;
        }
    }
}

void EconomicSystem::handleCrisisEffect(const CrisisEffectEvent& event) {
    if (event.type == CrisisType::ECONOMIC_COLLAPSE) {
        m_active_economic_crisis_severity = event.intensity;
    }
}

void EconomicSystem::handleCrisisResolved(const CrisisResolvedEvent& event) {
    if (event.type == CrisisType::ECONOMIC_COLLAPSE) {
        m_active_economic_crisis_severity = 0.0f;
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

void EconomicSystem::handleResourceExtracted(const ResourceExtractedEvent& event) {
    if (m_registry.all_of<Layer3EconomicComponent>(event.actor)) {
        auto& econ = m_registry.get<Layer3EconomicComponent>(event.actor);
        
        float scarcity = 1.0f;
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (!config_view.empty()) {
            int cell_size = m_registry.get<WorldConfigComponent>(config_view.front()).macro_cell_size;
            int mx = event.x / cell_size;
            int my = event.y / cell_size;
            
            auto market_view = m_registry.view<MacroMarketComponent, MacroZoneComponent>();
            for (auto m_ent : market_view) {
                const auto& zone = market_view.get<MacroZoneComponent>(m_ent);
                if (zone.macro_x == mx && zone.macro_y == my) {
                    auto& market = market_view.get<MacroMarketComponent>(m_ent);
                    if (market.material_scarcity.count(event.material_type)) {
                        scarcity = market.material_scarcity[event.material_type];
                    }
                    break;
                }
            }
        }

        int base_value = 10;
        switch(event.material_type) {
            case RawMaterialType::METAL: base_value = 15; break;
            case RawMaterialType::CHEMICAL: base_value = 20; break;
            case RawMaterialType::ELECTRONIC: base_value = 25; break;
            case RawMaterialType::ENERGY: base_value = 30; break;
            case RawMaterialType::BIOMASS: base_value = 10; break;
        }

        int payment = static_cast<int>(static_cast<float>(base_value) * event.amount * scarcity);
        econ.cash_on_hand += payment;

        m_dispatcher.enqueue<LogEvent>({
            "Agent extracted resources worth " + std::to_string(payment) + " credits.",
            LogSeverity::INFO,
            "EconomicSystem"
        });
    }
}

} // namespace NeonOubliette
