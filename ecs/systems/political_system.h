#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_SYSTEM_H

#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

class PoliticalSystem : public ISimulationSystem {
public:
    PoliticalSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Initialization logic for politics
    }

    void update(double delta_time) override {
        // Process L4 Political components
        auto view = m_registry.view<Layer4PoliticalComponent, Layer3EconomicComponent>();
        for (auto entity : view) {
            auto& political = view.get<Layer4PoliticalComponent>(entity);
            auto& economic = view.get<Layer3EconomicComponent>(entity);
            
            // Example: L3 -> L4 propagation (Economic -> Political)
            // If the entity has significant cash, hard power increases
            if (economic.cash_on_hand > 500) {
                political.hard_power += 0.05f;
                if (political.hard_power > 1.0f) political.hard_power = 1.0f;
            }
            
            // Example: Political influence normalization
            if (political.soft_power > 0.0f) political.soft_power -= 0.01f;
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L4_Political;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_SYSTEM_H
