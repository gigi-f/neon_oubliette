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
    (void)dispatcher.sink<TurnEvent>();
    (void)dispatcher.sink<AdvanceTurnRequestEvent>();
    
    // Core Movement & Interaction
    (void)dispatcher.sink<MoveEvent>();
    (void)dispatcher.sink<PlayerMoveEvent>();
    (void)dispatcher.sink<PickupItemEvent>();
    (void)dispatcher.sink<InspectEvent>();
    (void)dispatcher.sink<DialogueEvent>();
    (void)dispatcher.sink<DialogueChoiceEvent>();
    (void)dispatcher.sink<InformationPropagationEvent>();
    (void)dispatcher.sink<InformationCreatedEvent>();
    (void)dispatcher.sink<CloseDialogueWindowEvent>();
    (void)dispatcher.sink<UseItemEvent>();
    (void)dispatcher.sink<ConsumeItemEvent>();
    (void)dispatcher.sink<ContainerInteractionEvent>();
    (void)dispatcher.sink<BuildingEntranceEvent>();
    (void)dispatcher.sink<InteriorActivatedEvent>();

    // Activity & Progress
    (void)dispatcher.sink<StartActivityEvent>();
    (void)dispatcher.sink<ActivityProgressEvent>();
    (void)dispatcher.sink<ActivityCompletedEvent>();
    (void)dispatcher.sink<ActivityInterruptedEvent>();

    // Economy & Barter
    (void)dispatcher.sink<BarterEvent>();
    (void)dispatcher.sink<OpenBarterEvent>();
    (void)dispatcher.sink<CloseBarterEvent>();
    (void)dispatcher.sink<ChangeFactionStandingEvent>();
    (void)dispatcher.sink<AgentFactionReputationEvent>();
    (void)dispatcher.sink<AgentDeathEvent>();
    (void)dispatcher.sink<BirthEvent>();
    (void)dispatcher.sink<PurchaseEvent>();
    (void)dispatcher.sink<CommerceEvent>();
    (void)dispatcher.sink<RawMaterialDeliveryEvent>();
    (void)dispatcher.sink<ResourceExtractedEvent>();
    (void)dispatcher.sink<ProductionCompletedEvent>();
    (void)dispatcher.sink<SupplyChainDisruptedEvent>();

    // Infrastructure & Environment
    (void)dispatcher.sink<InfrastructureDegradationEvent>();
    (void)dispatcher.sink<InfrastructureBreakdownEvent>();
    (void)dispatcher.sink<InfrastructureRepairedEvent>();
    (void)dispatcher.sink<DegradationEvent>();
    (void)dispatcher.sink<WaterMainBreakEvent>();
    (void)dispatcher.sink<PowerOutageEvent>();
    (void)dispatcher.sink<CongestionEvent>();

    // Social & Political
    (void)dispatcher.sink<PolicyChangeEvent>();
    (void)dispatcher.sink<BackroomDealEvent>();
    (void)dispatcher.sink<CampaignEvent>();
    (void)dispatcher.sink<ScandalEvent>();
    (void)dispatcher.sink<ElectionEvent>();
    (void)dispatcher.sink<LegislationEvent>();
    (void)dispatcher.sink<CorruptionEvent>();
    (void)dispatcher.sink<DiscoveryEvent>();
    (void)dispatcher.sink<DiseaseEvent>();
    (void)dispatcher.sink<JobOpeningEvent>();
    (void)dispatcher.sink<LayoffEvent>();

    // Religion
    (void)dispatcher.sink<WorshipEvent>();
    (void)dispatcher.sink<HolyDayEvent>();
    (void)dispatcher.sink<ReligiousTensionEvent>();
    (void)dispatcher.sink<ProselytizingEvent>();
    (void)dispatcher.sink<RaidEvent>();

    // Crime & Security
    (void)dispatcher.sink<CrimeReportEvent>();
    (void)dispatcher.sink<WantedAlertEvent>();

    // Pathfinding
    (void)dispatcher.sink<PathfindingRequestEvent>();
    (void)dispatcher.sink<PathfindingResponseEvent>();

    // System Events
    (void)dispatcher.sink<LogEvent>();
    (void)dispatcher.sink<HUDNotificationEvent>();
    (void)dispatcher.sink<ShutdownEvent>();
    (void)dispatcher.sink<SaveGameEvent>();
    (void)dispatcher.sink<LoadGameEvent>();
    (void)dispatcher.sink<InventoryToggleEvent>();
    (void)dispatcher.sink<PlayerLayerChangeEvent>();
    (void)dispatcher.sink<BroadcastPulseEvent>();
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
