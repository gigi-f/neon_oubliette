#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include <iostream>
#include <algorithm>

namespace NeonOubliette {

/**
 * @brief Processes Layer 1 Biological simulation.
 *        Links physical environment (L0) and agent needs to internal vitals.
 */
class BiologySystem : public ISimulationSystem {
public:
    BiologySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
    }

    void update(double delta_time) override {
        m_turn_counter++;
        auto view = m_registry.view<Layer1BiologyComponent, Layer0PhysicsComponent, NeedsComponent>();
        for (auto entity : view) {
            auto& bio = view.get<Layer1BiologyComponent>(entity);
            auto& physics = view.get<Layer0PhysicsComponent>(entity);
            auto& needs = view.get<NeedsComponent>(entity);
            
            // 1. Environmental Effects (L0 -> L1)
            // Hyperthermia / Hypothermia
            if (physics.temperature_celsius > 45.0f) {
                bio.pain_level = std::min(10, bio.pain_level + 1);
                bio.vitals.heart_rate += 5.0f;
                bio.consciousness_level -= 0.05f;
            } else if (physics.temperature_celsius < 5.0f) {
                bio.vitals.core_temperature -= 0.1f;
            } else {
                // Homeostasis
                if (bio.vitals.core_temperature < 37.0f) bio.vitals.core_temperature += 0.05f;
                if (bio.vitals.core_temperature > 37.0f) bio.vitals.core_temperature -= 0.05f;
            }
            
            // 2. Needs Effects (Needs -> L1)
            // Severe thirst drops consciousness
            if (needs.thirst < 10.0f) {
                bio.consciousness_level -= 0.02f;
                bio.vitals.oxygen_saturation -= 0.1f;
            }
            
            // 3. Recovery
            if (bio.consciousness_level < 1.0f && needs.thirst > 20.0f && bio.pain_level < 5) {
                bio.consciousness_level = std::min(1.0f, bio.consciousness_level + 0.01f);
            }
            
            if (bio.pain_level > 0 && (m_turn_counter % 10 == 0)) {
                bio.pain_level--; // Natural pain decay
            }

            // Clamp values
            bio.consciousness_level = std::clamp(bio.consciousness_level, 0.0f, 1.0f);
            bio.vitals.heart_rate = std::clamp(bio.vitals.heart_rate, 0.0f, 220.0f);
            bio.vitals.oxygen_saturation = std::clamp(bio.vitals.oxygen_saturation, 0.0f, 100.0f);
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L1_Biology;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_turn_counter = 0;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
