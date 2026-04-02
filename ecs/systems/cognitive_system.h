#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_COGNITIVE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_COGNITIVE_SYSTEM_H

#include "simulation_coordinator.h"
#include <iostream>

namespace NeonOubliette {

class CognitiveSystem : public ISimulationSystem {
public:
    CognitiveSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Initialization logic for cognition
    }

    void update(double delta_time) override {
        // Process L2 Cognitive components
        auto view = m_registry.view<Layer2CognitiveComponent, Layer1BiologyComponent>();
        for (auto entity : view) {
            auto& cognitive = view.get<Layer2CognitiveComponent>(entity);
            auto& biology = view.get<Layer1BiologyComponent>(entity);
            
            // Example: L1 -> L2 propagation (Biology -> Cognitive)
            // If the entity is in pain, dominance increases (aggression)
            if (biology.pain_level > 7) {
                cognitive.dominance += 0.1f;
                if (cognitive.dominance > 1.0f) cognitive.dominance = 1.0f;
            }
            
            // Example: Arousal normalization over time (slow L2 cycle)
            if (cognitive.arousal > 0.0f) cognitive.arousal -= 0.01f;
            else if (cognitive.arousal < 0.0f) cognitive.arousal += 0.01f;
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L2_Cognitive;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_COGNITIVE_SYSTEM_H
