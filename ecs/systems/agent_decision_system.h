#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H

#include <entt/entt.hpp>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class AgentDecisionSystem : public ISystem {
public:
    AgentDecisionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handleTurnEvent(const TurnEvent& event);
    void handlePathfindingResponseEvent(const PathfindingResponseEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    uint32_t m_nextPathRequestId = 0; // Simple counter for unique request IDs

    std::pair<entt::entity, PositionComponent> findNearestResource(entt::entity agent_entity,
                                                                   const PositionComponent& agent_pos,
                                                                   NeedsComponent& needs, bool& is_container_resource,
                                                                   entt::entity& container_entity);

    entt::entity findNearestTransitStation(const PositionComponent& pos, float max_dist);

    void evaluateAgentNeedsAndSetTask(entt::entity agent_entity, PositionComponent& agent_pos,
                                      NeedsComponent& agent_needs, InventoryComponent* agent_inventory,
                                      AgentTaskComponent& agent_task, GoalComponent& agent_goal);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H
