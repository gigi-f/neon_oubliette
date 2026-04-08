#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_RESOURCE_DISTRIBUTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_RESOURCE_DISTRIBUTION_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include <algorithm>

namespace NeonOubliette {

/**
 * @brief Manages regeneration of raw material fields and calculates scarcity (L3).
 */
class ResourceDistributionSystem : public ISimulationSystem {
public:
    ResourceDistributionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {
        m_dispatcher.sink<ResourceExtractedEvent>().connect<&ResourceDistributionSystem::handleResourceExtracted>(*this);
    }

    void initialize() override {}

    void update(double delta_time) override {
        (void)delta_time;
        // Run on L3 ticks (Economic)
        auto view = m_registry.view<RawMaterialFieldComponent, MacroMarketComponent>();
        
        for (auto entity : view) {
            auto& field = view.get<RawMaterialFieldComponent>(entity);
            auto& market = view.get<MacroMarketComponent>(entity);

            // 1. Regeneration
            for (auto const& [type, rate] : field.regen_rates) {
                field.concentrations[type] = std::clamp(field.concentrations[type] + rate, 0.0f, 1.0f);
            }

            // 2. Scarcity Calculation
            // Scarcity = 1.0 / (Concentration + 0.2) -> higher if concentration is low
            market.material_scarcity[RawMaterialType::METAL] = 1.0f / (get_conc(field, RawMaterialType::METAL) + 0.2f);
            market.material_scarcity[RawMaterialType::CHEMICAL] = 1.0f / (get_conc(field, RawMaterialType::CHEMICAL) + 0.2f);
            market.material_scarcity[RawMaterialType::BIOMASS] = 1.0f / (get_conc(field, RawMaterialType::BIOMASS) + 0.2f);
            market.material_scarcity[RawMaterialType::ELECTRONIC] = 1.0f / (get_conc(field, RawMaterialType::ELECTRONIC) + 0.2f);
            market.material_scarcity[RawMaterialType::ENERGY] = 1.0f / (get_conc(field, RawMaterialType::ENERGY) + 0.2f);
        }
    }

    void handleResourceExtracted(const ResourceExtractedEvent& event) {
        // Find macro zone/chunk where extraction happened
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        int cell_size = m_registry.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

        int mx = event.x / cell_size;
        int my = event.y / cell_size;

        auto view = m_registry.view<MacroZoneComponent, RawMaterialFieldComponent>();
        for (auto entity : view) {
            const auto& zone = view.get<MacroZoneComponent>(entity);
            if (zone.macro_x == mx && zone.macro_y == my) {
                auto& field = view.get<RawMaterialFieldComponent>(entity);
                
                // Deplete the field concentration
                float depletion = 0.02f; 
                field.concentrations[event.material_type] = std::max(0.0f, field.concentrations[event.material_type] - depletion);
                
                m_dispatcher.enqueue<LogEvent>({"Resource depleted in zone " + std::to_string(mx) + "," + std::to_string(my), LogSeverity::DEBUG, "ResourceDistributionSystem"});
                break;
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L3_Economic;
    }

private:
    float get_conc(const RawMaterialFieldComponent& field, RawMaterialType type) {
        auto it = field.concentrations.find(type);
        return (it != field.concentrations.end()) ? it->second : 0.05f;
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_RESOURCE_DISTRIBUTION_SYSTEM_H
