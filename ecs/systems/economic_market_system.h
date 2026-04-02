#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

/**
 * @brief Aggregates individual economic status into global market trends (L3).
 */
class EconomicMarketSystem : public ISimulationSystem {
public:
    EconomicMarketSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        auto market_view = m_registry.view<MacroMarketComponent>();
        for (auto market_entity : market_view) {
            auto& market = market_view.get<MacroMarketComponent>(market_entity);
            
            double total_gdp = 0.0;
            int total_citizens = 0;
            int unemployed = 0;

            auto citizens = m_registry.view<CitizenComponent, Layer3EconomicComponent>();
            for (auto citizen_entity : citizens) {
                auto& econ = citizens.get<Layer3EconomicComponent>(citizen_entity);
                
                total_gdp += econ.cash_on_hand;
                total_citizens++;

                if (!m_registry.all_of<EmploymentContractComponent>(citizen_entity)) {
                    unemployed++;
                }

                // Automatic Taxation logic (L3 feedback)
                if (m_registry.all_of<TaxationComponent>(market_entity)) {
                    auto& tax = m_registry.get<TaxationComponent>(market_entity);
                    float income_tax = econ.cash_on_hand * tax.income_tax_rate;
                    econ.cash_on_hand -= static_cast<int>(income_tax);
                    market.tax_revenue += static_cast<uint64_t>(income_tax);
                }
            }

            market.GDP = total_gdp;
            if (total_citizens > 0) {
                market.unemployment_rate = static_cast<float>(unemployed) / total_citizens;
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L3_Economic;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H
