#include "guard_response_system.h"
#include <cmath>
#include <algorithm>
#include <random>

#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/zoning_components.h"

namespace NeonOubliette {

GuardResponseSystem::GuardResponseSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<CrimeReportEvent>().connect<&GuardResponseSystem::handleCrimeReport>(*this);
    m_dispatcher.sink<WantedAlertEvent>().connect<&GuardResponseSystem::handleWantedAlert>(*this);
}

void GuardResponseSystem::update(double delta_time) {
    (void)delta_time; // Turn-based

    auto guard_view = m_registry.view<PositionComponent, AgentTaskComponent, PatrolComponent>();
    for (auto guard : guard_view) {
        auto& task = guard_view.get<AgentTaskComponent>(guard);
        auto& pos = guard_view.get<PositionComponent>(guard);

        if (task.task_type == AgentTaskType::PURSUE) {
            if (m_registry.valid(task.target_entity)) {
                processPursue(guard, task.target_entity, task, pos);
            } else {
                task.task_type = AgentTaskType::IDLE;
            }
        } else if (task.task_type == AgentTaskType::INVESTIGATE) {
            processInvestigate(guard, task, pos);
        } else if (task.task_type == AgentTaskType::ARREST) {
            if (m_registry.valid(task.target_entity)) {
                processArrest(guard, task.target_entity, task);
            } else {
                task.task_type = AgentTaskType::IDLE;
            }
        }
    }
}

void GuardResponseSystem::handleCrimeReport(const CrimeReportEvent& event) {
    auto guard_view = m_registry.view<PositionComponent, PatrolComponent, AgentTaskComponent>();
    for (auto guard : guard_view) {
        auto& g_pos = guard_view.get<PositionComponent>(guard);
        auto& g_task = guard_view.get<AgentTaskComponent>(guard);

        // Don't interrupt an existing arrest or pursue
        if (g_task.task_type == AgentTaskType::PURSUE || g_task.task_type == AgentTaskType::ARREST) continue;

        if (g_pos.layer_id == event.layer) {
            int dist = std::abs(g_pos.x - event.x) + std::abs(g_pos.y - event.y);
            if (dist < 20) {
                g_task.task_type = AgentTaskType::PURSUE;
                g_task.target_entity = event.perpetrator;
                
                auto& goal = m_registry.get_or_emplace<GoalComponent>(guard);
                goal.target_x = event.x;
                goal.target_y = event.y;
                goal.target_layer = event.layer;

                m_dispatcher.enqueue<PathfindingRequestEvent>({guard, g_pos, {event.x, event.y, event.layer}, m_nextPathRequestId++});
                m_dispatcher.enqueue<SpeechEvent>({guard, "Reporting unit engaging suspect!", 15, AudibilityLevel::CLEAR});
            }
        }
    }
}

void GuardResponseSystem::handleWantedAlert(const WantedAlertEvent& event) {
    auto guard_view = m_registry.view<PositionComponent, VisibilityComponent, PatrolComponent, AgentTaskComponent, Layer4PoliticalComponent>();
    for (auto guard : guard_view) {
        const auto& pol = guard_view.get<Layer4PoliticalComponent>(guard);
        if (pol.primary_faction != event.reporting_faction_id) continue;

        auto& vis = guard_view.get<VisibilityComponent>(guard);
        auto* t_pos = m_registry.try_get<PositionComponent>(event.target);
        
        if (t_pos && t_pos->layer_id == guard_view.get<PositionComponent>(guard).layer_id) {
            if (vis.visible_tiles.count(*t_pos)) {
                auto& g_task = guard_view.get<AgentTaskComponent>(guard);
                if (g_task.task_type != AgentTaskType::PURSUE && g_task.task_type != AgentTaskType::ARREST) {
                    g_task.task_type = AgentTaskType::PURSUE;
                    g_task.target_entity = event.target;
                    
                    auto& goal = m_registry.get_or_emplace<GoalComponent>(guard);
                    goal.target_x = t_pos->x;
                    goal.target_y = t_pos->y;
                    goal.target_layer = t_pos->layer_id;

                    m_dispatcher.enqueue<PathfindingRequestEvent>({guard, guard_view.get<PositionComponent>(guard), *t_pos, m_nextPathRequestId++});
                    m_dispatcher.enqueue<SpeechEvent>({guard, "Suspect identified. Moving to intercept.", 20, AudibilityLevel::CLEAR});
                }
            }
        }
    }
}

void GuardResponseSystem::processPursue(entt::entity guard, entt::entity target, AgentTaskComponent& task, PositionComponent& pos) {
    auto* t_pos = m_registry.try_get<PositionComponent>(target);
    if (!t_pos) {
        task.task_type = AgentTaskType::INVESTIGATE;
        return;
    }

    auto& vis = m_registry.get<VisibilityComponent>(guard);
    bool can_see = (pos.layer_id == t_pos->layer_id) && vis.visible_tiles.count(*t_pos);

    if (can_see) {
        // Update Goal (LKP)
        auto& goal = m_registry.get_or_emplace<GoalComponent>(guard);
        goal.target_x = t_pos->x;
        goal.target_y = t_pos->y;
        goal.target_layer = t_pos->layer_id;

        int dist = std::abs(pos.x - t_pos->x) + std::abs(pos.y - t_pos->y);
        if (dist <= 1) {
            task.task_type = AgentTaskType::ARREST;
            return;
        }

        // Periodic path refresh
        auto city_view = m_registry.view<CityComponent>();
        if (!city_view.empty()) {
            uint64_t current_tick = m_registry.get<CityComponent>(city_view.front()).time_tick;
            if (current_tick % 5 == 0) {
                 m_dispatcher.enqueue<PathfindingRequestEvent>({guard, pos, *t_pos, m_nextPathRequestId++});
            }
            if (current_tick % 10 == 0) {
                 m_dispatcher.enqueue<SpeechEvent>({guard, "Halt! You're flagged!", 15, AudibilityLevel::CLEAR});
            }
        }
    } else {
        // Target lost. Move to LKP.
        auto& goal = m_registry.get<GoalComponent>(guard);
        if (pos.x == goal.target_x && pos.y == goal.target_y && pos.layer_id == goal.target_layer) {
            task.task_type = AgentTaskType::INVESTIGATE;
            // Set some search parameters here if needed
        }
    }
}

void GuardResponseSystem::processInvestigate(entt::entity guard, AgentTaskComponent& task, PositionComponent& pos) {
    // Wander around LKP for a few ticks
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distrib(-2, 2);

    auto city_view = m_registry.view<CityComponent>();
    if (!city_view.empty()) {
        uint64_t current_tick = m_registry.get<CityComponent>(city_view.front()).time_tick;
        
        if (current_tick % 2 == 0) {
             int dx = distrib(gen);
             int dy = distrib(gen);
             m_dispatcher.trigger<MoveEvent>({guard, dx, dy, pos.layer_id});
        }
        
        if (current_tick % 10 == 0) {
             m_dispatcher.enqueue<SpeechEvent>({guard, "Suspect lost. Searching area.", 15, AudibilityLevel::MUFFLED});
        }

        // Stop investigating after 20 ticks
        // (Wait, we need a way to track investigation time. I'll add a temporary counter in AgentTaskComponent or just random chance)
        static std::uniform_real_distribution<> finish_chance(0, 1);
        if (finish_chance(gen) < 0.05f) {
            task.task_type = AgentTaskType::IDLE;
        }
    }
}

void GuardResponseSystem::processArrest(entt::entity guard, entt::entity target, AgentTaskComponent& task) {
    m_dispatcher.enqueue<SpeechEvent>({guard, "You are under arrest. Surrender contraband immediately.", 30, AudibilityLevel::CLEAR});

    // Handle Player Arrest
    if (m_registry.all_of<PlayerComponent>(target)) {
        m_dispatcher.enqueue<HUDNotificationEvent>({"ARRESTED! Goods confiscated.", 4.0f, "#FF0000"});
        
        // Confiscate
        auto* inventory = m_registry.try_get<InventoryComponent>(target);
        if (inventory) {
            std::vector<entt::entity> to_remove;
            for (auto item : inventory->contained_items) {
                if (m_registry.all_of<StolenComponent>(item) || m_registry.all_of<ContrabandComponent>(item)) {
                    to_remove.push_back(item);
                }
            }
            for (auto item : to_remove) {
                // Remove from player inventory
                auto it = std::find(inventory->contained_items.begin(), inventory->contained_items.end(), item);
                if (it != inventory->contained_items.end()) {
                    inventory->contained_items.erase(it);
                    // In a fuller sim, move to evidence locker. For now, delete.
                    m_registry.destroy(item);
                }
            }
        }

        // Confiscate credits (fine)
        auto* hud = m_registry.try_get<HUDComponent>(target);
        if (hud) {
            hud->credits = std::max(0, hud->credits - 100);
        }

        // Reduce Wanted Level
        auto* wanted = m_registry.try_get<WantedComponent>(target);
        if (wanted) {
            auto* pol = m_registry.try_get<Layer4PoliticalComponent>(guard);
            if (pol) {
                wanted->faction_wanted_levels[pol->primary_faction] = 0;
            }
            wanted->global_notoriety *= 0.5f;
        }

        // Teleport
        entt::entity holding_cell = findNearestHoldingCell(m_registry.get<PositionComponent>(target));
        if (m_registry.valid(holding_cell)) {
            const auto& hc_pos = m_registry.get<PositionComponent>(holding_cell);
            auto& p_pos = m_registry.get<PositionComponent>(target);
            p_pos.x = hc_pos.x;
            p_pos.y = hc_pos.y;
            p_pos.layer_id = hc_pos.layer_id;
        } else {
            // Default teleport to spawn or similar
             auto& p_pos = m_registry.get<PositionComponent>(target);
             p_pos.x = 10; p_pos.y = 10; p_pos.layer_id = 0;
        }
    }

    // Reset Guard Task
    task.task_type = AgentTaskType::IDLE;
}

entt::entity GuardResponseSystem::findNearestHoldingCell(const PositionComponent& pos) {
    // Look for a building with RoomTag::OFFICE or similar (placeholder for specialized JAIL tag)
    auto b_view = m_registry.view<BuildingInteriorComponent, PositionComponent>();
    entt::entity best = entt::null;
    int min_dist = 1000;

    for (auto b : b_view) {
        const auto& b_pos = b_view.get<PositionComponent>(b);
        if (b_pos.layer_id != 0) continue; // Building roots are at layer 0

        const auto& interior = b_view.get<BuildingInteriorComponent>(b);
        bool has_security_room = false;
        for (const auto& room : interior.rooms) {
            if (room.tag == RoomTag::HOLDING_CELL || room.tag == RoomTag::SECURITY_HUB) {
                has_security_room = true;
                break;
            }
        }

        if (has_security_room) {
            int dist = std::abs(pos.x - b_pos.x) + std::abs(pos.y - b_pos.y);
            if (dist < min_dist) {
                min_dist = dist;
                best = b;
            }
        }
    }
    return best;
}

} // namespace NeonOubliette
