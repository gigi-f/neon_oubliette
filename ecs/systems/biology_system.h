#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H

#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

class BiologySystem : public ISimulationSystem {
public:
    BiologySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Initialization logic for biology
    }

    void update(double delta_time) override {
        // Process L1 Biology components
        auto view = m_registry.view<Layer1BiologyComponent, Layer0PhysicsComponent>();
        for (auto entity : view) {
            auto& biology = view.get<Layer1BiologyComponent>(entity);
            auto& physics = view.get<Layer0PhysicsComponent>(entity);
            
            // Example: L0 -> L1 propagation (Physics -> Biology)
            // If the environment is too hot, consciousness level drops (fainting)
            if (physics.temperature_celsius > 40.0f) {
                biology.consciousness_level -= 0.05f;
                if (biology.consciousness_level < 0.0f) biology.consciousness_level = 0.0f;
            }
            
            // Example: Metabolism recovery
            if (biology.consciousness_level > 0.0f && biology.consciousness_level < 1.0f) {
                biology.consciousness_level += 0.01f; // Recover consciousness slowly
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L1_Biology;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
