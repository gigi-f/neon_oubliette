#include "activity_system.h"
#include "../components/simulation_layers.h"

namespace NeonOubliette {

ActivitySystem::ActivitySystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<TurnEvent>().connect<&ActivitySystem::handleTurnEvent>(*this);
    m_dispatcher.sink<StartActivityEvent>().connect<&ActivitySystem::handleStartActivityEvent>(*this);
    m_dispatcher.sink<ActivityCompletedEvent>().connect<&ActivitySystem::handleActivityCompletedEvent>(*this);
}

void ActivitySystem::handleTurnEvent(const TurnEvent& event)
{
    auto view = m_registry.view<ActivityComponent, NameComponent>();
    for (auto entity : view)
    {
        auto& activity = view.get<ActivityComponent>(entity);
        auto& name = view.get<NameComponent>(entity);

        if (activity.turns_remaining > 0)
        {
            activity.turns_remaining--;
            
            // [O.4] Factory Labor Contribution
            if (activity.type == ActivityType::WORKING && m_registry.valid(activity.target_entity) && m_registry.all_of<FactoryComponent>(activity.target_entity)) {
                auto& factory = m_registry.get<FactoryComponent>(activity.target_entity);
                if (factory.is_active && !factory.available_recipes.empty()) {
                    float contribution = 1.0f; // Could scale with skills
                    // Apply efficiency factors
                    auto* disruption = m_registry.try_get<SupplyChainDisruptionComponent>(activity.target_entity);
                    float efficiency = (disruption) ? (1.0f - disruption->labor_strike) : 1.0f;
                    
                    factory.current_progress += contribution * efficiency * factory.base_efficiency;
                }
            }

            // Dispatch progress event
            m_dispatcher.enqueue<ActivityProgressEvent>({
                entity, activity.type, activity.turns_remaining, activity.total_turns_required
            });

            // Log progress
            m_dispatcher.enqueue<LogEvent>({
                "ActivitySystem: " + name.name + " is " + activity.description + ". Turns remaining: " + std::to_string(activity.turns_remaining),
                LogSeverity::DEBUG,
                "ActivitySystem"
            });

            /* [MOD] Remove HUD spam for NPC activities
            m_dispatcher.enqueue<HUDNotificationEvent>({
                name.name + " is " + activity.description + " (" + std::to_string(activity.total_turns_required - activity.turns_remaining) + "/" + std::to_string(activity.total_turns_required) + ")",
                2.0f,
                "#FFFFFF"
            });
            */
        }

        if (activity.turns_remaining == 0)
        {
            // Dispatch completion event
            m_dispatcher.enqueue<ActivityCompletedEvent>({
                entity, activity.type, activity.target_entity, activity.secondary_entity
            });
            // Remove the ActivityComponent as it's completed
            m_registry.remove<ActivityComponent>(entity);

            // Log completion
            m_dispatcher.enqueue<LogEvent>({
                "ActivitySystem: " + name.name + " completed " + activity.description + ".",
                LogSeverity::INFO,
                "ActivitySystem"
            });

            /* [MOD] Remove HUD spam for NPC activities
            m_dispatcher.enqueue<HUDNotificationEvent>({
                name.name + " completed " + activity.description + "!",
                3.0f,
                "#00FF00"
            });
            */
        }
    }
}

void ActivitySystem::handleStartActivityEvent(const StartActivityEvent& event)
{
    // Check if entity already has an activity
    if (m_registry.any_of<ActivityComponent>(event.actor_entity))
    {
        m_dispatcher.enqueue<LogEvent>({
            "ActivitySystem: " + m_registry.get<NameComponent>(event.actor_entity).name + " already has an active activity. Ignoring StartActivityEvent.",
            LogSeverity::WARNING,
            "ActivitySystem"
        });
        return;
    }

    ActivityComponent new_activity;
    new_activity.type = event.type;
    new_activity.turns_remaining = event.total_turns_required;
    new_activity.total_turns_required = event.total_turns_required;
    new_activity.target_entity = event.target_entity;
    new_activity.secondary_entity = event.secondary_entity;
    new_activity.description = event.description;

    m_registry.emplace<ActivityComponent>(event.actor_entity, new_activity);

    m_dispatcher.enqueue<LogEvent>({
        "ActivitySystem: " + m_registry.get<NameComponent>(event.actor_entity).name + " started " + event.description + " for " + std::to_string(event.total_turns_required) + " turns.",
        LogSeverity::INFO,
        "ActivitySystem"
    });
}

void ActivitySystem::handleActivityCompletedEvent(const ActivityCompletedEvent& event)
{
    if (event.type == ActivityType::WORKING && m_registry.valid(event.target_entity) && m_registry.all_of<FactoryComponent>(event.target_entity)) {
        // [O.4] Pay the worker
        int wage = 50; // Default wage
        if (auto* job = m_registry.try_get<EmploymentContractComponent>(event.actor_entity)) {
            wage = job->wage;
        }

        if (auto* econ = m_registry.try_get<Layer3EconomicComponent>(event.actor_entity)) {
            econ->cash_on_hand += wage;
        }
        
        if (m_registry.all_of<PlayerComponent>(event.actor_entity)) {
            if (auto* hud = m_registry.try_get<HUDComponent>(event.actor_entity)) {
                hud->credits += wage;
            }
            m_dispatcher.trigger<HUDNotificationEvent>({"Shift complete. Earned " + std::to_string(wage) + " CR.", 3.0f, "#00FF00"});
        }

        // [P.2] Reputation boost for working for a faction
        auto& factory = m_registry.get<FactoryComponent>(event.target_entity);
        if (!factory.owning_faction.empty()) {
            m_dispatcher.enqueue<AgentFactionReputationEvent>({
                event.actor_entity,
                factory.owning_faction,
                1.0f // Small consistent boost per shift
            });
        }
    }

    if (event.type == ActivityType::CRAFTING)
    {
        // Re-dispatch the CraftItemEvent
        m_dispatcher.enqueue<CraftItemEvent>({
            event.actor_entity, "recipe_placeholder" // recipes in neon_oubliette use string recipe_id
        });

        m_dispatcher.enqueue<LogEvent>({
            "ActivitySystem: Crafting activity completed. Dispatching CraftItemEvent for " + m_registry.get<NameComponent>(event.actor_entity).name + ".",
            LogSeverity::INFO,
            "ActivitySystem"
        });
    }
}

} // namespace NeonOubliette
