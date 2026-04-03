#include "agent_decision_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

AgentDecisionSystem::AgentDecisionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<TurnEvent>().connect<&AgentDecisionSystem::handleTurnEvent>(*this);
    m_dispatcher.sink<PathfindingResponseEvent>().connect<&AgentDecisionSystem::handlePathfindingResponseEvent>(*this);
}

void AgentDecisionSystem::handleTurnEvent(const TurnEvent& event) {
    auto agent_view = m_registry.view<AgentComponent, PositionComponent, NeedsComponent, NameComponent>();

    for (auto entity : agent_view) {
        auto& pos = agent_view.get<PositionComponent>(entity);
        auto& needs = agent_view.get<NeedsComponent>(entity);
        auto& name = agent_view.get<NameComponent>(entity);

        // 1. Update needs
        needs.hunger = std::max(0.0f, needs.hunger - 0.5f); // Slower decay
        needs.thirst = std::max(0.0f, needs.thirst - 0.8f);

        // 2. Already has an active task?
        if (m_registry.all_of<AgentTaskComponent>(entity)) {
            auto& task = m_registry.get<AgentTaskComponent>(entity);
            if (task.task_type != AgentTaskType::IDLE) {
                continue; // Action system will handle the current task
            }
        } else {
            m_registry.emplace<AgentTaskComponent>(entity);
        }

        // 3. Needs evaluation and task setting
        InventoryComponent* inventory = m_registry.try_get<InventoryComponent>(entity);
        auto& task = m_registry.get<AgentTaskComponent>(entity);

        // Ensure GoalComponent exists for agents
        if (!m_registry.all_of<GoalComponent>(entity)) {
            m_registry.emplace<GoalComponent>(entity);
        }
        auto& goal = m_registry.get<GoalComponent>(entity);

        evaluateAgentNeedsAndSetTask(entity, pos, needs, inventory, task, goal);
    }
}

void AgentDecisionSystem::evaluateAgentNeedsAndSetTask(entt::entity agent_entity, PositionComponent& agent_pos,
                                                       NeedsComponent& agent_needs, InventoryComponent* agent_inventory,
                                                       AgentTaskComponent& agent_task, GoalComponent& agent_goal) {
    // Priority: Thirst > Hunger
    bool seeking_resource = false;
    entt::entity target_resource = entt::null;
    bool is_container_resource = false;
    entt::entity container_entity = entt::null;

    if (agent_needs.thirst < 50 || agent_needs.hunger < 50) {
        // 1. Check inventory first
        if (agent_inventory) {
            for (auto item_ent : agent_inventory->contained_items) {
                if (m_registry.valid(item_ent) && m_registry.all_of<ConsumableComponent>(item_ent)) {
                    auto& consumable = m_registry.get<ConsumableComponent>(item_ent);
                    bool satisfies = (agent_needs.hunger < 50 && consumable.restores_hunger > 0) ||
                                     (agent_needs.thirst < 50 && consumable.restores_thirst > 0);

                    if (satisfies) {
                        agent_task.task_type = AgentTaskType::CONSUME_ITEM;
                        agent_task.target_entity = item_ent;
                        seeking_resource = true;
                        break;
                    }
                }
            }
        }

        // 2. Search the world if not satisfied
        if (!seeking_resource) {
            auto resource =
                findNearestResource(agent_entity, agent_pos, agent_needs, is_container_resource, container_entity);
            target_resource = resource.first;

            if (target_resource != entt::null) {
                seeking_resource = true;
                PositionComponent target_pos = resource.second;

                // Check if adjacent or on same tile
                int dx = std::abs(agent_pos.x - target_pos.x);
                int dy = std::abs(agent_pos.y - target_pos.y);
                bool is_at_target = (dx == 0 && dy == 0 && agent_pos.layer_id == target_pos.layer_id);
                bool is_adjacent = (dx <= 1 && dy <= 1 && agent_pos.layer_id == target_pos.layer_id);

                if (is_at_target || (is_adjacent && is_container_resource)) {
                    // At target, perform the action
                    if (is_container_resource) {
                        auto& container = m_registry.get<ContainerComponent>(container_entity);
                        if (!container.is_open) {
                            agent_task.task_type = AgentTaskType::OPEN_CONTAINER;
                            agent_task.target_entity = container_entity;
                        } else {
                            agent_task.task_type = AgentTaskType::TAKE_ITEM_FROM_CONTAINER;
                            agent_task.target_entity = container_entity;
                            agent_task.secondary_entity = target_resource; // The item to take
                        }
                    } else {
                        agent_task.task_type = AgentTaskType::PICK_UP_ITEM;
                        agent_task.target_entity = target_resource;
                    }
                } else {
                    // Not adjacent, request path
                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_goal.target_x = target_pos.x;
                    agent_goal.target_y = target_pos.y;
                    agent_goal.target_layer = target_pos.layer_id;

                    m_dispatcher.trigger<PathfindingRequestEvent>(
                        {agent_entity, 
                         {agent_pos.x, agent_pos.y, agent_pos.layer_id}, 
                         {target_pos.x, target_pos.y, target_pos.layer_id}, 
                         m_nextPathRequestId++});
                }
            }
        }
    }

    if (!seeking_resource) {
        // Handle Patrol vs Wander vs Idle
        if (m_registry.all_of<PatrolComponent>(agent_entity)) {
            auto& patrol = m_registry.get<PatrolComponent>(agent_entity);
            if (!patrol.waypoints.empty()) {
                const auto& wp = patrol.waypoints[patrol.current_waypoint_index];
                
                // If at waypoint, next one
                if (agent_pos.x == wp.x && agent_pos.y == wp.y && agent_pos.layer_id == wp.layer_id) {
                    patrol.current_waypoint_index = (patrol.current_waypoint_index + 1) % patrol.waypoints.size();
                }
                
                // Path to current waypoint
                const auto& target_wp = patrol.waypoints[patrol.current_waypoint_index];
                agent_task.task_type = AgentTaskType::PATROL;
                agent_goal.target_x = target_wp.x;
                agent_goal.target_y = target_wp.y;
                agent_goal.target_layer = target_wp.layer_id;

                m_dispatcher.trigger<PathfindingRequestEvent>(
                    {agent_entity, 
                     {agent_pos.x, agent_pos.y, agent_pos.layer_id}, 
                     {target_wp.x, target_wp.y, target_wp.layer_id}, 
                     m_nextPathRequestId++});
            } else {
                agent_task.task_type = AgentTaskType::WANDER;
            }
        } else {
            agent_task.task_type = AgentTaskType::WANDER;
        }
    }
}

std::pair<entt::entity, PositionComponent>
AgentDecisionSystem::findNearestResource(entt::entity agent_entity, const PositionComponent& agent_pos,
                                         NeedsComponent& needs, bool& is_container_resource_out,
                                         entt::entity& container_entity_out) {
    entt::entity best_item = entt::null;
    PositionComponent best_pos;
    double min_dist = 1e9;
    is_container_resource_out = false;
    container_entity_out = entt::null;

    // 1. Check world for ground items
    auto item_view = m_registry.view<ItemComponent, ConsumableComponent, PositionComponent>();
    for (auto item_ent : item_view) {
        auto& item_pos = item_view.get<PositionComponent>(item_ent);
        if (item_pos.layer_id != agent_pos.layer_id)
            continue;

        auto& consumable = item_view.get<ConsumableComponent>(item_ent);
        bool satisfies = (needs.hunger < 50 && consumable.restores_hunger > 0) ||
                         (needs.thirst < 50 && consumable.restores_thirst > 0);

        if (satisfies) {
            double dist = std::sqrt(std::pow(agent_pos.x - item_pos.x, 2) + std::pow(agent_pos.y - item_pos.y, 2));
            if (dist < min_dist) {
                min_dist = dist;
                best_item = item_ent;
                best_pos = item_pos;
            }
        }
    }

    // 2. Check world for containers
    auto container_view = m_registry.view<ContainerComponent, PositionComponent>();
    for (auto cont_ent : container_view) {
        auto& cont_pos = container_view.get<PositionComponent>(cont_ent);
        if (cont_pos.layer_id != agent_pos.layer_id)
            continue;

        auto& container = container_view.get<ContainerComponent>(cont_ent);
        for (auto item_in_cont : container.contained_items) {
            if (m_registry.all_of<ConsumableComponent>(item_in_cont)) {
                auto& consumable = m_registry.get<ConsumableComponent>(item_in_cont);
                bool satisfies = (needs.hunger < 50 && consumable.restores_hunger > 0) ||
                                 (needs.thirst < 50 && consumable.restores_thirst > 0);

                if (satisfies) {
                    double dist =
                        std::sqrt(std::pow(agent_pos.x - cont_pos.x, 2) + std::pow(agent_pos.y - cont_pos.y, 2));
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_item = item_in_cont;
                        best_pos = cont_pos;
                        is_container_resource_out = true;
                        container_entity_out = cont_ent;
                    }
                }
            }
        }
    }

    return {best_item, best_pos};
}

void AgentDecisionSystem::handlePathfindingResponseEvent(const PathfindingResponseEvent& event) {
    if (event.success && m_registry.valid(event.entity)) {
        // Attach CurrentPathComponent to the agent
        if (m_registry.all_of<CurrentPathComponent>(event.entity)) {
            auto& path_comp = m_registry.get<CurrentPathComponent>(event.entity);
            path_comp.path = event.path;
            path_comp.current_step_index = 0;
            path_comp.request_id = event.request_id;
        } else {
            m_registry.emplace<CurrentPathComponent>(event.entity, event.path, (size_t)0, entt::null, event.request_id);
        }

        // Update task to follow path
        if (m_registry.all_of<AgentTaskComponent>(event.entity)) {
            auto& task = m_registry.get<AgentTaskComponent>(event.entity);
            task.task_type = AgentTaskType::MOVE_ALONG_PATH;
        }
    }
}

} // namespace NeonOubliette
