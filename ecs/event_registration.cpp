#include "entt_event_registration.h"
#include "event_declarations.h"

void register_events(entt::dispatcher& dispatcher) {
    // UI/HUD Events
    dispatcher.sink<HUDNotificationEvent>().connect<&entt::dispatcher::trigger<HUDNotificationEvent>>(dispatcher);

    // Input Events (dispatched by InputSystem, received by various)
    dispatcher.sink<PlayerMoveEvent>().connect<&entt::dispatcher::trigger<PlayerMoveEvent>>(dispatcher);
    dispatcher.sink<PlayerLayerChangeEvent>().connect<&entt::dispatcher::trigger<PlayerLayerChangeEvent>>(dispatcher);
    dispatcher.sink<MoveEvent>().connect<&entt::dispatcher::trigger<MoveEvent>>(dispatcher);
    dispatcher.sink<AdvanceTurnRequestEvent>().connect<&entt::dispatcher::trigger<AdvanceTurnRequestEvent>>(dispatcher);

    // Game State Events
    dispatcher.sink<TurnEvent>().connect<&entt::dispatcher::trigger<TurnEvent>>(dispatcher);
    dispatcher.sink<SaveGameEvent>().connect<&entt::dispatcher::trigger<SaveGameEvent>>(dispatcher);
    dispatcher.sink<LoadGameEvent>().connect<&entt::dispatcher::trigger<LoadGameEvent>>(dispatcher);

    // Interaction Events
    dispatcher.sink<PickupItemEvent>().connect<&entt::dispatcher::trigger<PickupItemEvent>>(dispatcher);
    dispatcher.sink<InspectEvent>().connect<&entt::dispatcher::trigger<InspectEvent>>(dispatcher);
    dispatcher.sink<UseItemEvent>().connect<&entt::dispatcher::trigger<UseItemEvent>>(dispatcher);
    dispatcher.sink<ContainerInteractionEvent>().connect<&entt::dispatcher::trigger<ContainerInteractionEvent>>(dispatcher);

    // Crafting Events
    dispatcher.sink<CraftItemEvent>().connect<&entt::dispatcher::trigger<CraftItemEvent>>(dispatcher);

    // Pathfinding Events
    dispatcher.sink<PathRequestEvent>().connect<&entt::dispatcher::trigger<PathRequestEvent>>(dispatcher);
    dispatcher.sink<PathReadyEvent>().connect<&entt::dispatcher::trigger<PathReadyEvent>>(dispatcher);

    // Logging Events
    dispatcher.sink<LogEvent>().connect<&entt::dispatcher::trigger<LogEvent>>(dispatcher);

    // Activity Events
    dispatcher.sink<StartActivityEvent>().connect<&entt::dispatcher::trigger<StartActivityEvent>>(dispatcher);
    dispatcher.sink<ActivityProgressEvent>().connect<&entt::dispatcher::trigger<ActivityProgressEvent>>(dispatcher);
    dispatcher.sink<ActivityCompletedEvent>().connect<&entt::dispatcher::trigger<ActivityCompletedEvent>>(dispatcher);
    dispatcher.sink<ActivityInterruptedEvent>().connect<&entt::dispatcher::trigger<ActivityInterruptedEvent>>(dispatcher);
}
