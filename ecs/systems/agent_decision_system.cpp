#include "agent_decision_system.h"
#include "../components/transit_components.h"
#include "../components/infrastructure_components.h"
#include "../components/simulation_layers.h"

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

    // Get current time of day
    TimeOfDay current_time = TimeOfDay::DAY;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) {
        current_time = weather_view.get<WeatherComponent>(*weather_view.begin()).time_of_day;
    }

    for (auto entity : agent_view) {
        auto& pos = agent_view.get<PositionComponent>(entity);
        auto& needs = agent_view.get<NeedsComponent>(entity);
        auto& name = agent_view.get<NameComponent>(entity);

        // 1. Update needs
        needs.hunger = std::max(0.0f, needs.hunger - 0.5f); // Slower decay
        needs.thirst = std::max(0.0f, needs.thirst - 0.8f);

        // [NEW] Frustration logic (Nature-based relaxation)
        float frustration_change = 0.3f; // Base increase (getting tired/annoyed by the city)
        
        // Is the agent near any nature effects?
        auto nature_view = m_registry.view<PositionComponent, NatureEffectComponent>();
        for (auto nature_ent : nature_view) {
            const auto& n_pos = nature_view.get<PositionComponent>(nature_ent);
            if (n_pos.layer_id == pos.layer_id) {
                int dx = std::abs(n_pos.x - pos.x);
                int dy = std::abs(n_pos.y - pos.y);
                if (dx <= 3 && dy <= 3) {
                    const auto& effect = nature_view.get<NatureEffectComponent>(nature_ent);
                    frustration_change -= effect.frustration_reduction_per_tick;
                }
            }
        }
        needs.frustration = std::clamp(needs.frustration + frustration_change, 0.0f, 100.0f);

        // 2. Already has an active task?
        if (m_registry.all_of<AgentTaskComponent>(entity)) {
            auto& task = m_registry.get<AgentTaskComponent>(entity);
            if (task.task_type != AgentTaskType::IDLE) {
                // If it's night and agent is wandering/patrolling, maybe interrupt?
                if (current_time == TimeOfDay::NIGHT && (task.task_type == AgentTaskType::WANDER || task.task_type == AgentTaskType::PATROL)) {
                    // Force re-evaluation
                } else {
                    continue; // Action system will handle the current task
                }
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
    // Check time of day first
    TimeOfDay current_time = TimeOfDay::DAY;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) {
        current_time = weather_view.get<WeatherComponent>(*weather_view.begin()).time_of_day;
    }

    // [NEW SYSTEM] Routine-based decision making
    RoutineState current_routine = RoutineState::LEISURE;
    if (m_registry.all_of<ScheduleComponent>(agent_entity)) {
        current_routine = m_registry.get<ScheduleComponent>(agent_entity).get_current_state(current_time);
    }

    // Priority: Urgent Survival Needs > Faction Directives > Routine
    bool urgent_need = (agent_needs.hunger < 20 || agent_needs.thirst < 20);
    
    // Check for Faction Directive
    if (!urgent_need && m_registry.all_of<Layer4PoliticalComponent>(agent_entity)) {
        auto& pol = m_registry.get<Layer4PoliticalComponent>(agent_entity);
        
        // Find leader and directive
        auto leader_view = m_registry.view<FactionLeaderComponent, FactionDirectiveComponent, FactionComponent>();
        for (auto leader_ent : leader_view) {
            auto& f_comp = leader_view.get<FactionComponent>(leader_ent);
            if (f_comp.faction_id == pol.primary_faction) {
                auto& directive = leader_view.get<FactionDirectiveComponent>(leader_ent);
                
                if (directive.active_directive == DirectiveType::SYNCHRONICITY) {
                    // Synchronicity overrides routine and non-urgent needs
                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_goal.target_x = directive.target_x;
                    agent_goal.target_y = directive.target_y;
                    agent_goal.target_layer = directive.target_layer;
                    m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, {directive.target_x, directive.target_y, directive.target_layer}, m_nextPathRequestId++});
                    return;
                }
                
                if (directive.active_directive == DirectiveType::UTILITY_BURST) {
                    // Utility burst makes them ignore non-urgent needs longer
                    if (agent_needs.hunger > 10 && agent_needs.thirst > 10) {
                        // For now, just continue whatever they were doing or wander
                        if (agent_task.task_type == AgentTaskType::IDLE || agent_task.task_type == AgentTaskType::WANDER) {
                             agent_task.task_type = AgentTaskType::WANDER;
                             return;
                        }
                    }
                }
            }
        }
    }
    
    if (!urgent_need) {
        if (current_routine == RoutineState::SLEEPING) {
            if (auto* home = m_registry.try_get<HomeComponent>(agent_entity)) {
                if (agent_pos.x == home->x && agent_pos.y == home->y && agent_pos.layer_id == home->layer) {
                    agent_task.task_type = AgentTaskType::IDLE;
                    return;
                } else {
                    // [NEW] Use Transit?
                    int dist_to_home = std::abs(home->x - agent_pos.x) + std::abs(home->y - agent_pos.y);
                    if (dist_to_home > 40) {
                        entt::entity station = findNearestTransitStation(agent_pos, 15);
                        if (station != entt::null) {
                            const auto& s_pos = m_registry.get<PositionComponent>(station);
                            if (agent_pos.x == s_pos.x && agent_pos.y == s_pos.y) {
                                agent_task.task_type = AgentTaskType::WAIT_FOR_TRANSIT;
                                return;
                            }
                            agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                            agent_goal.target_x = s_pos.x;
                            agent_goal.target_y = s_pos.y;
                            agent_goal.target_layer = s_pos.layer_id;
                            m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, {s_pos.x, s_pos.y, s_pos.layer_id}, m_nextPathRequestId++});
                            return;
                        }
                    }

                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_goal.target_x = home->x;
                    agent_goal.target_y = home->y;
                    agent_goal.target_layer = home->layer;
                    m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, {home->x, home->y, home->layer}, m_nextPathRequestId++});
                    return;
                }
            }
        } else if (current_routine == RoutineState::WORKING) {
            if (auto* work = m_registry.try_get<WorkplaceComponent>(agent_entity)) {
                if (agent_pos.x == work->x && agent_pos.y == work->y && agent_pos.layer_id == work->layer) {
                    agent_task.task_type = AgentTaskType::IDLE; // Working (Placeholder for real work tasks)
                    return;
                } else {
                    // [NEW] Use Transit?
                    int dist_to_work = std::abs(work->x - agent_pos.x) + std::abs(work->y - agent_pos.y);
                    if (dist_to_work > 40) {
                        entt::entity station = findNearestTransitStation(agent_pos, 15);
                        if (station != entt::null) {
                            const auto& s_pos = m_registry.get<PositionComponent>(station);
                            if (agent_pos.x == s_pos.x && agent_pos.y == s_pos.y) {
                                agent_task.task_type = AgentTaskType::WAIT_FOR_TRANSIT;
                                return;
                            }
                            agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                            agent_goal.target_x = s_pos.x;
                            agent_goal.target_y = s_pos.y;
                            agent_goal.target_layer = s_pos.layer_id;
                            m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, {s_pos.x, s_pos.y, s_pos.layer_id}, m_nextPathRequestId++});
                            return;
                        }
                    }
                    agent_task.task_type = AgentTaskType::GO_TO_WORK;
                    agent_goal.target_x = work->x;
                    agent_goal.target_y = work->y;
                    agent_goal.target_layer = work->layer;
                    m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, {work->x, work->y, work->layer}, m_nextPathRequestId++});
                    return;
                }
            }
        }
    }

    // Normal need satisfaction (non-urgent) or fallback
    bool seeking_resource = false;
    entt::entity target_resource = entt::null;
    bool is_container_resource = false;
    entt::entity container_entity = entt::null;

    if (agent_needs.thirst < 60 || agent_needs.hunger < 60) {
        // 1. Check inventory first
        if (agent_inventory) {
            for (auto item_ent : agent_inventory->contained_items) {
                if (m_registry.valid(item_ent) && m_registry.all_of<ConsumableComponent>(item_ent)) {
                    auto& consumable = m_registry.get<ConsumableComponent>(item_ent);
                    bool satisfies = (agent_needs.hunger < 60 && consumable.restores_hunger > 0) ||
                                     (agent_needs.thirst < 60 && consumable.restores_thirst > 0);

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
            auto resource = findNearestResource(agent_entity, agent_pos, agent_needs, is_container_resource, container_entity);
            target_resource = resource.first;

            if (target_resource != entt::null) {
                seeking_resource = true;
                PositionComponent target_pos = resource.second;

                int dx = std::abs(agent_pos.x - target_pos.x);
                int dy = std::abs(agent_pos.y - target_pos.y);
                bool is_at_target = (dx == 0 && dy == 0 && agent_pos.layer_id == target_pos.layer_id);
                bool is_adjacent = (dx <= 1 && dy <= 1 && agent_pos.layer_id == target_pos.layer_id);

                if (is_at_target || (is_adjacent && is_container_resource)) {
                    if (is_container_resource) {
                        auto& container = m_registry.get<ContainerComponent>(container_entity);
                        if (!container.is_open) {
                            agent_task.task_type = AgentTaskType::OPEN_CONTAINER;
                            agent_task.target_entity = container_entity;
                        } else {
                            agent_task.task_type = AgentTaskType::TAKE_ITEM_FROM_CONTAINER;
                            agent_task.target_entity = container_entity;
                            agent_task.secondary_entity = target_resource;
                        }
                    } else {
                        agent_task.task_type = AgentTaskType::PICK_UP_ITEM;
                        agent_task.target_entity = target_resource;
                    }
                } else {
                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_goal.target_x = target_pos.x;
                    agent_goal.target_y = target_pos.y;
                    agent_goal.target_layer = target_pos.layer_id;
                    m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, target_pos, m_nextPathRequestId++});
                }
            }
        }
    }

    if (!seeking_resource) {
        if (m_registry.all_of<PatrolComponent>(agent_entity)) {
            auto& patrol = m_registry.get<PatrolComponent>(agent_entity);
            if (!patrol.waypoints.empty()) {
                const auto& wp = patrol.waypoints[patrol.current_waypoint_index];
                if (agent_pos.x == wp.x && agent_pos.y == wp.y && agent_pos.layer_id == wp.layer_id) {
                    patrol.current_waypoint_index = (patrol.current_waypoint_index + 1) % patrol.waypoints.size();
                }
                const auto& target_wp = patrol.waypoints[patrol.current_waypoint_index];
                agent_task.task_type = AgentTaskType::PATROL;
                agent_goal.target_x = target_wp.x;
                agent_goal.target_y = target_wp.y;
                agent_goal.target_layer = target_wp.layer_id;
                m_dispatcher.trigger<PathfindingRequestEvent>({agent_entity, agent_pos, target_wp, m_nextPathRequestId++});
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

    auto item_view = m_registry.view<ItemComponent, ConsumableComponent, PositionComponent>();
    for (auto item_ent : item_view) {
        auto& item_pos = item_view.get<PositionComponent>(item_ent);
        if (item_pos.layer_id != agent_pos.layer_id) continue;

        auto& consumable = item_view.get<ConsumableComponent>(item_ent);
        bool satisfies = (needs.hunger < 60 && consumable.restores_hunger > 0) ||
                         (needs.thirst < 60 && consumable.restores_thirst > 0);

        if (satisfies) {
            double dist = std::sqrt(std::pow(agent_pos.x - item_pos.x, 2) + std::pow(agent_pos.y - item_pos.y, 2));
            if (dist < min_dist) {
                min_dist = dist;
                best_item = item_ent;
                best_pos = item_pos;
            }
        }
    }

    auto container_view = m_registry.view<ContainerComponent, PositionComponent>();
    for (auto cont_ent : container_view) {
        auto& cont_pos = container_view.get<PositionComponent>(cont_ent);
        if (cont_pos.layer_id != agent_pos.layer_id) continue;

        auto& container = container_view.get<ContainerComponent>(cont_ent);
        for (auto item_in_cont : container.contained_items) {
            if (m_registry.all_of<ConsumableComponent>(item_in_cont)) {
                auto& consumable = m_registry.get<ConsumableComponent>(item_in_cont);
                bool satisfies = (needs.hunger < 60 && consumable.restores_hunger > 0) ||
                                 (needs.thirst < 60 && consumable.restores_thirst > 0);

                if (satisfies) {
                    double dist = std::sqrt(std::pow(agent_pos.x - cont_pos.x, 2) + std::pow(agent_pos.y - cont_pos.y, 2));
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
        auto& path_comp = m_registry.get_or_emplace<CurrentPathComponent>(event.entity);
        path_comp.path = event.path;
        path_comp.current_step_index = 0;
        path_comp.request_id = event.request_id;
        
        if (!event.macro_path.empty()) {
            path_comp.macro_path = event.macro_path;
        }

        if (m_registry.all_of<AgentTaskComponent>(event.entity)) {
            auto& task = m_registry.get<AgentTaskComponent>(event.entity);
            task.task_type = AgentTaskType::MOVE_ALONG_PATH;
        }
    }
}

entt::entity AgentDecisionSystem::findNearestTransitStation(const PositionComponent& pos, float max_dist) {
    auto station_view = m_registry.view<TransitStationComponent, PositionComponent>();
    entt::entity best_station = entt::null;
    float min_dist = max_dist;

    for (auto entity : station_view) {
        const auto& s_pos = station_view.get<PositionComponent>(entity);
        if (s_pos.layer_id != pos.layer_id) continue;
        
        float dist = std::abs(s_pos.x - pos.x) + std::abs(s_pos.y - pos.y);
        if (dist < min_dist) {
            min_dist = dist;
            best_station = entity;
        }
    }
    return best_station;
}

} // namespace NeonOubliette
