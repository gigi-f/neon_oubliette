#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_SOCIAL_INTERACTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_SOCIAL_INTERACTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

/**
 * [NEW SYSTEM]
 * @brief Handles status-based social interactions, such as yielding and deference.
 */
class SocialInteractionSystem : public ISimulationSystem {
public:
    SocialInteractionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override;
    
    SimulationLayer simulation_layer() const override { return SimulationLayer::L2_Cognitive; }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void handle_social_yielding(entt::entity agent, entt::entity other);
    
    // Config: Status delta required to trigger deference
    const float STATUS_DELTA_THRESHOLD = 0.2f;
    const int YIELD_DURATION_TICKS = 5;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_SOCIAL_INTERACTION_SYSTEM_H
