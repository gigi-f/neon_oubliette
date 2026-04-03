#include "agent_action_system.h"

#include <cmath>
#include <random>

#include "../components/components.h"
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

    auto agent_view = m_registry.view<AgentComponent, PositionComponent, NameComponent, AgentTaskComponent>();
    for (auto entity : agent_view) {
        auto& position = agent_view.get<PositionComponent>(entity);
        auto& name = agent_view.get<NameComponent>(entity);
        auto& task = agent_view.get<AgentTaskComponent>(entity);

        switch (task.task_type) {
            case AgentTaskType::SEEK_FOOD:
            case AgentTaskType::SEEK_WATER: {
                if (m_registry.all_of<GoalComponent>(entity)) {
                    auto& goal = m_registry.get<GoalComponent>(entity);
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
                        m_dispatcher.trigger<HUDNotificationEvent>(
                            {name.name + " reached its goal!", 1.5f, "#FFFFFF"});
                        m_registry.remove<GoalComponent>(entity);
                        m_registry.remove<AgentTaskComponent>(entity);
                    } else {
                        m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                    }
                } else {
                    m_registry.remove<AgentTaskComponent>(entity);
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

            case AgentTaskType::MOVE_ALONG_PATH: {
                if (m_registry.all_of<CurrentPathComponent>(entity)) {
                    auto& path_comp = m_registry.get<CurrentPathComponent>(entity);
                    if (path_comp.current_step_index < path_comp.path.size()) {
                        auto next_pos = path_comp.path[path_comp.current_step_index];
                        int dx = next_pos.x - position.x;
                        int dy = next_pos.y - position.y;

                        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
                            m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                            path_comp.current_step_index++;
                        } else {
                            m_registry.remove<CurrentPathComponent>(entity);
                            m_registry.remove<AgentTaskComponent>(entity);
                        }

                        if (path_comp.current_step_index >= path_comp.path.size()) {
                            m_dispatcher.trigger<HUDNotificationEvent>(
                                {name.name + " completed its path.", 1.5f, "#FFFFFF"});
                            m_registry.remove<CurrentPathComponent>(entity);
                            m_registry.remove<AgentTaskComponent>(entity);
                        }
                    } else {
                        m_registry.remove<CurrentPathComponent>(entity);
                        m_registry.remove<AgentTaskComponent>(entity);
                    }
                }
                break;
            }

            case AgentTaskType::WANDER:
            case AgentTaskType::IDLE:
            default: {
                int dx = distrib(gen);
                int dy = distrib(gen);
                m_dispatcher.trigger<MoveEvent>({entity, dx, dy, position.layer_id});
                m_registry.remove<AgentTaskComponent>(entity);
                break;
            }
        }
    }
}

} // namespace NeonOubliette
