#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H

#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

class PhysicsSystem : public ISimulationSystem {
public:
    PhysicsSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Initialization logic for physics
    }

    void update(double delta_time) override {
        // Process L0 Physics components
        auto view = m_registry.view<Layer0PhysicsComponent>();
        for (auto entity : view) {
            auto& physics = view.get<Layer0PhysicsComponent>(entity);
            
            // Example: Simple temperature normalization or heat dissipation
            if (physics.temperature_celsius > 25.0f) {
                physics.temperature_celsius -= 0.1f; // Dissipate heat
            } else if (physics.temperature_celsius < 15.0f) {
                physics.temperature_celsius += 0.1f; // Heat up toward ambient
            }
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

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_PHYSICS_SYSTEM_H
