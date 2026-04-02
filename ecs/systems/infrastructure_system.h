#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Handles the physical decay of infrastructure (L0) with Causal Feedback.
 */
class InfrastructureSystem : public ISimulationSystem {
public:
    InfrastructureSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher), m_gen(m_rd()), m_dist(0.0f, 1.0f) {}

    void initialize() override {}

    void update(double delta_time) override {
        // 1. Calculate Global Modifiers (L4 Political Sentiment -> L0 Infrastructure)
        float maintenance_penalty = 1.0f;
        auto opinion_view = m_registry.view<PublicOpinionComponent>();
        for (auto entity : opinion_view) {
            auto& opinion = opinion_view.get<PublicOpinionComponent>(entity);
            // If average approval for the ruling 'City Infrastructure' faction is low, 
            // the maintenance_penalty increases (Simulating budget cuts/neglect).
            // (Placeholder logic: checking for a hypothetical "Civic Union" faction)
            for (auto const& [faction, approval] : opinion.faction_approval) {
                if (approval < 0.3f) {
                    maintenance_penalty += 0.5f; 
                }
            }
        }

        // 2. Process Infrastructure Decay
        auto view = m_registry.view<ConditionComponent>();
        for (auto entity : view) {
            auto& condition = view.get<ConditionComponent>(entity);
            
            // Base decay rate
            float decay = 0.0001f * maintenance_penalty;
            
            // Environmental acceleration (L0 Physics/Waste)
            if (m_registry.all_of<WasteComponent>(entity)) {
                auto& waste = m_registry.get<WasteComponent>(entity);
                if (waste.waste_level > 20.0f) {
                    decay *= 1.5f; // Corrosive/Bio-decay
                }
            }

            condition.integrity -= decay;

            // Trigger failure events
            if (condition.integrity <= 0.0f) {
                condition.integrity = 0.0f;
                m_dispatcher.enqueue<InfrastructureBreakdownEvent>({entity, "Systemic failure due to neglect", 0});
            } else if (condition.integrity < 0.25f && m_dist(m_gen) < 0.01f) {
                m_dispatcher.enqueue<InfrastructureDegradationEvent>({entity, condition.integrity, decay, 0});
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::random_device m_rd;
    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_dist;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_SYSTEM_H
