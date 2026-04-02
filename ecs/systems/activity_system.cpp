#include "activity_system.h"
#include <iostream>

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

            // HUD for progress
            m_dispatcher.enqueue<HUDNotificationEvent>({
                name.name + " is " + activity.description + " (" + std::to_string(activity.total_turns_required - activity.turns_remaining) + "/" + std::to_string(activity.total_turns_required) + ")",
                2.0f,
                "#FFFFFF"
            });
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

            // HUD for completion
            m_dispatcher.enqueue<HUDNotificationEvent>({
                name.name + " completed " + activity.description + "!",
                3.0f,
                "#00FF00"
            });
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
