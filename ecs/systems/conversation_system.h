#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CONVERSATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CONVERSATION_SYSTEM_H

#include <entt/entt.hpp>
#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "dialogue_atoms.h"

namespace NeonOubliette {

/**
 * @brief [F.4] Autonomous NPC-to-NPC conversation engine.
 *        Selects nearby agents to engage in systemic dialogue based on shared interests.
 */
class ConversationSystem : public ISimulationSystem {
public:
    ConversationSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    
    SimulationLayer simulation_layer() const override { return SimulationLayer::L2_Cognitive; }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void process_ongoing_conversations();
    void find_new_conversations();
    
    std::string select_shared_topic(entt::entity a, entt::entity b);
    
    const int CONVERSATION_MAX_DIST = 2;
    const int CONVERSATION_MIN_DURATION = 3;
    const int CONVERSATION_MAX_DURATION = 6;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CONVERSATION_SYSTEM_H
