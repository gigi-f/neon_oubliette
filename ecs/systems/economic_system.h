#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H

#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

class EconomicSystem : public ISimulationSystem {
public:
    EconomicSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Initialization logic for economics
    }

    void update(double delta_time) override {
        // Process L3 Economic components
        auto view = m_registry.view<Layer3EconomicComponent, Layer2CognitiveComponent>();
        for (auto entity : view) {
            auto& economic = view.get<Layer3EconomicComponent>(entity);
            auto& cognitive = view.get<Layer2CognitiveComponent>(entity);
            
            // Example: L2 -> L3 propagation (Cognitive -> Economic)
            // If dominance is high (aggression), cash flow increases (risk-taking/looting/high-stakes trade)
            if (cognitive.dominance > 0.8f) {
                economic.cash_on_hand += 5;
            }
            
            // Example: Passive economic decay/interest
            if (economic.cash_on_hand > 100) {
                economic.cash_on_hand -= 1; // "Taxation" or "Maintenance"
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

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H
