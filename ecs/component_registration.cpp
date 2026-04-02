#include "component_declarations.h"
#include "entt/entt.hpp"
#include "event_declarations.h"

namespace NeonOubliette {

/**
 * @brief Ensures all component types are known and facilitates potential reflection/meta registration.
 *        EnTT components are implicitly registered by virtue of being used.
 */
void register_all_components(entt::registry& registry) {
    // This serves as a central point for any explicit registration logic.
    (void)registry;
}

/**
 * @brief Registers all event types with the dispatcher.
 *        While EnTT's dispatcher doesn't require explicit registration for standard usage,
 *        this provides a unified location for event-related setup or documentation.
 */
void register_all_events(entt::dispatcher& dispatcher) {
    // Simulation Cycle Events
    dispatcher.sink<TurnEvent>();
    dispatcher.sink<AdvanceTurnRequestEvent>();
    
    // Core Movement & Interaction
    dispatcher.sink<MoveEvent>();
    dispatcher.sink<PlayerMoveEvent>();
    dispatcher.sink<PickupItemEvent>();
    dispatcher.sink<InspectEvent>();
    dispatcher.sink<UseItemEvent>();
    dispatcher.sink<ConsumeItemEvent>();
    dispatcher.sink<ContainerInteractionEvent>();
    dispatcher.sink<BuildingEntranceEvent>();
    dispatcher.sink<InteriorActivatedEvent>();

    // Activity & Progress
    dispatcher.sink<StartActivityEvent>();
    dispatcher.sink<ActivityProgressEvent>();
    dispatcher.sink<ActivityCompletedEvent>();
    dispatcher.sink<ActivityInterruptedEvent>();

    // Economy & Barter
    dispatcher.sink<BarterEvent>();
    dispatcher.sink<ChangeFactionStandingEvent>();
    dispatcher.sink<PurchaseEvent>();
    dispatcher.sink<CommerceEvent>();
    dispatcher.sink<RawMaterialDeliveryEvent>();

    // Infrastructure & Environment
    dispatcher.sink<InfrastructureDegradationEvent>();
    dispatcher.sink<InfrastructureBreakdownEvent>();
    dispatcher.sink<InfrastructureRepairedEvent>();
    dispatcher.sink<DegradationEvent>();
    dispatcher.sink<WaterMainBreakEvent>();
    dispatcher.sink<PowerOutageEvent>();
    dispatcher.sink<CongestionEvent>();

    // Social & Political
    dispatcher.sink<PolicyChangeEvent>();
    dispatcher.sink<BackroomDealEvent>();
    dispatcher.sink<CampaignEvent>();
    dispatcher.sink<ScandalEvent>();
    dispatcher.sink<ElectionEvent>();
    dispatcher.sink<LegislationEvent>();
    dispatcher.sink<CorruptionEvent>();
    dispatcher.sink<DiscoveryEvent>();
    dispatcher.sink<DiseaseEvent>();
    dispatcher.sink<JobOpeningEvent>();
    dispatcher.sink<LayoffEvent>();

    // Pathfinding
    dispatcher.sink<PathfindingRequestEvent>();
    dispatcher.sink<PathfindingResponseEvent>();

    // System Events
    dispatcher.sink<LogEvent>();
    dispatcher.sink<HUDNotificationEvent>();
    dispatcher.sink<ShutdownEvent>();
    dispatcher.sink<SaveGameEvent>();
    dispatcher.sink<LoadGameEvent>();
    dispatcher.sink<InventoryToggleEvent>();
    dispatcher.sink<PlayerLayerChangeEvent>();
}

// Legacy Redirection for backward compatibility
namespace ECS {
    void register_all_components(entt::registry& registry) {
        NeonOubliette::register_all_components(registry);
    }
    void register_all_events(entt::dispatcher& dispatcher) {
        NeonOubliette::register_all_events(dispatcher);
    }
}

} // namespace NeonOubliette
