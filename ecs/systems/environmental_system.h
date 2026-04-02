#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Manages the L0-L1 feedback loops: Physical waste and biological impact.
 */
class EnvironmentalSystem : public ISimulationSystem {
public:
    EnvironmentalSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        // 1. Waste Decay and Propagation
        auto building_view = m_registry.view<BuildingComponent, WasteComponent>();
        for (auto entity : building_view) {
            auto& waste = building_view.get<WasteComponent>(entity);
            auto& building = building_view.get<BuildingComponent>(entity);

            // Waste increases with occupancy
            waste.waste_level += building.occupant_count * 0.005f;

            // At high waste levels, structural integrity begins to drop (L0 feedback)
            if (waste.waste_level > 75.0f && m_registry.all_of<ConditionComponent>(entity)) {
                auto& condition = m_registry.get<ConditionComponent>(entity);
                condition.integrity -= 0.001f;
            }
        }

        // 2. Air Quality based on Traffic (Macro Scale)
        auto road_view = m_registry.view<RoadComponent, PollutionLevelComponent>();
        for (auto entity : road_view) {
            auto& road = road_view.get<RoadComponent>(entity);
            auto& pollution = road_view.get<PollutionLevelComponent>(entity);

            pollution.air_pollution += road.traffic_density * 0.01f;
            
            // Natural dispersal
            pollution.air_pollution *= 0.99f;
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ENVIRONMENTAL_SYSTEM_H
