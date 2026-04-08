#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/zoning_components.h"
#include "../event_declarations.h"
#include <map>

namespace NeonOubliette {

/**
 * @brief Aggregates individual economic status into global market trends (L3).
 *        Updated for J.4 Demographic Pressure.
 */
class EconomicMarketSystem : public ISimulationSystem {
public:
    EconomicMarketSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {
        m_dispatcher.sink<CrimeReportEvent>().connect<&EconomicMarketSystem::handleCrimeReport>(*this);
    }

    void initialize() override {}

    void update(double delta_time) override {
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

        auto market_view = m_registry.view<MacroMarketComponent, MacroZoneComponent>();
        for (auto market_entity : market_view) {
            auto& market = market_view.get<MacroMarketComponent>(market_entity);
            auto& zone = market_view.get<MacroZoneComponent>(market_entity);
            
            double total_gdp = 0.0;
            int total_citizens = 0;
            int unemployed = 0;

            int min_x = zone.macro_x * macro_cell_size;
            int max_x = (zone.macro_x + 1) * macro_cell_size;
            int min_y = zone.macro_y * macro_cell_size;
            int max_y = (zone.macro_y + 1) * macro_cell_size;

            auto citizens = m_registry.view<CitizenComponent, PositionComponent, Layer3EconomicComponent>();
            for (auto citizen_entity : citizens) {
                const auto& pos = citizens.get<PositionComponent>(citizen_entity);
                if (pos.x < min_x || pos.x >= max_x || pos.y < min_y || pos.y >= max_y) continue;

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
                market.average_wealth = static_cast<float>(total_gdp) / total_citizens;
            } else {
                market.unemployment_rate = 0.0f;
                market.average_wealth = 0.0f;
            }

            // [J.4] Process Crime Rate
            // Decay crime rate over time, then add recent crimes
            market.crime_rate = (market.crime_rate * 0.8f) + (float)m_crimeCounts[market_entity];
            m_crimeCounts[market_entity] = 0; // Reset for next cycle
        }
    }

    void handleCrimeReport(const CrimeReportEvent& event) {
        // Find which macro zone the crime happened in
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

        int macro_x = event.x / macro_cell_size;
        int macro_y = event.y / macro_cell_size;

        auto zone_view = m_registry.view<MacroZoneComponent>();
        for (auto entity : zone_view) {
            const auto& zone = zone_view.get<MacroZoneComponent>(entity);
            if (zone.macro_x == macro_x && zone.macro_y == macro_y) {
                m_crimeCounts[entity]++;
                break;
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L3_Economic;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::map<entt::entity, int> m_crimeCounts;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_MARKET_SYSTEM_H
