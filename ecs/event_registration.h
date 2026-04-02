#ifndef EVENT_REGISTRATION_H
#define EVENT_REGISTRATION_H

#include "event_declarations.h"
#include <entt/entt.hpp>

// Function to register all event types with the EnTT dispatcher
inline void register_events(entt::dispatcher& dispatcher) {
    dispatcher.sink<MoveEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<PlayerLayerChangeEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<HUDNotificationEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<PickupItemEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<InspectEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<SaveGameEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<LoadGameEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<TurnEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<AdvanceTurnRequestEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<LogEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<ConsumeItemEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<UseItemEvent>().connect<&entt::dispatcher::update>();
    dispatcher.sink<ContainerInteractionEvent>().connect<&entt::dispatcher::update>(); // Register ContainerInteractionEvent
}

#endif // EVENT_REGISTRATION_H
