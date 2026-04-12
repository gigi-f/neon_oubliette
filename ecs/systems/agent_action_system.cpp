#include "agent_action_system.h"

#include <cmath>
#include <random>
#include <cstdlib>

#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

AgentActionSystem::AgentActionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<TurnEvent>().connect<&AgentActionSystem::handleTurnEvent>(*this);
}

void AgentActionSystem::handleTurnEvent(const TurnEvent& event) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distrib(-1, 1);

    // Collect entities FIRST to avoid iterator invalidation from trigger()/remove() inside the loop
    std::vector<entt::entity> entities_to_process;
    auto agent_view = m_registry.view<AgentComponent, PositionComponent, NameComponent, AgentTaskComponent>();
    for (auto entity : agent_view) {
        entities_to_process.push_back(entity);
    }

    // Cache dialogue target so agent won't move while player is talking to them
    entt::entity dlg_target = entt::null;
    auto dlg_view = m_registry.view<DialogueStateComponent>();
    if (!dlg_view.empty()) {
        const auto& dlg = dlg_view.get<DialogueStateComponent>(dlg_view.front());
        if (dlg.is_open) dlg_target = dlg.target_agent;
    }

    for (auto entity : entities_to_process) {
        // Guard: entity or required components may have been destroyed by a previous iteration's trigger()
        if (!m_registry.valid(entity) ||
            !m_registry.all_of<PositionComponent, AgentTaskComponent>(entity)) {
            continue;
        }

        // Don't issue move actions to the entity the player is currently speaking with
        if (entity == dlg_target) continue;

        // Don't issue move actions to agents who are currently speaking
        if (m_registry.all_of<SpeechComponent>(entity)) continue;

        // [NEW] Don't issue move actions to agents in conversation or procession (except leader)
        if (m_registry.all_of<ConversationComponent>(entity)) continue;
        
        // If in procession and not leader, we might follow leader via AgentTaskType::FOLLOW_LEADER
        // If leader, we follow Patrol.

        auto& position = m_registry.get<PositionComponent>(entity);
        auto& task = m_registry.get<AgentTaskComponent>(entity);

        switch (task.task_type) {
            case AgentTaskType::SEEK_FOOD:
            case AgentTaskType::SEEK_WATER: 
            case AgentTaskType::PATROL:
            case AgentTaskType::GO_TO_WORK:
            case AgentTaskType::VISIT_SHRINE:
            case AgentTaskType::SEEK_FENCE:
            case AgentTaskType::REPAIR: // [K.2]
            case AgentTaskType::SQUAT:  // [K.2]
            case AgentTaskType::MOVE_TO_TARGET:
            case AgentTaskType::HURRY_TO_TRANSIT:
            case AgentTaskType::FOLLOW_LEADER: {
                if (task.task_type == AgentTaskType::FOLLOW_LEADER && m_registry.all_of<FollowComponent>(entity)) {
                    auto& follow = m_registry.get<FollowComponent>(entity);
                    if (m_registry.valid(follow.target)) {
                        auto& t_pos = m_registry.get<PositionComponent>(follow.target);
                        if (t_pos.layer_id == position.layer_id) {
                            int dist = std::abs(t_pos.x - position.x) + std::abs(t_pos.y - position.y);
                            if (dist > follow.target_distance) {
                                int dx = (t_pos.x > position.x) ? 1 : (t_pos.x < position.x ? -1 : 0);
                                int dy = (t_pos.y > position.y) ? 1 : (t_pos.y < position.y ? -1 : 0);
                                m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                            }
                        }
                    }
                    break;
                }

                if (m_registry.all_of<GoalComponent>(entity)) {
                    auto& goal = m_registry.get<GoalComponent>(entity);
                    
                    // This is simple direct movement if not using Pathfinding yet
                    // Usually AgentDecisionSystem triggers pathfinding first.
                    int dx = 0;
                    int dy = 0;

                    if (position.x < goal.target_x)
                        dx = 1;
                    else if (position.x > goal.target_x)
                        dx = -1;

                    if (position.y < goal.target_y)
                        dy = 1;
                    else if (position.y > goal.target_y)
                        dy = -1;

                    if (position.x == goal.target_x && position.y == goal.target_y &&
                        position.layer_id == goal.target_layer) {
                        m_registry.remove<AgentTaskComponent>(entity);
                    } else {
                        m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                        // [NEW] Double move for hurry
                        if (task.task_type == AgentTaskType::HURRY_TO_TRANSIT) {
                            m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                        }
                    }
                } else {
                    m_registry.remove<AgentTaskComponent>(entity);
                }
                break;
            }

            case AgentTaskType::EXTRACT_RESOURCE: {
                if (!m_registry.valid(task.target_entity)) {
                    m_registry.remove<AgentTaskComponent>(entity);
                    break;
                }

                auto& node_pos = m_registry.get<PositionComponent>(task.target_entity);
                if (node_pos.x == position.x && node_pos.y == position.y && node_pos.layer_id == position.layer_id) {
                    auto& progress = m_registry.get_or_emplace<ExtractionProgressComponent>(entity);
                    progress.target_node = task.target_entity;
                    progress.progress += progress.work_rate;

                    if (progress.progress >= 1.0f) {
                        auto& node = m_registry.get<ResourceNodeComponent>(task.target_entity);
                        
                        // Emit event
                        m_dispatcher.trigger<ResourceExtractedEvent>({entity, task.target_entity, node.material_type, node.current_yield, position.x, position.y, position.layer_id});
                        
                        // Deplete node
                        node.current_yield -= node.depletion_rate;
                        if (node.current_yield <= 0.1f) node.is_exhausted = true;

                        m_dispatcher.enqueue<SpeechEvent>({entity, "Material secured.", 10, AudibilityLevel::MUFFLED});
                        
                        m_registry.remove<ExtractionProgressComponent>(entity);
                        m_registry.remove<AgentTaskComponent>(entity);
                    } else if (rand() % 15 == 0) {
                        m_dispatcher.enqueue<SpeechEvent>({entity, "Extracting...", 5, AudibilityLevel::MUFFLED});
                    }
                } else {
                    m_registry.emplace_or_replace<GoalComponent>(entity, node_pos.x, node_pos.y, node_pos.layer_id);
                }
                break;
            }

            case AgentTaskType::PRODUCE_GOODS: {
                if (!m_registry.valid(task.target_entity) || !m_registry.all_of<FactoryComponent>(task.target_entity)) {
                    m_registry.remove<AgentTaskComponent>(entity);
                    break;
                }

                auto& f_pos = m_registry.get<PositionComponent>(task.target_entity);
                if (f_pos.x == position.x && f_pos.y == position.y && f_pos.layer_id == position.layer_id) {
                    
                    // [O.3] Labor Strike Check: Agents may refuse to work
                    auto* disruption = m_registry.try_get<SupplyChainDisruptionComponent>(task.target_entity);
                    if (disruption && disruption->labor_strike > 0.3f) {
                        float roll = (float)(rand() % 100) / 100.0f;
                        if (roll < disruption->labor_strike) {
                            m_dispatcher.enqueue<SpeechEvent>({entity, "Strike! Fair wages now!", 15, AudibilityLevel::CLEAR});
                            task.task_type = AgentTaskType::WANDER;
                            break;
                        }
                    }

                    auto& work = m_registry.get_or_emplace<WorkOrderComponent>(entity);
                    work.factory_entity = task.target_entity;
                    work.contribution_per_tick = 1.0f; // Could scale with skills

                    if (rand() % 20 == 0) {
                        m_dispatcher.enqueue<SpeechEvent>({entity, "Machine cycle active.", 8, AudibilityLevel::MUFFLED});
                    }
                } else {
                    m_registry.emplace_or_replace<GoalComponent>(entity, f_pos.x, f_pos.y, f_pos.layer_id);
                }
                break;
            }

            case AgentTaskType::PICK_UP_ITEM: {
                if (m_registry.valid(task.target_entity)) {
                    auto& item_pos = m_registry.get<PositionComponent>(task.target_entity);
                    if (item_pos.x == position.x && item_pos.y == position.y && item_pos.layer_id == position.layer_id) {
                        m_dispatcher.trigger<PickupItemEvent>({entity, task.target_entity});
                    }
                }
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }

            case AgentTaskType::CONSUME_ITEM: {
                if (m_registry.valid(task.target_entity)) {
                    m_dispatcher.trigger<ConsumeItemEvent>({entity, task.target_entity});
                }
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }

            case AgentTaskType::MOVE_ALONG_PATH:
            case AgentTaskType::PURSUE:
            case AgentTaskType::ARREST:
            case AgentTaskType::STEAL_FROM_AGENT:
            case AgentTaskType::MUG_AGENT:
            case AgentTaskType::MULE_GOODS:
            case AgentTaskType::SELL_CONTRABAND: {
                if (m_registry.all_of<CurrentPathComponent>(entity)) {
                    auto& path_comp = m_registry.get<CurrentPathComponent>(entity);
                    
                    // [M.2] Skip steps already reached (e.g. after portal teleport)
                    while (path_comp.current_step_index < path_comp.path.size()) {
                        const auto& next_pos = path_comp.path[path_comp.current_step_index];
                        if (next_pos.x == position.x && next_pos.y == position.y && next_pos.layer_id == position.layer_id) {
                            path_comp.current_step_index++;
                        } else {
                            break;
                        }
                    }

                    if (path_comp.current_step_index < path_comp.path.size()) {
                        auto next_pos = path_comp.path[path_comp.current_step_index];
                        int dx = next_pos.x - position.x;
                        int dy = next_pos.y - position.y;

                        // [M.2] Allow layer changes in MoveEvent
                        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
                            m_dispatcher.trigger<MoveEvent>({entity, dx, dy, next_pos.layer_id});
                            path_comp.current_step_index++;
                        } else {
                            // Path broken (e.g. obstacle spawned)
                            m_registry.remove<CurrentPathComponent>(entity);
                            m_registry.remove<AgentTaskComponent>(entity);
                        }
                    } else {
                        // Current local path finished. Check macro path.
                        if (!path_comp.macro_path.empty()) {
                            PositionComponent next_macro_goal = path_comp.macro_path.front();
                            // Request path to the next node in the macro sequence
                            m_dispatcher.enqueue<PathfindingRequestEvent>({entity, position, next_macro_goal, path_comp.request_id});
                            path_comp.path.clear();
                            path_comp.current_step_index = 0;
                            
                            // Don't change task_type if it's already a crime task
                            if (task.task_type == AgentTaskType::MOVE_ALONG_PATH) {
                                task.task_type = AgentTaskType::AWAITING_PATH;
                            }
                        } else {
                            // [I.1] Execution of Crime Actions on path completion
                            if (task.task_type == AgentTaskType::STEAL_FROM_AGENT && m_registry.valid(task.target_entity)) {
                                auto& self_econ = m_registry.get<Layer3EconomicComponent>(entity);
                                auto& target_econ = m_registry.get<Layer3EconomicComponent>(task.target_entity);
                                auto& crime = m_registry.get<CrimeRiskComponent>(entity);
                                
                                int amount = std::min(target_econ.cash_on_hand, 20 + (int)(crime.boldness / 2.0f));
                                target_econ.cash_on_hand -= amount;
                                self_econ.cash_on_hand += amount;
                                crime.last_crime_tick = event.turn_number;
                                
                                // Risk of catch
                                if (rand() % 100 > crime.boldness) {
                                    m_dispatcher.trigger<CrimeReportEvent>({entity, task.target_entity, position.x, position.y, position.layer_id});
                                }
                                m_dispatcher.enqueue<SpeechEvent>({entity, "Easy credits.", 10, AudibilityLevel::MUFFLED});
                            }

                            if (task.task_type == AgentTaskType::MUG_AGENT && m_registry.valid(task.target_entity)) {
                                m_dispatcher.enqueue<SpeechEvent>({entity, "Credits now, or it's a slow night for you.", 15, AudibilityLevel::CLEAR});
                                
                                auto& victim_task = m_registry.get_or_emplace<AgentTaskComponent>(task.target_entity);
                                victim_task.task_type = AgentTaskType::REACT_TO_MUGGING;
                                victim_task.target_entity = entity; // Who is mugging them
                                
                                task.task_type = AgentTaskType::AWAIT_MUGGING_COMPLIANCE;
                            }

                            if (task.task_type == AgentTaskType::SEEK_FENCE && m_registry.valid(task.target_entity)) {
                                std::vector<entt::entity> stolen_items;
                                int total_request = 0;
                                auto* inv = m_registry.try_get<InventoryComponent>(entity);
                                if (inv) {
                                    for (auto item : inv->contained_items) {
                                        if (m_registry.all_of<StolenComponent>(item)) {
                                            stolen_items.push_back(item);
                                            int val = 10;
                                            if (auto* v = m_registry.try_get<ItemValueComponent>(item)) val = v->value;
                                            total_request += val / 2;
                                        }
                                    }
                                }
                                
                                if (!stolen_items.empty()) {
                                    m_dispatcher.trigger(BarterEvent{entity, task.target_entity, stolen_items, {}, {}, {}, 0, total_request, BarterState::REQUEST});
                                    m_dispatcher.enqueue<SpeechEvent>({entity, "I've got the goods.", 10, AudibilityLevel::CLEAR});
                                }
                            }
                            
                            // [K.2] Maintenance Repair
                            if (task.task_type == AgentTaskType::REPAIR && m_registry.valid(task.target_entity)) {
                                auto& health = m_registry.get<BuildingHealthComponent>(task.target_entity);
                                if (health.integrity < health.max_integrity && health.maintenance_budget > 0) {
                                    float repair_amount = 2.0f; // More integrity per turn
                                    health.integrity = std::min(health.max_integrity, health.integrity + repair_amount);
                                    health.maintenance_budget -= 1.0f; // Consume budget
                                    
                                    auto* econ = m_registry.try_get<Layer3EconomicComponent>(entity);
                                    if (econ) econ->cash_on_hand += 1; // Get paid
                                    
                                    m_dispatcher.enqueue<SpeechEvent>({entity, "Reinforcing the structure.", 8, AudibilityLevel::CLEAR});
                                }
                                if (health.integrity >= health.max_integrity || health.maintenance_budget <= 0) {
                                    m_registry.remove<AgentTaskComponent>(entity);
                                }
                                return; // Stay at the building while repairing
                            }

                            // [K.2] Squatting
                            if (task.task_type == AgentTaskType::SQUAT && m_registry.valid(task.target_entity)) {
                                auto& home = m_registry.get_or_emplace<HomeComponent>(entity);
                                const auto& b_pos = m_registry.get<PositionComponent>(task.target_entity);
                                home.x = b_pos.x;
                                home.y = b_pos.y;
                                home.layer = b_pos.layer_id;
                                home.building_entity = task.target_entity;
                                home.is_squatting = true;
                                
                                m_dispatcher.enqueue<SpeechEvent>({entity, "This place will do.", 12, AudibilityLevel::CLEAR});
                                // Trigger entry
                                m_dispatcher.trigger<BuildingEntranceEvent>({entity, task.target_entity, entt::null, b_pos.x, b_pos.y, b_pos.layer_id});
                                m_registry.remove<AgentTaskComponent>(entity);
                                return;
                            }

                            // [O.4] Logistic Mule Logic Arrival
                            if (task.task_type == AgentTaskType::MULE_GOODS && m_registry.valid(task.target_entity)) {
                                if (m_registry.all_of<MuleComponent>(entity)) {
                                    auto& mule = m_registry.get<MuleComponent>(entity);
                                    if (!mule.is_carrying) {
                                        if (m_registry.all_of<ContainerComponent>(task.target_entity)) {
                                            auto& cont = m_registry.get<ContainerComponent>(task.target_entity);
                                            if (!cont.contained_items.empty()) {
                                                mule.carried_item = cont.contained_items.back();
                                                cont.contained_items.pop_back();
                                                mule.is_carrying = true;
                                                auto market_view = m_registry.view<PositionComponent, ShopComponent>();
                                                if (market_view.begin() != market_view.end()) {
                                                    mule.destination_entity = *market_view.begin();
                                                    m_dispatcher.enqueue<SpeechEvent>({entity, "Load secured. Moving to market.", 10, AudibilityLevel::CLEAR});
                                                }
                                            }
                                        }
                                    } else {
                                        if (m_registry.all_of<ContainerComponent>(task.target_entity)) {
                                            auto& cont = m_registry.get<ContainerComponent>(task.target_entity);
                                            cont.contained_items.push_back(mule.carried_item);
                                            mule.carried_item = entt::null;
                                            mule.is_carrying = false;
                                            m_dispatcher.enqueue<SpeechEvent>({entity, "Delivery complete.", 10, AudibilityLevel::CLEAR});
                                            if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(entity)) econ->cash_on_hand += 15;
                                        }
                                    }
                                }
                            }

                            m_registry.remove<CurrentPathComponent>(entity);
                            m_registry.remove<AgentTaskComponent>(entity);
                        }
                    }
                } else {
                    // Fallback if already adjacent or no path
                    if (task.task_type == AgentTaskType::STEAL_FROM_AGENT && m_registry.valid(task.target_entity)) {
                        auto& t_pos = m_registry.get<PositionComponent>(task.target_entity);
                        if (std::abs(t_pos.x - position.x) <= 1 && std::abs(t_pos.y - position.y) <= 1 && t_pos.layer_id == position.layer_id) {
                            auto& self_econ = m_registry.get<Layer3EconomicComponent>(entity);
                            auto& target_econ = m_registry.get<Layer3EconomicComponent>(task.target_entity);
                            auto& crime = m_registry.get<CrimeRiskComponent>(entity);
                            
                            int amount = std::min(target_econ.cash_on_hand, 20 + (int)(crime.boldness / 2.0f));
                            target_econ.cash_on_hand -= amount;
                            self_econ.cash_on_hand += amount;
                            crime.last_crime_tick = event.turn_number;
                            
                            if (rand() % 100 > crime.boldness) {
                                m_dispatcher.trigger<CrimeReportEvent>({entity, task.target_entity, position.x, position.y, position.layer_id});
                            }
                            m_dispatcher.enqueue<SpeechEvent>({entity, "Credits acquired.", 10, AudibilityLevel::MUFFLED});
                        }
                    }
                    if (task.task_type == AgentTaskType::MUG_AGENT && m_registry.valid(task.target_entity)) {
                        auto& t_pos = m_registry.get<PositionComponent>(task.target_entity);
                        if (std::abs(t_pos.x - position.x) <= 1 && std::abs(t_pos.y - position.y) <= 1 && t_pos.layer_id == position.layer_id) {
                            m_dispatcher.enqueue<SpeechEvent>({entity, "Empty your accounts. Now.", 15, AudibilityLevel::CLEAR});
                            auto& victim_task = m_registry.get_or_emplace<AgentTaskComponent>(task.target_entity);
                            victim_task.task_type = AgentTaskType::REACT_TO_MUGGING;
                            victim_task.target_entity = entity;
                            task.task_type = AgentTaskType::AWAIT_MUGGING_COMPLIANCE;
                        }
                    }
                    
                    // [O.4] Mule fallback arrival
                    if (task.task_type == AgentTaskType::MULE_GOODS && m_registry.valid(task.target_entity)) {
                        auto& t_pos = m_registry.get<PositionComponent>(task.target_entity);
                        if (std::abs(t_pos.x - position.x) <= 1 && std::abs(t_pos.y - position.y) <= 1 && t_pos.layer_id == position.layer_id) {
                            if (m_registry.all_of<MuleComponent>(entity)) {
                                auto& mule = m_registry.get<MuleComponent>(entity);
                                if (!mule.is_carrying) {
                                    if (m_registry.all_of<ContainerComponent>(task.target_entity)) {
                                        auto& cont = m_registry.get<ContainerComponent>(task.target_entity);
                                        if (!cont.contained_items.empty()) {
                                            mule.carried_item = cont.contained_items.back();
                                            cont.contained_items.pop_back();
                                            mule.is_carrying = true;
                                            auto market_view = m_registry.view<PositionComponent, ShopComponent>();
                                            if (market_view.begin() != market_view.end()) mule.destination_entity = *market_view.begin();
                                        }
                                    }
                                } else {
                                    if (m_registry.all_of<ContainerComponent>(task.target_entity)) {
                                        auto& cont = m_registry.get<ContainerComponent>(task.target_entity);
                                        cont.contained_items.push_back(mule.carried_item);
                                        mule.carried_item = entt::null;
                                        mule.is_carrying = false;
                                    }
                                }
                            }
                        }
                    }

                    m_registry.remove<AgentTaskComponent>(entity);
                }
                break;
            }

            case AgentTaskType::REACT_TO_MUGGING: {
                auto mugger = task.target_entity;
                if (!m_registry.valid(mugger)) {
                    m_registry.remove<AgentTaskComponent>(entity);
                    break;
                }

                auto self_econ_ptr = m_registry.try_get<Layer3EconomicComponent>(entity);
                auto crime_ptr = m_registry.try_get<CrimeRiskComponent>(mugger);

                if (self_econ_ptr && crime_ptr) {
                    // 70% chance to comply if they have credits
                    if (rand() % 100 < 70 && self_econ_ptr->cash_on_hand > 0) {
                        int amount = std::min(self_econ_ptr->cash_on_hand, 50 + (int)(crime_ptr->boldness));
                        self_econ_ptr->cash_on_hand -= amount;

                        auto& mugger_econ = m_registry.get<Layer3EconomicComponent>(mugger);
                        mugger_econ.cash_on_hand += amount;

                        m_dispatcher.enqueue<SpeechEvent>({entity, "Take it, just don't hurt me!", 10, AudibilityLevel::CLEAR});

                        if (m_registry.all_of<AgentTaskComponent>(mugger)) {
                            auto& mugger_task = m_registry.get<AgentTaskComponent>(mugger);
                            if (mugger_task.task_type == AgentTaskType::AWAIT_MUGGING_COMPLIANCE) {
                                m_registry.remove<AgentTaskComponent>(mugger);
                            }
                        }
                        m_registry.remove<AgentTaskComponent>(entity);
                    } else {
                        // Flee
                        m_dispatcher.enqueue<SpeechEvent>({entity, "GUARDS! HELP!", 20, AudibilityLevel::CLEAR});
                        m_dispatcher.trigger<CrimeReportEvent>({mugger, entity, position.x, position.y, position.layer_id});

                        task.task_type = AgentTaskType::FLEE;
                        task.target_entity = mugger;
                        
                        if (m_registry.all_of<AgentTaskComponent>(mugger)) {
                             m_registry.remove<AgentTaskComponent>(mugger);
                        }
                    }
                } else {
                    m_registry.remove<AgentTaskComponent>(entity);
                }
                break;
            }

            case AgentTaskType::FLEE: {
                if (m_registry.valid(task.target_entity)) {
                    auto& t_pos = m_registry.get<PositionComponent>(task.target_entity);
                    int dx = (position.x > t_pos.x) ? 1 : (position.x < t_pos.x ? -1 : distrib(gen));
                    int dy = (position.y > t_pos.y) ? 1 : (position.y < t_pos.y ? -1 : distrib(gen));
                    m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                }
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }

            case AgentTaskType::AWAIT_MUGGING_COMPLIANCE:
                break;

            case AgentTaskType::WANDER: {
                int dx = distrib(gen);
                int dy = distrib(gen);
                m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }

            case AgentTaskType::WORSHIP: {
                // Agent is at a worship place. Worship for a few ticks.
                // For now, let's just emit the event and finish.
                auto city_view = m_registry.view<CityComponent>();
                uint64_t tick = 0;
                if (!city_view.empty()) {
                    tick = city_view.get<CityComponent>(city_view.front()).time_tick;
                }

                if (auto* religiosity = m_registry.try_get<ReligiosityComponent>(entity)) {
                    // Find the nearest worship place again (or cache it in the task)
                    // For simplicity, we assume they are at one if the decision system set this task.
                    
                    // Trigger worship event
                    m_dispatcher.trigger<WorshipEvent>({entity, religiosity->religion_id, entt::null, tick});
                    
                    if (tick % 5 == 0) { // Speech flavor
                         m_dispatcher.enqueue<SpeechEvent>({entity, "In prayer.", 5, AudibilityLevel::MUFFLED});
                    }
                }
                
                // Let them stay here for a bit (random chance to finish)
                static std::uniform_real_distribution<> finish_chance(0, 1);
                if (finish_chance(gen) < 0.2f) {
                    m_registry.remove<AgentTaskComponent>(entity);
                }
                break;
            }

            case AgentTaskType::AWAITING_PATH:
            case AgentTaskType::WAIT_FOR_TRANSIT:
            case AgentTaskType::RIDE_TRANSIT:
                break;

            case AgentTaskType::IDLE:
            default: {
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }
        }
    }
}

} // namespace NeonOubliette
