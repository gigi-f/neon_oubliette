#include "agent_decision_system.h"
#include "../../src/util/profiling.h"
#include "../components/transit_components.h"
#include "../components/infrastructure_components.h"
#include "../components/simulation_layers.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

AgentDecisionSystem::AgentDecisionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<TurnEvent>().connect<&AgentDecisionSystem::handleTurnEvent>(*this);
    m_dispatcher.sink<PathfindingResponseEvent>().connect<&AgentDecisionSystem::handlePathfindingResponseEvent>(*this);
    m_dispatcher.sink<CrisisEffectEvent>().connect<&AgentDecisionSystem::handleCrisisEffect>(*this);
    m_dispatcher.sink<CrisisResolvedEvent>().connect<&AgentDecisionSystem::handleCrisisResolved>(*this);
}

void AgentDecisionSystem::handleTurnEvent(const TurnEvent& event) {
    ZoneScoped;
    auto agent_view = m_registry.view<AgentComponent, PositionComponent, NeedsComponent, NameComponent>();

    std::vector<entt::entity> agents;
    for (auto entity : agent_view) {
        agents.push_back(entity);
    }
    if (agents.empty()) {
        return;
    }

    const size_t total_agents = agents.size();
    const size_t decisions_this_turn = std::min(kMaxAgentDecisionsPerTurn, total_agents);
    const size_t start_index = m_nextAgentDecisionIndex % total_agents;

    // Get current time of day
    TimeOfDay current_time = TimeOfDay::DAY;
    auto weather_view = m_registry.view<WeatherComponent>();
    if (!weather_view.empty()) {
        current_time = weather_view.get<WeatherComponent>(*weather_view.begin()).time_of_day;
    }

    for (size_t i = 0; i < decisions_this_turn; ++i) {
        ZoneScopedN("AgentDecisionBlock");
        auto entity = agents[(start_index + i) % total_agents];
        auto& pos = agent_view.get<PositionComponent>(entity);
        auto& needs = agent_view.get<NeedsComponent>(entity);
        auto& name = agent_view.get<NameComponent>(entity);

        // 1. Update needs
        needs.hunger = std::max(0.0f, needs.hunger - 0.5f); // Slower decay
        needs.thirst = std::max(0.0f, needs.thirst - 0.8f);

        // [NEW] Frustration logic (Nature-based relaxation)
        float frustration_change = 0.3f; // Base increase (getting tired/annoyed by the city)
        
        // [L.2] Economic Crisis impact on frustration
        if (m_active_economic_crisis_severity > 0.0f) {
            frustration_change += m_active_economic_crisis_severity * 0.5f;
        }
        
        // [L.4] Political Unrest impact on frustration
        if (m_active_political_crisis_severity > 0.0f) {
            frustration_change += m_active_political_crisis_severity * 0.7f;
        }

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

        // [NEW] Skip decisions for agents in conversation or procession
        if (m_registry.all_of<ConversationComponent>(entity) || m_registry.all_of<InProcessionComponent>(entity)) {
            continue;
        }

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

        evaluateAgentNeedsAndSetTask(entity, pos, needs, inventory, task, goal, event.turn_number);
    }

    m_nextAgentDecisionIndex = (start_index + decisions_this_turn) % total_agents;
}

void AgentDecisionSystem::evaluateAgentNeedsAndSetTask(entt::entity agent_entity, PositionComponent& agent_pos,
                                                       NeedsComponent& agent_needs, InventoryComponent* agent_inventory,
                                                       AgentTaskComponent& agent_task, GoalComponent& agent_goal,
                                                       uint64_t current_tick) {
    ZoneScoped;
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

    // [P.3] Reputation-Based Reaction to Player
    auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
    if (player_view.begin() != player_view.end()) {
        auto player_ent = *player_view.begin();
        auto& player_pos = player_view.get<PositionComponent>(player_ent);
        
        // Is player in "awareness" range?
        if (player_pos.layer_id == agent_pos.layer_id) {
            int dist = std::abs(player_pos.x - agent_pos.x) + std::abs(player_pos.y - agent_pos.y);
            if (dist < 10) {
                if (auto* pol = m_registry.try_get<Layer4PoliticalComponent>(agent_entity)) {
                    if (auto* rep = m_registry.try_get<ReputationComponent>(player_ent)) {
                        ReputationTier tier = rep->get_tier(pol->primary_faction);
                        
                        // Guard Response to Excommunicated
                        if (tier == ReputationTier::EXCOMMUNICATED && m_registry.all_of<PatrolComponent>(agent_entity)) {
                            agent_task.task_type = AgentTaskType::PURSUE;
                            agent_task.target_entity = player_ent;
                            agent_goal.target_x = player_pos.x;
                            agent_goal.target_y = player_pos.y;
                            agent_goal.target_layer = player_pos.layer_id;
                            // Re-request path if target moved
                            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, player_pos, m_nextPathRequestId++});
                            return;
                        }

                        // Civilian Response to Excommunicated (Flee)
                        if (tier == ReputationTier::EXCOMMUNICATED && m_registry.all_of<CitizenComponent>(agent_entity)) {
                             agent_task.task_type = AgentTaskType::FLEE;
                             agent_task.target_entity = player_ent;
                             // Goal for flee is roughly away from player (simplified)
                             agent_goal.target_x = agent_pos.x + (agent_pos.x - player_pos.x) * 5;
                             agent_goal.target_y = agent_pos.y + (agent_pos.y - player_pos.y) * 5;
                             agent_goal.target_layer = agent_pos.layer_id;
                             m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {agent_goal.target_x, agent_goal.target_y, agent_goal.target_layer}, m_nextPathRequestId++});
                             return;
                        }
                    }
                }
            }
        }
    }
    
    // [L.5] Infrastructure Crisis (Power Outage) - Fear Response
    // If power is out, agents who aren't at home become fearful and seek shelter/home.
    if (!urgent_need && m_active_infrastructure_crisis_severity > 0.4f) {
        auto* home = m_registry.try_get<HomeComponent>(agent_entity);
        if (home && (agent_pos.x != home->x || agent_pos.y != home->y || agent_pos.layer_id != home->layer)) {
            agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
            agent_goal.target_x = home->x;
            agent_goal.target_y = home->y;
            agent_goal.target_layer = home->layer;
            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {home->x, home->y, home->layer}, m_nextPathRequestId++});
            return;
        }
    }
    
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
                    m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {directive.target_x, directive.target_y, directive.target_layer}, m_nextPathRequestId++});
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
            auto* home = m_registry.try_get<HomeComponent>(agent_entity);
            bool has_valid_home = false;
            if (home && home->x != -1) {
                // Check if home is still valid (not condemned)
                if (m_registry.valid(home->building_entity)) {
                    if (auto* health = m_registry.try_get<BuildingHealthComponent>(home->building_entity)) {
                        if (!health->is_condemned) has_valid_home = true;
                    } else {
                        has_valid_home = true;
                    }
                } else {
                    has_valid_home = true; // Position-only home
                }
            }

            if (has_valid_home) {
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
                            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {s_pos.x, s_pos.y, s_pos.layer_id}, m_nextPathRequestId++});

                            // [NEW] Check if should HURRY
                            auto transit_view = m_registry.view<TransitVehicleComponent, PositionComponent>();
                            for (auto vehicle : transit_view) {
                                const auto& v_pos = transit_view.get<PositionComponent>(vehicle);
                                if (v_pos.layer_id == s_pos.layer_id) {
                                    float dist_v_s = (float)std::abs(v_pos.x - s_pos.x) + (float)std::abs(v_pos.y - s_pos.y);
                                    if (dist_v_s < 10) { // Train is coming!
                                        agent_task.task_type = AgentTaskType::HURRY_TO_TRANSIT;
                                        break;
                                    }
                                }
                            }
                            return;
                        }
                    }

                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_goal.target_x = home->x;
                    agent_goal.target_y = home->y;
                    agent_goal.target_layer = home->layer;
                    m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {home->x, home->y, home->layer}, m_nextPathRequestId++});
                    return;
                }
            } else {
                // Homeless or home is condemned. Seek a squat!
                entt::entity squat = findNearestSquattableBuilding(agent_pos);
                if (squat != entt::null) {
                    const auto& s_pos = m_registry.get<PositionComponent>(squat);
                    agent_task.task_type = AgentTaskType::SQUAT;
                    agent_task.target_entity = squat;
                    agent_goal.target_x = s_pos.x;
                    agent_goal.target_y = s_pos.y;
                    agent_goal.target_layer = s_pos.layer_id;
                    m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, s_pos, m_nextPathRequestId++});
                    return;
                }
            }
        } else if (current_routine == RoutineState::WORKING) {
            // [K.2] Check for REPAIR jobs if they need money or are faction workers
            auto repair_job = findNearestRepairJob(agent_entity, agent_pos);
            if (repair_job != entt::null) {
                const auto& r_pos = m_registry.get<PositionComponent>(repair_job);
                agent_task.task_type = AgentTaskType::REPAIR;
                agent_task.target_entity = repair_job;
                agent_goal.target_x = r_pos.x;
                agent_goal.target_y = r_pos.y;
                agent_goal.target_layer = r_pos.layer_id;
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, r_pos, m_nextPathRequestId++});
                return;
            }

            if (auto* work = m_registry.try_get<WorkplaceComponent>(agent_entity)) {
                if (agent_pos.x == work->x && agent_pos.y == work->y && agent_pos.layer_id == work->layer) {
                    if (m_registry.valid(work->building_entity) && m_registry.all_of<FactoryComponent>(work->building_entity)) {
                        agent_task.task_type = AgentTaskType::PRODUCE_GOODS;
                        agent_task.target_entity = work->building_entity;
                        return;
                    }
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
                            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {s_pos.x, s_pos.y, s_pos.layer_id}, m_nextPathRequestId++});

                            // [NEW] Check if should HURRY
                            auto transit_view = m_registry.view<TransitVehicleComponent, PositionComponent>();
                            for (auto vehicle : transit_view) {
                                const auto& v_pos = transit_view.get<PositionComponent>(vehicle);
                                if (v_pos.layer_id == s_pos.layer_id) {
                                    float dist_v_s = (float)std::abs(v_pos.x - s_pos.x) + (float)std::abs(v_pos.y - s_pos.y);
                                    if (dist_v_s < 10) { // Train is coming!
                                        agent_task.task_type = AgentTaskType::HURRY_TO_TRANSIT;
                                        break;
                                    }
                                }
                            }
                            return;
                        }
                    }
                    agent_task.task_type = AgentTaskType::GO_TO_WORK;
                    agent_goal.target_x = work->x;
                    agent_goal.target_y = work->y;
                    agent_goal.target_layer = work->layer;
                    m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {work->x, work->y, work->layer}, m_nextPathRequestId++});
                    return;
                }
            }
        }
    }

    // [O.4] Logistic Mule Logic
    if (!urgent_need && current_routine == RoutineState::WORKING && m_registry.all_of<MuleComponent>(agent_entity)) {
        auto& mule = m_registry.get<MuleComponent>(agent_entity);
        
        if (!mule.is_carrying) {
            // Find a factory with finished goods OR a resource node with materials
            auto factory_view = m_registry.view<FactoryComponent, PositionComponent, ContainerComponent>();
            for (auto f_ent : factory_view) {
                auto& cont = factory_view.get<ContainerComponent>(f_ent);
                if (!cont.contained_items.empty()) {
                    auto& f_pos = factory_view.get<PositionComponent>(f_ent);
                    mule.source_entity = f_ent;
                    mule.destination_entity = entt::null; // To be decided after pickup
                    
                    agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                    agent_task.target_entity = f_ent;
                    agent_goal.target_x = f_pos.x;
                    agent_goal.target_y = f_pos.y;
                    agent_goal.target_layer = f_pos.layer_id;
                    m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, f_pos, m_nextPathRequestId++});
                    return;
                }
            }

            // Fallback: search for resource nodes
            entt::entity node = findNearestResourceNode(agent_pos, RawMaterialType::METAL, 50);
            if (node != entt::null) {
                const auto& n_pos = m_registry.get<PositionComponent>(node);
                agent_task.task_type = AgentTaskType::EXTRACT_RESOURCE;
                agent_task.target_entity = node;
                agent_goal.target_x = n_pos.x;
                agent_goal.target_y = n_pos.y;
                agent_goal.target_layer = n_pos.layer_id;
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, n_pos, m_nextPathRequestId++});
                return;
            }
        } else {
            // Move to destination
            if (m_registry.valid(mule.destination_entity)) {
                const auto& d_pos = m_registry.get<PositionComponent>(mule.destination_entity);
                agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
                agent_task.target_entity = mule.destination_entity;
                agent_goal.target_x = d_pos.x;
                agent_goal.target_y = d_pos.y;
                agent_goal.target_layer = d_pos.layer_id;
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, d_pos, m_nextPathRequestId++});
                return;
            }
        }
    }

    // [O.1] Resource Scavenging/Extraction
    if (!urgent_need && current_routine == RoutineState::WORKING) {
        bool needs_work = true;
        if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(agent_entity)) {
            if (econ->cash_on_hand > 200) needs_work = false;
        }

        if (needs_work) {
            // Find a nearby resource node to extract from
            entt::entity node = findNearestResourceNode(agent_pos, RawMaterialType::METAL, 25);
            if (node == entt::null) node = findNearestResourceNode(agent_pos, RawMaterialType::BIOMASS, 25);
            
            if (node != entt::null) {
                const auto& n_pos = m_registry.get<PositionComponent>(node);
                agent_task.task_type = AgentTaskType::EXTRACT_RESOURCE;
                agent_task.target_entity = node;
                agent_goal.target_x = n_pos.x;
                agent_goal.target_y = n_pos.y;
                agent_goal.target_layer = n_pos.layer_id;
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, n_pos, m_nextPathRequestId++});
                return;
            }
        }
    }

    // [H.2] Religiosity during LEISURE
    if (!urgent_need && current_routine == RoutineState::LEISURE) {
        if (auto* religiosity = m_registry.try_get<ReligiosityComponent>(agent_entity)) {
            // High devotion or holy day?
            bool is_devout = religiosity->devotion > 70.0f;
            
            // Check for holy day
            bool is_holy_day = false;
            auto* registry_ptr = m_registry.ctx().find<ReligionRegistryComponent>();
            if (registry_ptr) {
                auto it = registry_ptr->religions.find(religiosity->religion_id);
                if (it != registry_ptr->religions.end()) {
                    for (auto holy_tick : it->second.holy_day_ticks) {
                         if (current_tick >= holy_tick && current_tick < holy_tick + 200) {
                             is_holy_day = true;
                             break;
                         }
                    }
                }
            }

            // If devout or holy day, and haven't worshipped recently
            if ((is_devout || is_holy_day) && (current_tick - religiosity->last_worship_tick > 500)) {
                entt::entity worship_place = findNearestWorshipPlace(religiosity->religion_id, agent_pos, 40);
                if (worship_place != entt::null) {
                    const auto& w_pos = m_registry.get<PositionComponent>(worship_place);
                    if (agent_pos.x == w_pos.x && agent_pos.y == w_pos.y && agent_pos.layer_id == w_pos.layer_id) {
                        agent_task.task_type = AgentTaskType::WORSHIP;
                        return;
                    } else {
                        agent_task.task_type = AgentTaskType::VISIT_SHRINE;
                        agent_goal.target_x = w_pos.x;
                        agent_goal.target_y = w_pos.y;
                        agent_goal.target_layer = w_pos.layer_id;
                        m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {w_pos.x, w_pos.y, w_pos.layer_id}, m_nextPathRequestId++});
                        return;
                    }
                }
            }

            // [H.3] Worship contagion: join if nearby worshipper of same religion
            if (agent_task.task_type == AgentTaskType::IDLE || agent_task.task_type == AgentTaskType::WANDER) {
                bool found_nearby_worshipper = false;
                auto other_worshippers = m_registry.view<PositionComponent, AgentTaskComponent, ReligiosityComponent>();
                for (auto other_ent : other_worshippers) {
                    if (other_ent == agent_entity) continue;
                    auto& other_task = other_worshippers.get<AgentTaskComponent>(other_ent);
                    if (other_task.task_type == AgentTaskType::WORSHIP) {
                        auto& other_rel = other_worshippers.get<ReligiosityComponent>(other_ent);
                        if (other_rel.religion_id == religiosity->religion_id) {
                            auto& other_pos = other_worshippers.get<PositionComponent>(other_ent);
                            if (other_pos.layer_id == agent_pos.layer_id) {
                                int dist = std::abs(other_pos.x - agent_pos.x) + std::abs(other_pos.y - agent_pos.y);
                                if (dist < 10) { found_nearby_worshipper = true; break; }
                            }
                        }
                    }
                }
                
                if (found_nearby_worshipper) {
                    entt::entity worship_place = findNearestWorshipPlace(religiosity->religion_id, agent_pos, 20);
                    if (worship_place != entt::null) {
                        const auto& w_pos = m_registry.get<PositionComponent>(worship_place);
                        if (agent_pos.x == w_pos.x && agent_pos.y == w_pos.y && agent_pos.layer_id == w_pos.layer_id) {
                            agent_task.task_type = AgentTaskType::WORSHIP;
                            return;
                        } else {
                            agent_task.task_type = AgentTaskType::VISIT_SHRINE;
                            agent_goal.target_x = w_pos.x;
                            agent_goal.target_y = w_pos.y;
                            agent_goal.target_layer = w_pos.layer_id;
                            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, {w_pos.x, w_pos.y, w_pos.layer_id}, m_nextPathRequestId++});
                            return;
                        }
                    }
                }
            }
        }
    }

    // [I.1] Crime Behavior Archetypes during LEISURE
    if (!urgent_need && current_routine == RoutineState::LEISURE && m_registry.all_of<CrimeRiskComponent>(agent_entity)) {
        auto& crime = m_registry.get<CrimeRiskComponent>(agent_entity);
        
        // [I.3] Check for stolen goods in inventory. If any, seek a FENCE.
        bool has_stolen_goods = false;
        if (agent_inventory) {
            for (auto item : agent_inventory->contained_items) {
                if (m_registry.all_of<StolenComponent>(item)) {
                    has_stolen_goods = true;
                    break;
                }
            }
        }

        if (has_stolen_goods) {
             auto fence = findNearestFence(agent_pos);
             if (fence != entt::null) {
                 const auto& f_pos = m_registry.get<PositionComponent>(fence);
                 agent_task.task_type = AgentTaskType::SEEK_FENCE;
                 agent_task.target_entity = fence;
                 agent_goal.target_x = f_pos.x;
                 agent_goal.target_y = f_pos.y;
                 agent_goal.target_layer = f_pos.layer_id;
                 m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, f_pos, m_nextPathRequestId++});
                 return;
             }
        }

        if (crime.is_active_criminal && (current_tick - crime.last_crime_tick > 100)) {
            // [L.2] Desperation during crisis: increase effective boldness
            float effective_boldness = crime.boldness + (m_active_economic_crisis_severity * 30.0f);
            
            // Check for nearby targets
            auto target = findNearestCrimeTarget(agent_entity, agent_pos);
            if (target != entt::null) {
                const auto& t_pos = m_registry.get<PositionComponent>(target);

                // [I.2] Theft & Mugging logic
                if (current_time == TimeOfDay::NIGHT && effective_boldness > 60) {
                    agent_task.task_type = AgentTaskType::MUG_AGENT;
                } else {
                    agent_task.task_type = AgentTaskType::STEAL_FROM_AGENT;
                }

                agent_task.target_entity = target;
                agent_goal.target_x = t_pos.x;
                agent_goal.target_y = t_pos.y;
                agent_goal.target_layer = t_pos.layer_id;
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, t_pos, m_nextPathRequestId++});
                return;
            }
        }
    }

    // Normal need satisfaction (non-urgent) or fallback
    bool seeking_resource = false;
    entt::entity target_resource = entt::null;
    bool is_container_resource = false;
    entt::entity container_entity = entt::null;

    if (agent_needs.thirst < 60 || agent_needs.hunger < 60) {
        // (Existing resource search code...)
    }

    // [NEW] G.5 Social need fulfillment
    if (!seeking_resource && agent_needs.socialization < 40) {
        auto social = findNearestSocialContact(agent_entity, agent_pos);
        if (social.first != entt::null) {
            agent_task.task_type = AgentTaskType::MOVE_TO_TARGET;
            agent_goal.target_x = social.second.x;
            agent_goal.target_y = social.second.y;
            agent_goal.target_layer = social.second.layer_id;
            m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, social.second, m_nextPathRequestId++});
            return;
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
                m_dispatcher.enqueue<PathfindingRequestEvent>({agent_entity, agent_pos, target_wp, m_nextPathRequestId++});
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
            // If it's a generic movement or idle task, transition to MOVE_ALONG_PATH.
            // If it's a specific goal-action task (STEAL, MULE, SELL), KEEP the type
            // so AgentActionSystem knows what to do when the path is completed.
            if (task.task_type == AgentTaskType::IDLE || 
                task.task_type == AgentTaskType::WANDER || 
                task.task_type == AgentTaskType::MOVE_TO_TARGET ||
                task.task_type == AgentTaskType::GO_TO_WORK ||
                task.task_type == AgentTaskType::VISIT_SHRINE ||
                task.task_type == AgentTaskType::SEEK_FENCE) {
                task.task_type = AgentTaskType::MOVE_ALONG_PATH;
            }
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

std::pair<entt::entity, NeonOubliette::PositionComponent>
AgentDecisionSystem::findNearestSocialContact(entt::entity agent_entity, const NeonOubliette::PositionComponent& agent_pos) {
    auto rel_ptr = m_registry.try_get<RelationshipComponent>(agent_entity);
    if (!rel_ptr) return {entt::null, {}};

    auto mapping_view = m_registry.view<MacroIdMappingTag>();
    if (mapping_view.empty()) return {entt::null, {}};
    auto& mapping = m_registry.get<MacroIdMappingTag>(mapping_view.front()).mapping;

    entt::entity best_target = entt::null;
    PositionComponent best_pos;
    float min_dist = 50.0f; // Limit search radius

    for (auto const& [macro_id, record] : rel_ptr->records) {
        if (record.tier == RelationshipTier::FRIEND || record.tier == RelationshipTier::FAMILY) {
            auto it = mapping.find(macro_id);
            if (it != mapping.end() && m_registry.valid(it->second)) {
                entt::entity live_target = it->second;
                if (auto* t_pos = m_registry.try_get<PositionComponent>(live_target)) {
                    if (t_pos->layer_id == agent_pos.layer_id) {
                        float dist = std::abs(t_pos->x - agent_pos.x) + std::abs(t_pos->y - agent_pos.y);
                        if (dist < min_dist) {
                            min_dist = dist;
                            best_target = live_target;
                            best_pos = *t_pos;
                        }
                    }
                }
            }
        }
    }

    return {best_target, best_pos};
}

// Crime reporting handled by GuardResponseSystem [I.6]
// void AgentDecisionSystem::handleCrimeReportEvent(const CrimeReportEvent& event) { ... }

entt::entity AgentDecisionSystem::findNearestWorshipPlace(const std::string& religion_id, const PositionComponent& pos, float max_dist) {
    auto worship_view = m_registry.view<WorshipPlaceComponent, PositionComponent>();
    entt::entity best_place = entt::null;
    float min_dist = max_dist;

    for (auto entity : worship_view) {
        const auto& w_comp = worship_view.get<WorshipPlaceComponent>(entity);
        if (w_comp.religion_id != religion_id) continue;
        
        const auto& w_pos = worship_view.get<PositionComponent>(entity);
        if (w_pos.layer_id != pos.layer_id) continue;
        
        float dist = std::abs(w_pos.x - pos.x) + std::abs(w_pos.y - pos.y);
        if (dist < min_dist) {
            min_dist = dist;
            best_place = entity;
        }
    }
    return best_place;
}

entt::entity AgentDecisionSystem::findNearestFence(const PositionComponent& pos) {
    auto fence_view = m_registry.view<FenceComponent, PositionComponent>();
    entt::entity best_fence = entt::null;
    float min_dist = 50.0f; // Limit search radius

    for (auto entity : fence_view) {
        const auto& f_pos = fence_view.get<PositionComponent>(entity);
        if (f_pos.layer_id != pos.layer_id) continue;
        
        float dist = std::abs(f_pos.x - pos.x) + std::abs(f_pos.y - pos.y);
        if (dist < min_dist) {
            min_dist = dist;
            best_fence = entity;
        }
    }
    return best_fence;
}

entt::entity AgentDecisionSystem::findNearestCrimeTarget(entt::entity agent_entity, const PositionComponent& agent_pos) {
    auto target_view = m_registry.view<PositionComponent, Layer3EconomicComponent>();
    entt::entity best_target = entt::null;
    float min_dist = 10.0f; // Detection radius for a mark

    for (auto entity : target_view) {
        if (entity == agent_entity) continue;
        const auto& t_pos = target_view.get<PositionComponent>(entity);
        if (t_pos.layer_id != agent_pos.layer_id) continue;
        
        // Don't target fellow Syndicate members (honor among thieves)
        if (auto* pol = m_registry.try_get<Layer4PoliticalComponent>(entity)) {
            if (pol->primary_faction == "SYNDICATE") continue;
        }

        const auto& econ = target_view.get<Layer3EconomicComponent>(entity);
        if (econ.cash_on_hand > 50) { // Only target those with credits
            float dist = std::abs(t_pos.x - agent_pos.x) + std::abs(t_pos.y - agent_pos.y);
            if (dist < min_dist) {
                min_dist = dist;
                best_target = entity;
            }
        }
    }
    return best_target;
}

entt::entity AgentDecisionSystem::findNearestSquattableBuilding(const PositionComponent& pos) {
    auto building_view = m_registry.view<BuildingHealthComponent, PositionComponent>();
    entt::entity best_squat = entt::null;
    float min_dist = 60.0f; // Range for seeking a squat

    for (auto entity : building_view) {
        auto& health = building_view.get<BuildingHealthComponent>(entity);
        if (health.integrity < 50.0f && !health.is_condemned) { // Only squat in "safe" but decaying buildings
            const auto& b_pos = building_view.get<PositionComponent>(entity);
            if (b_pos.layer_id != pos.layer_id) continue;

            float dist = std::abs(b_pos.x - pos.x) + std::abs(b_pos.y - pos.y);
            if (dist < min_dist) {
                min_dist = dist;
                best_squat = entity;
            }
        }
    }
    return best_squat;
}

entt::entity AgentDecisionSystem::findNearestRepairJob(entt::entity agent_entity, const PositionComponent& pos) {
    auto job_view = m_registry.view<BuildingHealthComponent, PositionComponent, PropertyComponent>();
    entt::entity best_job = entt::null;
    float min_dist = 40.0f;

    auto* faction_comp = m_registry.try_get<Layer4PoliticalComponent>(agent_entity);

    for (auto entity : job_view) {
        auto& health = job_view.get<BuildingHealthComponent>(entity);
        if (health.maintenance_urgency > 0.3f && health.maintenance_budget > 0) {
            const auto& prop = job_view.get<PropertyComponent>(entity);
            
            // Priority for own faction properties
            bool faction_match = faction_comp && (prop.owner_faction == faction_comp->primary_faction);
            
            const auto& b_pos = job_view.get<PositionComponent>(entity);
            if (b_pos.layer_id != pos.layer_id) continue;

            float dist = std::abs(b_pos.x - pos.x) + std::abs(b_pos.y - pos.y);
            if (faction_match) dist *= 0.5f; // Bias toward own faction

            if (dist < min_dist) {
                min_dist = dist;
                best_job = entity;
            }
        }
    }
    return best_job;
}

entt::entity AgentDecisionSystem::findNearestResourceNode(const PositionComponent& pos, RawMaterialType type, float max_dist) {
    auto node_view = m_registry.view<ResourceNodeComponent, PositionComponent>();
    entt::entity best_node = entt::null;
    float min_dist = max_dist;

    for (auto entity : node_view) {
        const auto& node = node_view.get<ResourceNodeComponent>(entity);
        if (node.is_exhausted || node.material_type != type) continue;
        
        const auto& n_pos = node_view.get<PositionComponent>(entity);
        if (n_pos.layer_id != pos.layer_id) continue;
        
        float dist = (float)std::abs(n_pos.x - pos.x) + (float)std::abs(n_pos.y - pos.y);
        if (dist < min_dist) {
            min_dist = dist;
            best_node = entity;
        }
    }
    return best_node;
}

void AgentDecisionSystem::handleCrisisEffect(const CrisisEffectEvent& event) {
    if (event.type == CrisisType::ECONOMIC_COLLAPSE) {
        m_active_economic_crisis_severity = event.intensity;
    } else if (event.type == CrisisType::POLITICAL_UNREST || event.type == CrisisType::FACTION_WAR) {
        m_active_political_crisis_severity = event.intensity;
    } else if (event.type == CrisisType::INFRASTRUCTURE_FAILURE) {
        m_active_infrastructure_crisis_severity = event.intensity;
    }
}

void AgentDecisionSystem::handleCrisisResolved(const CrisisResolvedEvent& event) {
    if (event.type == CrisisType::ECONOMIC_COLLAPSE) {
        m_active_economic_crisis_severity = 0.0f;
    } else if (event.type == CrisisType::POLITICAL_UNREST || event.type == CrisisType::FACTION_WAR) {
        m_active_political_crisis_severity = 0.0f;
    } else if (event.type == CrisisType::INFRASTRUCTURE_FAILURE) {
        m_active_infrastructure_crisis_severity = 0.0f;
    }
}

} // namespace NeonOubliette
