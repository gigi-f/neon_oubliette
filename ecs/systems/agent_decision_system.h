#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H

#include <entt/entt.hpp>
#include <utility>

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
    void handleCrisisEffect(const CrisisEffectEvent& event);
    void handleCrisisResolved(const CrisisResolvedEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    float m_active_economic_crisis_severity = 0.0f;
    float m_active_political_crisis_severity = 0.0f; // [L.4]
    float m_active_infrastructure_crisis_severity = 0.0f; // [L.5]

    uint32_t m_nextPathRequestId = 0; // Simple counter for unique request IDs
    size_t m_nextAgentDecisionIndex = 0; // Round-robin cursor for per-turn decision budget
    static constexpr size_t kMaxAgentDecisionsPerTurn = 40;

    std::pair<entt::entity, PositionComponent> findNearestResource(entt::entity agent_entity,
                                                                   const PositionComponent& agent_pos,
                                                                   NeedsComponent& needs, bool& is_container_resource,
                                                                   entt::entity& container_entity);

    std::pair<entt::entity, PositionComponent> findNearestSocialContact(entt::entity agent_entity,
                                                                         const PositionComponent& agent_pos);

    entt::entity findNearestTransitStation(const PositionComponent& pos, float max_dist);
    entt::entity findNearestWorshipPlace(const std::string& religion_id, const PositionComponent& pos, float max_dist);
    entt::entity findNearestFence(const PositionComponent& pos);
    entt::entity findNearestCrimeTarget(entt::entity agent_entity, const PositionComponent& agent_pos);
    entt::entity findNearestSquattableBuilding(const PositionComponent& pos); // [K.2]
    entt::entity findNearestRepairJob(entt::entity agent_entity, const PositionComponent& pos); // [K.2]
    entt::entity findNearestResourceNode(const PositionComponent& pos, RawMaterialType type, float max_dist); // [O.1]

    void evaluateAgentNeedsAndSetTask(entt::entity agent_entity, PositionComponent& agent_pos,
                                      NeedsComponent& agent_needs, InventoryComponent* agent_inventory,
                                      AgentTaskComponent& agent_task, GoalComponent& agent_goal,
                                      uint64_t current_tick);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_DECISION_SYSTEM_H
